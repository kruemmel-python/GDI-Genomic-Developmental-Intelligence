#include "genome/genome_bootstrap.hpp"
#include "gpi/integrated_development.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t bounded(const std::string& text,std::size_t minimum,std::size_t maximum,const std::string& option) {
    std::size_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || end!=text.data()+text.size() || value<minimum || value>maximum)
        throw std::invalid_argument(option+" is outside its allowed range");
    return value;
}
genomic::gpi::IntegratedArchitecture architecture(const std::string& value) {
    using A=genomic::gpi::IntegratedArchitecture;
    if(value=="single")return A::Single;
    if(value=="isolated")return A::Isolated;
    if(value=="communicating")return A::Communicating;
    if(value=="experienced")return A::Experienced;
    throw std::invalid_argument("--architecture must be single, isolated, communicating, experienced or compare");
}
void printResult(const genomic::gpi::IntegratedResult& result) {
    std::cout<<"ARCHITECTURE="<<genomic::gpi::integratedArchitectureName(result.architecture)
             <<"\nGENOME_HASH="<<result.development.genomeHash
             <<"\nCONTRACT_ACCEPTED="<<(result.contract.accepted?"YES":"NO")
             <<"\nCONTRACT_HASH="<<result.contract.semanticHash
             <<"\nNORMALIZED_INTENT="<<result.contract.normalizedIntent
             <<"\nCAPABILITY_DIAGNOSIS="<<result.capabilityDiagnosis
             <<"\nGOAL_FULFILLED="<<(result.metrics.goalFulfilled?"YES":"NO")
             <<"\nCONTRACT_VERIFIED="<<(result.metrics.contractVerified?"YES":"NO")
             <<"\nCOMPILED_CANDIDATES="<<result.metrics.compiledCandidates
             <<"\nSEMANTIC_CHECKS="<<result.metrics.semanticChecks
             <<"\nCOMPILE_MS="<<result.metrics.compileMilliseconds
             <<"\nEXECUTION_MS="<<result.metrics.executionMilliseconds
             <<"\nDECISION_ERRORS="<<result.metrics.decisionErrors
             <<"\nSUCCESSFUL_TRANSFERS="<<result.metrics.successfulTransfers<<'\n';
    for(const auto& compartment:result.compartments)
        std::cout<<"COMPARTMENT="<<compartment.id<<" MOLECULES="<<compartment.molecules.size()
                 <<" ACCESSIBLE_REGIONS="<<compartment.accessibleRegions.size()
                 <<" RECEIVED="<<compartment.receivedSignals<<" EMITTED="<<compartment.emittedSignals<<'\n';
    for(const auto& signal:result.signals)
        std::cout<<"SIGNAL="<<static_cast<unsigned>(signal.kind)<<" SOURCE="<<signal.source
                 <<" TARGET="<<signal.target<<" SIGNATURE="<<signal.signature
                 <<" DELIVERED="<<(signal.delivered?"YES":"NO")<<" EVIDENCE="<<signal.evidence<<'\n';
}
}

int main(int argc,char** argv) {
    std::string prompt,workspace,compiler="g++",memory,architectureName="compare";
    std::size_t cycles=2,autonomousCycles=3;
    try {
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(i+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++i];
            if(key=="--prompt")prompt=value;
            else if(key=="--workspace")workspace=value;
            else if(key=="--compiler")compiler=value;
            else if(key=="--memory")memory=value;
            else if(key=="--architecture")architectureName=value;
            else if(key=="--cycles")cycles=bounded(value,1,8,key);
            else if(key=="--autonomous-cycles")autonomousCycles=bounded(value,2,8,key);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        if(prompt.empty() || workspace.empty())throw std::invalid_argument("--prompt and --workspace are required");
        const auto plan=genomic::gpi::decomposeResearchTask(prompt,false);
        std::cout<<"ORIGINAL_GOAL="<<prompt<<'\n';
        for(const auto& item:plan.contract.unresolvedRequirements)std::cout<<"UNRESOLVED_REQUIREMENT="<<item<<'\n';
        for(const auto& item:plan.contract.contradictions)std::cout<<"CONTRADICTION="<<item<<'\n';
        if(!plan.supported || !plan.contract.accepted){std::cout<<"RESULT=BLOCKED\n";return 3;}
        const auto system=genomic::GenomeBootstrap::buildCompleteSystem();
        genomic::gpi::ResearchConfig config;config.workspace=workspace;config.compiler=compiler;
        config.memory=memory.empty()?std::filesystem::path(workspace).parent_path()/"integrated.memory":memory;
        config.genome=system.strand.get();config.maxCycles=cycles;
        if(architectureName=="compare") {
            const auto results=genomic::gpi::runArchitectureComparison(plan,config,autonomousCycles);
            bool passed=true;
            for(const auto& result:results){printResult(result);passed=passed&&result.metrics.goalFulfilled;}
            std::cout<<"RESULT="<<(passed?"PASS":"BLOCKED")<<'\n';return passed?0:4;
        }
        const auto result=genomic::gpi::runIntegratedDevelopment(plan,config,architecture(architectureName),autonomousCycles);
        printResult(result);std::cout<<"RESULT="<<(result.metrics.goalFulfilled?"PASS":"BLOCKED")<<'\n';
        return result.metrics.goalFulfilled?0:4;
    } catch(const std::exception& error) {
        std::cerr<<"GPI INTEGRATED ERROR: "<<error.what()<<'\n';return 1;
    }
}
