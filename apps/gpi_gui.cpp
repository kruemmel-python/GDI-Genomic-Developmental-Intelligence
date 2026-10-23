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
#include "gpi/behavioral_induction.hpp"
#include "gpi/developmental_intelligence.hpp"
#include "gpi/foreign_project_intelligence.hpp"
#include "gpi/build_contract.hpp"
#include "gpi/development_strategy.hpp"
#include "gpi/autonomous_software_engineering.hpp"
#include "gpi/autonomous_behavioral_evolution.hpp"
#include "gpi/autonomous_semantic_integration.hpp"
#include "gpi/goal_architect.hpp"
#include "gpi/project_blueprint.hpp"
#include "gpi/project_manifest.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
constexpr UINT WM_GPI_DIRECTORY=WM_APP+3;
constexpr int ID_PROMPT=1001,ID_ANALYZE=1002,ID_RUN=1003,ID_STOP=1004;
constexpr int ID_ARCHITECTURE=1005,ID_COMPILER=1006,ID_COMPILER_BROWSE=1007;
constexpr int ID_WORKSPACE=1008,ID_WORKSPACE_BROWSE=1009,ID_CONTRACT=1010,ID_LOG=1011;
constexpr int ID_BEHAVIOR=1012,ID_BEHAVIOR_OPEN=1013,ID_BEHAVIOR_EXAMPLE=1014,ID_BEHAVIOR_SAVE=1015;
constexpr int ID_CYCLES=1016,ID_MEMORY=1017,ID_OPEN_RESULT=1018;

HWND g_window{},g_prompt{},g_analyze{},g_run{},g_stop{},g_architecture{},g_compiler{};
HWND g_workspace{},g_contract{},g_log{},g_status{},g_behavior{},g_behaviorOpen{},g_behaviorExample{},g_behaviorSave{};
HWND g_cycles{},g_memory{},g_openResult{};
std::wstring g_lastRunDirectory;
std::atomic<unsigned> g_runSerial{0};
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

std::filesystem::path executablePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(length==0||length>=buffer.size())throw std::runtime_error("Cannot locate GUI executable");
    return std::filesystem::path(std::wstring(buffer.data(),length));
}

std::filesystem::path executableDirectory() {
    return executablePath().parent_path();
}

std::filesystem::path uniqueRunDirectory(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    const auto ticks=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for(int i=0;i<1000;++i) {
        const auto candidate=root/(L"gdi_run_"+std::to_wstring(ticks)+L"_"+
            std::to_wstring(++g_runSerial));
        if(std::filesystem::create_directory(candidate))return candidate;
    }
    throw std::runtime_error("Cannot allocate a new run directory");
}

enum class GuiMode {
    ForeignProject = 0,
    R17Development = 1,
    R18BehavioralEvolution = 2,
    AutonomousPlanner = 3,
    DevelopmentalIntelligence = 4,
    Behavioral = 5,
    Architecture = 6,
    Blueprint = 7,
    Integrated = 8
};

GuiMode currentGuiMode() {
    const LRESULT sel=SendMessageW(g_architecture,CB_GETCURSEL,0,0);
    if(sel==0)return GuiMode::ForeignProject;
    if(sel==1)return GuiMode::R17Development;
    if(sel==2)return GuiMode::R18BehavioralEvolution;
    if(sel==3)return GuiMode::AutonomousPlanner;
    if(sel==4)return GuiMode::DevelopmentalIntelligence;
    if(sel==5)return GuiMode::Behavioral;
    if(sel==6)return GuiMode::Architecture;
    if(sel==7)return GuiMode::Blueprint;
    return GuiMode::Integrated;
}

bool generativeSpecMode() {
    const auto mode=currentGuiMode();
    return mode==GuiMode::ForeignProject||mode==GuiMode::R17Development||mode==GuiMode::R18BehavioralEvolution||mode==GuiMode::AutonomousPlanner||
           mode==GuiMode::DevelopmentalIntelligence||mode==GuiMode::Behavioral||
           mode==GuiMode::Architecture||mode==GuiMode::Blueprint;
}

std::filesystem::path examplePath(GuiMode mode) {
    const auto bin=executableDirectory();
    for(const auto& root:{bin,bin.parent_path(),bin.parent_path().parent_path()}) {
        std::filesystem::path candidate;
        if(mode==GuiMode::R17Development) {
            candidate=root/L"examples"/L"r17_neutral_project";
            if(std::filesystem::is_directory(candidate))return std::filesystem::absolute(candidate);
            continue;
        } else if(mode==GuiMode::R18BehavioralEvolution) {
            candidate=root/L"examples"/L"r18_csv_application";
            if(std::filesystem::is_directory(candidate))return std::filesystem::absolute(candidate);
            continue;
        } else if(mode==GuiMode::ForeignProject) {
            candidate=root/L"examples"/L"foreign_telemetry_project";
            if(!std::filesystem::is_directory(candidate))
                candidate=root/L"examples"/L"repair_header_project";
            if(!candidate.empty()&&std::filesystem::is_directory(candidate))
                return std::filesystem::absolute(candidate);
            continue;
        } else if(mode==GuiMode::DevelopmentalIntelligence) {
            candidate=root/L"examples"/L"developmental_curriculum"/L"ontogeny.curriculum";
        } else if(mode==GuiMode::Behavioral) {
            candidate=root/L"examples"/L"behavioral_geometry"/L"geometry.behavior";
            if(!std::filesystem::is_regular_file(candidate))
                candidate=root/L"examples"/L"behavioral_max"/L"max.behavior";
        } else if(mode==GuiMode::Architecture) {
            candidate=root/L"examples"/L"typed_geometry"/L"geometry.goal";
            if(!std::filesystem::is_regular_file(candidate))
                candidate=root/L"examples"/L"typed_components"/L"components.goal";
        } else if(mode==GuiMode::Blueprint) {
            candidate=root/L"examples"/L"blueprint_geometry"/L"project.blueprint";
            if(!std::filesystem::is_regular_file(candidate))
                candidate=root/L"examples"/L"blueprint_fibonacci"/L"project.blueprint";
        }
        if(!candidate.empty()&&std::filesystem::is_regular_file(candidate))return candidate;
    }
    return {};
}

