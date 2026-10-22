#include "gpi/project_verifier.hpp"

#include "gpi/line_endings.hpp"
#include "gpi/process_runner.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace genomic::gpi {
namespace {
std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path& path,const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(content.data(),static_cast<std::streamsize>(content.size()));
    if(!stream)throw std::runtime_error("Cannot write verification artifact: "+path.string());
}
bool safeRelative(const std::filesystem::path& path) {
    if(path.empty()||path.is_absolute()||path.has_root_name()||path.has_root_directory())return false;
    return std::none_of(path.begin(),path.end(),[](const auto& part){return part==".."||part==".";});
}
std::string json(const std::string& value) {
    std::ostringstream result;result<<'"';
    for(const unsigned char c:value) {
        if(c=='"'||c=='\\')result<<'\\'<<static_cast<char>(c);
        else if(c=='\n')result<<"\\n";else if(c=='\r')result<<"\\r";else if(c=='\t')result<<"\\t";
        else if(c<0x20)result<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
        else result<<static_cast<char>(c);
    }
    return result.str()+'"';
}
}

ProjectVerificationEvidence verifyGenerativeProject(const GenerativeProjectGraph& graph,
    const ProjectVerificationContract& contract,const std::filesystem::path& emptyWorkspace,
    const std::filesystem::path& compiler,const std::filesystem::path& sandboxRunner) {
    graph.validate();
    if(contract.buildTargets.empty()||contract.buildTargets.size()>64||contract.behaviors.size()>128)
        throw std::invalid_argument("Verification contract has an invalid number of targets or behaviors");
    if(contract.compileTimeout.count()<1||contract.compileTimeout.count()>300||
       contract.executionTimeout.count()<1||contract.executionTimeout.count()>300)
        throw std::invalid_argument("Verification timeouts must be in 1..300 seconds");
    if(compiler.empty()||!std::filesystem::is_regular_file(compiler))
        throw std::invalid_argument("Verification compiler must be an existing regular file");
    if(contract.requireSandbox&&(sandboxRunner.empty()||!std::filesystem::is_regular_file(sandboxRunner)))
        throw std::invalid_argument("Verification contract requires an available sandbox runner");
    for(const auto& root:contract.includeRoots)if(!safeRelative(root))
        throw std::invalid_argument("Include roots must be safe relative paths");
    graph.materialize(emptyWorkspace);
    const auto workspace=std::filesystem::absolute(emptyWorkspace).lexically_normal();
    const auto artifacts=workspace/"verification";std::filesystem::create_directories(artifacts/"bin");
#ifdef _WIN32
    if(!sandboxRunner.empty()) {
        const auto compilerBin=std::filesystem::absolute(compiler).parent_path();
        for(const auto* name:{"libstdc++-6.dll","libgcc_s_seh-1.dll","libwinpthread-1.dll"}) {
            const auto source=compilerBin/name;
            if(std::filesystem::is_regular_file(source))
                std::filesystem::copy_file(source,workspace/name,std::filesystem::copy_options::overwrite_existing);
        }
    }
#endif
    std::unordered_map<std::string,const GenerativeProjectNode*> nodes;
    for(const auto& node:graph.nodes())nodes.emplace(node.id,&node);
    std::unordered_map<std::string,const GenerativeProjectTarget*> targets;
    for(const auto& target:graph.targets())targets.emplace(target.name,&target);
    std::unordered_map<std::string,std::filesystem::path> binaries;
    ProjectVerificationEvidence evidence;evidence.genomeHash=graph.genomeHash();evidence.graphHash=graph.graphHash();
    std::unordered_set<std::string> requested;
    for(const auto& name:contract.buildTargets) {
        if(!requested.insert(name).second)throw std::invalid_argument("Build target requested more than once");
        const auto found=targets.find(name);if(found==targets.end())throw std::invalid_argument("Unknown build target: "+name);
        const auto& target=*found->second;
        std::vector<std::string> command={compiler.string(),"-std=c++20","-Wall","-Wextra","-Wpedantic","-Werror","-O2"};
        for(const auto& include:contract.includeRoots)command.push_back("-I"+(workspace/include).string());
        std::size_t translationUnits=0;
        for(const auto& nodeId:graph.resolvedTargetNodes(target.name)) {
            const auto& node=*nodes.at(nodeId);
            if(node.kind==ProjectNodeKind::Source||node.kind==ProjectNodeKind::ModuleImplementation||node.kind==ProjectNodeKind::Test) {
                command.push_back((workspace/node.artifact.relativePath).string());++translationUnits;
            }
        }
        if(translationUnits==0)throw std::invalid_argument("Build target has no translation units: "+name);
        std::filesystem::path binary;
        if(target.kind==ProjectTargetKind::Library)command.push_back("-fsyntax-only");
        else {
            binary=artifacts/"bin"/(name+
#ifdef _WIN32
                ".exe"
#else
                ""
#endif
            );
            command.push_back("-o");command.push_back(binary.string());binaries.emplace(name,binary);
        }
        ProjectVerificationCheck check;check.name=name;check.kind="compile";
        check.standardOutput=artifacts/(name+".compiler.out");check.standardError=artifacts/(name+".compiler.err");
        const auto begin=std::chrono::steady_clock::now();
        const auto process=runProcess(command,workspace,{},check.standardOutput,check.standardError,contract.compileTimeout);
        check.milliseconds=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count());
        check.timedOut=process.timedOut;check.exitCode=process.exitCode;
        check.passed=process.launched&&!process.timedOut&&process.exitCode==0;
        if(!check.passed)check.diagnostic=process.error.empty()?read(check.standardError):process.error;
        evidence.checks.push_back(check);if(!check.passed)break;
    }
    if(std::all_of(evidence.checks.begin(),evidence.checks.end(),[](const auto& check){return check.passed;})) {
        std::unordered_set<std::string> behaviorNames;
        for(std::size_t i=0;i<contract.behaviors.size();++i) {
            const auto& behavior=contract.behaviors[i];
            if(behavior.name.empty()||!behaviorNames.insert(behavior.name).second)
                throw std::invalid_argument("Behavior names must be non-empty and unique");
            const auto binary=binaries.find(behavior.target);
            if(binary==binaries.end())throw std::invalid_argument("Behavior references a non-executable build target");
            ProjectVerificationCheck check;check.name=behavior.name;check.kind="behavior";
            check.sandboxed=!sandboxRunner.empty();check.standardOutput=artifacts/("behavior_"+std::to_string(i)+".out");
            check.standardError=artifacts/("behavior_"+std::to_string(i)+".err");
            const auto input=artifacts/("behavior_"+std::to_string(i)+".in");write(input,behavior.input);
            std::vector<std::string> command={binary->second.string()};
            auto timeout=contract.executionTimeout;
            if(!sandboxRunner.empty()) {
                command={sandboxRunner.string(),"--workspace",workspace.string(),"--timeout-seconds",
                    std::to_string(contract.executionTimeout.count()),"--",binary->second.string()};
                timeout+=std::chrono::seconds(5);
            }
            const auto begin=std::chrono::steady_clock::now();
            const auto process=runProcess(command,workspace,input,check.standardOutput,check.standardError,timeout);
            check.milliseconds=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count());
            check.timedOut=process.timedOut;check.exitCode=process.exitCode;
            check.passed=process.launched&&!process.timedOut&&process.exitCode==behavior.expectedExitCode&&
                normalizeLineEndings(read(check.standardOutput))==normalizeLineEndings(behavior.expectedOutput)&&
                normalizeLineEndings(read(check.standardError))==normalizeLineEndings(behavior.expectedError);
            if(!check.passed)check.diagnostic=process.error.empty()?"Runtime behavior differs from contract":process.error;
            evidence.checks.push_back(check);if(!check.passed)break;
        }
    }
    evidence.success=evidence.checks.size()==contract.buildTargets.size()+contract.behaviors.size()&&
        std::all_of(evidence.checks.begin(),evidence.checks.end(),[](const auto& check){return check.passed;});
    evidence.report=artifacts/"evidence.json";std::ostringstream report;
    report<<"{\n  \"format\": \"gpi-project-verification-v1\",\n  \"genome_hash\": "<<evidence.genomeHash
          <<",\n  \"graph_hash\": "<<evidence.graphHash<<",\n  \"success\": "<<(evidence.success?"true":"false")<<",\n  \"checks\": [\n";
    for(std::size_t i=0;i<evidence.checks.size();++i) {
        const auto& check=evidence.checks[i];report<<"    {\"name\": "<<json(check.name)<<", \"kind\": "<<json(check.kind)
            <<", \"passed\": "<<(check.passed?"true":"false")<<", \"sandboxed\": "<<(check.sandboxed?"true":"false")
            <<", \"timed_out\": "<<(check.timedOut?"true":"false")<<", \"exit_code\": "<<check.exitCode
            <<", \"milliseconds\": "<<check.milliseconds<<", \"diagnostic\": "<<json(check.diagnostic)<<"}"
            <<(i+1==evidence.checks.size()?"\n":",\n");
    }
    report<<"  ]\n}\n";write(evidence.report,report.str());return evidence;
}
} // namespace genomic::gpi
