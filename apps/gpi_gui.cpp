#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gpi/research_developer.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
constexpr UINT WM_GPI_LOG=WM_APP+1;
constexpr UINT WM_GPI_DONE=WM_APP+2;
constexpr int ID_PROMPT=1001,ID_ANALYZE=1002,ID_RUN=1003,ID_STOP=1004;
constexpr int ID_ARCHITECTURE=1005,ID_COMPILER=1006,ID_COMPILER_BROWSE=1007;
constexpr int ID_WORKSPACE=1008,ID_WORKSPACE_BROWSE=1009,ID_CONTRACT=1010,ID_LOG=1011;

HWND g_window{},g_prompt{},g_analyze{},g_run{},g_stop{},g_architecture{},g_compiler{};
HWND g_workspace{},g_contract{},g_log{},g_status{};
HFONT g_uiFont{},g_titleFont{},g_monoFont{};
HBRUSH g_background{},g_panel{};
std::thread g_worker;
std::atomic<bool> g_running{false};
bool g_contractAccepted=false;
std::mutex g_jobMutex;
HANDLE g_job{};

struct Handle {
    HANDLE value{};
    Handle()=default;
    explicit Handle(HANDLE handle):value(handle){}
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};

std::string narrow(const std::wstring& value) {
    if(value.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),
        static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(size<=0)throw std::runtime_error("UTF-16 conversion failed");
    std::string result(static_cast<std::size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),
        result.data(),size,nullptr,nullptr))throw std::runtime_error("UTF-16 conversion failed");
    return result;
}

std::wstring widen(const std::string& value) {
    if(value.empty())return {};
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),
        static_cast<int>(value.size()),nullptr,0);
    if(size<=0)throw std::runtime_error("UTF-8 conversion failed");
    std::wstring result(static_cast<std::size_t>(size),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),
        result.data(),size))throw std::runtime_error("UTF-8 conversion failed");
    return result;
}

std::wstring textOf(HWND control) {
    const int length=GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length)+1,L'\0');
    GetWindowTextW(control,result.data(),length+1);result.resize(static_cast<std::size_t>(length));
    return result;
}

void setFont(HWND control,HFONT font=g_uiFont) {
    SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
}

HWND makeControl(const wchar_t* type,const wchar_t* text,DWORD style,int id) {
    HWND control=CreateWindowExW(std::wstring_view(type)==L"Edit"?WS_EX_CLIENTEDGE:0,type,text,
        WS_CHILD|WS_VISIBLE|style,0,0,0,0,g_window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),nullptr);
    if(!control)throw std::runtime_error("Cannot create GUI control");
    setFont(control);return control;
}

void appendLog(const std::wstring& text) {
    const LRESULT end=GetWindowTextLengthW(g_log);
    SendMessageW(g_log,EM_SETSEL,static_cast<WPARAM>(end),end);
    SendMessageW(g_log,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(g_log,EM_SCROLLCARET,0,0);
}

void postLog(const std::wstring& text) {
    auto* copy=new std::wstring(text);
    if(!PostMessageW(g_window,WM_GPI_LOG,0,reinterpret_cast<LPARAM>(copy)))delete copy;
}

std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";std::size_t slashes=0;
    for(const wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;slashes=0;continue;}
        result.append(slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');return result+L'\"';
}

std::filesystem::path executableDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(length==0||length>=buffer.size())throw std::runtime_error("Cannot locate GUI executable");
    return std::filesystem::path(std::wstring(buffer.data(),length)).parent_path();
}

std::filesystem::path uniqueRunDirectory(const std::filesystem::path& root) {
    const auto ticks=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return root/(L"gpi_run_"+std::to_wstring(ticks));
}

void setRunning(bool running,const wchar_t* status) {
    g_running=running;EnableWindow(g_analyze,!running);EnableWindow(g_run,!running&&g_contractAccepted);
    EnableWindow(g_stop,running);EnableWindow(g_prompt,!running);EnableWindow(g_architecture,!running);
    SetWindowTextW(g_status,status);
}