std::wstring readUtf8File(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot read example file");
    const std::string content((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(content.size()>1024*1024)throw std::runtime_error("Example file is too large");
    auto value=widen(content);
    std::wstring normalized;
    normalized.reserve(value.size()+32);
    for(std::size_t i=0;i<value.size();++i) {
        if(value[i]==L'\n'&&(i==0||value[i-1]!=L'\r'))normalized+=L'\r';
        normalized+=value[i];
    }
    return normalized;
}

void writeUtf8File(const std::filesystem::path& file,const std::wstring& content) {
    std::ofstream out(file,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("Cannot write specification file");
    out<<narrow(content);
    if(!out)throw std::runtime_error("Cannot write specification file");
}

std::wstring defaultAutonomousObjective() {
    return L"Entwickle eine C++20-Anwendung, die zweidimensionale geometrische Objekte verwaltet, deren Eigenschaften berechnet und die Ergebnisse speichert.";
}

std::wstring defaultCurriculum() {
    return LR"(GDI_DEVELOPMENTAL_CURRICULUM_V1
curriculum	ontogenetic_geometry_intelligence
namespace	generated

stage	stage1_neonatal_math	Neonatal
project	stage1_math_primitives
namespace	generated
module	math
function	square	long long	math
parameter	square	long long	v
max-depth	square	2
sample	square	0	0
sample	square	2	4
sample	square	-3	9
sample	square	5	25
check	square	7	49
check	square	-8	64
entry-function	square
end-stage

stage	stage2_juvenile_types	Juvenile
project	stage2_point_structure
namespace	generated
module	math
module	geometry	math
type	Point2D	geometry
field	Point2D	long long	x
field	Point2D	long long	y
method	Point2D	manhattan	long long
method-sample	Point2D	manhattan	0	0	0
method-sample	Point2D	manhattan	3	4	7
method-sample	Point2D	manhattan	2	5	7
method-sample	Point2D	manhattan	6	8	14
method-check	Point2D	manhattan	5	12	17
method-check	Point2D	manhattan	10	3	13
function	norm_sq	long long	geometry
parameter	norm_sq	Point2D	p
max-depth	norm_sq	2
sample	norm_sq	Point2D{0,0}	0
sample	norm_sq	Point2D{3,4}	25
sample	norm_sq	Point2D{-2,5}	29
sample	norm_sq	Point2D{6,8}	100
check	norm_sq	Point2D{5,12}	169
check	norm_sq	Point2D{-4,-3}	25
entry-function	norm_sq
end-stage

stage	stage3_mature_system	Mature
project	stage3_complete_geometry_system
namespace	generated
module	math
module	geometry	math
module	analytics	geometry	math
function	point_energy	long long	analytics
parameter	point_energy	Point2D	p
max-depth	point_energy	2
sample	point_energy	Point2D{0,0}	0
sample	point_energy	Point2D{3,4}	32
sample	point_energy	Point2D{2,5}	36
sample	point_energy	Point2D{6,8}	114
check	point_energy	Point2D{5,12}	186
check	point_energy	Point2D{1,2}	8
entry-function	point_energy
end-stage
)";
}

std::wstring defaultBehavior() {
    return LR"(GDI_BEHAVIORAL_GOAL_V2
project	developed_geometry
namespace	generated
module	math
module	geometry	math
type	Point2D	geometry
field	Point2D	long long	x
field	Point2D	long long	y
function	square	long long	math
parameter	square	long long	v
max-depth	square	2
sample	square	0	0
sample	square	2	4
sample	square	-3	9
sample	square	5	25
check	square	7	49
check	square	-8	64
function	norm_sq	long long	geometry
parameter	norm_sq	Point2D	p
max-depth	norm_sq	2
sample	norm_sq	Point2D{0,0}	0
sample	norm_sq	Point2D{3,4}	25
sample	norm_sq	Point2D{-2,5}	29
sample	norm_sq	Point2D{6,8}	100
check	norm_sq	Point2D{5,12}	169
check	norm_sq	Point2D{-4,-3}	25
entry-function	norm_sq
)";
}

std::wstring defaultArchitectureGoal() {
    return LR"(GPI_TYPED_ARCHITECTURE_GOAL_V2
project	architected_geometry
namespace	generated
module	math
module	geometry	math
concept	IntegralScalar	T	std::integral<T>	math
type	Point2D	geometry
field	Point2D	long long	x
field	Point2D	long long	y
method	Point2D	manhattan	long long
method-operation	Point2D	manhattan	ax	abs	x
method-operation	Point2D	manhattan	ay	abs	y
method-operation	Point2D	manhattan	total	add	ax	ay
method-return	Point2D	manhattan	total
function	square	T	math	T	IntegralScalar
parameter	square	T	v
operation	square	prod	multiply	v	v
return	square	prod
function	make_point	Point2D	geometry
parameter	make_point	long long	px
parameter	make_point	long long	py
operation	make_point	pt	construct:Point2D	px	py
return	make_point	pt
function	point_energy	long long	geometry
parameter	point_energy	Point2D	p
operation	point_energy	px	field:x	p
operation	point_energy	py	field:y	p
operation	point_energy	sx	call:square	px
operation	point_energy	sy	call:square	py
operation	point_energy	sum	add	sx	sy
operation	point_energy	man	method:manhattan	p
operation	point_energy	res	add	sum	man
return	point_energy	res
entry	point_energy	#32	Point2D{#3,#4}
case	sq_five	square	#25	#5
case	mk_pt	make_point	Point2D{#3,#4}	#3	#4
case	energy_origin	point_energy	#0	Point2D{#0,#0}
case	energy_3_4	point_energy	#32	Point2D{#3,#4}
timeout-seconds	20
require-sandbox	true
)";
}

std::wstring defaultBlueprintSpec() {
    return LR"(GPI_PROJECT_BLUEPRINT_V2
project	blueprint_geometry
namespace	generated
module	math
module	geometry	math
concept	IntegralScalar	T	std::integral<T>	math
type	Point2D	geometry
field	Point2D	long long	x
field	Point2D	long long	y
method	Point2D	manhattan	long long	(this->x < 0 ? -this->x : this->x) + (this->y < 0 ? -this->y : this->y)
function	square	T	v * v	math	T	IntegralScalar
parameter	square	T	v
function	norm_sq	long long	square(p.x) + square(p.y)	geometry
parameter	norm_sq	Point2D	p
entry	long long	norm_sq(Point2D{3, 4}) + Point2D{-3, 4}.manhattan()
test	square_test	square(6) == 36
test	manhattan_test	Point2D{-3, 4}.manhattan() == 7
test	norm_test	norm_sq(Point2D{3, 4}) == 25
expected-output-literal	32
timeout-seconds	20
require-sandbox	true
)";
}

std::wstring detectCompiler() {
    const auto bin=executableDirectory();
    std::vector<std::filesystem::path> candidates={
        bin/L"toolchain"/L"bin"/L"g++.exe",
        bin.parent_path()/L"toolchain"/L"bin"/L"g++.exe",
        bin.parent_path().parent_path()/L"toolchain"/L"bin"/L"g++.exe",
        L"F:\\KR-Compiler-Windows-x64-v4.3.1\\toolchain\\bin\\g++.exe"};
    const DWORD needed=GetEnvironmentVariableW(L"PATH",nullptr,0);
    if(needed>1&&needed<32768) {
        std::wstring path(needed,L'\0');
        const DWORD got=GetEnvironmentVariableW(L"PATH",path.data(),needed);
        if(got>0&&got<needed) {
            path.resize(got);
            std::wistringstream parts(path);
            std::wstring dir;
            while(std::getline(parts,dir,L';')) {
                if(dir.size()>1&&dir.front()==L'"'&&dir.back()==L'"')dir=dir.substr(1,dir.size()-2);
                if(!dir.empty())candidates.push_back(std::filesystem::path(dir)/L"g++.exe");
            }
        }
    }
    for(const auto& candidate:candidates) {
        std::error_code ec;
        if(std::filesystem::is_regular_file(candidate,ec))return std::filesystem::absolute(candidate).wstring();
    }
    return L"";
}

void setRunning(bool running,const wchar_t* status) {
    g_running=running;EnableWindow(g_analyze,!running);EnableWindow(g_run,!running&&g_contractAccepted);
    EnableWindow(g_stop,running);EnableWindow(g_prompt,!running);EnableWindow(g_behavior,!running);
    EnableWindow(g_architecture,!running);EnableWindow(g_behaviorOpen,!running);
    EnableWindow(g_behaviorExample,!running);EnableWindow(g_behaviorSave,!running);
    EnableWindow(g_cycles,!running);EnableWindow(g_memory,!running);
    EnableWindow(g_openResult,!running&&!g_lastRunDirectory.empty());
    SetWindowTextW(g_status,status);
}

