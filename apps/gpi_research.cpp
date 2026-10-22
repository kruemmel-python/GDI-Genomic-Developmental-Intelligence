#include "gpi/research_developer.hpp"
#include "genome/genome_bootstrap.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
std::string readFile(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read prompt file");
    std::string text{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    if (text.size()>65536) throw std::runtime_error("Prompt file exceeds 64 KiB");
    return text;
}
std::size_t parseCycles(const std::string& text) {
    std::size_t value=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if (error!=std::errc{} || end!=text.data()+text.size() || value<1 || value>8)
        throw std::invalid_argument("--cycles must be in 1..8");
    return value;
}
void usage() {
    std::cerr<<"Usage: gpi_research --prompt TEXT --workspace DIR [--compiler PATH] [--cycles 1..8] [--memory FILE]\n"
                "           [--development-state explore|control|developed] [--autonomous-cycles 2..8]\n"
                "       gpi_research --prompt-file FILE --workspace DIR --research-unsafe\n"
                "           --sandbox-runner PATH --fixture-input FILE --fixture-expected FILE\n";
}
}

int main(int argc,char** argv) {
    std::string prompt,promptFile,workspace,compiler="g++",fixtureInput,fixtureExpected,sandboxRunner,memory;
    auto developmentState=genomic::gpi::ResearchDevelopmentState::Explore;
    std::size_t cycles=3,autonomousCycles=0; bool unsafeMode=false;
    for (int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if (key=="--research-unsafe") { unsafeMode=true; continue; }
        if (i+1>=argc) { usage(); return 2; }
        const std::string value=argv[++i];
        if (key=="--prompt") prompt=value;
        else if (key=="--prompt-file") promptFile=value;
        else if (key=="--workspace") workspace=value;
        else if (key=="--compiler") compiler=value;
        else if (key=="--cycles") cycles=parseCycles(value);
        else if (key=="--autonomous-cycles") {
            autonomousCycles=parseCycles(value);
            if(autonomousCycles<2)throw std::invalid_argument("--autonomous-cycles must be in 2..8");
        }
        else if (key=="--fixture-input") fixtureInput=value;
        else if (key=="--fixture-expected") fixtureExpected=value;
        else if (key=="--sandbox-runner") sandboxRunner=value;
        else if (key=="--memory") memory=value;
        else if (key=="--development-state") {
            if (value=="explore") developmentState=genomic::gpi::ResearchDevelopmentState::Explore;
            else if (value=="control") developmentState=genomic::gpi::ResearchDevelopmentState::Control;
            else if (value=="developed") developmentState=genomic::gpi::ResearchDevelopmentState::Developed;
            else throw std::invalid_argument("--development-state must be explore, control or developed");
        }
        else { usage(); return 2; }
    }
    if (workspace.empty() || (!prompt.empty())+(!promptFile.empty())!=1) { usage(); return 2; }
    try {
        if (!promptFile.empty()) prompt=readFile(promptFile);
        if (unsafeMode && sandboxRunner.empty()) {
            auto bundled=std::filesystem::absolute(argv[0]).parent_path()/
#ifdef _WIN32
                "gpi_sandbox.exe";
#else
                "gpi_sandbox";
#endif
            if (std::filesystem::is_regular_file(bundled)) sandboxRunner=bundled.string();
        }
        const auto plan=genomic::gpi::decomposeResearchTask(prompt,unsafeMode);
        std::cout<<"RESEARCH_MODE="<<(autonomousCycles?"AUTONOMOUS_SAFE":(unsafeMode?"UNSAFE_SANDBOXED":"TEMPLATE_SANDBOXED"))<<'\n';
        for (const auto& component:plan.components)
            std::cout<<"COMPONENT="<<component.component<<" RESPONSIBILITY="<<component.responsibility
                     <<" DATA_STRUCTURE="<<component.dataStructure<<" ALGORITHM="<<component.algorithm<<'\n';
        for (const auto& node:plan.graph) {
            std::cout<<"GRAPH_NODE="<<node.id<<" OPERATION="<<node.operation<<" INPUT="<<node.inputType
                     <<" OUTPUT="<<node.outputType<<" DEPENDS=";
            for (std::size_t i=0;i<node.dependencies.size();++i) std::cout<<(i?",":"")<<node.dependencies[i];
            std::cout<<'\n';
        }
        for (const auto& diagnostic:plan.diagnostics) std::cout<<"DIAGNOSTIC="<<diagnostic<<'\n';
        std::cout<<"GOAL_CONTRACT_ACCEPTED="<<(plan.contract.accepted?"YES":"NO")
                 <<"\nGOAL_CONTRACT_HASH="<<plan.contract.semanticHash
                 <<"\nGOAL_NORMALIZED="<<plan.contract.normalizedIntent<<'\n';
        if (!plan.supported) { std::cout<<"RESULT=BLOCKED\n"; return 3; }
        genomic::gpi::ResearchConfig config;
        const auto genomicSystem=genomic::GenomeBootstrap::buildCompleteSystem();
        config.workspace=workspace; config.compiler=compiler; config.maxCycles=cycles; config.unsafeMode=unsafeMode;
        config.fixtureInput=fixtureInput; config.fixtureExpected=fixtureExpected; config.sandboxRunner=sandboxRunner;
        config.memory=memory;config.genome=genomicSystem.strand.get();config.developmentState=developmentState;
        const auto result=autonomousCycles?
            genomic::gpi::runAutonomousDevelopment(plan,config,autonomousCycles):
            genomic::gpi::runResearchDevelopment(plan,config);
        std::cout<<"GENOME_HASH="<<result.genomeHash<<'\n';
        for (const auto& activation:result.regulation)
            std::cout<<"REGULATION: STRATEGY="<<activation.strategy<<" REGION="<<activation.regionId
                     <<" SIGNATURE="<<activation.moleculeSignature<<" ACTIVITY="<<activation.activity
                     <<" EXPRESSED="<<(activation.expressed?"YES":"NO")
                     <<" CONDITION="<<activation.developmentalCondition<<'\n';
        for (const auto& cycle:result.history)
            std::cout<<"CYCLE="<<cycle.number<<" STRATEGY="<<cycle.strategy<<" COMPILED="
                     <<(cycle.compiled?"YES":"NO")<<" VERIFIED="<<(cycle.behaviorVerified?"YES":"NO")
                     <<" SCORE="<<cycle.score<<" REGION="<<cycle.genomeRegion
                     <<" SIGNATURE="<<cycle.moleculeSignature<<" ACTIVITY="<<cycle.regulatoryActivity
                     <<" SEMANTIC_CHECKS="<<cycle.semanticChecks<<" COMPILE_MS="<<cycle.compileMilliseconds
                     <<" EXECUTION_MS="<<cycle.executionMilliseconds<<'\n';
        for (const auto& event:result.events) std::cout<<"RESEARCH: "<<event<<'\n';
        std::cout<<"AUTONOMOUS_MODE="<<(result.autonomousMode?"YES":"NO")
                 <<"\nAUTONOMOUS_CYCLES="<<result.autonomousCycles<<'\n';
        for(const auto& gap:result.capabilityGaps)std::cout<<"CAPABILITY_GAP="<<gap<<'\n';
        for(const auto& goal:result.derivedGoals)std::cout<<"DERIVED_GOAL="<<goal<<'\n';
        std::cout<<"CAPABILITY_DIAGNOSIS="<<result.capabilityDiagnosis
                 <<"\nBASELINE_VERIFIED="<<(result.baselineVerified?"YES":"NO")
                 <<"\nTOTAL_COMPILED_CANDIDATES="<<result.totalCompiledCandidates
                 <<"\nTOTAL_DECISION_ERRORS="<<result.totalDecisionErrors
                 <<"\nTOTAL_SEMANTIC_CHECKS="<<result.totalSemanticChecks
                 <<"\nTOTAL_COMPILE_MS="<<result.totalCompileMilliseconds
                 <<"\nTOTAL_EXECUTION_MS="<<result.totalExecutionMilliseconds<<'\n';
        std::cout<<"EXPERIENCE_SIGNATURE="<<result.experienceSignature
                 <<"\nEXPERIENCE_SOURCE="<<result.experienceSourceSignature
                 <<"\nEXPERIENCE_SIMILARITY="<<result.experienceSimilarity
                 <<"\nEXPERIENCE_TRANSFERRED="<<(result.experienceTransferred?"YES":"NO")<<'\n';
        std::cout<<"DEVELOPED_COMPONENT="<<result.developedComponent
                 <<"\nREUSED_COMPONENT="<<result.reusedComponent
                 <<"\nCOMPONENT_ARTIFACT="<<result.componentArtifact.string()<<'\n';
        std::cout<<"CYCLES="<<result.cycles<<"\nBEHAVIOR_VERIFIED="<<(result.behaviorVerified?"YES":"NO")
                 <<"\nSELECTED_STRATEGY="<<result.selectedStrategy<<"\nSOURCE="<<result.selectedSource.string()
                 <<"\nRESULT="<<(result.success?"PASS":"BLOCKED")<<'\n';
        return result.success?0:4;
    } catch (const std::exception& error) {
        std::cerr<<"GPI RESEARCH ERROR: "<<error.what()<<'\n'; return 1;
    }
}
