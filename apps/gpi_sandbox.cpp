#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <objbase.h>
#include <sddl.h>
#include <userenv.h>

namespace {
struct LocalMemory { void* value{}; ~LocalMemory(){ if(value) LocalFree(value); } };
struct Handle { HANDLE value{INVALID_HANDLE_VALUE}; ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);} };
struct Module { HMODULE value{}; ~Module(){if(value)FreeLibrary(value);} };

std::wstring widen(const std::string& value) {
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0);
    if (!size) throw std::runtime_error("Invalid UTF-8 argument");
    std::wstring result(static_cast<std::size_t>(size),L'\0');
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,result.data(),size))
        throw std::runtime_error("UTF-8 conversion failed");
    result.pop_back(); return result;
}
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\""; std::size_t slashes=0;
    for (wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=L'\"';slashes=0;continue;}
        result.append(slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');return result+L'\"';
}
bool inside(const std::filesystem::path& child,const std::filesystem::path& parent) {
    const auto c=std::filesystem::weakly_canonical(child);
    const auto p=std::filesystem::weakly_canonical(parent);
    auto ci=c.begin();
    for(auto pi=p.begin();pi!=p.end();++pi,++ci)
        if(ci==c.end() || _wcsicmp(pi->c_str(),ci->c_str())!=0) return false;
    return true;
}
PSID sandboxSid() {
    constexpr wchar_t name[]=L"GenomicGPIResearchSandbox";
    using DeriveSid=HRESULT (WINAPI*)(PCWSTR,PSID*);
    using CreateProfile=HRESULT (WINAPI*)(PCWSTR,PCWSTR,PCWSTR,PSID_AND_ATTRIBUTES,DWORD,PSID*);
    Module userenv; userenv.value=LoadLibraryW(L"userenv.dll");
    if(!userenv.value) throw std::runtime_error("AppContainer API is unavailable");
    const auto deriveAddress=GetProcAddress(static_cast<HMODULE>(userenv.value),
        "DeriveAppContainerSidFromAppContainerName");
    const auto createAddress=GetProcAddress(static_cast<HMODULE>(userenv.value),"CreateAppContainerProfile");
    static_assert(sizeof(DeriveSid)==sizeof(deriveAddress) && sizeof(CreateProfile)==sizeof(createAddress));
    DeriveSid derive{}; CreateProfile create{};
    std::memcpy(&derive,&deriveAddress,sizeof(derive));
    std::memcpy(&create,&createAddress,sizeof(create));
    if(!derive || !create) throw std::runtime_error("AppContainer API entry points are unavailable");
    PSID sid=nullptr;
    HRESULT hr=create(name,L"GPI Research Sandbox",
        L"Networkless sandbox for generated research candidates",nullptr,0,&sid);
    if(hr==HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) hr=derive(name,&sid);
    if(FAILED(hr) || !sid) throw std::runtime_error("Cannot create AppContainer profile");
    return sid;
}
std::filesystem::path appContainerFolder(PSID sid) {
    LPWSTR sidText=nullptr;
    if(!ConvertSidToStringSidW(sid,&sidText)) throw std::runtime_error("Cannot format AppContainer SID");
    LocalMemory sidTextOwner{sidText};
    using GetFolder=HRESULT (WINAPI*)(PCWSTR,PWSTR*);
    Module userenv;userenv.value=LoadLibraryW(L"userenv.dll");
    const auto address=userenv.value?GetProcAddress(userenv.value,"GetAppContainerFolderPath"):nullptr;
    static_assert(sizeof(GetFolder)==sizeof(address));
    GetFolder getFolder{};std::memcpy(&getFolder,&address,sizeof(getFolder));
    if(!getFolder) throw std::runtime_error("AppContainer storage API is unavailable");
    PWSTR folder=nullptr;
    if(SUCCEEDED(getFolder(sidText,&folder)) && folder) {
        const std::filesystem::path result(folder);CoTaskMemFree(folder);return result;
    }
    wchar_t localAppData[32768]{};
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",localAppData,32768);
    if(length==0 || length>=32768) throw std::runtime_error("Cannot resolve AppContainer storage");
    return std::filesystem::path(localAppData)/L"Packages"/L"GenomicGPIResearchSandbox"/L"LocalState";
}
void grantSandboxStorage(const std::filesystem::path& path,PSID sid) {
    const auto marker=path/L".acl_granted";
    std::error_code ec;
    if(std::filesystem::is_regular_file(marker,ec))return;
    PACL oldAcl=nullptr;PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(GetNamedSecurityInfoW(path.wstring().c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,
        nullptr,nullptr,&oldAcl,nullptr,&descriptor)!=ERROR_SUCCESS) throw std::runtime_error("Cannot read sandbox storage ACL");
    LocalMemory descriptorOwner{descriptor};
    EXPLICIT_ACCESSW access{};access.grfAccessPermissions=FILE_ALL_ACCESS;access.grfAccessMode=GRANT_ACCESS;
    access.grfInheritance=SUB_CONTAINERS_AND_OBJECTS_INHERIT;access.Trustee.TrusteeForm=TRUSTEE_IS_SID;
    access.Trustee.TrusteeType=TRUSTEE_IS_WELL_KNOWN_GROUP;access.Trustee.ptstrName=static_cast<LPWSTR>(sid);
    PACL newAcl=nullptr;
    if(SetEntriesInAclW(1,&access,oldAcl,&newAcl)!=ERROR_SUCCESS) throw std::runtime_error("Cannot construct sandbox storage ACL");
    LocalMemory aclOwner{newAcl};
    if(SetNamedSecurityInfoW(const_cast<LPWSTR>(path.wstring().c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,
        nullptr,nullptr,newAcl,nullptr)!=ERROR_SUCCESS) throw std::runtime_error("Cannot grant sandbox storage access");
    HANDLE m=CreateFileW(marker.wstring().c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(m!=INVALID_HANDLE_VALUE)CloseHandle(m);
}
}
#endif

int main(int argc,char** argv) {
    std::string workspace; unsigned long timeoutSeconds=0; int commandIndex=-1;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--" ){commandIndex=i+1;break;}
        if(i+1>=argc){std::cerr<<"Invalid sandbox arguments\n";return 2;}
        const std::string value=argv[++i];
        if(arg=="--workspace")workspace=value;
        else if(arg=="--timeout-seconds") {
            try { timeoutSeconds=std::stoul(value); } catch(...) { return 2; }
        } else {std::cerr<<"Unknown sandbox argument\n";return 2;}
    }
    if(workspace.empty()||timeoutSeconds<1||timeoutSeconds>300||commandIndex<0||commandIndex>=argc) return 2;
#ifndef _WIN32
    std::cerr<<"Integrated AppContainer sandbox is available only on Windows\n"; return 125;
#else
    try {
        const auto root=std::filesystem::weakly_canonical(std::filesystem::path(widen(workspace)));
        const auto executable=std::filesystem::weakly_canonical(std::filesystem::path(widen(argv[commandIndex])));
        if(!inside(executable,root) || !std::filesystem::is_regular_file(executable)) {
            std::cerr<<"Sandbox executable must be a regular file inside the workspace\n";return 126;
        }
        LocalMemory sidOwner{sandboxSid()};
        const auto containerRoot=appContainerFolder(static_cast<PSID>(sidOwner.value));
        std::filesystem::create_directories(containerRoot);
        grantSandboxStorage(containerRoot,static_cast<PSID>(sidOwner.value));
        const auto sharedLibs=containerRoot/L"shared_libs";
        std::filesystem::create_directories(sharedLibs);
        for(const auto& entry:std::filesystem::directory_iterator(root)) {
            if(entry.is_regular_file() && _wcsicmp(entry.path().extension().c_str(),L".dll")==0) {
                const auto dstDll=sharedLibs/entry.path().filename();
                std::error_code ec;
                if(!std::filesystem::is_regular_file(dstDll,ec) ||
                   std::filesystem::file_size(dstDll,ec)!=std::filesystem::file_size(entry.path(),ec)) {
                    std::filesystem::copy_file(entry.path(),dstDll,std::filesystem::copy_options::overwrite_existing,ec);
                }
            }
        }
        const auto runRoot=containerRoot/(L"run_"+std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(runRoot);
        struct RunCleanup { std::filesystem::path path; ~RunCleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);} } runCleanup{runRoot};
        const auto sandboxExecutable=runRoot/executable.filename();
        std::filesystem::copy_file(executable,sandboxExecutable,std::filesystem::copy_options::overwrite_existing);
        {
            const DWORD pathLen=GetEnvironmentVariableW(L"PATH",nullptr,0);
            std::wstring curPath(pathLen>0?pathLen:1,L'\0');
            if(pathLen>0)GetEnvironmentVariableW(L"PATH",curPath.data(),pathLen);
            if(!curPath.empty()&&curPath.back()==L'\0')curPath.pop_back();
            const std::wstring newPath=sharedLibs.wstring()+L";"+curPath;
            SetEnvironmentVariableW(L"PATH",newPath.c_str());
        }

        SECURITY_CAPABILITIES capabilities{};
        capabilities.AppContainerSid=static_cast<PSID>(sidOwner.value);
        SIZE_T attributeBytes=0;
        InitializeProcThreadAttributeList(nullptr,1,0,&attributeBytes);
        std::vector<unsigned char> attributes(attributeBytes);
        auto* attributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if(!InitializeProcThreadAttributeList(attributeList,1,0,&attributeBytes)) throw std::runtime_error("Cannot initialize sandbox attributes");
        struct AttributeCleanup { LPPROC_THREAD_ATTRIBUTE_LIST value; ~AttributeCleanup(){DeleteProcThreadAttributeList(value);} } cleanup{attributeList};
        constexpr DWORD_PTR securityCapabilitiesAttribute=0x00020009;
        if(!UpdateProcThreadAttribute(attributeList,0,securityCapabilitiesAttribute,
            &capabilities,sizeof(capabilities),nullptr,nullptr)) throw std::runtime_error("Cannot apply AppContainer capability policy");

        std::wstring command;
        command=quote(sandboxExecutable.wstring());
        for(int i=commandIndex+1;i<argc;++i){command+=L' ';command+=quote(widen(argv[i]));}
        std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(L'\0');
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributeList;
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
        startup.StartupInfo.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);
        startup.StartupInfo.hStdError=GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION process{};

        Handle job;job.value=CreateJobObjectW(nullptr,nullptr);
        if(!job.value)throw std::runtime_error("Cannot create sandbox job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|
            JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        limits.BasicLimitInformation.ActiveProcessLimit=1;
        limits.ProcessMemoryLimit=256ULL*1024ULL*1024ULL;
        if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
            throw std::runtime_error("Cannot configure sandbox resource limits");

        if(!CreateProcessW(sandboxExecutable.wstring().c_str(),buffer.data(),nullptr,nullptr,TRUE,
            EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,runRoot.wstring().c_str(),
            &startup.StartupInfo,&process)) throw std::runtime_error("Cannot launch AppContainer candidate: "+std::to_string(GetLastError()));
        Handle processHandle;processHandle.value=process.hProcess;
        Handle threadHandle;threadHandle.value=process.hThread;
        if(!AssignProcessToJobObject(job.value,process.hProcess)){TerminateProcess(process.hProcess,126);throw std::runtime_error("Cannot assign sandbox job");}
        if(ResumeThread(process.hThread)==static_cast<DWORD>(-1)){TerminateProcess(process.hProcess,126);throw std::runtime_error("Cannot resume sandbox candidate");}
        const DWORD wait=WaitForSingleObject(process.hProcess,timeoutSeconds*1000UL);
        if(wait==WAIT_TIMEOUT){TerminateJobObject(job.value,124);return 124;}
        DWORD code=126;if(!GetExitCodeProcess(process.hProcess,&code))return 126;
        return static_cast<int>(code);
    } catch(const std::exception& error) {
        std::cerr<<"GPI SANDBOX ERROR: "<<error.what()<<'\n';return 125;
    }
#endif
}
