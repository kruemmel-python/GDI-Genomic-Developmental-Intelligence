#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/behavioral_induction.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void write(const std::filesystem::path& path,const std::string& content) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream<<content;
    if(!stream)throw std::runtime_error("Cannot write development artifact: "+path.string());
}
std::size_t cycles(const std::string& value) {
    std::size_t count{};
    const auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),count);
    if(ec!=std::errc{}||end!=value.data()+value.size()||count<2||count>16)
        throw std::invalid_argument("--cycles must be 2..16");
    return count;
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path behavior,workspace,compiler="g++",sandbox,memory;
        std::size_t maxCycles=8;
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(i+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++i];
            if(key=="--behavior")behavior=value;
            else if(key=="--workspace")workspace=value;
            else if(key=="--compiler")compiler=value;
            else if(key=="--sandbox")sandbox=value;
            else if(key=="--memory")memory=value;
            else if(key=="--cycles")maxCycles=cycles(value);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        if(behavior.empty()||workspace.empty()||sandbox.empty())
            throw std::invalid_argument("--behavior, --workspace and --sandbox are required");
        if(std::filesystem::exists(workspace)&&!std::filesystem::is_empty(workspace))
            throw std::invalid_argument("--workspace must be new or empty");
        const auto system=genomic::GenomeBootstrap::buildCompleteSystem();
        const auto genome=genomic::Cpp20SourceGenome::fromGenome(*system.strand);
        auto induction=genomic::gpi::induceArchitectureGoal(genomic::gpi::loadBehavioralGoal(behavior),genome);
        auto architecture=genomic::gpi::architectTypedGoal(induction.goal,genome);
        architecture.project.contract.requireSandbox=true;
        std::filesystem::create_directories(workspace);
        write(workspace/"induction.json",induction.report);
        write(workspace/"inferred.goal",genomic::gpi::serializeTypedArchitectureGoal(induction.goal));
        write(workspace/"architecture.json",architecture.architectureReport);
        if(!memory.empty()&&std::filesystem::exists(memory)&&std::filesystem::file_size(memory)>0) {
            genomic::gpi::AutonomousProjectConfig transfer{workspace/"transfer",compiler,sandbox,maxCycles,{}};
            const auto reused=genomic::gpi::runExperienceTransferredProjectDevelopment(
                architecture.project.graph,architecture.project.contract,transfer,
                genomic::gpi::ProjectExperienceStore(memory));
            if(reused.success) {
                std::cout<<"GENOME_HASH="<<reused.immutableGenomeHash<<"\nRESULT=PASS\nREPORT="<<reused.report<<'\n';
                return 0;
            }
        }
        genomic::gpi::AutonomousProjectConfig config{workspace/"development",compiler,sandbox,maxCycles,memory};
        const auto result=genomic::gpi::runCumulativeAutonomousProjectRepair(
            architecture.project.graph,architecture.project.contract,config);
        std::cout<<"GENOME_HASH="<<result.immutableGenomeHash<<"\nCYCLES="<<result.cycles.size()
                 <<"\nRESULT="<<(result.success?"PASS":"BLOCKED")<<"\nREPORT="<<result.report<<'\n';
        return result.success?0:4;
    } catch(const std::exception& error) {
        std::cerr<<"GDI CREATE ERROR: "<<error.what()<<'\n';
        return 1;
    }
}