void analyzeGoal() {
    try {
        const auto prompt=textOf(g_prompt);
        if(prompt.empty())throw std::invalid_argument("Bitte zuerst eine Aufgabe eingeben.");
        const auto plan=genomic::gpi::decomposeResearchTask(narrow(prompt),false);
        std::wostringstream report;
        report<<L"Originalziel\r\n"<<prompt<<L"\r\n\r\nInterpretation\r\n"<<widen(plan.contract.normalizedIntent)
              <<L"\r\n\r\nEingabe: "<<widen(plan.contract.inputType)<<L"    Ausgabe: "<<widen(plan.contract.outputType)
              <<L"\r\nVertrag: "<<(plan.contract.accepted?L"AKZEPTIERT":L"ABGELEHNT")
              <<L"    Hash: "<<plan.contract.semanticHash<<L"\r\n";
        for(const auto& operation:plan.contract.requiredOperations)
            report<<L"\r\n  • "<<widen(operation.operation)<<L"  ["<<widen(operation.inputType)
                  <<L" → "<<widen(operation.outputType)<<L"]";
        for(const auto& item:plan.contract.unresolvedRequirements)
            report<<L"\r\n  ⚠ Ungeklärt: "<<widen(item);
        for(const auto& item:plan.contract.contradictions)
            report<<L"\r\n  ✕ Widerspruch: "<<widen(item);
        SetWindowTextW(g_contract,report.str().c_str());
        g_contractAccepted=plan.supported&&plan.contract.accepted;EnableWindow(g_run,g_contractAccepted);
        SetWindowTextW(g_status,plan.contract.accepted?L"Zielvertrag akzeptiert – bereit":L"Ziel wurde sicher abgelehnt");
    } catch(const std::exception& error) {
        MessageBoxW(g_window,widen(error.what()).c_str(),L"GPI Zielanalyse",MB_ICONWARNING);
    }
}

std::wstring selectedArchitecture() {
    const LRESULT index=SendMessageW(g_architecture,CB_GETCURSEL,0,0);
    wchar_t value[64]{};SendMessageW(g_architecture,CB_GETLBTEXT,static_cast<WPARAM>(index),reinterpret_cast<LPARAM>(value));
    if(std::wstring(value)==L"Einzelkontext")return L"single";
    if(std::wstring(value)==L"Isolierte Kompartimente")return L"isolated";
    if(std::wstring(value)==L"Kommunizierende Kompartimente")return L"communicating";
    if(std::wstring(value)==L"Mit Entwicklungserfahrung")return L"experienced";
    return L"compare";
}