void analyzeGoal() {
    try {
        std::wostringstream report;
        const auto mode=currentGuiMode();
        if(mode==GuiMode::R17Development) {
            const auto project=textOf(g_behavior);
            const auto requirement=textOf(g_prompt);
            if(project.empty()||requirement.empty())throw std::invalid_argument("Projektordner und neue Entwicklungsanforderung sind erforderlich.");
            const std::filesystem::path projectRoot(project);
            if(!std::filesystem::is_directory(projectRoot))throw std::invalid_argument("Der angegebene Projektordner existiert nicht.");
            const auto requirementContract=genomic::gpi::interpretDevelopmentRequirement(narrow(requirement));
            if(requirementContract.status!=genomic::gpi::RequirementStatus::Clarified)throw std::invalid_argument("Die Anforderung ist nicht sicher im R17-Syntheseumfang klassifizierbar.");
            const auto build=genomic::gpi::discoverBuildContract(projectRoot);
            const auto impact=genomic::gpi::analyzeChangeImpact(projectRoot,build,requirementContract);
            const auto architecture=genomic::gpi::synthesizeArchitectureChange(requirementContract,impact,build);
            report<<L"R17 – AUTONOME SOFTWAREERWEITERUNG (isoliert)\r\n\r\n"
                  <<L"Buildvertrag: "<<widen(genomic::gpi::toString(build.buildSystem))
                  <<L"    Ziele: "<<build.targets.size()
                  <<L"\r\nAnforderung: "<<widen(requirementContract.capabilityClass)
                  <<L"    Vertrauen: "<<requirementContract.confidence
                  <<L"\r\nImpact-Knoten: "<<impact.nodes.size()
                  <<L"    Architektur: "<<widen(architecture.selectedAlternative)
                  <<L"\r\nGenerierte Komponenten: "<<widen(architecture.headerPath)<<L", "<<widen(architecture.sourcePath)
                  <<L"\r\nAkzeptanzkriterien: "<<requirementContract.acceptance.size()
                  <<L"\r\n[Sicherheitsgrenze] Die Ausfuehrung erzeugt und prueft nur eine isolierte Arbeitskopie; Originalquellen bleiben read-only.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::R18BehavioralEvolution) {
            const std::filesystem::path projectRoot(textOf(g_behavior));
            const auto requirement=textOf(g_prompt);
            if(!std::filesystem::is_directory(projectRoot)||requirement.empty())throw std::invalid_argument("Projektordner und R18-Verhaltensanforderung sind erforderlich.");
            const auto contract=genomic::gpi::interpretBehavioralChangeRequirement(narrow(requirement));
            const auto model=genomic::gpi::analyzeBehavioralProgram(projectRoot);
            const auto plan=genomic::gpi::synthesizeBehavioralIntegration(model,contract);
            if(!plan.executable)throw std::invalid_argument("R18 konnte keine eindeutige sichere Integrationsstelle nachweisen.");
            report<<L"R18 – VERHALTENSENTWICKLUNG (isoliert)\r\n\r\nVertrag: "<<widen(contract.capabilityClass)
                  <<L"\r\nEinstiegspunkte: "<<model.entryPoints.size()<<L"\r\nIntegration: "<<widen(plan.sourceFile)
                  <<L"\r\n[Sicherheitsgrenze] Nur digest-geprüfte, isolierte Quelltransformationen werden ausgeführt.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::ForeignProject) {
            std::wstring rawPath=textOf(g_behavior);
            while(!rawPath.empty()&&(rawPath.back()==L'\r'||rawPath.back()==L'\n'||rawPath.back()==L' '||rawPath.back()==L'\t'))rawPath.pop_back();
            while(!rawPath.empty()&&(rawPath.front()==L'\r'||rawPath.front()==L'\n'||rawPath.front()==L' '||rawPath.front()==L'\t'))rawPath.erase(rawPath.begin());
            if(rawPath.empty())throw std::invalid_argument("Bitte den Ordnerpfad eines fremden C++20-Projekts eingeben oder ueber 'Ordner oeffnen' waehlen.");
            const std::filesystem::path projRoot(rawPath);
            if(!std::filesystem::is_directory(projRoot))
                throw std::invalid_argument("Der angegebene Fremdprojekt-Ordner existiert nicht.");
            const auto analysis=genomic::gpi::analyzeForeignCpp20Project(projRoot);
            report<<L"FREMDPROJEKT-ANALYSE & GLEICHZEITIGE HYPOTHESENBILDUNG\r\n\r\n"
                  <<L"Projekt: "<<widen(analysis.projectName)
                  <<L"    Read-Only Tree-Hash: "<<analysis.originalDirectoryHash
                  <<L"\r\nEntdeckte Module: "<<analysis.modules.size()
                  <<L"    AST-/Semantik-Symbole: "<<analysis.symbols.size()
                  <<L"    Datenfluesse: "<<analysis.dataflowEdges.size()
                  <<L"\r\nArchitektur-Hypothesen & abgeleitete Entwicklungsziele: "<<analysis.hypotheses.size();
            if(analysis.discoveredGeneratedKrBuildFiles > 0 || analysis.discoveredKrSourceFiles > 0) {
                report<<L"\r\nKR-Projekt-Separation: "
                      <<analysis.discoveredKrSourceFiles<<L" .kr-Primaerquellen, "
                      <<analysis.discoveredGeneratedKrBuildFiles<<L" generierte KR_BUILD-Dateien (Originale geschuetzt vor Writeback)";
            }
            for(const auto& mod:analysis.modules) {
                report<<L"\r\n  [Modul] "<<widen(mod.moduleName)
                      <<L" (Header-Knoten: "<<widen(mod.headerNodeId)
                      <<L", Source-Knoten: "<<widen(mod.sourceNodeId)
                      <<L", Typen: "<<mod.exportedTypes.size()
                      <<L", Funktionen: "<<mod.exportedFunctions.size()<<L")";
            }
            for(const auto& hyp:analysis.hypotheses) {
                report<<L"\r\n  [Hypothese "<<widen(hyp.id)<<L"] "<<widen(hyp.hypothesisStatement)
                      <<L"\r\n    -> Entwicklungsziel: "<<widen(hyp.derivedDevelopmentGoal);
            }
            report<<L"\r\n\r\n[Sicherheitsgrenze] Originalprojekt bleibt unveraendert (Read-Only). Synthese, Builds und Tests erfolgen isoliert in der Sandbox.\r\n"
                  <<L"[Doppeltes Gedaechtnis] Projektgedaechtnis (GDIPM1) + uebertragbares Entwicklungsgedaechtnis (GDIDP1) aktiv.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::AutonomousPlanner) {
            const auto source=textOf(g_behavior);
            if(source.empty())throw std::invalid_argument("Bitte ein uebergeordnetes Ziel eingeben.");
            const bool useMemory=SendMessageW(g_memory,BM_GETCHECK,0,0)==BST_CHECKED;
            const auto root=std::filesystem::absolute(std::filesystem::path(textOf(g_workspace)));
            const auto memPath=useMemory?(root/L"gdi_gui_components.memory"):std::filesystem::path{};
            const auto plan=genomic::gpi::planAutonomousOntogenyFromObjective(narrow(source),memPath);
            if(!plan.audit.objectiveSupported) {
                std::string diag=plan.audit.diagnostics.empty()?"Ziel ausserhalb des genomisch verifizierbaren Bereichs":plan.audit.diagnostics.front();
                throw std::invalid_argument(diag);
            }
            std::wstring rat=widen(plan.plannerRationale);
            std::wstring normRat;
            for(std::size_t i=0;i<rat.size();++i) {
                if(rat[i]==L'\n'&&(i==0||rat[i-1]!=L'\r'))normRat+=L'\r';
                normRat+=rat[i];
            }
            report<<normRat
                  <<L"\r\n[Kontrollversuch] Wenn 'Erfahrung speichern/nutzen' aktiviert ist, fuehrt GDI den Lauf mit dem persistenten Komponentengedächtnis aus; ansonsten als naiver Organismus.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::DevelopmentalIntelligence) {
            const auto source=textOf(g_behavior);
            if(source.empty())throw std::invalid_argument("Bitte ein Entwicklungs-Curriculum (.curriculum oder .behavior) eingeben.");
            const auto curriculum=genomic::gpi::parseDevelopmentalCurriculumText(narrow(source));
            report<<L"AUTONOME ENTWICKLUNGSINTELLIGENZ (Ontogenese & Erfahrungsakkumulation)\r\n\r\nCurriculum: "<<widen(curriculum.curriculumName)
                  <<L"    Namespace: "<<widen(curriculum.namespaceName)
                  <<L"\r\nOntogenetische Entwicklungsstufen: "<<curriculum.stages.size();
            std::size_t totalFns=0,totalTypes=0;
            for(const auto& st:curriculum.stages) {
                totalFns+=st.goal.functions.size();
                totalTypes+=st.goal.types.size();
                const wchar_t* stName=st.requiredStage==genomic::NeuralDevelopmentStage::Neonatal?L"Neonatal":
                    (st.requiredStage==genomic::NeuralDevelopmentStage::Juvenile?L"Juvenile":L"Mature");
                report<<L"\r\n  [Stufe "<<widen(st.stageId)<<L" | "<<stName<<L"] Projekt: "<<widen(st.goal.projectName)
                      <<L" ("<<st.goal.types.size()<<L" Typen, "<<st.goal.functions.size()<<L" Funktionen, Einstieg: "<<widen(st.goal.entryFunction)<<L")";
            }
            report<<L"\r\n\r\nGesamt: "<<totalTypes<<L" eigene Datentypen, "<<totalFns<<L" zusammenhaengende Komponenten."
                  <<L"\r\nMulti-Kompartiment-Regulation, verketteter Komponenten-Erfahrungsspeicher und Sandbox bereit.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::Behavioral) {
            const auto source=textOf(g_behavior);
            if(source.empty())throw std::invalid_argument("Bitte Verhaltensbeispiele eingeben.");
            const auto behavior=genomic::gpi::parseBehavioralGoalText(narrow(source));
            if(behavior.functions.empty())
                throw std::invalid_argument("Mindestens eine Zielfunktion erforderlich.");
            std::size_t totalSamples=0,totalChecks=0;
            for(const auto& fn:behavior.functions) {
                if(fn.parameters.empty()||fn.parameters.size()>4||fn.samples.size()<4||
                   fn.checks.size()<2||fn.maxDepth<1||fn.maxDepth>4)
                    throw std::invalid_argument("Jede Funktion benoetigt 1..4 Parameter, >=4 Trainings- und >=2 Pruefbeispiele (Tiefe 1..4).");
                totalSamples+=fn.samples.size();
                totalChecks+=fn.checks.size();
            }
            report<<L"VERHALTENSZIEL (GDI v2.0 – Komposition & Eigene Typen)\r\n\r\nProjekt: "<<widen(behavior.projectName)
                  <<L"    Namespace: "<<widen(behavior.namespaceName)
                  <<L"\r\nModule: "<<behavior.modules.size()
                  <<L"    Eigene Datentypen: "<<behavior.types.size()
                  <<L"    Enums/Concepts: "<<(behavior.enums.size()+behavior.concepts.size())
                  <<L"\r\nZu entwickelnde Funktionen: "<<behavior.functions.size()
                  <<L" (Einstieg: "<<widen(behavior.entryFunction)<<L")";
            for(const auto& t:behavior.types) {
                report<<L"\r\n  [Typ] struct "<<widen(t.name)<<L" ("<<t.fields.size()<<L" Felder, "<<t.methods.size()<<L" Methoden)";
            }
            for(const auto& fn:behavior.functions) {
                report<<L"\r\n  [Funktion] "<<widen(fn.name)<<L" -> "<<widen(fn.returnType)
                      <<L" ("<<fn.parameters.size()<<L" Parameter, "<<fn.samples.size()<<L" Samples, "<<fn.checks.size()<<L" Checks)";
            }
            report<<L"\r\n\r\nGesamt: "<<totalSamples<<L" Trainingsbeispiele, "<<totalChecks<<L" zurueckgehaltene Pruefbeispiele."
                  <<L"\r\nFormat und Grundvertrag geprueft. Kompositionelle Synthese und Sandbox-Verifikation bereit.";
            g_contractAccepted=true;
        } else if(mode==GuiMode::Architecture) {
            const auto source=textOf(g_behavior);
            if(source.empty())throw std::invalid_argument("Bitte eine typisierte Zielarchitektur (.goal) eingeben.");
            const auto goal=genomic::gpi::parseTypedArchitectureGoalText(narrow(source));
            report<<L"TYPISIERTE ZIELARCHITEKTUR (GDI v2.0)\r\n\r\nProjekt: "<<widen(goal.projectName)
                  <<L"    Namespace: "<<widen(goal.namespaceName)
                  <<L"\r\nModule: "<<goal.modules.size()
                  <<L"    Eigene Datentypen: "<<goal.types.size()
                  <<L"    Enums: "<<goal.enums.size()
                  <<L"    Concepts: "<<goal.concepts.size()
                  <<L"\r\nKomponenten-Funktionen: "<<goal.functions.size()
                  <<L"    Unabhaengige Testfaelle: "<<goal.cases.size();
            for(const auto& t:goal.types)
                report<<L"\r\n  [Typ] struct "<<widen(t.name)<<L" ("<<t.fields.size()<<L" Felder, "<<t.methods.size()<<L" Methoden)";
            for(const auto& fn:goal.functions)
                report<<L"\r\n  [Komponente] "<<widen(fn.name)<<L" -> "<<widen(fn.returnType)<<L" ("<<fn.operations.size()<<L" Operationen)";
            report<<L"\r\n\r\nEinstiegspunkt: "<<widen(goal.entryFunction)<<L" -> Erwartet: "<<widen(goal.entryExpected);
            g_contractAccepted=true;
        } else if(mode==GuiMode::Blueprint) {
            const auto source=textOf(g_behavior);
            if(source.empty())throw std::invalid_argument("Bitte einen Projekt-Blueprint (.blueprint) eingeben.");
            const auto bp=genomic::gpi::parseProjectBlueprintText(narrow(source),executableDirectory());
            report<<L"PROJEKT-BLUEPRINT (GDI v2.0 – Mehrdatei-Synthese)\r\n\r\nProjekt: "<<widen(bp.projectName)
                  <<L"    Namespace: "<<widen(bp.namespaceName)
                  <<L"\r\nModule: "<<bp.modules.size()
                  <<L"    Eigene Datentypen: "<<bp.types.size()
                  <<L"    Enums: "<<bp.enums.size()
                  <<L"    Concepts: "<<bp.concepts.size()
                  <<L"\r\nFunktionen: "<<bp.functions.size()
                  <<L"    Semantische Tests: "<<bp.tests.size()
                  <<L"\r\nEinstieg: "<<widen(bp.entryType)<<L" = "<<widen(bp.entryExpression);
            g_contractAccepted=true;
        } else {
            const auto prompt=textOf(g_prompt);
            if(prompt.empty())throw std::invalid_argument("Bitte zuerst eine Aufgabe eingeben.");
            const auto plan=genomic::gpi::decomposeResearchTask(narrow(prompt),false);
            report<<L"Originalziel\r\n"<<prompt<<L"\r\n\r\nInterpretation\r\n"<<widen(plan.contract.normalizedIntent)
                  <<L"\r\n\r\nEingabe: "<<widen(plan.contract.inputType)<<L"    Ausgabe: "<<widen(plan.contract.outputType)
                  <<L"\r\nVertrag: "<<(plan.contract.accepted?L"AKZEPTIERT":L"ABGELEHNT")
                  <<L"    Hash: "<<plan.contract.semanticHash<<L"\r\n";
            for(const auto& operation:plan.contract.requiredOperations)
                report<<L"\r\n  * "<<widen(operation.operation)<<L"  ["<<widen(operation.inputType)
                      <<L" -> "<<widen(operation.outputType)<<L"]";
            for(const auto& item:plan.contract.unresolvedRequirements)
                report<<L"\r\n  Ungeklaert: "<<widen(item);
            for(const auto& item:plan.contract.contradictions)
                report<<L"\r\n  Widerspruch: "<<widen(item);
            g_contractAccepted=plan.supported&&plan.contract.accepted;
        }
        SetWindowTextW(g_contract,report.str().c_str());
        EnableWindow(g_run,g_contractAccepted);
        SetWindowTextW(g_status,g_contractAccepted?L"Zielvertrag akzeptiert":L"Ziel abgelehnt");
    } catch(const std::exception& error) {
        g_contractAccepted=false;EnableWindow(g_run,FALSE);
        SetWindowTextW(g_contract,L"Zielvertrag abgelehnt. Bitte Eingabe korrigieren.");
        MessageBoxW(g_window,widen(error.what()).c_str(),L"GDI Zielanalyse",MB_ICONWARNING);
    }
}

