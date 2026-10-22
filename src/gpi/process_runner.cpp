#include "gpi/process_runner.hpp"
#include <chrono>
#include <cwchar>
#include <thread>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace genomic::gpi {
#ifdef _WIN32
namespace {
std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0);
    if (!size) throw std::runtime_error("Invalid UTF-8 process argument");
    std::wstring wide(static_cast<std::size_t>(size),L'\0');
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,wide.data(),size))
        throw std::runtime_error("UTF-8 conversion failed");
    wide.pop_back(); return wide;
}
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";
    std::size_t slashes=0;
    for (wchar_t c:value) {
        if (c==L'\\') { ++slashes; continue; }
        if (c==L'"') { result.append(slashes*2+1,L'\\'); result+=L'"'; slashes=0; continue; }
        result.append(slashes,L'\\'); slashes=0; result+=c;
    }
    result.append(slashes*2,L'\\'); return result+L'"';
}
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() { if (value!=INVALID_HANDLE_VALUE && value!=nullptr) CloseHandle(value); }
    Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete;
    Handle()=default;
};

std::vector<wchar_t> childEnvironmentWithExecutableDirectory(const std::vector<std::string>& args) {
    std::vector<wchar_t> environment;
    const auto executable=std::filesystem::path(widen(args.front()));
    if (!executable.has_parent_path()) return environment;

    const auto directory=std::filesystem::absolute(executable).parent_path().wstring();
    LPWCH currentEnvironment=GetEnvironmentStringsW();
    if (!currentEnvironment) throw std::runtime_error("Cannot read process environment");

    bool pathReplaced=false;
    for (const wchar_t* entry=currentEnvironment; *entry!='\0'; entry+=std::wcslen(entry)+1) {
        std::wstring value(entry);
        const bool isPath=value.size()>=5 &&
            (value[0]==L'P' || value[0]==L'p') &&
            (value[1]==L'A' || value[1]==L'a') &&
            (value[2]==L'T' || value[2]==L't') &&
            (value[3]==L'H' || value[3]==L'h') && value[4]==L'=';
        if (isPath) {
            value=L"PATH="+directory+L";"+value.substr(5);
            pathReplaced=true;
        }
        environment.insert(environment.end(),value.begin(),value.end());
        environment.push_back(L'\0');
    }
    FreeEnvironmentStringsW(currentEnvironment);
    if (!pathReplaced) {
        const auto value=L"PATH="+directory;
        environment.insert(environment.end(),value.begin(),value.end());
        environment.push_back(L'\0');
    }
    environment.push_back(L'\0');
    return environment;
}
}
#endif
ProcessResult runProcess(const std::vector<std::string>& args,
                         const std::filesystem::path& cwd,
                         const std::filesystem::path& stdinFile,
                         const std::filesystem::path& stdoutFile,
                         const std::filesystem::path& stderrFile,
                         std::chrono::milliseconds timeout) {
    ProcessResult result;
    if (args.empty() || args.front().empty() || timeout.count()<=0)
        return {false,false,-1,"Invalid process arguments or timeout"};
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    Handle input,output,error;
    input.value=CreateFileW(stdinFile.empty()?L"NUL":stdinFile.wstring().c_str(),GENERIC_READ,FILE_SHARE_READ,
                            &security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    output.value=CreateFileW(stdoutFile.wstring().c_str(),GENERIC_WRITE,FILE_SHARE_READ,
                             &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    error.value=CreateFileW(stderrFile.wstring().c_str(),GENERIC_WRITE,FILE_SHARE_READ,
                            &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (input.value==INVALID_HANDLE_VALUE || output.value==INVALID_HANDLE_VALUE || error.value==INVALID_HANDLE_VALUE)
        return {false,false,-1,"Cannot open process redirection files"};
    std::wstring command;
    for (const auto& a:args) { if (!command.empty()) command+=L' '; command+=quote(widen(a)); }
    std::vector<wchar_t> buffer(command.begin(),command.end()); buffer.push_back(L'\0');
    auto childEnvironment=childEnvironmentWithExecutableDirectory(args);
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input.value; startup.hStdOutput=output.value; startup.hStdError=error.value;
    PROCESS_INFORMATION process{};
    Handle job;
    job.value=CreateJobObjectW(nullptr,nullptr);
    if (job.value!=nullptr && job.value!=INVALID_HANDLE_VALUE) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
            return {false,false,-1,"Cannot configure child-process timeout job"};
    } else return {false,false,-1,"Cannot create child-process timeout job"};
    const auto dir=cwd.wstring();
    if (!CreateProcessW(nullptr,buffer.data(),nullptr,nullptr,TRUE,
                        CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,
                        childEnvironment.empty()?nullptr:childEnvironment.data(),
                        dir.empty()?nullptr:dir.c_str(),&startup,&process))
        return {false,false,-1,"CreateProcessW failed: "+std::to_string(GetLastError())};
    result.launched=true;
    if (!AssignProcessToJobObject(job.value,process.hProcess)) {
        TerminateProcess(process.hProcess,126);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return {false,false,-1,"Cannot assign child to timeout job (nested job restriction?)"};
    }
    if (ResumeThread(process.hThread)==static_cast<DWORD>(-1)) {
        TerminateProcess(process.hProcess,126);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return {false,false,-1,"Cannot resume child process"};
    }
    CloseHandle(process.hThread);
    const DWORD wait=WaitForSingleObject(process.hProcess,static_cast<DWORD>(timeout.count()));
    if (wait==WAIT_TIMEOUT) {
        result.timedOut=true; TerminateJobObject(job.value,124);
        WaitForSingleObject(process.hProcess,INFINITE);
    }
    DWORD code=0;
    if (GetExitCodeProcess(process.hProcess,&code)) result.exitCode=static_cast<int>(code);
    else result.error="GetExitCodeProcess failed";
    CloseHandle(process.hProcess);
#else
    const pid_t pid=fork();
    if (pid<0) return {false,false,-1,"fork failed"};
    if (pid==0) {
        (void)setsid();
        if (!cwd.empty() && chdir(cwd.c_str())!=0) _exit(126);
        const int in=open(stdinFile.empty()?"/dev/null":stdinFile.c_str(),O_RDONLY);
        const int out=open(stdoutFile.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
        const int err=open(stderrFile.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
        if (in<0 || out<0 || err<0) _exit(126);
        if (dup2(in,STDIN_FILENO)<0 || dup2(out,STDOUT_FILENO)<0 || dup2(err,STDERR_FILENO)<0) _exit(126);
        close(in); close(out); close(err);
        std::vector<char*> argv;
        for (const auto& arg:args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0],argv.data());
        _exit(127);
    }
    result.launched=true;
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    int status=0;
    while (true) {
        const pid_t done=waitpid(pid,&status,WNOHANG);
        if (done==pid) {
            result.exitCode=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
            break;
        }
        if (done<0) { result.error="waitpid failed"; break; }
        if (std::chrono::steady_clock::now()>=deadline) {
            result.timedOut=true;
            kill(-pid,SIGKILL);
            kill(pid,SIGKILL);
            (void)waitpid(pid,&status,0);
            result.exitCode=124;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
#endif
    return result;
}
}
