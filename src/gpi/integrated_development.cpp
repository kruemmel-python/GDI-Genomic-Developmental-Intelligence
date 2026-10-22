#include "gpi/integrated_development.hpp"

#include "genome/genome_strand.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace genomic::gpi {
namespace {

std::string roleName(CompartmentRole role) {
    if(role==CompartmentRole::Unified)return "unified";
    if(role==CompartmentRole::Analysis)return "analysis";
    if(role==CompartmentRole::Synthesis)return "synthesis";
    return "research";
}

bool accepts(const CompartmentDefinition& definition,DevelopmentSignalKind kind) {
    return std::find(definition.acceptedSignals.begin(),definition.acceptedSignals.end(),kind)!=
           definition.acceptedSignals.end();
}
bool canEmit(CompartmentRole role,DevelopmentSignalKind kind) {
    if(role==CompartmentRole::Unified)return true;
    if(role==CompartmentRole::Analysis)
        return kind==DevelopmentSignalKind::GoalAccepted || kind==DevelopmentSignalKind::GoalUncertain ||
               kind==DevelopmentSignalKind::CapabilityProbeRequested || kind==DevelopmentSignalKind::ResearchAuthorized;
    if(role==CompartmentRole::Research)
        return kind==DevelopmentSignalKind::EvidenceVerified || kind==DevelopmentSignalKind::NegativeEvidence ||
               kind==DevelopmentSignalKind::ComponentReady;
    return kind==DevelopmentSignalKind::SynthesisCompleted;
}

void writeText(const std::filesystem::path& path,const std::string& text) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if(!file || !(file<<text))throw std::runtime_error("Cannot write integrated development artifact: "+path.string());
}

IntegratedMetrics metricsFrom(const ResearchResult& result) {
    IntegratedMetrics metrics;
    metrics.goalFulfilled=result.success&&result.behaviorVerified;
    metrics.contractVerified=result.behaviorVerified;
    metrics.semanticChecks=result.totalSemanticChecks;
    metrics.compileMilliseconds=result.totalCompileMilliseconds;
    metrics.executionMilliseconds=result.totalExecutionMilliseconds;
    metrics.successfulTransfers=result.reusedComponent.empty()?0:1;
    metrics.compiledCandidates=result.totalCompiledCandidates;
    metrics.decisionErrors=result.totalDecisionErrors;
    return metrics;
}

ResearchConfig childConfig(const ResearchConfig& base,const std::filesystem::path& workspace,
                           ResearchDevelopmentState state) {
    auto child=base;child.workspace=workspace;child.developmentState=state;child.unsafeMode=false;return child;
}

} // namespace

MultiCompartmentSystem::MultiCompartmentSystem(uint64_t genomeHash,bool communicationEnabled)
    : genomeHash_(genomeHash),communicationEnabled_(communicationEnabled) {
    if(genomeHash==0)throw std::invalid_argument("Multi-compartment regulation requires a non-zero genome hash");
}

void MultiCompartmentSystem::addCompartment(const CompartmentDefinition& definition) {
    if(definition.id==0 || std::any_of(compartments_.begin(),compartments_.end(),[&](const auto& state){
        return state.definition.id==definition.id;}))throw std::invalid_argument("Compartment IDs must be unique and non-zero");
    compartments_.push_back({definition,0,0});
}

uint64_t MultiCompartmentSystem::signature(DevelopmentSignalKind kind) noexcept {
    // Equal low 48 bits deliberately exercise full-width molecule signatures.
    return 0xA100000000000111ULL+(static_cast<uint64_t>(kind)<<48);
}

bool MultiCompartmentSystem::exchange(TypedDevelopmentSignal signal) {
    auto source=std::find_if(compartments_.begin(),compartments_.end(),[&](const auto& state){return state.definition.id==signal.source;});
    auto target=std::find_if(compartments_.begin(),compartments_.end(),[&](const auto& state){return state.definition.id==signal.target;});
    if(source==compartments_.end() || target==compartments_.end())throw std::invalid_argument("Signal references an unknown compartment");
    if(!canEmit(source->definition.role,signal.kind))
        throw std::invalid_argument("Compartment role is not allowed to emit this signal type");
    signal.signature=signature(signal.kind);signal.concentration=std::clamp(signal.concentration,0.0,1.0);
    ++source->emitted;
    signal.delivered=communicationEnabled_&&accepts(target->definition,signal.kind)&&signal.contractHash!=0;
    if(signal.delivered) {
        molecules_.setConcentration(target->definition.id,signal.signature,signal.concentration,
                                    signal.evidence,MoleculeClass::Development);
        ++target->received;
    }
    signals_.push_back(std::move(signal));return signals_.back().delivered;
}