void loadExample();

void changeMode() {
    const auto mode=currentGuiMode();
    const bool genSpec=generativeSpecMode();
    ShowWindow(g_behavior,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(g_prompt,genSpec?SW_HIDE:SW_SHOW);
    ShowWindow(g_behaviorOpen,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(g_behaviorExample,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(g_behaviorSave,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(g_cycles,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(g_memory,genSpec?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(g_window,2007),genSpec?SW_SHOW:SW_HIDE);
    if(mode==GuiMode::ForeignProject) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"FREMDES C++20-PROJEKT (Ordnerpfad – Read-Only Analyse & Hypothesen)");
        SetWindowTextW(g_behaviorOpen,L"Ordner oeffnen");
    } else if(mode==GuiMode::R17Development) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"ZIELPROJEKT (Ordnerpfad – R17 entwickelt nur isolierte Kopie)");
        SetWindowTextW(GetDlgItem(g_window,2000),L"NEUE ENTWICKLUNGSANFORDERUNG");
        SetWindowTextW(g_behaviorOpen,L"Ordner oeffnen");
    } else if(mode==GuiMode::R18BehavioralEvolution) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"ZIELPROJEKT (Ordnerpfad – R18 transformiert nur isolierte Kopie)");
        SetWindowTextW(GetDlgItem(g_window,2000),L"VERHALTENSANFORDERUNG");
        SetWindowTextW(g_behaviorOpen,L"Ordner oeffnen");
    } else if(mode==GuiMode::AutonomousPlanner) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"UEBERGEORDNETES ZIEL (Autonomer ontogenetischer Entwicklungsplaner)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    } else if(mode==GuiMode::DevelopmentalIntelligence) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"ONTOGENETISCHES CURRICULUM (.curriculum – Mehrstufige Entwicklung)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    } else if(mode==GuiMode::Behavioral) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"SPEZIFIKATION (.behavior – Funktionen, Typen & Module)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    } else if(mode==GuiMode::Architecture) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"ZIELARCHITEKTUR (.goal – Typisierte Komponenten & Typen)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    } else if(mode==GuiMode::Blueprint) {
        SetWindowTextW(GetDlgItem(g_window,2001),L"PROJEKT-BLUEPRINT (.blueprint – Mehrdatei-Synthese)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    } else {
        SetWindowTextW(GetDlgItem(g_window,2001),L"AUFGABE (TYPISIERTE FORSCHUNG)");
        SetWindowTextW(g_behaviorOpen,L"Datei oeffnen");
    }
    SetWindowTextW(GetDlgItem(g_window,2003),L"ENTWICKLUNGSMODUS");
    if(genSpec)loadExample();
    g_contractAccepted=false;EnableWindow(g_run,FALSE);
    SetWindowTextW(g_contract,L"Bitte zuerst das Ziel analysieren.");
    SetWindowTextW(g_status,L"Modus gewechselt – Ziel analysieren");
}

void loadBehavior(const std::filesystem::path& file) {
    SetWindowTextW(g_behavior,readUtf8File(file).c_str());
    g_contractAccepted=false;EnableWindow(g_run,FALSE);
}

void openBehaviorFile() {
    if(currentGuiMode()==GuiMode::ForeignProject||currentGuiMode()==GuiMode::R17Development||currentGuiMode()==GuiMode::R18BehavioralEvolution) {
        BROWSEINFOW info{};info.hwndOwner=g_window;info.lpszTitle=L"Fremdes C++20-Projektverzeichnis zur Read-Only-Analyse auswaehlen";
        info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
        if(PIDLIST_ABSOLUTE item=SHBrowseForFolderW(&info)) {
            wchar_t path[MAX_PATH]{};
            if(SHGetPathFromIDListW(item,path)) {
                SetWindowTextW(g_behavior,path);
                g_contractAccepted=false;EnableWindow(g_run,FALSE);
            }
            CoTaskMemFree(item);
        }
        return;
    }
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g_window;
    dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"GDI-Spezifikationen (*.curriculum;*.behavior;*.goal;*.blueprint)\0*.curriculum;*.behavior;*.goal;*.blueprint\0Alle Dateien\0*.*\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&dialog)) {
        try {loadBehavior(file);}catch(const std::exception& e){MessageBoxW(g_window,widen(e.what()).c_str(),L"GDI",MB_ICONWARNING);}
    }
}

void saveBehaviorFile() {
    const auto mode=currentGuiMode();
    wchar_t file[32768]=L"new_spec.behavior";
    if(mode==GuiMode::AutonomousPlanner)wcscpy_s(file,L"overarching_objective.txt");
    else if(mode==GuiMode::DevelopmentalIntelligence)wcscpy_s(file,L"new_ontogeny.curriculum");
    else if(mode==GuiMode::Architecture)wcscpy_s(file,L"new_architecture.goal");
    else if(mode==GuiMode::Blueprint)wcscpy_s(file,L"new_project.blueprint");
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g_window;
    dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"GDI-Spezifikationen (*.curriculum;*.behavior;*.goal;*.blueprint;*.txt)\0*.curriculum;*.behavior;*.goal;*.blueprint;*.txt\0Alle Dateien\0*.*\0";
    dialog.lpstrDefExt=mode==GuiMode::AutonomousPlanner?L"txt":
        (mode==GuiMode::DevelopmentalIntelligence?L"curriculum":
        (mode==GuiMode::Architecture?L"goal":(mode==GuiMode::Blueprint?L"blueprint":L"behavior")));
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(GetSaveFileNameW(&dialog)) {
        try {writeUtf8File(file,textOf(g_behavior));}catch(const std::exception& e){MessageBoxW(g_window,widen(e.what()).c_str(),L"GDI",MB_ICONWARNING);}
    }
}

