#pragma once

#include "gpi/project_verifier.hpp"
#include "gpi/project_experience.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

struct ProjectHypothesis {
    std::string id;
    std::string rationale;
    GenerativeProjectGraph graph;
};

struct ProjectDevelopmentCycle {
    std::size_t number{0};
    std::string hypothesis;
    std::string rationale;
    ProjectVerificationEvidence evidence;
};

struct AutonomousProjectConfig {
    std::filesystem::path workspace;
    std::filesystem::path compiler;
    std::filesystem::path sandboxRunner;
    std::size_t maxCycles{4};
    // Optional append-only, integrity-chained store. Only independently
    // verified transformations discovered by this campaign are persisted.
    std::filesystem::path experienceStore;
};

struct AutonomousProjectResult {
    bool success{false};
    std::string selectedHypothesis;
    uint64_t immutableGenomeHash{0};
    std::vector<ProjectDevelopmentCycle> cycles;
    std::vector<VerifiedProjectTransformation> transformations;
    bool experienceTransferred{false};
    uint64_t experienceOriginGraphHash{0};
    std::filesystem::path report;
};

AutonomousProjectResult runEvidenceGuidedProjectDevelopment(
    const std::vector<ProjectHypothesis>& hypotheses,
    const ProjectVerificationContract& invariantGoalContract,
    const AutonomousProjectConfig& config);

// Starts from one seed graph. Follow-up hypotheses are derived from failed
// compiler/behavior evidence through genome-authorized source edit operators.
AutonomousProjectResult runAutonomousProjectRepair(
    const GenerativeProjectGraph& seed,
    const ProjectVerificationContract& invariantGoalContract,
    const AutonomousProjectConfig& config);

// Preserves evidence-backed intermediate graphs, allowing a later cycle to
// repair a newly exposed failure without discarding earlier valid changes.
AutonomousProjectResult runCumulativeAutonomousProjectRepair(
    const GenerativeProjectGraph& seed,
    const ProjectVerificationContract& invariantGoalContract,
    const AutonomousProjectConfig& config);

AutonomousProjectResult runExperienceTransferredProjectDevelopment(
    const GenerativeProjectGraph& seed,
    const ProjectVerificationContract& invariantGoalContract,
    const AutonomousProjectConfig& config,
    const ProjectExperienceStore& experience);

} // namespace genomic::gpi
