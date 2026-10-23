#pragma once

#include "gpi/build_contract.hpp"
#include "gpi/development_strategy.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class RequirementStatus { Clarified, Assumed, Ambiguous, Contradictory, Blocked };
enum class DevelopmentOutcome { Complete, Incomplete, Blocked, Failed };
enum class AcceptanceStatus { Verified, Unverified, Blocked, Failed };

struct RequirementClause {
    std::string id, text, category, evidence;
    RequirementStatus status{RequirementStatus::Assumed};
};
struct AcceptanceCriterion {
    std::string id, description, evidenceType, command, artifact;
    AcceptanceStatus status{AcceptanceStatus::Unverified};
};
struct RequirementContract {
    std::string format{"GDIR17C1"}, id, originalRequirement, normalizedIntent, capabilityClass;
    std::vector<RequirementClause> clauses;
    std::vector<std::string> inputs, outputs, errors, invariants, assumptions, tracedArtifacts;
    std::vector<AcceptanceCriterion> acceptance;
    RequirementStatus status{RequirementStatus::Assumed};
    double confidence{0.0};
};
struct ChangeImpactNode {
    std::string path, kind, role, evidence;
    bool generated{false}, external{false};
};
struct ChangeImpactEdge { std::string from, to, relation, evidence; bool inferred{false}; };
struct ChangeImpactGraph {
    std::string format{"GDIR17I1"};
    std::vector<ChangeImpactNode> nodes;
    std::vector<ChangeImpactEdge> edges;
    std::vector<std::string> publicInvariants, unknownRelations;
};
struct ArchitectureAlternative {
    std::string id, summary;
    double contractFit{0.0}, intrusion{0.0}, verifiability{0.0}, securityRisk{0.0}, reuse{0.0};
};
struct ArchitectureChangePlan {
    std::string format{"GDIR17A1"}, id, selectedAlternative, componentName, headerPath, sourcePath, testPath;
    std::string targetName, cmakeMutation, rationale;
    std::vector<std::string> requiredOperations, changedArtifacts, invariants;
    std::vector<ArchitectureAlternative> alternatives;
    bool executable{false};
};
struct GeneratedChange {
    std::filesystem::path path;
    std::string operatorId, purpose, contentDigest;
};
struct DevelopmentAcceptanceEvidence {
    std::string criterionId, command, artifact, details;
    int exitCode{-1};
    AcceptanceStatus status{AcceptanceStatus::Unverified};
};
struct SoftwareDevelopmentConfig {
    std::filesystem::path originalProject, isolatedWorkspace, strategyMemory, experiencePath;
    NativeBuildConfig nativeBuild;
    bool execute{false};
};
struct SoftwareDevelopmentResult {
    DevelopmentOutcome outcome{DevelopmentOutcome::Blocked};
    RequirementContract requirement;
    ChangeImpactGraph impact;
    ArchitectureChangePlan architecture;
    StrategyPlan strategy;
    NativeBuildResult baseline, developedBuild;
    std::vector<GeneratedChange> changes;
    std::vector<DevelopmentAcceptanceEvidence> acceptance;
    bool originalUntouched{false}, experiencePersisted{false};
    std::filesystem::path reportPath, developedProject;
    std::string reason;
};

RequirementContract interpretDevelopmentRequirement(const std::string& requirement);
ChangeImpactGraph analyzeChangeImpact(const std::filesystem::path& projectRoot,const BuildContract& contract,
                                      const RequirementContract& requirement);
ArchitectureChangePlan synthesizeArchitectureChange(const RequirementContract& requirement,
                                                    const ChangeImpactGraph& impact,const BuildContract& contract);
void saveRequirementContract(const RequirementContract& contract,const std::filesystem::path& path);
RequirementContract loadRequirementContract(const std::filesystem::path& path);
SoftwareDevelopmentResult runAutonomousSoftwareDevelopment(const SoftwareDevelopmentConfig& config,
                                                           const std::string& requirement);
const char* toString(RequirementStatus value) noexcept;
const char* toString(DevelopmentOutcome value) noexcept;
const char* toString(AcceptanceStatus value) noexcept;

} // namespace genomic::gpi