void loadExample() {
    try {
        const auto mode=currentGuiMode();
        const auto example=examplePath(mode);
        if(mode==GuiMode::ForeignProject) {
            SetWindowTextW(g_behavior,example.empty()?L"examples\\foreign_telemetry_project":example.wstring().c_str());
            g_contractAccepted=false;EnableWindow(g_run,FALSE);
            return;
        }
        if(mode==GuiMode::R17Development) {
            const auto bin=executableDirectory();
            for(const auto& root:{bin,bin.parent_path(),bin.parent_path().parent_path()}) {
                const auto project=root/L"examples"/L"r17_neutral_project";
                if(std::filesystem::is_directory(project)) {SetWindowTextW(g_behavior,std::filesystem::absolute(project).wstring().c_str());break;}
            }
            SetWindowTextW(g_prompt,L"Add persistent session state that is saved, restored after restart, and rejected when corrupted.");
            g_contractAccepted=false;EnableWindow(g_run,FALSE);
            return;
        }
        if(mode==GuiMode::R18BehavioralEvolution) {
            SetWindowTextW(g_behavior,example.empty()?L"examples\\r18_csv_application":example.wstring().c_str());
            SetWindowTextW(g_prompt,L"For CSV records skip invalid rows and write an error report.");
            g_contractAccepted=false;EnableWindow(g_run,FALSE);return;
        }
        if(!example.empty())loadBehavior(example);
        else {
            std::wstring fallback=defaultBehavior();
            if(mode==GuiMode::AutonomousPlanner)fallback=defaultAutonomousObjective();
            else if(mode==GuiMode::DevelopmentalIntelligence)fallback=defaultCurriculum();
            else if(mode==GuiMode::Architecture)fallback=defaultArchitectureGoal();
            else if(mode==GuiMode::Blueprint)fallback=defaultBlueprintSpec();
            std::wstring normalized;
            for(std::size_t i=0;i<fallback.size();++i) {
                if(fallback[i]==L'\n'&&(i==0||fallback[i-1]!=L'\r'))normalized+=L'\r';
                normalized+=fallback[i];
            }
            SetWindowTextW(g_behavior,normalized.c_str());
        }
    } catch(const std::exception& error) {
        MessageBoxW(g_window,widen(error.what()).c_str(),L"GDI Beispiel",MB_ICONWARNING);
    }
}

std::wstring selectedArchitecture() {
    const auto mode=currentGuiMode();
    if(mode==GuiMode::ForeignProject)return L"foreign";
    if(mode==GuiMode::R17Development)return L"r17";
    if(mode==GuiMode::R18BehavioralEvolution)return L"r18";
    if(mode==GuiMode::AutonomousPlanner)return L"planner";
    if(mode==GuiMode::DevelopmentalIntelligence)return L"intelligence";
    if(mode==GuiMode::Behavioral)return L"behavior";
    if(mode==GuiMode::Architecture)return L"architect";
    if(mode==GuiMode::Blueprint)return L"blueprint";
    const LRESULT index=SendMessageW(g_architecture,CB_GETCURSEL,0,0);
    wchar_t value[64]{};SendMessageW(g_architecture,CB_GETLBTEXT,static_cast<WPARAM>(index),reinterpret_cast<LPARAM>(value));
    if(std::wstring(value)==L"Einzelkontext")return L"single";
    if(std::wstring(value)==L"Isolierte Kompartimente")return L"isolated";
    if(std::wstring(value)==L"Kommunizierende Kompartimente")return L"communicating";
    if(std::wstring(value)==L"Mit Entwicklungserfahrung")return L"experienced";
    return L"compare";
}

void runChild(std::wstring input,std::wstring requirement,std::filesystem::path compiler,std::filesystem::path root,
              std::wstring architecture,unsigned cycles,bool useMemory) {
    DWORD exitCode=1;
    try {
        const bool isForeign=architecture==L"foreign";
        const bool isR17=architecture==L"r17";
        const bool isR18=architecture==L"r18";
        const bool isPlanner=architecture==L"planner";
        const bool isIntelligence=architecture==L"intelligence";
        const bool isBehavior=architecture==L"behavior";
        const bool isArchitect=architecture==L"architect";
        const bool isBlueprint=architecture==L"blueprint";
        const auto bin=executableDirectory();
        std::wstring exeName=L"gpi_integrated.exe";
        if(isForeign||isR17||isR18||isPlanner||isIntelligence)exeName=L"gdi_intelligence.exe";
        else if(isBehavior)exeName=L"gdi_create.exe";
        else if(isArchitect)exeName=L"gpi_architect.exe";
        else if(isBlueprint)exeName=L"gpi_synthesize.exe";
        const auto executable=bin/exeName;
        if(!std::filesystem::is_regular_file(executable))
            throw std::runtime_error("The required development executable was not found beside the GUI");
        if(!std::filesystem::is_regular_file(compiler))
            throw std::runtime_error("The selected C++20 compiler must be an existing file");
        const auto runDirectory=uniqueRunDirectory(root);
        postLog(L"Arbeitsordner: "+runDirectory.wstring()+L"\r\n\r\n");
        auto* folder=new std::wstring(runDirectory.wstring());
        if(!PostMessageW(g_window,WM_GPI_DIRECTORY,0,reinterpret_cast<LPARAM>(folder)))delete folder;
        std::wstring command=quote(executable.wstring());
        if(isR17) {
            std::wstring cleanPath=input;
            while(!cleanPath.empty()&&(cleanPath.back()==L'\r'||cleanPath.back()==L'\n'||cleanPath.back()==L' '||cleanPath.back()==L'\t'))cleanPath.pop_back();
            while(!cleanPath.empty()&&(cleanPath.front()==L'\r'||cleanPath.front()==L'\n'||cleanPath.front()==L' '||cleanPath.front()==L'\t'))cleanPath.erase(cleanPath.begin());
            command+=L" --foreign-project "+quote(cleanPath)+
                L" --development-requirement "+quote(requirement)+
                L" --workspace "+quote((runDirectory/L"r17_workspace").wstring())+
                L" --compiler "+quote(compiler.wstring())+
                L" --r17-execute --strategy-memory "+quote((root/L"gdi_gui_r17_strategies.gdis").wstring());
        } else if(isR18) {
            command+=L" --foreign-project "+quote(input)+L" --behavioral-change-requirement "+quote(requirement)+
                L" --workspace "+quote((runDirectory/L"r18_workspace").wstring())+L" --compiler "+quote(compiler.wstring())+
                L" --r18-execute --strategy-memory "+quote((root/L"gdi_gui_r18_strategies.gdis").wstring());
        } else if(isForeign) {
            const auto sandbox=bin/L"gpi_sandbox.exe";
            if(!std::filesystem::is_regular_file(sandbox))
                throw std::runtime_error("gpi_sandbox.exe is missing beside the GUI (fail closed)");
            std::wstring cleanPath=input;
            while(!cleanPath.empty()&&(cleanPath.back()==L'\r'||cleanPath.back()==L'\n'||cleanPath.back()==L' '||cleanPath.back()==L'\t'))cleanPath.pop_back();
            while(!cleanPath.empty()&&(cleanPath.front()==L'\r'||cleanPath.front()==L'\n'||cleanPath.front()==L' '||cleanPath.front()==L'\t'))cleanPath.erase(cleanPath.begin());
            command+=L" --foreign-project "+quote(cleanPath)+
                L" --workspace "+quote((runDirectory/L"foreign_workspace").wstring())+
                L" --compiler "+quote(compiler.wstring())+
                L" --sandbox "+quote(sandbox.wstring())+
                L" --cycles "+std::to_wstring(cycles)+
                L" --project-memory "+quote((root/L"gdi_gui_foreign_projects.gdipm").wstring())+
                L" --dev-memory "+quote((root/L"gdi_gui_dev_principles.gdidp").wstring())+
                L" --execute-strategy --strategy-memory "+quote((root/L"gdi_gui_strategies.gdis").wstring());
            if(useMemory) {
                command+=L" --memory "+quote((root/L"gdi_gui_components.memory").wstring())+
                         L" --repair-memory "+quote((root/L"gdi_gui_experience.memory").wstring());
            }
        } else if(isPlanner) {
            const auto sandbox=bin/L"gpi_sandbox.exe";
            if(!std::filesystem::is_regular_file(sandbox))
                throw std::runtime_error("gpi_sandbox.exe is missing beside the GUI (fail closed)");
            command+=L" --objective "+quote(input)+
                L" --workspace "+quote((runDirectory/L"project").wstring())+
                L" --compiler "+quote(compiler.wstring())+
                L" --sandbox "+quote(sandbox.wstring())+
                L" --cycles "+std::to_wstring(cycles);
            if(useMemory) {
                command+=L" --memory "+quote((root/L"gdi_gui_components.memory").wstring())+
                         L" --repair-memory "+quote((root/L"gdi_gui_experience.memory").wstring());
            }
        } else if(isIntelligence||isBehavior||isArchitect||isBlueprint) {
            const auto sandbox=bin/L"gpi_sandbox.exe";
            if(!std::filesystem::is_regular_file(sandbox))
                throw std::runtime_error("gpi_sandbox.exe is missing beside the GUI (fail closed)");
            const auto file=runDirectory/(isIntelligence?L"ontogeny.curriculum":
                (isBehavior?L"goal.behavior":(isArchitect?L"project.goal":L"project.blueprint")));
            writeUtf8File(file,input);
            const std::wstring flag=isIntelligence?L" --curriculum ":
                (isBehavior?L" --behavior ":(isArchitect?L" --goal ":L" --blueprint "));
            command+=flag+quote(file.wstring())+
                L" --workspace "+quote((runDirectory/L"project").wstring())+
                L" --compiler "+quote(compiler.wstring())+
                L" --sandbox "+quote(sandbox.wstring())+
                L" --cycles "+std::to_wstring(cycles);
            if(useMemory) {
                if(isIntelligence)
                    command+=L" --memory "+quote((root/L"gdi_gui_components.memory").wstring())+
                             L" --repair-memory "+quote((root/L"gdi_gui_experience.memory").wstring());
                else
                    command+=L" --memory "+quote((root/L"gdi_gui_experience.memory").wstring());
            }
        } else {
            command+=L" --prompt "+quote(input)+L" --workspace "+quote((runDirectory/L"research").wstring())+
                L" --compiler "+quote(compiler.wstring())+
                L" --architecture "+architecture+L" --cycles 2 --autonomous-cycles 3";
        }

        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        HANDLE rawRead{},rawWrite{};
        if(!CreatePipe(&rawRead,&rawWrite,&security,0))throw std::runtime_error("Cannot create output channel");
        Handle readPipe(rawRead),writePipe(rawWrite);
        SetHandleInformation(readPipe.value,HANDLE_FLAG_INHERIT,0);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);startup.hStdOutput=writePipe.value;startup.hStdError=writePipe.value;
        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(L'\0');
        if(!CreateProcessW(executable.c_str(),mutableCommand.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,bin.c_str(),&startup,&process))
            throw std::runtime_error("Cannot start development process");
        Handle processHandle(process.hProcess),threadHandle(process.hThread);
        Handle job(CreateJobObjectW(nullptr,nullptr));
        if(!job.value){TerminateProcess(processHandle.value,1);throw std::runtime_error("Cannot create cancellation job");}
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
           !AssignProcessToJobObject(job.value,processHandle.value)) {
            TerminateProcess(processHandle.value,1);
            throw std::runtime_error("Cannot secure development process tree");
        }
        {
            std::lock_guard lock(g_jobMutex);g_job=job.value;
        }
        if(ResumeThread(threadHandle.value)==static_cast<DWORD>(-1)) {
            TerminateJobObject(job.value,1);
            throw std::runtime_error("Cannot resume development process");
        }
        CloseHandle(writePipe.value);writePipe.value=nullptr;
        char buffer[4096];DWORD bytes=0;
        std::string pending;
        while(ReadFile(readPipe.value,buffer,sizeof(buffer),&bytes,nullptr)&&bytes>0) {
            pending.append(buffer,buffer+bytes);
            std::size_t pos;
            while((pos=pending.find('\n'))!=std::string::npos) {
                std::string line=pending.substr(0,pos+1);
                pending.erase(0,pos+1);
                try {postLog(widen(line));}catch(const std::exception&){postLog(L"[Output encoding error]\r\n");}
            }
            if(pending.size()>65536) {postLog(L"[Long output line truncated]\r\n");pending.clear();}
        }
        if(!pending.empty()) {
            try {postLog(widen(pending));}catch(const std::exception&){postLog(L"[Output encoding error]\r\n");}
        }
        WaitForSingleObject(processHandle.value,INFINITE);
        GetExitCodeProcess(processHandle.value,&exitCode);
        {
            std::lock_guard lock(g_jobMutex);g_job=nullptr;
        }
        postLog(L"\r\nErgebnisordner: "+runDirectory.wstring()+L"\r\n");
    } catch(const std::exception& error) {
        std::lock_guard lock(g_jobMutex);g_job=nullptr;
        postLog(L"\r\nGUI-FEHLER: "+widen(error.what())+L"\r\n");
    }
    PostMessageW(g_window,WM_GPI_DONE,exitCode,0);
}

