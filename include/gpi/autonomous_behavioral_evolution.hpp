#pragma once

#include "gpi/build_contract.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {
enum class BehavioralEvolutionOutcome { Complete, Incomplete, Blocked, Failed };
enum class BehavioralFactStatus { Observed, Inferred, Required, Verified, Unsupported };
struct BehavioralChangeContract { std::string id, originalRequirement, capabilityClass; BehavioralFactStatus status{BehavioralFactStatus::Unsupported}; double confidence{}; std::vector<std::string> invariants, acceptance; };
struct BehavioralProgramModel { std::vector<std::string> entryPoints, callRelations, dataFlows, stateRelations, integrationAnchors; std::vector<std::filesystem::path> sourceFiles; };
struct BehavioralImpactGraph { std::vector<std::string> nodes, edges, unknowns; };
struct BehavioralIntegrationPlan { std::string id, componentName, sourceFile, anchor, rationale; std::vector<std::string> operations; bool executable{}; };
struct VerifiedSourceTransformation { std::filesystem::path path; std::string operatorId, anchor, beforeDigest, afterDigest; bool preconditionsMet{}, applied{}, verified{}; };
struct BehavioralTestEvidence { std::string name, command, details; int exitCode{-1}; bool passed{}; std::filesystem::path stdoutPath, stderrPath; };
struct BehavioralEvolutionEvidence { std::uint64_t originalManifestBefore{}, originalManifestAfter{}; std::vector<BehavioralTestEvidence> tests; std::filesystem::path reportPath, experiencePath; bool originalUntouched{}, persistent{}; };
struct BehavioralEvolutionConfig { std::filesystem::path originalProject, isolatedWorkspace, strategyMemory, experiencePath; NativeBuildConfig nativeBuild; bool execute{}; };
struct BehavioralEvolutionResult { BehavioralEvolutionOutcome outcome{BehavioralEvolutionOutcome::Blocked}; BehavioralChangeContract contract; BehavioralProgramModel program; BehavioralImpactGraph impact; BehavioralIntegrationPlan plan; NativeBuildResult baseline, developedBuild; std::vector<VerifiedSourceTransformation> transformations; BehavioralEvolutionEvidence evidence; std::filesystem::path developedProject; std::string reason; };

BehavioralChangeContract interpretBehavioralChangeRequirement(const std::string& requirement);
BehavioralProgramModel analyzeBehavioralProgram(const std::filesystem::path& root);
BehavioralImpactGraph deriveBehavioralImpact(const BehavioralProgramModel& model,const BehavioralChangeContract& contract);
BehavioralIntegrationPlan synthesizeBehavioralIntegration(const BehavioralProgramModel& model,const BehavioralChangeContract& contract);
BehavioralEvolutionResult runAutonomousBehavioralEvolution(const BehavioralEvolutionConfig& config,const std::string& requirement);
const char* toString(BehavioralEvolutionOutcome value) noexcept;
}
