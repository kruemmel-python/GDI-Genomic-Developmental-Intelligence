#pragma once

#include "gpi/build_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {
enum class SemanticResolution { Resolved, Partial, Ambiguous, Unsupported, Conflicting };
enum class SemanticIntegrationOutcome { Complete, Incomplete, Blocked, Failed };
struct SemanticSourceNode { std::filesystem::path path; std::string symbol, kind, digest; std::size_t offset{}; SemanticResolution resolution{SemanticResolution::Partial}; };
struct SemanticFunction { std::string name, returnContract; std::filesystem::path path; std::size_t bodyBegin{}, bodyEnd{}; bool entryPoint{}, readsInput{}, writesOutput{}, readsState{}, writesState{}, hasLoop{}, hasEarlyReturn{}; SemanticResolution resolution{SemanticResolution::Partial}; std::vector<std::string> calls, parameters; };
struct SemanticProgramModel { std::string format{"GDIR19M1"}; std::vector<SemanticFunction> functions; std::vector<SemanticSourceNode> nodes; std::vector<std::string> includes, callEdges, dataFlows, stateFlows, uncertainties; std::vector<std::string> buildTargets; SemanticResolution resolution{SemanticResolution::Partial}; };
struct BehavioralRequirementContract { std::string id, original, capability; std::vector<std::string> preconditions, postconditions, acceptance; SemanticResolution resolution{SemanticResolution::Unsupported}; };
struct BehavioralDelta { std::string requiredPath, inputBoundary, outputBoundary, stateBoundary, rationale; SemanticResolution resolution{SemanticResolution::Unsupported}; std::vector<std::string> callChain; };
struct SemanticIntegrationCandidate { std::string id, kind, rationale, rejection; SemanticSourceNode node; std::vector<std::string> callChain, preconditions, risks; int score{}; bool hardConditionsMet{}, selected{}; };
struct SemanticIntegrationPlan { std::string id, componentName, targetName, operationSequence; BehavioralRequirementContract requirement; BehavioralDelta delta; SemanticIntegrationCandidate selected; std::vector<SemanticIntegrationCandidate> rejected; bool executable{}; };
struct SemanticTransformation { std::filesystem::path source; std::string beforeDigest, afterDigest, contextDigest, operation; bool atomic{}, verified{}; };
struct SemanticRuntimeEvidence { std::string name, expected; std::filesystem::path stdoutPath, stderrPath; int exitCode{-1}; bool passed{}; };
struct SemanticIntegrationConfig { std::filesystem::path originalProject, isolatedWorkspace, experiencePath; NativeBuildConfig nativeBuild; bool execute{}; };
struct SemanticIntegrationResult { SemanticIntegrationOutcome outcome{SemanticIntegrationOutcome::Blocked}; BehavioralRequirementContract requirement; SemanticProgramModel model; BehavioralDelta delta; SemanticIntegrationPlan plan; NativeBuildResult baseline, developedBuild; std::vector<SemanticTransformation> transformations; std::vector<SemanticRuntimeEvidence> runtime; std::filesystem::path developedProject, reportPath, experiencePath; std::uint64_t originalBefore{}, originalAfter{}; bool originalUntouched{}, experiencePersisted{}; std::string reason; };

BehavioralRequirementContract interpretSemanticIntegrationRequirement(const std::string& text);
SemanticProgramModel buildSemanticProgramModel(const std::filesystem::path& root,const BuildContract& contract);
BehavioralDelta analyzeBehavioralDelta(const BehavioralRequirementContract& requirement,const SemanticProgramModel& model);
SemanticIntegrationPlan discoverSemanticIntegrationPoint(const BehavioralRequirementContract& requirement,const SemanticProgramModel& model,const BehavioralDelta& delta,const BuildContract& contract);
SemanticIntegrationResult runAutonomousSemanticIntegration(const SemanticIntegrationConfig& config,const std::string& requirement);
const char* toString(SemanticIntegrationOutcome value) noexcept;
const char* toString(SemanticResolution value) noexcept;
}