void startRun() {
    if(g_running)return;
    try {
        const auto mode=currentGuiMode();
        const auto genSpec=generativeSpecMode();
        const auto input=genSpec?textOf(g_behavior):textOf(g_prompt);
        const auto requirement=(mode==GuiMode::R17Development||mode==GuiMode::R18BehavioralEvolution)?textOf(g_prompt):L"";
        const auto compiler=textOf(g_compiler),workspace=textOf(g_workspace);
        if(input.empty()||compiler.empty()||workspace.empty())
            throw std::invalid_argument("Ziel, Compiler und Arbeitsordner sind erforderlich.");
        if(!g_contractAccepted)throw std::invalid_argument("Bitte zuerst das Ziel analysieren.");
        if(mode==GuiMode::R17Development) {
            if(requirement.empty())throw std::invalid_argument("Bitte eine neue Entwicklungsanforderung eingeben.");
            std::wstring cleanPath=input;
            while(!cleanPath.empty()&&(cleanPath.back()==L'\r'||cleanPath.back()==L'\n'||cleanPath.back()==L' '||cleanPath.back()==L'\t'))cleanPath.pop_back();
            while(!cleanPath.empty()&&(cleanPath.front()==L'\r'||cleanPath.front()==L'\n'||cleanPath.front()==L' '||cleanPath.front()==L'\t'))cleanPath.erase(cleanPath.begin());
            if(!std::filesystem::is_directory(std::filesystem::path(cleanPath)))throw std::invalid_argument("Der Projektordner existiert nicht.");
            const auto contract=genomic::gpi::interpretDevelopmentRequirement(narrow(requirement));
            if(contract.status!=genomic::gpi::RequirementStatus::Clarified)throw std::invalid_argument("Anforderung liegt ausserhalb des sicheren R17-Syntheseumfangs.");
        } else if(mode==GuiMode::R18BehavioralEvolution) {
            if(requirement.empty())throw std::invalid_argument("Bitte eine R18-Verhaltensanforderung eingeben.");
            if(!std::filesystem::is_directory(std::filesystem::path(input)))throw std::invalid_argument("Das R18-Zielprojekt existiert nicht.");
            if(!genomic::gpi::synthesizeBehavioralIntegration(genomic::gpi::analyzeBehavioralProgram(input),genomic::gpi::interpretBehavioralChangeRequirement(narrow(requirement))).executable)throw std::invalid_argument("Keine sichere R18-Integrationsstelle nachgewiesen.");
        } else if(mode==GuiMode::ForeignProject) {
            std::wstring cleanPath=input;
            while(!cleanPath.empty()&&(cleanPath.back()==L'\r'||cleanPath.back()==L'\n'||cleanPath.back()==L' '||cleanPath.back()==L'\t'))cleanPath.pop_back();
            while(!cleanPath.empty()&&(cleanPath.front()==L'\r'||cleanPath.front()==L'\n'||cleanPath.front()==L' '||cleanPath.front()==L'\t'))cleanPath.erase(cleanPath.begin());
            if(!std::filesystem::is_directory(std::filesystem::path(cleanPath)))
                throw std::invalid_argument("Der Fremdprojekt-Ordner existiert nicht.");
        } else if(mode==GuiMode::AutonomousPlanner) {
            const auto plan=genomic::gpi::planAutonomousOntogenyFromObjective(narrow(input));
            if(!plan.audit.objectiveSupported)throw std::invalid_argument("Uebergeordnetes Ziel vom ontogenetischen Planer abgelehnt.");
        } else if(mode==GuiMode::DevelopmentalIntelligence)genomic::gpi::parseDevelopmentalCurriculumText(narrow(input));
        else if(mode==GuiMode::Behavioral)genomic::gpi::parseBehavioralGoalText(narrow(input));
        else if(mode==GuiMode::Architecture)genomic::gpi::parseTypedArchitectureGoalText(narrow(input));
        else if(mode==GuiMode::Blueprint)genomic::gpi::parseProjectBlueprintText(narrow(input),executableDirectory());
        else {
            const auto plan=genomic::gpi::decomposeResearchTask(narrow(input),false);
            if(!plan.supported||!plan.contract.accepted)throw std::invalid_argument("Zielvertrag nicht akzeptiert.");
        }
        const auto cyclesText=textOf(g_cycles);
        const unsigned cycles=static_cast<unsigned>(std::stoul(cyclesText));
        if(cycles<2||cycles>16)throw std::invalid_argument("Zyklen muessen zwischen 2 und 16 liegen.");
        const bool memory=SendMessageW(g_memory,BM_GETCHECK,0,0)==BST_CHECKED;
        const auto architecture=selectedArchitecture();
        const auto root=std::filesystem::absolute(std::filesystem::path(workspace));
        if(!std::filesystem::is_regular_file(std::filesystem::path(compiler)))
            throw std::invalid_argument("Compilerpfad muss auf eine vorhandene Datei zeigen.");
        SetWindowTextW(g_log,L"");g_lastRunDirectory.clear();
        setRunning(true,L"Entwicklung laeuft ...");
        if(g_worker.joinable())g_worker.join();
        g_worker=std::thread(runChild,input,requirement,std::filesystem::path(compiler),root,architecture,cycles,memory);
        // Unique run path is emitted by the worker, and shown in the result pane.
    } catch(const std::exception& error) {
        setRunning(false,L"Start fehlgeschlagen");
        MessageBoxW(g_window,widen(error.what()).c_str(),L"GDI starten",MB_ICONWARNING);
    }
}

