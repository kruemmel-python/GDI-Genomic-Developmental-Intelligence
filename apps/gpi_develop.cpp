#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/project_manifest.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t cycles(const std::string& value) {
    std::size_t result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size()||result<2||result>16)
        throw std::invalid_argument("--cycles must be in 2..16");
    return result;
}
void print(const genomic::gpi::AutonomousProjectResult& result) {
    std::cout<<"GENOME_HASH="<<result.immutableGenomeHash<<"\nSUCCESS="<<(result.success?"YES":"NO")
             <<"\nSELECTED_HYPOTHESIS="<<result.selectedHypothesis
             <<"\nCYCLES="<<result.cycles.size()
             <<"\nEXPERIENCE_TRANSFERRED="<<(result.experienceTransferred?"YES":"NO")
             <<"\nLEARNED_TRANSFORMATIONS="<<result.transformations.size()
             <<"\nREPORT="<<result.report.string()<<'\n';
    for(const auto& cycle:result.cycles)
        std::cout<<"CYCLE="<<cycle.number<<" HYPOTHESIS="<<cycle.hypothesis
                 <<" VERIFIED="<<(cycle.evidence.success?"YES":"NO")
                 <<" EVIDENCE="<<cycle.evidence.report.string()<<'\n';
}
}

int main(int argc,char** argv) {
    std::filesystem::path manifest,projectRoot,workspace,compiler="g++",sandbox,memory;
    std::size_t maxCycles=8;
    try {
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++i];
            if(key=="--manifest")manifest=value;else if(key=="--project-root")projectRoot=value;
            else if(key=="--workspace")workspace=value;else if(key=="--compiler")compiler=value;
            else if(key=="--sandbox")sandbox=value;else if(key=="--memory")memory=value;
            else if(key=="--cycles")maxCycles=cycles(value);else throw std::invalid_argument("Unknown option: "+key);
        }
        if(manifest.empty()||projectRoot.empty()||workspace.empty())
            throw std::invalid_argument("--manifest, --project-root and --workspace are required");
        if(std::filesystem::exists(workspace)&&!std::filesystem::is_empty(workspace))
            throw std::invalid_argument("--workspace must be new or empty");
        const auto system=genomic::GenomeBootstrap::buildCompleteSystem();
        const auto specification=genomic::gpi::loadProjectManifest(manifest,projectRoot,
            genomic::Cpp20SourceGenome::fromGenome(*system.strand));
        std::filesystem::create_directories(workspace);
        if(!memory.empty()&&std::filesystem::exists(memory)&&std::filesystem::file_size(memory)>0) {
            genomic::gpi::AutonomousProjectConfig transfer{workspace/"transfer",compiler,sandbox,maxCycles,{}};
            const auto transferred=genomic::gpi::runExperienceTransferredProjectDevelopment(
                specification.graph,specification.contract,transfer,genomic::gpi::ProjectExperienceStore(memory));
            if(transferred.success){print(transferred);std::cout<<"RESULT=PASS\n";return 0;}
        }
        genomic::gpi::AutonomousProjectConfig repair{workspace/"repair",compiler,sandbox,maxCycles,memory};
        const auto result=genomic::gpi::runCumulativeAutonomousProjectRepair(specification.graph,specification.contract,repair);
        print(result);std::cout<<"RESULT="<<(result.success?"PASS":"BLOCKED")<<'\n';return result.success?0:4;
    } catch(const std::exception& error) {
        std::cerr<<"GPI DEVELOP ERROR: "<<error.what()<<'\n';return 1;
    }
}
