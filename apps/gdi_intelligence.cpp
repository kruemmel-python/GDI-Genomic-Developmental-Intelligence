#include "gpi/developmental_intelligence.hpp"
#include "gpi/foreign_project_intelligence.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t parseCycles(const std::string& value) {
    std::size_t count{};
    const auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),count);
    if(ec!=std::errc{}||end!=value.data()+value.size()||count<2||count>16)
        throw std::invalid_argument("--cycles must be 2..16");
    return count;
}
} // namespace

int main(int argc,char** argv) {
    try {
        std::filesystem::path curriculumPath,foreignProjectPath,workspace,compiler="g++",sandbox,componentMemory,repairMemory;
        std::filesystem::path projectMemoryPath,devPrincipleMemoryPath;
        std::string objective;
        bool controlExperiment=false;
        bool allowWriteback=false;
        std::size_t maxCycles=8;
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(key=="--control-experiment") {
                controlExperiment=true;
                continue;
            }
            if(key=="--allow-writeback") {
                allowWriteback=true;
                continue;
            }
            if(i+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++i];
            if(key=="--curriculum"||key=="--behavior")curriculumPath=value;
            else if(key=="--foreign-project")foreignProjectPath=value;
            else if(key=="--objective"||key=="--goal-prompt")objective=value;
            else if(key=="--workspace")workspace=value;
            else if(key=="--compiler")compiler=value;
            else if(key=="--sandbox")sandbox=value;
            else if(key=="--memory")componentMemory=value;
            else if(key=="--repair-memory")repairMemory=value;
            else if(key=="--project-memory")projectMemoryPath=value;
            else if(key=="--dev-memory")devPrincipleMemoryPath=value;
            else if(key=="--cycles")maxCycles=parseCycles(value);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        if((curriculumPath.empty()&&objective.empty()&&foreignProjectPath.empty())||workspace.empty()||sandbox.empty())
            throw std::invalid_argument("Either --foreign-project, --objective, or --curriculum (or --behavior), plus --workspace and --sandbox are required");

        if(!foreignProjectPath.empty()) {
            genomic::gpi::ForeignProjectDevelopmentConfig fpConfig;
            fpConfig.foreignProjectRoot=foreignProjectPath;
            fpConfig.isolatedWorkspace=workspace;
            fpConfig.compiler=compiler;
            fpConfig.sandboxRunner=sandbox;
            fpConfig.projectMemoryPath=projectMemoryPath.empty()?(workspace/"foreign_project_memory.gdipm"):projectMemoryPath;
            fpConfig.developmentalMemoryPath=devPrincipleMemoryPath.empty()?(workspace/"developmental_principles.gdidp"):devPrincipleMemoryPath;
            fpConfig.componentMemoryPath=componentMemory;
            fpConfig.repairExperiencePath=repairMemory;
            fpConfig.allowWritebackToOriginal=allowWriteback;
            fpConfig.requireSandbox=true;
            fpConfig.maxIterations=maxCycles;

            const auto fpResult=genomic::gpi::runForeignProjectDevelopmentalLoop(fpConfig);
            std::cout<<"FOREIGN_PROJECT="<<fpResult.finalAnalysis.projectName
                     <<"\nORIGINAL_UNTOUCHED="<<(fpResult.originalProjectUntouched?"YES":"NO")
                     <<"\nORIGINAL_HASH_BEFORE="<<fpResult.originalDirectoryHashBefore
                     <<"\nORIGINAL_HASH_AFTER="<<fpResult.originalDirectoryHashAfter
                     <<"\nORIGINAL_SHA256_BEFORE="<<fpResult.originalSha256Before
                     <<"\nORIGINAL_SHA256_AFTER="<<fpResult.originalSha256After
                     <<"\nDISCOVERED_MODULES="<<fpResult.initialAnalysis.modules.size()
                     <<"\nDISCOVERED_SYMBOLS="<<fpResult.initialAnalysis.symbols.size()
                     <<"\nDISCOVERED_ABI_CONTRACTS="<<fpResult.initialAnalysis.abiContracts.size()
                     <<"\nINITIAL_HYPOTHESES="<<fpResult.initialAnalysis.hypotheses.size()
                     <<"\nITERATIONS="<<fpResult.iterations.size()
                     <<"\nLEARNED_PRINCIPLES="<<fpResult.learnedPrinciples.size()
                     <<"\nTRANSFERRED_PRINCIPLE="<<(fpResult.transferredPrincipleApplied?"YES":"NO")
                     <<"\nPROJECT_MEMORY="<<fpConfig.projectMemoryPath.string()
                     <<"\nDEV_PRINCIPLE_MEMORY="<<fpConfig.developmentalMemoryPath.string()
                     <<"\nRESULT="<<(fpResult.developmentVerified?"PASS":"BLOCKED")
                     <<"\nREPORT="<<fpResult.reportPath.string()<<'\n';
            return fpResult.developmentVerified?0:4;
        }

        genomic::gpi::DevelopmentalIntelligenceConfig config;
        config.workspace=workspace;
        config.compiler=compiler;
        config.sandboxRunner=sandbox;
        config.componentMemory=componentMemory;
        config.projectRepairMemory=repairMemory;
        config.maxCyclesPerStage=maxCycles;
        config.requireSandbox=true;

        if(!objective.empty()) {
            if(controlExperiment) {
                const auto cmp=genomic::gpi::runOntogeneticControlExperiment(objective,config,componentMemory);
                std::cout<<"GENOME_HASH="<<cmp.naiveResult.immutableGenomeHash
                         <<"\nNAIVE_STAGES="<<cmp.naiveResult.totalStagesCompleted<<"/"<<cmp.naivePlan.derivedCurriculum.stages.size()
                         <<"\nNAIVE_GAPS="<<cmp.naivePlan.audit.knowledgeGaps.size()
                         <<"\nEXPERIENCED_STAGES="<<cmp.experiencedResult.totalStagesCompleted<<"/"<<cmp.experiencedPlan.derivedCurriculum.stages.size()
                         <<"\nEXPERIENCED_GAPS="<<cmp.experiencedPlan.audit.knowledgeGaps.size()
                         <<"\nSTAGES_SAVED_BY_EXPERIENCE="<<cmp.stagesSavedByExperience
                         <<"\nMEMORY_ALTERED_PATH="<<(cmp.memoryAlteredDevelopmentalPath?"YES":"NO")
                         <<"\nRESULT="<<((cmp.naiveResult.success&&cmp.experiencedResult.success&&cmp.memoryAlteredDevelopmentalPath)?"PASS":"BLOCKED")
                         <<"\nREPORT="<<cmp.comparisonReportPath<<'\n';
                return (cmp.naiveResult.success&&cmp.experiencedResult.success&&cmp.memoryAlteredDevelopmentalPath)?0:4;
            }
            const auto plan=genomic::gpi::planAutonomousOntogenyFromObjective(objective,componentMemory);
            std::cout<<plan.plannerRationale<<'\n';
            const auto result=genomic::gpi::runAutonomousGoalDirectedOntogeny(objective,config);
            std::cout<<"GENOME_HASH="<<result.immutableGenomeHash
                     <<"\nDERIVED_STAGES="<<plan.derivedCurriculum.stages.size()
                     <<"\nKNOWLEDGE_GAPS="<<plan.audit.knowledgeGaps.size()
                     <<"\nSATISFIED_FROM_MEMORY="<<plan.audit.satisfiedFromMemory.size()
                     <<"\nSTAGES_COMPLETED="<<result.totalStagesCompleted<<"/"<<plan.derivedCurriculum.stages.size()
                     <<"\nREUSED_COMPONENTS="<<result.totalReusedComponents
                     <<"\nINDUCED_COMPONENTS="<<result.totalNewlyInducedComponents
                     <<"\nSIGNALS_EXCHANGED="<<result.signals.size()
                     <<"\nSNAPSHOT_HASH="<<result.finalSnapshot.stateHash
                     <<"\nRESULT="<<(result.success?"PASS":"BLOCKED")
                     <<"\nREPORT="<<result.reportPath<<'\n';
            return result.success?0:4;
        }

        const auto curriculum=genomic::gpi::loadDevelopmentalCurriculum(curriculumPath);
        const auto result=genomic::gpi::runDevelopmentalProgrammingIntelligence(curriculum,config);
        std::cout<<"GENOME_HASH="<<result.immutableGenomeHash
                 <<"\nSTAGES_COMPLETED="<<result.totalStagesCompleted<<"/"<<curriculum.stages.size()
                 <<"\nREUSED_COMPONENTS="<<result.totalReusedComponents
                 <<"\nINDUCED_COMPONENTS="<<result.totalNewlyInducedComponents
                 <<"\nSIGNALS_EXCHANGED="<<result.signals.size()
                 <<"\nSNAPSHOT_HASH="<<result.finalSnapshot.stateHash
                 <<"\nRESULT="<<(result.success?"PASS":"BLOCKED")
                 <<"\nREPORT="<<result.reportPath<<'\n';
        return result.success?0:4;
    } catch(const std::exception& error) {
        std::cerr<<"GDI INTELLIGENCE ERROR: "<<error.what()<<'\n';
        return 1;
    }
}