void runChild(std::wstring prompt,std::filesystem::path compiler,std::filesystem::path root,std::wstring architecture) {
    DWORD exitCode=1;
    try {
        const auto integrated=executableDirectory()/L"gpi_integrated.exe";
        if(!std::filesystem::is_regular_file(integrated))throw std::runtime_error("gpi_integrated.exe was not found beside the GUI");
        if(compiler!=L"g++"&&!std::filesystem::is_regular_file(compiler))throw std::runtime_error("The selected compiler does not exist");
        std::filesystem::create_directories(root);
        const auto runDirectory=uniqueRunDirectory(root);
        std::wstring command=quote(integrated.wstring())+L" --prompt "+quote(prompt)+L" --workspace "+
            quote(runDirectory.wstring())+L" --compiler "+quote(compiler.wstring())+
            L" --architecture "+architecture+L" --cycles 2 --autonomous-cycles 3";

        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        HANDLE rawRead{},rawWrite{};
        if(!CreatePipe(&rawRead,&rawWrite,&security,0))throw std::runtime_error("Cannot create output channel");
        Handle readPipe(rawRead),writePipe(rawWrite);
        SetHandleInformation(readPipe.value,HANDLE_FLAG_INHERIT,0);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);startup.hStdOutput=writePipe.value;startup.hStdError=writePipe.value;
        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(L'\0');
        if(!CreateProcessW(integrated.c_str(),mutableCommand.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,integrated.parent_path().c_str(),&startup,&process))
            throw std::runtime_error("Cannot start integrated GPI process");
        Handle processHandle(process.hProcess),threadHandle(process.hThread);
        Handle job(CreateJobObjectW(nullptr,nullptr));
        if(!job.value)throw std::runtime_error("Cannot create cancellation job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
           !AssignProcessToJobObject(job.value,processHandle.value))
            throw std::runtime_error("Cannot secure GPI process tree");
        {
            std::lock_guard lock(g_jobMutex);g_job=job.value;
        }
        if(ResumeThread(threadHandle.value)==static_cast<DWORD>(-1))throw std::runtime_error("Cannot resume GPI process");
        CloseHandle(writePipe.value);writePipe.value=nullptr;
        postLog(L"Arbeitsordner: "+runDirectory.wstring()+L"\r\n\r\n");
        char buffer[4096];DWORD bytes=0;
        while(ReadFile(readPipe.value,buffer,sizeof(buffer),&bytes,nullptr)&&bytes>0)
            postLog(widen(std::string(buffer,buffer+bytes)));
        WaitForSingleObject(processHandle.value,INFINITE);GetExitCodeProcess(processHandle.value,&exitCode);
        {
            std::lock_guard lock(g_jobMutex);g_job=nullptr;
        }
    } catch(const std::exception& error) {
        std::lock_guard lock(g_jobMutex);g_job=nullptr;
        postLog(L"\r\nGUI-FEHLER: "+widen(error.what())+L"\r\n");
    }
    PostMessageW(g_window,WM_GPI_DONE,exitCode,0);
}

void startRun() {
    if(g_running)return;
    try {
        const auto prompt=textOf(g_prompt),compiler=textOf(g_compiler),workspace=textOf(g_workspace);
        if(prompt.empty()||compiler.empty()||workspace.empty())throw std::invalid_argument("Aufgabe, Compiler und Arbeitsordner sind erforderlich.");
        const auto plan=genomic::gpi::decomposeResearchTask(narrow(prompt),false);
        if(!plan.supported||!plan.contract.accepted)throw std::invalid_argument("Der Zielvertrag muss vor dem Start akzeptiert sein.");
        SetWindowTextW(g_log,L"");setRunning(true,L"Entwicklung läuft …");
        if(g_worker.joinable())g_worker.join();
        g_worker=std::thread(runChild,prompt,std::filesystem::path(compiler),std::filesystem::path(workspace),selectedArchitecture());
    } catch(const std::exception& error) {
        MessageBoxW(g_window,widen(error.what()).c_str(),L"GPI starten",MB_ICONWARNING);
    }
}

void stopRun() {
    std::lock_guard lock(g_jobMutex);
    if(g_job)TerminateJobObject(g_job,1223);
    SetWindowTextW(g_status,L"Abbruch angefordert …");
}

void browseCompiler() {
    wchar_t file[MAX_PATH]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g_window;
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrFilter=L"C++ Compiler (*.exe)\0*.exe\0Alle Dateien\0*.*\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&dialog))SetWindowTextW(g_compiler,file);
}

void browseWorkspace() {
    BROWSEINFOW info{};info.hwndOwner=g_window;info.lpszTitle=L"Basisordner für neue GPI-Läufe auswählen";
    info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    if(PIDLIST_ABSOLUTE item=SHBrowseForFolderW(&info)) {
        wchar_t path[MAX_PATH]{};if(SHGetPathFromIDListW(item,path))SetWindowTextW(g_workspace,path);CoTaskMemFree(item);
    }
}

