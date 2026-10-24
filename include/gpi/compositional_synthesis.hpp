#pragma once

#include "gpi/build_contract.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class CompositionalSynthesisOutcome { Complete, AlreadySatisfied, Blocked, Failed };

struct CompositionalSynthesisConfig {
    std::filesystem::path protectedProject;
    std::filesystem::path isolatedWorkspace;
    std::filesystem::path experienceStore;
    NativeBuildConfig nativeBuild;
    std::size_t maxCandidates{50};
    std::size_t timeLimitMilliseconds{1'500'000};
};

struct CompositionalSynthesisAttempt {
    std::filesystem::path source;
    std::string program;
    bool compiled{};
    bool testsPassed{};
};

struct CompositionalSynthesisResult {
    CompositionalSynthesisOutcome outcome{CompositionalSynthesisOutcome::Blocked};
    NativeBuildResult baseline;
    std::vector<CompositionalSynthesisAttempt> attempts;
    std::filesystem::path developedProject;
    bool originalUntouched{};
    bool experienceUsed{};
    bool experiencePersisted{};
    bool timeLimitReached{};
    std::size_t elapsedMilliseconds{};
    std::string reason;
};

CompositionalSynthesisResult runCompositionalSynthesis(const CompositionalSynthesisConfig& config);
const char* toString(CompositionalSynthesisOutcome outcome) noexcept;

} // namespace genomic::gpi
