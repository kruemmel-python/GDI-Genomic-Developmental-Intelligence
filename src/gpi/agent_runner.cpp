#include "gpi/agent_runner.hpp"
#include "gpi/process_runner.hpp"
#include "gpi/line_endings.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
namespace genomic::gpi {
namespace {
bool safePath(std::string_view path) {
    if (path.empty() || path.size()>2048) return false;
    for (unsigned char c : path) {
        if (!(c>='a'&&c<='z') && !(c>='A'&&c<='Z') && !(c>='0'&&c<='9') &&
            c!='_' && c!='-' && c!='.' && c!='/' && c!='\\' && c!=':' && c!='+' && c!=' ')
            return false;
    }
    return true;
}
std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path,std::ios::binary | std::ios::trunc);
    if (!out || !(out << text)) throw std::runtime_error("Cannot write: " + path.string());
}
}
AgentResult runAgent(const TaskIR& task, uint64_t genomeHash, const AgentConfig& config) {
    AgentResult result;
    if (!safePath(config.compiler) || !safePath(config.output.string()) || config.output.extension() != ".cpp")
        throw std::invalid_argument("Unsafe compiler/output path or missing .cpp extension");
    if (!config.output.parent_path().empty()) std::filesystem::create_directories(config.output.parent_path());
    auto binary = config.output;
#ifdef _WIN32
    binary.replace_extension(".exe");
#else
    binary.replace_extension(".bin");
#endif
    result.executable=binary;
    const auto log = std::filesystem::path(config.output.string()+".compiler.log");
    const auto logerr = std::filesystem::path(config.output.string()+".compiler.err");
    const auto input = std::filesystem::path(config.output.string()+".smoke.in");
    const auto output = std::filesystem::path(config.output.string()+".smoke.out");
    const auto errors = std::filesystem::path(config.output.string()+".smoke.err");
    for (const auto& path : {binary,log,logerr,input,output,errors}) {
        if (!safePath(path.string())) throw std::invalid_argument("Unsafe generated path");
    }
    bool includeMath = !config.injectMissingMathHeader || config.learnedMathHeaderRepair;
    if (config.injectMissingMathHeader && config.learnedMathHeaderRepair)
        result.events.push_back("Reused prior successful header repair from experience");
    for (std::size_t attempt=1;attempt<=3;++attempt) {
        result.attempts=attempt;
        result.source=generateTaskSource(task,genomeHash,includeMath);
        writeText(config.output,result.source);
        // Execute compiler directly without a shell; bound CPU wall time.
        const auto compile=runProcess({config.compiler,"-std=c++20","-Wall","-Wextra","-Wpedantic","-Werror","-O2",
            config.output.string(),"-o",binary.string()}, {},{},log,logerr,std::chrono::seconds(120));
        const bool compileFailed=!compile.launched || compile.timedOut || compile.exitCode!=0;
        if (compileFailed) {
            const auto diagnostics=readText(logerr);
            result.events.push_back("Compile attempt " + std::to_string(attempt) + " failed; diagnostics: " + log.string());
            if (!compile.timedOut && !includeMath && (diagnostics.find("isfinite")!=std::string::npos ||
                                 diagnostics.find("fmod")!=std::string::npos)) {
                includeMath=true;
                result.events.push_back("Repair: add required <cmath> include from compiler diagnostics");
                continue;
            }
            result.events.push_back("BLOCKED: compiler error outside audited repair rules");
            return result;
        }
        result.events.push_back("Compile attempt " + std::to_string(attempt) + " passed");
        // Execute a bounded, deterministic fixture and compare with an independent
        // host-side AST interpreter. The program cannot execute arbitrary prompt code.
        const auto count = task.samples.value_or(4);
        if (count > 32) {
            result.events.push_back("BLOCKED: samples > 32; automatic behavior oracle cannot verify this task");
            return result;
        }
        std::ostringstream fixture;
        for (std::size_t i=1;i<=count;++i) fixture << i << '\n';
        writeText(input,fixture.str());
        std::string expected;
        try { expected=expectedSmokeOutput(task); }
        catch (const std::exception& e) {
            result.events.push_back(std::string("BLOCKED: oracle cannot verify fixture: ")+e.what());
            return result;
        }
        const auto run=runProcess({std::filesystem::absolute(binary).string()}, {},input,output,errors,std::chrono::seconds(10));
        if (!run.launched || run.timedOut || run.exitCode != 0 || normalizeLineEndings(readText(output)) != normalizeLineEndings(expected)) {
            result.events.push_back("BLOCKED: runtime mismatch; inspect " + output.string() + " and " + errors.string());
            return result;
        }
        result.behaviorVerified=true;
        result.success=true;
        result.events.push_back("Independent behavior oracle: PASS");
        return result;
    }
    result.events.push_back("BLOCKED: attempt budget exhausted");
    return result;
}
} // namespace genomic::gpi