void layout(int width,int height) {
    const int margin=24,left=width*46/100,rightX=left+margin;
    MoveWindow(g_prompt,margin,92,left-margin*2,126,TRUE);
    MoveWindow(g_analyze,margin,230,150,34,TRUE);MoveWindow(g_run,186,230,150,34,TRUE);MoveWindow(g_stop,348,230,120,34,TRUE);
    MoveWindow(g_architecture,margin,302,left-margin*2,180,TRUE);
    MoveWindow(g_compiler,margin,374,left-margin*2-90,28,TRUE);MoveWindow(GetDlgItem(g_window,ID_COMPILER_BROWSE),left-102,374,78,28,TRUE);
    MoveWindow(g_workspace,margin,442,left-margin*2-90,28,TRUE);MoveWindow(GetDlgItem(g_window,ID_WORKSPACE_BROWSE),left-102,442,78,28,TRUE);
    MoveWindow(g_contract,margin,512,left-margin*2,height-548,TRUE);
    MoveWindow(g_log,rightX,92,width-rightX-margin,height-128,TRUE);
    MoveWindow(g_status,width-350,26,326,28,TRUE);
    MoveWindow(GetDlgItem(g_window,2001),margin,66,260,22,TRUE);MoveWindow(GetDlgItem(g_window,2002),rightX,66,260,22,TRUE);
    MoveWindow(GetDlgItem(g_window,2003),margin,278,260,20,TRUE);MoveWindow(GetDlgItem(g_window,2004),margin,350,260,20,TRUE);
    MoveWindow(GetDlgItem(g_window,2005),margin,418,260,20,TRUE);MoveWindow(GetDlgItem(g_window,2006),margin,488,260,20,TRUE);
}

