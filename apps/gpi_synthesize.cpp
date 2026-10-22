#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/project_blueprint.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t cycleCount(const std::string& value) {
    std::size_t result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size()||result<2||result>16)
        throw std::invalid_argument("--cycles must be in 2..16");
    return result;
}
void printResult(const genomic::gpi::AutonomousProjectResult& result) {
    std::cout<<"GENOME_HASH="<<result.immutableGenomeHash<<"\nSUCCESS="<<(result.success?"YES":"NO")
             <<"\nSELECTED_HYPOTHESIS="<<result.selectedHypothesis<<"\nCYCLES="<<result.cycles.size()
             <<"\nEXPERIENCE_TRANSFERRED="<<(result.experienceTransferred?"YES":"NO")
             <<"\nLEARNED_TRANSFORMATIONS="<<result.transformations.size()<<"\nREPORT="<<result.report.string()<<'\n';
    for(const auto& cycle:result.cycles)
        std::cout<<"CYCLE="<<cycle.number<<" HYPOTHESIS="<<cycle.hypothesis
                 <<" VERIFIED="<<(cycle.evidence.success?"YES":"NO")
                 <<" EVIDENCE="<<cycle.evidence.report.string()<<'\n';
}
}

int main(int argc,char** argv) {
    std::filesystem::path blueprint,workspace,compiler="g++",sandbox,memory;std::size_t cycles=8;
    try {
        for(int index=1;index<argc;++index) {
            const std::string key=argv[index];if(index+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++index];
            if(key=="--blueprint")blueprint=value;else if(key=="--workspace")workspace=value;
            else if(key=="--compiler")compiler=value;else if(key=="--sandbox")sandbox=value;
            else if(key=="--memory")memory=value;else if(key=="--cycles")cycles=cycleCount(value);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        if(blueprint.empty()||workspace.empty()||sandbox.empty())
            throw std::invalid_argument("--blueprint, --workspace and --sandbox are required");
        if(std::filesystem::exists(workspace)&&!std::filesystem::is_empty(workspace))
            throw std::invalid_argument("--workspace must be new or empty");
        const auto system=genomic::GenomeBootstrap::buildCompleteSystem();
        auto specification=genomic::gpi::synthesizeProjectBlueprint(
            genomic::gpi::loadProjectBlueprint(blueprint),genomic::Cpp20SourceGenome::fromGenome(*system.strand));
        specification.contract.requireSandbox=true;
        std::filesystem::create_directories(workspace);
        if(!memory.empty()&&std::filesystem::exists(memory)&&std::filesystem::file_size(memory)>0) {
            genomic::gpi::AutonomousProjectConfig transfer{workspace/"transfer",compiler,sandbox,cycles,{}};
            const auto reused=genomic::gpi::runExperienceTransferredProjectDevelopment(specification.graph,
                specification.contract,transfer,genomic::gpi::ProjectExperienceStore(memory));
            if(reused.success){printResult(reused);std::cout<<"RESULT=PASS\n";return 0;}
        }
        genomic::gpi::AutonomousProjectConfig development{workspace/"development",compiler,sandbox,cycles,memory};
        const auto result=genomic::gpi::runCumulativeAutonomousProjectRepair(
            specification.graph,specification.contract,development);
        printResult(result);std::cout<<"RESULT="<<(result.success?"PASS":"BLOCKED")<<'\n';return result.success?0:4;
    } catch(const std::exception& error) {
        std::cerr<<"GPI SYNTHESIZE ERROR: "<<error.what()<<'\n';return 1;
    }
}
