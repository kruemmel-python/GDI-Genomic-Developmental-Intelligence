#pragma once

#include "gpi/build_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class AdaptiveExperimentOutcome { Complete, AlreadySatisfied, Blocked, Failed };

// A behavioural oracle deliberately contains no source location, symbol, or
// transformation.  It describes only the externally observable result.
struct AdaptiveBehaviourOracle {
    std::vector<std::string> arguments;
    std::string requiredOutput;
    std::string forbiddenOutput;
    int expectedExitCode{0};
};

struct AdaptiveExperimentConfig {
    std::filesystem::path protectedProject, isolatedWorkspace, experienceStore;
    NativeBuildConfig nativeBuild;
    AdaptiveBehaviourOracle oracle;
    std::size_t maxAttempts{50};
    bool execute{true};
};

struct AdaptiveHypothesisEvidence {
    std::filesystem::path source;
    std::string before, after, rationale;
    bool compiled{}, testsPassed{}, accepted{};
};

struct AdaptiveExperimentResult {
    AdaptiveExperimentOutcome outcome{AdaptiveExperimentOutcome::Blocked};
    NativeBuildResult baseline;
    std::vector<AdaptiveHypothesisEvidence> attempts;
    std::filesystem::path developedProject, reportPath;
    bool originalUntouched{}, experienceUsed{}, experiencePersisted{};
    std::size_t suppressedFailedHypotheses{};
    std::string reason;
};

struct AdaptiveTrainingConfig {
    std::vector<std::filesystem::path> protectedProjects;
    std::filesystem::path modelPath;
    bool verifyBaselines{false};
    NativeBuildConfig nativeBuild;
};

struct AdaptiveTrainingResult {
    std::size_t projectsAnalysed{}, observableLiteralContexts{};
    bool modelPersisted{};
    std::vector<std::string> rejectedProjects;
};

// Independent pre-use training: derives only project-agnostic structural
// evidence from protected projects.  It never writes to the training inputs.
AdaptiveTrainingResult trainAdaptiveExperienceModel(const AdaptiveTrainingConfig& config);

// Discovers transformations from a failing runtime oracle.  The initial
// operator grammar is intentionally structural (unique observable literal
// substitution), rather than a vocabulary of project/domain repair classes.
AdaptiveExperimentResult runAdaptiveExperimentation(const AdaptiveExperimentConfig& config);
const char* toString(AdaptiveExperimentOutcome value) noexcept;

} // namespace genomic::gpi
