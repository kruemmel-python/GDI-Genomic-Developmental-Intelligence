#include "gpi/developmental_intelligence.hpp"
#include "gpi/foreign_project_intelligence.hpp"
#include "gpi/build_contract.hpp"
#include "gpi/development_strategy.hpp"
#include "gpi/autonomous_software_engineering.hpp"
#include "gpi/autonomous_behavioral_evolution.hpp"
#include "gpi/autonomous_semantic_integration.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
        std::filesystem::path krJava,krCompilerJar,strategyMemory,cmakeExecutable="cmake",ctestExecutable="ctest",msbuildExecutable,ninjaExecutable="ninja",makeExecutable="make";
        std::filesystem::path projectMemoryPath,devPrincipleMemoryPath;
        std::string objective,runtimeReadyMarker,generatorOverride,generatorPlatformOverride,windowsSdkOverride,developmentRequirement,behavioralChangeRequirement,semanticIntegrationRequirement;
        bool controlExperiment=false;
        bool allowWriteback=false;
        bool krEndToEnd=false;
        bool contractOnly=false,baselineOnly=false,induceStrategy=false,executeStrategy=false,r17Execute=false,r18Analyze=false,r18Plan=false,r18Execute=false,r19Analyze=false,r19Discover=false,r19Plan=false,r19Execute=false;
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
            if(key=="--kr-e2e") {
                krEndToEnd=true;
                continue;
            }
            if(key=="--build-contract") { contractOnly=true; continue; }
            if(key=="--baseline") { baselineOnly=true; continue; }
            if(key=="--induce-strategy") { induceStrategy=true; continue; }
            if(key=="--execute-strategy") { executeStrategy=true; continue; }
            if(key=="--r17-execute") { r17Execute=true; continue; }
            if(key=="--r18-analyze") { r18Analyze=true; continue; }
            if(key=="--r18-plan") { r18Plan=true; continue; }
            if(key=="--r18-execute") { r18Execute=true; continue; }
            if(key=="--r19-analyze") { r19Analyze=true; continue; }
            if(key=="--r19-discover-points") { r19Discover=true; continue; }
            if(key=="--r19-plan") { r19Plan=true; continue; }
            if(key=="--r19-execute") { r19Execute=true; continue; }
            if(i+1>=argc)throw std::invalid_argument("Every option requires a value");
            const std::string value=argv[++i];
            if(key=="--curriculum"||key=="--behavior")curriculumPath=value;
            else if(key=="--foreign-project")foreignProjectPath=value;
            else if(key=="--objective"||key=="--goal-prompt")objective=value;
            else if(key=="--development-requirement")developmentRequirement=value;
            else if(key=="--behavioral-change-requirement")behavioralChangeRequirement=value;
            else if(key=="--semantic-integration-requirement")semanticIntegrationRequirement=value;
            else if(key=="--workspace")workspace=value;
            else if(key=="--compiler")compiler=value;
            else if(key=="--sandbox")sandbox=value;
            else if(key=="--memory")componentMemory=value;
            else if(key=="--repair-memory")repairMemory=value;
            else if(key=="--project-memory")projectMemoryPath=value;
            else if(key=="--dev-memory")devPrincipleMemoryPath=value;
            else if(key=="--kr-java")krJava=value;
            else if(key=="--kr-compiler-jar")krCompilerJar=value;
            else if(key=="--runtime-ready-marker")runtimeReadyMarker=value;
            else if(key=="--strategy-memory")strategyMemory=value;
            else if(key=="--cmake")cmakeExecutable=value;
            else if(key=="--ctest")ctestExecutable=value;
            else if(key=="--msbuild")msbuildExecutable=value;
            else if(key=="--ninja")ninjaExecutable=value;
            else if(key=="--make")makeExecutable=value;
            else if(key=="--generator")generatorOverride=value;
            else if(key=="--generator-platform")generatorPlatformOverride=value;
            else if(key=="--windows-sdk")windowsSdkOverride=value;
            else if(key=="--cycles")maxCycles=parseCycles(value);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        if((curriculumPath.empty()&&objective.empty()&&foreignProjectPath.empty()&&developmentRequirement.empty()&&behavioralChangeRequirement.empty()&&semanticIntegrationRequirement.empty())||workspace.empty()||
           (developmentRequirement.empty()&&behavioralChangeRequirement.empty()&&semanticIntegrationRequirement.empty()&&!krEndToEnd&&!contractOnly&&!baselineOnly&&!induceStrategy&&!executeStrategy&&sandbox.empty()))
            throw std::invalid_argument("Either --foreign-project, --objective, --development-requirement, or --curriculum (or --behavior), plus --workspace are required");

        if(!semanticIntegrationRequirement.empty()) {
            if(foreignProjectPath.empty())throw std::invalid_argument("--semantic-integration-requirement requires --foreign-project");
            genomic::gpi::SemanticIntegrationConfig r19;r19.originalProject=std::filesystem::absolute(foreignProjectPath);r19.isolatedWorkspace=std::filesystem::absolute(workspace);r19.experiencePath=r19.isolatedWorkspace/"r19_experience.gdire";r19.execute=r19Execute;
            r19.nativeBuild.originalProject=r19.originalProject;r19.nativeBuild.isolatedWorkspace=r19.isolatedWorkspace/"baseline";r19.nativeBuild.configuration="Release";r19.nativeBuild.timeoutMilliseconds=600000;r19.nativeBuild.runTests=true;
            r19.nativeBuild.cmakeExecutable=cmakeExecutable;r19.nativeBuild.ctestExecutable=ctestExecutable;r19.nativeBuild.msbuildExecutable=msbuildExecutable;r19.nativeBuild.ninjaExecutable=ninjaExecutable;r19.nativeBuild.makeExecutable=makeExecutable;
            r19.nativeBuild.generatorOverride=generatorOverride;r19.nativeBuild.generatorPlatformOverride=generatorPlatformOverride;r19.nativeBuild.windowsSdkOverride=windowsSdkOverride;
            const auto result=genomic::gpi::runAutonomousSemanticIntegration(r19,semanticIntegrationRequirement);
            std::cout<<"R19_CONTRACT="<<result.requirement.id<<"\nR19_CAPABILITY="<<result.requirement.capability<<"\nR19_ENTRY_POINTS="<<std::count_if(result.model.functions.begin(),result.model.functions.end(),[](const auto& f){return f.entryPoint;})<<"\nR19_CALL_EDGES="<<result.model.callEdges.size()<<"\nR19_SELECTED_POINT="<<result.plan.selected.node.path.string()<<":"<<result.plan.selected.node.symbol<<"\nR19_RATIONALE="<<result.plan.selected.rationale<<"\nR19_TRANSFORMATIONS="<<result.transformations.size()<<"\nR19_RUNTIME_TESTS="<<result.runtime.size()<<"\nORIGINAL_UNTOUCHED="<<(result.originalUntouched?"YES":"NO")<<"\nREASON="<<result.reason<<"\nRESULT="<<genomic::gpi::toString(result.outcome)<<"\nREPORT="<<result.reportPath.string()<<'\n';
            const bool analysisOnly=r19Analyze||r19Discover||r19Plan;
            return result.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete||(analysisOnly&&result.outcome==genomic::gpi::SemanticIntegrationOutcome::Incomplete)?0:4;
        }

        if(!behavioralChangeRequirement.empty()) {
            if(foreignProjectPath.empty())throw std::invalid_argument("--behavioral-change-requirement requires --foreign-project");
            genomic::gpi::BehavioralEvolutionConfig r18;
            r18.originalProject=std::filesystem::absolute(foreignProjectPath);r18.isolatedWorkspace=std::filesystem::absolute(workspace);
            r18.strategyMemory=strategyMemory.empty()?(workspace/"r18_strategies.gdis"):strategyMemory;
            r18.experiencePath=workspace/"r18_experience.gdire";r18.execute=r18Execute;
            r18.nativeBuild.originalProject=r18.originalProject;r18.nativeBuild.isolatedWorkspace=r18.isolatedWorkspace/"baseline";
            r18.nativeBuild.configuration="Release";r18.nativeBuild.timeoutMilliseconds=600000;r18.nativeBuild.runTests=true;
            r18.nativeBuild.cmakeExecutable=cmakeExecutable;r18.nativeBuild.ctestExecutable=ctestExecutable;
            r18.nativeBuild.msbuildExecutable=msbuildExecutable;r18.nativeBuild.ninjaExecutable=ninjaExecutable;r18.nativeBuild.makeExecutable=makeExecutable;
            r18.nativeBuild.generatorOverride=generatorOverride;r18.nativeBuild.generatorPlatformOverride=generatorPlatformOverride;r18.nativeBuild.windowsSdkOverride=windowsSdkOverride;
            const auto result=genomic::gpi::runAutonomousBehavioralEvolution(r18,behavioralChangeRequirement);
            std::cout<<"R18_CONTRACT="<<result.contract.id
                     <<"\nR18_CAPABILITY="<<result.contract.capabilityClass
                     <<"\nR18_ENTRY_POINTS="<<result.program.entryPoints.size()
                     <<"\nR18_INTEGRATION_PLAN="<<result.plan.id
                     <<"\nR18_TRANSFORMATIONS="<<result.transformations.size()
                     <<"\nR18_BEHAVIOR_TESTS="<<result.evidence.tests.size()
                     <<"\nORIGINAL_UNTOUCHED="<<(result.evidence.originalUntouched?"YES":"NO")
                     <<"\nREASON="<<result.reason
                     <<"\nRESULT="<<genomic::gpi::toString(result.outcome)
                     <<"\nREPORT="<<result.evidence.reportPath.string()<<'\n';
            const bool analysisOnly=r18Analyze||r18Plan;
            return result.outcome==genomic::gpi::BehavioralEvolutionOutcome::Complete||(analysisOnly&&result.outcome==genomic::gpi::BehavioralEvolutionOutcome::Incomplete)?0:4;
        }

        if(!developmentRequirement.empty()) {
            if(foreignProjectPath.empty())throw std::invalid_argument("--development-requirement requires --foreign-project");
            genomic::gpi::SoftwareDevelopmentConfig r17;
            r17.originalProject=foreignProjectPath;r17.isolatedWorkspace=workspace;
            r17.strategyMemory=strategyMemory.empty()?(workspace/"r17_strategies.gdis"):strategyMemory;
            r17.experiencePath=workspace/"r17_experience.gdire";r17.execute=r17Execute;
            r17.nativeBuild.originalProject=foreignProjectPath;r17.nativeBuild.isolatedWorkspace=workspace/"baseline";
            r17.nativeBuild.configuration="Release";r17.nativeBuild.timeoutMilliseconds=600000;r17.nativeBuild.runTests=true;
            r17.nativeBuild.cmakeExecutable=cmakeExecutable;r17.nativeBuild.ctestExecutable=ctestExecutable;
            r17.nativeBuild.msbuildExecutable=msbuildExecutable;r17.nativeBuild.ninjaExecutable=ninjaExecutable;r17.nativeBuild.makeExecutable=makeExecutable;
            r17.nativeBuild.generatorOverride=generatorOverride;r17.nativeBuild.generatorPlatformOverride=generatorPlatformOverride;r17.nativeBuild.windowsSdkOverride=windowsSdkOverride;
            const auto result=genomic::gpi::runAutonomousSoftwareDevelopment(r17,developmentRequirement);
            std::cout<<"R17_REQUIREMENT="<<result.requirement.id
                     <<"\nR17_CAPABILITY="<<result.requirement.capabilityClass
                     <<"\nR17_ARCHITECTURE="<<result.architecture.id
                     <<"\nR17_CHANGES="<<result.changes.size()
                     <<"\nR17_ACCEPTANCE="<<result.acceptance.size()
                     <<"\nORIGINAL_UNTOUCHED="<<(result.originalUntouched?"YES":"NO")
                     <<"\nEXPERIENCE_PERSISTED="<<(result.experiencePersisted?"YES":"NO")
                     <<"\nRESULT="<<genomic::gpi::toString(result.outcome)
                     <<"\nREPORT="<<result.reportPath.string()<<'\n';
            return result.outcome==genomic::gpi::DevelopmentOutcome::Complete?0:4;
        }

        if(!foreignProjectPath.empty()) {
            if(krEndToEnd) {
                genomic::gpi::KrProjectEndToEndConfig krConfig;
                krConfig.protectedProjectRoot=foreignProjectPath;
                krConfig.isolatedWorkspace=workspace;
                krConfig.javaExecutable=krJava;
                krConfig.krCompilerJar=krCompilerJar;
                krConfig.cppCompiler=compiler;
                krConfig.runtimeReadyMarker=runtimeReadyMarker;
                const auto krResult=genomic::gpi::verifyKrProjectEndToEnd(krConfig);
                std::cout<<"KR_BUILD="<<(krResult.buildPassed?"PASS":"FAIL")
                         <<"\nRUST_LINKED="<<(krResult.rustLinked?"YES":"NO")
                         <<"\nGO_LINKED="<<(krResult.goLinked?"YES":"NO")
                         <<"\nSDL_LINKED="<<(krResult.sdlLinked?"YES":"NO")
                         <<"\nRUNTIME_STARTED="<<(krResult.runtimeStarted?"YES":"NO")
                         <<"\nRUNTIME_READY="<<(krResult.runtimeReady?"YES":"NO")
                         <<"\nABI_FUNCTIONAL_TESTS="<<(krResult.abiFunctionalTestsPassed?"PASS":"FAIL")
                         <<"\nORIGINAL_UNTOUCHED="<<(krResult.originalProjectUntouched?"YES":"NO")
                         <<"\nORIGINAL_SHA256_BEFORE="<<krResult.originalSha256Before
                         <<"\nORIGINAL_SHA256_AFTER="<<krResult.originalSha256After
                         <<"\nEXECUTABLE="<<krResult.executablePath.string()
                         <<"\nRESULT="<<(krResult.success()?"PASS":"BLOCKED")
                         <<"\nREPORT="<<krResult.reportPath.string()<<'\n';
                return krResult.success()?0:4;
            }
            const auto contract=genomic::gpi::discoverBuildContract(foreignProjectPath);
            std::filesystem::create_directories(workspace);
            const auto contractPath=workspace/"build_contract.gdibc";
            genomic::gpi::saveBuildContract(contract,contractPath);
            { std::ofstream json(workspace/"build_contract.json"); json<<genomic::gpi::renderBuildContractJson(contract); }
            if(contractOnly) {
                std::cout<<"BUILD_SYSTEM="<<genomic::gpi::toString(contract.buildSystem)
                         <<"\nCOMPILER_FAMILY="<<contract.compilerFamily.value
                         <<"\nGENERATOR="<<contract.generator.value
                         <<"\nTARGETS="<<contract.targets.size()
                         <<"\nCONTRACT="<<contractPath.string()<<"\nRESULT=PASS\n";
                return 0;
            }
            // Native build contracts are binding. Direct-compiler projects retain
            // the existing genomic source-graph path below.
            if(contract.buildSystem!=genomic::gpi::BuildSystemKind::DirectCompiler) {
                genomic::gpi::NativeBuildConfig nativeConfig;
                nativeConfig.originalProject=foreignProjectPath;nativeConfig.isolatedWorkspace=workspace/"native_baseline";
                nativeConfig.configuration="Release";nativeConfig.timeoutMilliseconds=600000;nativeConfig.runTests=true;
                nativeConfig.cmakeExecutable=cmakeExecutable;nativeConfig.ctestExecutable=ctestExecutable;nativeConfig.msbuildExecutable=msbuildExecutable;
                nativeConfig.ninjaExecutable=ninjaExecutable;nativeConfig.makeExecutable=makeExecutable;
                nativeConfig.generatorOverride=generatorOverride;nativeConfig.generatorPlatformOverride=generatorPlatformOverride;
                nativeConfig.windowsSdkOverride=windowsSdkOverride;
                const auto baseline=genomic::gpi::executeNativeBuild(contract,nativeConfig);
                genomic::gpi::StrategyPlanningRequest request;
                request.goal=objective.empty()?"Reproduce, diagnose and develop the project under its native build contract":objective;
                request.contract=contract;request.baseline=baseline.baselineStatus;request.diagnostics=baseline.diagnostics;
                if(!strategyMemory.empty())request.experience=genomic::gpi::DevelopmentStrategyStore(strategyMemory).load();
                const auto strategyPlan=genomic::gpi::AutonomousStrategyPlanner().plan(request);
                std::optional<genomic::gpi::StrategyExecutionResult> execution;
                if(executeStrategy&&strategyPlan.executable) {
                    genomic::gpi::NativeBuildConfig executionConfig=nativeConfig;
                    executionConfig.isolatedWorkspace=workspace/"strategy_execution";
                    execution=genomic::gpi::executeDevelopmentStrategy(strategyPlan.selected,contract,executionConfig);
                    if(!strategyMemory.empty()){auto learned=execution->strategy;learned.successfulContexts.push_back(genomic::gpi::toString(contract.buildSystem));genomic::gpi::DevelopmentStrategyStore(strategyMemory).append(learned);}
                }
                std::cout<<"BUILD_SYSTEM="<<genomic::gpi::toString(contract.buildSystem)
                         <<"\nBASELINE="<<genomic::gpi::toString(baseline.baselineStatus)
                         <<"\nORIGINAL_UNTOUCHED="<<(baseline.originalUntouched?"YES":"NO")
                         <<"\nDIAGNOSTICS="<<baseline.diagnostics.size()
                         <<"\nSTRATEGY_CANDIDATES="<<strategyPlan.candidates.size()
                         <<"\nSELECTED_STRATEGY="<<(strategyPlan.executable?strategyPlan.selected.id:"")
                         <<"\nSTRATEGY_EXECUTED="<<(execution.has_value()?"YES":"NO")
                         <<"\nRESULT="<<((baseline.baselineStatus==genomic::gpi::BuildBaselineStatus::BaselineReproduced)?"PASS":"BLOCKED")
                         <<"\nREPORT="<<baseline.reportPath.string()<<'\n';
                if(baselineOnly||induceStrategy||executeStrategy)return baseline.baselineStatus==genomic::gpi::BuildBaselineStatus::BaselineReproduced?0:4;
                return baseline.baselineStatus==genomic::gpi::BuildBaselineStatus::BaselineReproduced?0:4;
            }
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