LRESULT CALLBACK windowProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_CREATE: {
        g_window=window;
        g_titleFont=CreateFontW(-25,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_uiFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_monoFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Cascadia Mono");
        HWND title=makeControl(WC_STATICW,L"GPI · Integriertes Entwicklungs- und Forschungssystem",0,0);setFont(title,g_titleFont);
        MoveWindow(title,24,22,720,34,TRUE);
        g_status=makeControl(WC_STATICW,L"Bereit",SS_RIGHT,0);
        makeControl(WC_STATICW,L"AUFGABE",0,2001);makeControl(WC_STATICW,L"LIVE-PROTOKOLL UND ERGEBNIS",0,2002);
        makeControl(WC_STATICW,L"ARCHITEKTUR",0,2003);makeControl(WC_STATICW,L"C++20-COMPILER",0,2004);
        makeControl(WC_STATICW,L"ARBEITSORDNER",0,2005);makeControl(WC_STATICW,L"ZIELVERTRAG",0,2006);
        g_prompt=makeControl(WC_EDITW,L"Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne die Summe",
            ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,ID_PROMPT);
        g_analyze=makeControl(WC_BUTTONW,L"Ziel analysieren",BS_PUSHBUTTON,ID_ANALYZE);
        g_run=makeControl(WC_BUTTONW,L"Entwicklung starten",BS_DEFPUSHBUTTON,ID_RUN);
        g_stop=makeControl(WC_BUTTONW,L"Abbrechen",BS_PUSHBUTTON,ID_STOP);EnableWindow(g_run,FALSE);EnableWindow(g_stop,FALSE);
        g_architecture=makeControl(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_ARCHITECTURE);
        for(const wchar_t* item:{L"A–D-Vergleich",L"Einzelkontext",L"Isolierte Kompartimente",L"Kommunizierende Kompartimente",L"Mit Entwicklungserfahrung"})
            SendMessageW(g_architecture,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(item));
        SendMessageW(g_architecture,CB_SETCURSEL,0,0);
        g_compiler=makeControl(WC_EDITW,L"F:\\KR-Compiler-Windows-x64-v4.3.1\\toolchain\\bin\\g++.exe",ES_AUTOHSCROLL,ID_COMPILER);
        makeControl(WC_BUTTONW,L"Wählen …",BS_PUSHBUTTON,ID_COMPILER_BROWSE);
        const auto defaultWorkspace=(std::filesystem::temp_directory_path()/L"GPI_GUI_RUNS").wstring();
        g_workspace=makeControl(WC_EDITW,defaultWorkspace.c_str(),ES_AUTOHSCROLL,ID_WORKSPACE);
        makeControl(WC_BUTTONW,L"Wählen …",BS_PUSHBUTTON,ID_WORKSPACE_BROWSE);
        g_contract=makeControl(WC_EDITW,L"Analysieren Sie zuerst die Aufgabe.",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,ID_CONTRACT);
        g_log=makeControl(WC_EDITW,L"Noch kein Entwicklungslauf gestartet.",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,ID_LOG);
        setFont(g_log,g_monoFont);return 0;
    }
    case WM_SIZE:layout(LOWORD(lParam),HIWORD(lParam));return 0;
    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_PROMPT:
            if(HIWORD(wParam)==EN_CHANGE&&!g_running){g_contractAccepted=false;EnableWindow(g_run,FALSE);SetWindowTextW(g_status,L"Aufgabe geändert – erneut analysieren");}
            return 0;
        case ID_ANALYZE:analyzeGoal();return 0;case ID_RUN:startRun();return 0;case ID_STOP:stopRun();return 0;
        case ID_COMPILER_BROWSE:browseCompiler();return 0;case ID_WORKSPACE_BROWSE:browseWorkspace();return 0;
        default:break;
        }break;
    case WM_GPI_LOG: {
        auto* value=reinterpret_cast<std::wstring*>(lParam);appendLog(*value);delete value;return 0;
    }
    case WM_GPI_DONE:
        if(g_worker.joinable())g_worker.join();
        setRunning(false,wParam==0?L"Erfolgreich verifiziert":(wParam==1223?L"Abgebrochen":L"Lauf fehlgeschlagen"));
        appendLog(wParam==0?L"\r\n✓ RESULT=PASS\r\n":L"\r\n✕ Lauf beendet\r\n");return 0;
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam),RGB(38,48,64));SetBkColor(reinterpret_cast<HDC>(wParam),RGB(245,247,250));
        return reinterpret_cast<LRESULT>(g_background);
    case WM_CTLCOLOREDIT:
        SetBkColor(reinterpret_cast<HDC>(wParam),RGB(255,255,255));return reinterpret_cast<LRESULT>(g_panel);
    case WM_CLOSE:
        if(g_running&&MessageBoxW(window,L"Der Entwicklungslauf ist noch aktiv. Wirklich abbrechen?",L"GPI",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;
        stopRun();DestroyWindow(window);return 0;
    case WM_DESTROY:
        if(g_worker.joinable())g_worker.join();
        DeleteObject(g_uiFont);DeleteObject(g_titleFont);DeleteObject(g_monoFont);DeleteObject(g_background);DeleteObject(g_panel);
        PostQuitMessage(0);return 0;
    default:break;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}

int selfTest() {
    const auto accepted=genomic::gpi::decomposeResearchTask(
        "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne die Summe",false);
    const auto rejected=genomic::gpi::decomposeResearchTask("Sortiere Zahlen aufsteigend und absteigend",false);
    return accepted.supported&&accepted.contract.accepted&&!accepted.graph.empty()&&
           (!rejected.supported||!rejected.contract.accepted)?0:1;
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR commandLine,int show) {
    if(std::wstring(commandLine).find(L"--self-test")!=std::wstring::npos)return selfTest();
    INITCOMMONCONTROLSEX common{};common.dwSize=sizeof(common);common.dwICC=ICC_STANDARD_CLASSES;InitCommonControlsEx(&common);
    g_background=CreateSolidBrush(RGB(245,247,250));g_panel=CreateSolidBrush(RGB(255,255,255));
    WNDCLASSEXW type{};type.cbSize=sizeof(type);type.lpfnWndProc=windowProcedure;type.hInstance=instance;
    type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=g_background;type.lpszClassName=L"GpiIntegratedGui";
    type.hIcon=LoadIconW(nullptr,IDI_APPLICATION);type.hIconSm=type.hIcon;
    if(!RegisterClassExW(&type))return 1;
    HWND window=CreateWindowExW(0,type.lpszClassName,L"GPI – Genomische Programmierintelligenz",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1200,800,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;
    ShowWindow(window,show);UpdateWindow(window);
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return static_cast<int>(message.wParam);
}