void stopRun() {
    std::lock_guard lock(g_jobMutex);
    if(g_job)TerminateJobObject(g_job,1223);
    SetWindowTextW(g_status,L"Abbruch angefordert ...");
}

void browseCompiler() {
    wchar_t file[MAX_PATH]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g_window;
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrFilter=L"C++ Compiler (*.exe)\0*.exe\0Alle Dateien\0*.*\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&dialog))SetWindowTextW(g_compiler,file);
}

void browseWorkspace() {
    BROWSEINFOW info{};info.hwndOwner=g_window;info.lpszTitle=L"Basisordner fuer neue GDI-Laeufe auswaehlen";
    info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    if(PIDLIST_ABSOLUTE item=SHBrowseForFolderW(&info)) {
        wchar_t path[MAX_PATH]{};if(SHGetPathFromIDListW(item,path))SetWindowTextW(g_workspace,path);CoTaskMemFree(item);
    }
}

void layout(int width,int height) {
    const int margin=24,left=width*46/100,rightX=left+margin;
    const int colWidth=left-margin*2;
    MoveWindow(GetDlgItem(g_window,2003),margin,70,260,20,TRUE);
    MoveWindow(g_architecture,margin,93,colWidth,220,TRUE);
    MoveWindow(GetDlgItem(g_window,2001),margin,133,350,22,TRUE);
    MoveWindow(g_prompt,margin,159,colWidth,177,TRUE);
    MoveWindow(g_behavior,margin,159,colWidth,177,TRUE);
    MoveWindow(g_behaviorOpen,margin,345,130,30,TRUE);
    MoveWindow(g_behaviorExample,margin+142,345,140,30,TRUE);
    MoveWindow(g_behaviorSave,margin+294,345,130,30,TRUE);
    MoveWindow(g_analyze,margin,386,150,34,TRUE);
    MoveWindow(g_run,margin+160,386,165,34,TRUE);
    MoveWindow(g_stop,margin+336,386,120,34,TRUE);
    MoveWindow(GetDlgItem(g_window,2004),margin,432,260,20,TRUE);
    MoveWindow(g_compiler,margin,456,colWidth-96,28,TRUE);
    MoveWindow(GetDlgItem(g_window,ID_COMPILER_BROWSE),left-112,456,88,28,TRUE);
    MoveWindow(GetDlgItem(g_window,2005),margin,498,260,20,TRUE);
    MoveWindow(g_workspace,margin,522,colWidth-96,28,TRUE);
    MoveWindow(GetDlgItem(g_window,ID_WORKSPACE_BROWSE),left-112,522,88,28,TRUE);
    MoveWindow(GetDlgItem(g_window,2007),margin,565,100,20,TRUE);
    MoveWindow(g_cycles,margin+108,561,65,28,TRUE);
    MoveWindow(g_memory,margin+200,562,colWidth-200,26,TRUE);
    MoveWindow(GetDlgItem(g_window,2006),margin,608,260,20,TRUE);
    MoveWindow(g_contract,margin,632,colWidth,std::max(80,height-663),TRUE);
    MoveWindow(g_log,rightX,99,width-rightX-margin,std::max(160,height-135),TRUE);
    MoveWindow(g_status,width-390,22,365,26,TRUE);
    MoveWindow(GetDlgItem(g_window,2002),rightX,70,300,22,TRUE);
    MoveWindow(g_openResult,width-margin-190,65,190,28,TRUE);
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
        HWND title=makeControl(WC_STATICW,L"GDI · Genomic Developmental Intelligence",0,0);setFont(title,g_titleFont);
        MoveWindow(title,24,22,720,34,TRUE);
        g_status=makeControl(WC_STATICW,L"Bereit",SS_RIGHT,0);
        makeControl(WC_STATICW,L"AUFGABE",0,2001);makeControl(WC_STATICW,L"LIVE-PROTOKOLL UND ERGEBNIS",0,2002);
        makeControl(WC_STATICW,L"ENTWICKLUNGSMODUS",0,2003);makeControl(WC_STATICW,L"C++20-COMPILER",0,2004);
        makeControl(WC_STATICW,L"ARBEITSORDNER",0,2005);makeControl(WC_STATICW,L"ZIELVERTRAG",0,2006);
        makeControl(WC_STATICW,L"ZYKLEN",0,2007);
        g_prompt=makeControl(WC_EDITW,L"Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne die Summe",
            ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,ID_PROMPT);
        g_behavior=makeControl(WC_EDITW,L"",ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|ES_WANTRETURN,ID_BEHAVIOR);
        g_behaviorOpen=makeControl(WC_BUTTONW,L"Datei oeffnen",BS_PUSHBUTTON,ID_BEHAVIOR_OPEN);
        g_behaviorExample=makeControl(WC_BUTTONW,L"Beispiel laden",BS_PUSHBUTTON,ID_BEHAVIOR_EXAMPLE);
        g_behaviorSave=makeControl(WC_BUTTONW,L"Speichern unter",BS_PUSHBUTTON,ID_BEHAVIOR_SAVE);
        g_analyze=makeControl(WC_BUTTONW,L"Ziel analysieren",BS_PUSHBUTTON,ID_ANALYZE);
        g_run=makeControl(WC_BUTTONW,L"Entwicklung starten",BS_DEFPUSHBUTTON,ID_RUN);
        g_stop=makeControl(WC_BUTTONW,L"Abbrechen",BS_PUSHBUTTON,ID_STOP);EnableWindow(g_run,FALSE);EnableWindow(g_stop,FALSE);
        g_architecture=makeControl(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_ARCHITECTURE);
        for(const wchar_t* item:{
            L"GDI – Fremdprojekt: Buildvertrag, Baseline, Strategie & Entwicklung (Read-Only)",
            L"GDI R17 – Neue Softwarefunktion aus Anforderung entwickeln (isoliert)",
            L"GDI R18 – Bestehende Anwendung sicher verhaltensintegrieren (isoliert)",
            L"GDI – Autonomer ontogenetischer Entwicklungsplaner (Ziel -> eigenes Curriculum)",
            L"GDI – Autonome Entwicklungsintelligenz (.curriculum / Ontogenese)",
            L"GDI – Software aus Verhaltensbeispielen (.behavior)",
            L"GDI – Typisierte Zielarchitektur (.goal)",
            L"GDI – Projekt-Blueprint (.blueprint)",
            L"A–D-Vergleich",
            L"Einzelkontext",
            L"Isolierte Kompartimente",
            L"Kommunizierende Kompartimente",
            L"Mit Entwicklungserfahrung"})
            SendMessageW(g_architecture,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(item));
        SendMessageW(g_architecture,CB_SETCURSEL,0,0);
        const auto compilerPath=detectCompiler();
        g_compiler=makeControl(WC_EDITW,compilerPath.c_str(),ES_AUTOHSCROLL,ID_COMPILER);
        makeControl(WC_BUTTONW,L"Wählen …",BS_PUSHBUTTON,ID_COMPILER_BROWSE);
        const auto defaultWorkspace=(std::filesystem::temp_directory_path()/L"GDI_GUI_RUNS").wstring();
        g_workspace=makeControl(WC_EDITW,defaultWorkspace.c_str(),ES_AUTOHSCROLL,ID_WORKSPACE);
        makeControl(WC_BUTTONW,L"Wählen …",BS_PUSHBUTTON,ID_WORKSPACE_BROWSE);
        g_cycles=makeControl(WC_EDITW,L"8",ES_AUTOHSCROLL|ES_NUMBER,ID_CYCLES);
        g_memory=makeControl(WC_BUTTONW,L"Erfahrung speichern/nutzen",BS_AUTOCHECKBOX,ID_MEMORY);
        g_openResult=makeControl(WC_BUTTONW,L"Ergebnisordner oeffnen",BS_PUSHBUTTON,ID_OPEN_RESULT);
        EnableWindow(g_openResult,FALSE);
        g_contract=makeControl(WC_EDITW,L"Analysieren Sie zuerst die Aufgabe.",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,ID_CONTRACT);
        g_log=makeControl(WC_EDITW,L"Noch kein Entwicklungslauf gestartet.",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,ID_LOG);
        setFont(g_log,g_monoFont);setFont(g_behavior,g_monoFont);
        loadExample();changeMode();return 0;
    }
    case WM_SIZE:layout(LOWORD(lParam),HIWORD(lParam));return 0;
    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_PROMPT:
        case ID_BEHAVIOR:
            if(HIWORD(wParam)==EN_CHANGE&&!g_running){g_contractAccepted=false;EnableWindow(g_run,FALSE);SetWindowTextW(g_status,L"Aufgabe geändert – erneut analysieren");}
            return 0;
        case ID_ARCHITECTURE:
            if (HIWORD(wParam) == CBN_SELCHANGE && !g_running) {
                changeMode();
            }
            return 0;
        case ID_BEHAVIOR_OPEN:openBehaviorFile();return 0;
        case ID_BEHAVIOR_EXAMPLE:loadExample();return 0;
        case ID_BEHAVIOR_SAVE:saveBehaviorFile();return 0;
        case ID_OPEN_RESULT:
            if(!g_lastRunDirectory.empty())ShellExecuteW(window,L"open",g_lastRunDirectory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            return 0;
        case ID_ANALYZE:analyzeGoal();return 0;case ID_RUN:startRun();return 0;case ID_STOP:stopRun();return 0;
        case ID_COMPILER_BROWSE:browseCompiler();return 0;case ID_WORKSPACE_BROWSE:browseWorkspace();return 0;
        default:break;
        }break;
    case WM_GPI_LOG: {
        auto* value=reinterpret_cast<std::wstring*>(lParam);appendLog(*value);delete value;return 0;
    }
    case WM_GPI_DIRECTORY: {
        auto* path=reinterpret_cast<std::wstring*>(lParam);
        g_lastRunDirectory=*path;delete path;
        EnableWindow(g_openResult,!g_running&&!g_lastRunDirectory.empty());
        return 0;
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
        if(g_running&&MessageBoxW(window,L"Der Entwicklungslauf ist noch aktiv. Wirklich abbrechen?",L"GDI",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;
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
    const auto planned=genomic::gpi::planAutonomousOntogenyFromObjective(narrow(defaultAutonomousObjective()));
    const auto curriculum=genomic::gpi::parseDevelopmentalCurriculumText(narrow(defaultCurriculum()));
    const auto example=genomic::gpi::parseBehavioralGoalText(narrow(defaultBehavior()));
    const auto archGoal=genomic::gpi::parseTypedArchitectureGoalText(narrow(defaultArchitectureGoal()));
    const auto bpSpec=genomic::gpi::parseProjectBlueprintText(narrow(defaultBlueprintSpec()));
    // The foreign telemetry example intentionally contains only sources.  It
    // remains useful for source analysis, but cannot establish a build
    // contract.  Exercise the GUI's R15/R16 integration with the dedicated
    // multi-target CMake fixture instead.
    std::filesystem::path foreignEx;
    const auto bin=executableDirectory();
    for(const auto& root:{bin,bin.parent_path(),bin.parent_path().parent_path()}) {
        const auto candidate=root/L"examples"/L"r15_multitarget";
        if(std::filesystem::is_directory(candidate)) {
            foreignEx=std::filesystem::absolute(candidate);
            break;
        }
    }
    if(foreignEx.empty()) foreignEx=examplePath(GuiMode::ForeignProject);
    bool foreignOk=true,buildContractOk=true,strategyOk=true,r17Ok=true,r18Ok=true,r19Ok=true;
    if(!foreignEx.empty()&&std::filesystem::is_directory(foreignEx)) {
        const auto fpAnalysis=genomic::gpi::analyzeForeignCpp20Project(foreignEx);
        // A successful analysis of a healthy project may legitimately produce
        // no repair hypotheses.  Module discovery is the relevant integration
        // criterion here.
        foreignOk=!fpAnalysis.modules.empty();
        const auto contract=genomic::gpi::discoverBuildContract(foreignEx);
        buildContractOk=contract.buildSystem!=genomic::gpi::BuildSystemKind::Unknown&&!contract.discoveryEvidence.empty();
        genomic::gpi::StrategyPlanningRequest request;
        request.goal="GUI R15/R16 integration self-test";request.contract=contract;
        const auto strategy=genomic::gpi::AutonomousStrategyPlanner().plan(request);
        strategyOk=strategy.executable&&!strategy.selected.operators.empty();
        const auto requirement=genomic::gpi::interpretDevelopmentRequirement(
            "Add persistent session state that is saved, restored after restart, and rejected when corrupted.");
        const auto impact=genomic::gpi::analyzeChangeImpact(foreignEx,contract,requirement);
        const auto architecture=genomic::gpi::synthesizeArchitectureChange(requirement,impact,contract);
        r17Ok=requirement.status==genomic::gpi::RequirementStatus::Clarified&&
              !impact.nodes.empty()&&architecture.executable;
        std::filesystem::path r18Example;
        for(const auto& root:{bin,bin.parent_path(),bin.parent_path().parent_path()}) {
            const auto candidate=root/L"examples"/L"r18_csv_application";
            if(std::filesystem::is_directory(candidate)){r18Example=candidate;break;}
        }
        if(!r18Example.empty()) {
            const auto r18Contract=genomic::gpi::interpretBehavioralChangeRequirement("For CSV records skip invalid rows and write an error report.");
            const auto r18Plan=genomic::gpi::synthesizeBehavioralIntegration(genomic::gpi::analyzeBehavioralProgram(r18Example),r18Contract);
            r18Ok=r18Contract.status==genomic::gpi::BehavioralFactStatus::Verified&&r18Plan.executable;
        }
        std::filesystem::path r19Example;
        for(const auto& root:{bin,bin.parent_path(),bin.parent_path().parent_path()}) {
            const auto candidate=root/L"examples"/L"r19_csv_semantic";
            if(std::filesystem::is_directory(candidate)){r19Example=candidate;break;}
        }
        if(!r19Example.empty()) {
            const auto r19Contract=genomic::gpi::interpretSemanticIntegrationRequirement("For CSV records skip invalid rows and write an error report.");
            const auto r19Model=genomic::gpi::buildSemanticProgramModel(r19Example,genomic::gpi::discoverBuildContract(r19Example));
            const auto r19Delta=genomic::gpi::analyzeBehavioralDelta(r19Contract,r19Model);
            const auto r19Plan=genomic::gpi::discoverSemanticIntegrationPoint(r19Contract,r19Model,r19Delta,genomic::gpi::discoverBuildContract(r19Example));
            r19Ok=r19Plan.executable&&!r19Plan.selected.node.symbol.empty();
        }
    }
    const bool passed=accepted.supported&&accepted.contract.accepted&&!accepted.graph.empty()&&
           (!rejected.supported||!rejected.contract.accepted)&&
           planned.audit.objectiveSupported&&planned.derivedCurriculum.stages.size()==3&&
           curriculum.stages.size()==3&&
           example.functions.size()>=2&&!example.types.empty()&&!example.modules.empty()&&
           archGoal.functions.size()>=2&&!archGoal.types.empty()&&!archGoal.modules.empty()&&
           bpSpec.functions.size()>=2&&!bpSpec.types.empty()&&!bpSpec.modules.empty()&&
           foreignOk&&buildContractOk&&strategyOk&&r17Ok&&r18Ok&&r19Ok;
    const auto executable=std::filesystem::absolute(executablePath());
    std::ofstream evidence(executableDirectory()/"gdi_gui_self_test.json",std::ios::binary|std::ios::trunc);
    evidence<<"{\n  \"format\": \"gdi-gui-self-test-v1\",\n"
            <<"  \"passed\": "<<(passed?"true":"false")<<",\n"
            <<"  \"engine_version\": \""<<GDI_ENGINE_VERSION<<"\",\n"
            <<"  \"compiler\": \"GCC "<<__VERSION__<<"\",\n"
            <<"  \"process_path\": \""<<executable.generic_string()<<"\",\n"
            <<"  \"foreign_project_analysis\": "<<(foreignOk?"true":"false")<<",\n"
            <<"  \"build_contract_integration\": "<<(buildContractOk?"true":"false")<<",\n"
            <<"  \"strategy_planner_integration\": "<<(strategyOk?"true":"false")<<",\n"
            <<"  \"r17_requirement_pipeline\": "<<(r17Ok?"true":"false")<<",\n"
            <<"  \"r18_behavioral_pipeline\": "<<(r18Ok?"true":"false")<<",\n"
            <<"  \"r19_semantic_pipeline\": "<<(r19Ok?"true":"false")<<"\n}\n";
    return passed&&evidence?0:1;
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR commandLine,int show) {
    if(std::wstring(commandLine).find(L"--self-test")!=std::wstring::npos)return selfTest();
    INITCOMMONCONTROLSEX common{};common.dwSize=sizeof(common);common.dwICC=ICC_STANDARD_CLASSES;InitCommonControlsEx(&common);
    g_background=CreateSolidBrush(RGB(245,247,250));g_panel=CreateSolidBrush(RGB(255,255,255));
    WNDCLASSEXW type{};type.cbSize=sizeof(type);type.lpfnWndProc=windowProcedure;type.hInstance=instance;
    type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=g_background;type.lpszClassName=L"GdiIntegratedGui";
    type.hIcon=LoadIconW(nullptr,IDI_APPLICATION);type.hIconSm=type.hIcon;
    if(!RegisterClassExW(&type))return 1;
    HWND window=CreateWindowExW(0,type.lpszClassName,L"GDI – Genomic Developmental Intelligence",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1300,900,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;
    ShowWindow(window,show);UpdateWindow(window);
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return static_cast<int>(message.wParam);
}
