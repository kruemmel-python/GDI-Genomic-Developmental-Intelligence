#include "gpi/project_agent.hpp"
#include "gpi/experience_store.hpp"
#include "gpi/process_runner.hpp"
#include "gpi/line_endings.hpp"
#include "gpi/project_emitter.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
namespace genomic::gpi {
namespace {
using namespace std::chrono_literals;
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
void write(const std::filesystem::path& path,const std::string& data) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if (!out || !(out<<data)) throw std::runtime_error("Cannot write project test fixture");
}
std::string describe(const ProcessResult& p,const std::filesystem::path& log) {
    if (p.timedOut) return "Process timed out; diagnostics: "+log.string();
    if (!p.launched) return "Process launch failed: "+p.error;
    return "Process exit "+std::to_string(p.exitCode)+"; diagnostics: "+log.string();
}
}
ProjectAgentResult runProjectAgent(const TaskIR& task,uint64_t genomeHash,
                                   const ProjectAgentConfig& config) {
    ProjectAgentResult result;
    if (config.projectDir.empty() || config.compiler.empty())
        throw std::invalid_argument("Project directory and compiler are required");
    const auto count=task.samples.value_or(4);
    if (count>32) { result.events.push_back("BLOCKED: oracle supports at most 32 samples"); return result; }
    bool includeMath=!config.repairProbe || config.learnedMathHeaderRepair;
    auto project=emitProject(task,genomeHash,config.projectDir,includeMath);
    const auto artifacts=project.root/"verification";
    std::filesystem::create_directories(artifacts);
    const auto buildLog=artifacts/"compiler.out";
    const auto buildErr=artifacts/"compiler.err";
    const auto testLog=artifacts/"test.out";
    const auto testErr=artifacts/"test.err";
    const auto input=artifacts/"input.txt";
    const auto output=artifacts/"output.txt";
    const auto errors=artifacts/"runtime.err";
    const auto executable=artifacts/
#ifdef _WIN32
        "gpi_program.exe";
    const auto testExecutable=artifacts/"gpi_test.exe";
#else
        "gpi_program";
    const auto testExecutable=artifacts/"gpi_test";
#endif
    const auto headerFlag="-I"+project.includeDirectory.string();
    auto compile=[&](const std::filesystem::path& first,const std::filesystem::path& binary) {
        return runProcess({config.compiler,"-std=c++20","-Wall","-Wextra","-Wpedantic","-Werror","-O2",
                           headerFlag,first.string(),project.pipelineSource.string(),"-o",binary.string()},
                          project.root,{},buildLog,buildErr,120s);
    };
    for (std::size_t attempt=1;attempt<=3;++attempt) {
        result.attempts=attempt;
        const auto build=compile(project.mainSource,executable);
        if (!build.launched || build.exitCode!=0 || build.timedOut) {
            const auto diagnostic=read(buildErr);
            result.events.push_back("Compile attempt "+std::to_string(attempt)+": "+describe(build,buildErr));
            if (!includeMath && !build.timedOut &&
                (diagnostic.find("isfinite")!=std::string::npos || diagnostic.find("fmod")!=std::string::npos)) {
                write(project.pipelineSource,"#include <cmath>\n"+read(project.pipelineSource));
                includeMath=true;
                result.events.push_back("Repair: add <cmath> to generated pipeline from compiler diagnostics");
                continue;
            }
            result.events.push_back("BLOCKED: unknown compiler failure");
            return result;
        }
        result.events.push_back("Compile attempt "+std::to_string(attempt)+" passed (two translation units)");
        const auto testBuild=compile(project.testSource,testExecutable);
        if (!testBuild.launched || testBuild.exitCode!=0 || testBuild.timedOut) {
            result.events.push_back("BLOCKED: generated project test build: "+describe(testBuild,buildErr));
            return result;
        }
        const auto test=runProcess({testExecutable.string()},project.root,{},testLog,testErr,10s);
        if (!test.launched || test.exitCode!=0 || test.timedOut) {
            result.events.push_back("BLOCKED: generated unit test failed: "+describe(test,testErr));
            return result;
        }
        result.events.push_back("Generated C++20 unit test: PASS");
        std::vector<double> fixture;
        for (std::size_t i=1;i<=count;++i) fixture.push_back(static_cast<double>(i));
        for (int round=0;round<2;++round) {
            if (round==1) std::reverse(fixture.begin(),fixture.end());
            std::ostringstream text;
            for (double x:fixture) text<<std::setprecision(17)<<x<<'\n';
            write(input,text.str());
            std::string expected;
            try { expected=expectedOutputForInputs(task,fixture); }
            catch (const std::exception& e) {
                result.events.push_back(std::string("BLOCKED: independent AST oracle: ")+e.what());
                return result;
            }
            const auto run=runProcess({executable.string()},project.root,input,output,errors,10s);
            if (!run.launched || run.exitCode!=0 || run.timedOut || normalizeLineEndings(read(output)) != normalizeLineEndings(expected)) {
                result.events.push_back("BLOCKED: behavioral fixture "+std::to_string(round+1)+
                                        " mismatch: "+describe(run,errors));
                return result;
            }
            result.events.push_back("Independent AST behavior oracle fixture "+std::to_string(round+1)+": PASS");
        }
        result.behaviorVerified=true;
        result.success=true;
        const auto source=read(project.pipelineSource);
        const auto report=std::string("{\n  \"format\": \"GPI-v0.3\",\n")+
            "  \"genome_hash\": "+std::to_string(genomeHash)+",\n"+
            "  \"source_fingerprint\": "+std::to_string(fingerprint(source))+",\n"+
            "  \"attempts\": "+std::to_string(result.attempts)+",\n"+
            "  \"generated_unit_test\": true,\n"+
            "  \"independent_oracle_fixtures\": 2,\n"+
            "  \"behavior_verified\": true\n}\n";
        write(artifacts/"report.json",report);
        return result;
    }
    result.events.push_back("BLOCKED: attempt budget exhausted");
    return result;
}
}