double MultiCompartmentSystem::concentration(uint32_t compartment,DevelopmentSignalKind kind) const noexcept {
    return molecules_.getConcentration(compartment,signature(kind));
}

std::vector<CompartmentSnapshot> MultiCompartmentSystem::snapshots() const {
    std::vector<CompartmentSnapshot> result;
    for(const auto& state:compartments_)
        result.push_back({state.definition.id,state.definition.role,state.definition.experienceAccess,
                          molecules_.getMolecules(state.definition.id),state.definition.accessibleRegions,
                          state.received,state.emitted});
    return result;
}

std::string integratedArchitectureName(IntegratedArchitecture architecture) {
    if(architecture==IntegratedArchitecture::Single)return "SINGLE";
    if(architecture==IntegratedArchitecture::Isolated)return "ISOLATED";
    if(architecture==IntegratedArchitecture::Communicating)return "COMMUNICATING";
    return "COMMUNICATING_EXPERIENCED";
}

IntegratedResult runIntegratedDevelopment(const ResearchPlan& plan,const ResearchConfig& config,
                                          IntegratedArchitecture architecture,std::size_t autonomousCycles) {
    if(!config.genome || !config.genome->isFrozen() || !config.genome->verifyIntegrity().passed)
        throw std::invalid_argument("Integrated development requires one intact immutable genome");
    if(!plan.contract.accepted)throw std::invalid_argument("Integrated development requires an accepted goal contract");
    if(config.workspace.empty() || (std::filesystem::exists(config.workspace)&&!std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Integrated workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    IntegratedResult result;result.architecture=architecture;result.contract=plan.contract;
    const bool communicates=architecture==IntegratedArchitecture::Single ||
        architecture==IntegratedArchitecture::Communicating || architecture==IntegratedArchitecture::Experienced;
    MultiCompartmentSystem system(config.genome->getGenomeHash(),communicates);
    std::vector<GenomeRegionId> allRegions,analysisRegions,synthesisRegions,researchRegions;
    for(const auto& region:config.genome->getAllRegions()) {
        allRegions.push_back(region.id);
        if(region.kind==RegionKind::TaskGrammar)analysisRegions.push_back(region.id);
        if(region.kind==RegionKind::ResearchCapability) {
            const std::string payload(region.payload.begin(),region.payload.end());
            if(payload.rfind("gpi-operation-v1|",0)==0)synthesisRegions.push_back(region.id);
            else if(payload.rfind("gpi-research-v1|",0)==0)researchRegions.push_back(region.id);
        }
    }
    if(architecture==IntegratedArchitecture::Single) {
        system.addCompartment({1,CompartmentRole::Unified,architecture==IntegratedArchitecture::Experienced,
            {DevelopmentSignalKind::GoalAccepted,DevelopmentSignalKind::CapabilityProbeRequested,
             DevelopmentSignalKind::EvidenceVerified,DevelopmentSignalKind::ComponentReady},allRegions});
    } else {
        system.addCompartment({1,CompartmentRole::Analysis,false,
            {DevelopmentSignalKind::SynthesisCompleted,DevelopmentSignalKind::NegativeEvidence},analysisRegions});
        system.addCompartment({2,CompartmentRole::Synthesis,architecture==IntegratedArchitecture::Experienced,
            {DevelopmentSignalKind::GoalAccepted,DevelopmentSignalKind::EvidenceVerified,DevelopmentSignalKind::ComponentReady},synthesisRegions});
        system.addCompartment({3,CompartmentRole::Research,architecture==IntegratedArchitecture::Experienced,
            {DevelopmentSignalKind::CapabilityProbeRequested,DevelopmentSignalKind::GoalUncertain,
             DevelopmentSignalKind::ResearchAuthorized},researchRegions});
    }
    const uint32_t analysis=1;
    const uint32_t synthesis=architecture==IntegratedArchitecture::Single?1:2;
    const uint32_t research=architecture==IntegratedArchitecture::Single?1:3;
    const bool contractSignal=system.exchange({DevelopmentSignalKind::GoalAccepted,analysis,synthesis,0,
        plan.contract.semanticHash,0.95,"accepted goal contract",false});
    const bool probeSignal=system.exchange({DevelopmentSignalKind::CapabilityProbeRequested,analysis,research,0,
        plan.contract.semanticHash,0.85,"request measured capability evidence",false});
    ResearchResult diagnosisEvidence;bool hasDiagnosisEvidence=false;
    if(architecture==IntegratedArchitecture::Isolated) {
        result.capabilityDiagnosis="SOLVABLE_WITHOUT_CROSS_COMPARTMENT_EVIDENCE";
        auto isolated=childConfig(config,config.workspace/"target",ResearchDevelopmentState::Control);
        result.development=runResearchDevelopment(plan,isolated);
        result.events.push_back("Specialized compartments remained isolated; synthesis used only its initial typed contract");
    } else if(architecture==IntegratedArchitecture::Single) {
        result.capabilityDiagnosis="SINGLE_CONTEXT_BASELINE";
        auto single=childConfig(config,config.workspace/"target",ResearchDevelopmentState::Control);
        result.development=runResearchDevelopment(plan,single);
    } else {
        if(!contractSignal || !probeSignal)throw std::runtime_error("Required typed inter-compartment signal was not delivered");
        auto probe=childConfig(config,config.workspace/"diagnosis",ResearchDevelopmentState::Control);
        probe.maxCycles=1;
        const auto evidence=runResearchDevelopment(plan,probe);
        diagnosisEvidence=evidence;hasDiagnosisEvidence=true;
        const auto measuredScore=evidence.history.empty()?static_cast<std::size_t>(-1):evidence.history.front().score;
        if(evidence.behaviorVerified) {
            result.capabilityDiagnosis=(plan.operations.size()>=3 && measuredScore>100)?
                "SOLVABLE_OPTIMIZATION_EVIDENCE":"SOLVABLE_NO_RESEARCH_REQUIRED";
            system.exchange({DevelopmentSignalKind::EvidenceVerified,research,synthesis,0,plan.contract.semanticHash,0.90,
                "baseline behavior verified; compile_ms="+std::to_string(evidence.totalCompileMilliseconds)+
                "; semantic_checks="+std::to_string(evidence.totalSemanticChecks),false});
        } else {
            result.capabilityDiagnosis="MISSING_EXECUTABLE_CAPABILITY";
            const bool negativeDelivered=system.exchange({DevelopmentSignalKind::NegativeEvidence,research,analysis,0,plan.contract.semanticHash,0.90,
                "baseline failed independent semantic verification",false});
            if(negativeDelivered)
                system.exchange({DevelopmentSignalKind::ResearchAuthorized,analysis,research,0,plan.contract.semanticHash,0.95,
                    "negative evidence changed regulatory state and authorized a bounded experiment",false});
        }
        if(architecture==IntegratedArchitecture::Experienced &&
           result.capabilityDiagnosis!="SOLVABLE_NO_RESEARCH_REQUIRED") {
            auto developed=childConfig(config,config.workspace/"campaign",ResearchDevelopmentState::Developed);
            result.development=runAutonomousDevelopment(plan,developed,autonomousCycles);
        } else {
            auto communicating=childConfig(config,config.workspace/"target",ResearchDevelopmentState::Explore);
            result.development=runResearchDevelopment(plan,communicating);
        }
        if(!result.development.reusedComponent.empty())
            system.exchange({DevelopmentSignalKind::ComponentReady,research,synthesis,0,plan.contract.semanticHash,0.95,
                "verified shared component="+result.development.reusedComponent,false});
        if(result.development.behaviorVerified)
            system.exchange({DevelopmentSignalKind::SynthesisCompleted,synthesis,analysis,0,plan.contract.semanticHash,0.95,
                "target passed independent goal-contract verification",false});
    }
    result.metrics=metricsFrom(result.development);
    if(hasDiagnosisEvidence) {
        result.metrics.semanticChecks+=diagnosisEvidence.totalSemanticChecks;
        result.metrics.compileMilliseconds+=diagnosisEvidence.totalCompileMilliseconds;
        result.metrics.executionMilliseconds+=diagnosisEvidence.totalExecutionMilliseconds;
        result.metrics.compiledCandidates+=diagnosisEvidence.totalCompiledCandidates;
        result.metrics.decisionErrors+=diagnosisEvidence.totalDecisionErrors;
    }
    result.metrics.contractVerified=result.development.behaviorVerified&&plan.contract.accepted;
    result.compartments=system.snapshots();result.signals=system.signals();
    std::ostringstream report;
    report<<"ARCHITECTURE="<<integratedArchitectureName(architecture)<<"\nGENOME_HASH="<<system.genomeHash()
          <<"\nCONTRACT_HASH="<<plan.contract.semanticHash<<"\nDIAGNOSIS="<<result.capabilityDiagnosis
          <<"\nGOAL_FULFILLED="<<(result.metrics.goalFulfilled?"YES":"NO")
          <<"\nCONTRACT_VERIFIED="<<(result.metrics.contractVerified?"YES":"NO")
          <<"\nCOMPILED_CANDIDATES="<<result.metrics.compiledCandidates
          <<"\nSEMANTIC_CHECKS="<<result.metrics.semanticChecks
          <<"\nCOMPILE_MS="<<result.metrics.compileMilliseconds
          <<"\nEXECUTION_MS="<<result.metrics.executionMilliseconds
          <<"\nDECISION_ERRORS="<<result.metrics.decisionErrors
          <<"\nSUCCESSFUL_TRANSFERS="<<result.metrics.successfulTransfers<<'\n';
    for(const auto& compartment:result.compartments)
        report<<"COMPARTMENT="<<compartment.id<<" ROLE="<<roleName(compartment.role)
              <<" EXPERIENCE="<<(compartment.experienceAccess?"YES":"NO")
              <<" ACCESSIBLE_REGIONS="<<compartment.accessibleRegions.size()
              <<" RECEIVED="<<compartment.receivedSignals<<" EMITTED="<<compartment.emittedSignals<<'\n';
    writeText(config.workspace/"integrated_report.txt",report.str());
    return result;
}

std::vector<IntegratedResult> runArchitectureComparison(const ResearchPlan& plan,const ResearchConfig& config,
                                                        std::size_t autonomousCycles) {
    std::vector<IntegratedResult> results;
    for(const auto architecture:{IntegratedArchitecture::Single,IntegratedArchitecture::Isolated,
                                IntegratedArchitecture::Communicating,IntegratedArchitecture::Experienced}) {
        auto variant=config;variant.workspace=config.workspace/integratedArchitectureName(architecture);
        variant.memory=config.workspace/(integratedArchitectureName(architecture)+".memory");
        results.push_back(runIntegratedDevelopment(plan,variant,architecture,autonomousCycles));
    }
    std::ostringstream comparison;
    comparison<<"architecture,genome_hash,goal_fulfilled,contract_verified,compiled_candidates,semantic_checks,"
                "compile_ms,execution_ms,decision_errors,successful_transfers,diagnosis\n";
    for(const auto& result:results)
        comparison<<integratedArchitectureName(result.architecture)<<','<<result.development.genomeHash<<','
                  <<(result.metrics.goalFulfilled?1:0)<<','<<(result.metrics.contractVerified?1:0)<<','
                  <<result.metrics.compiledCandidates<<','<<result.metrics.semanticChecks<<','
                  <<result.metrics.compileMilliseconds<<','<<result.metrics.executionMilliseconds<<','
                  <<result.metrics.decisionErrors<<','<<result.metrics.successfulTransfers<<','
                  <<result.capabilityDiagnosis<<'\n';
    writeText(config.workspace/"architecture_comparison.csv",comparison.str());
    return results;
}

} // namespace genomic::gpi
