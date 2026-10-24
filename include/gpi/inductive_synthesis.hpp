#pragma once

#include "gpi/build_contract.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {
enum class InductiveSynthesisOutcome { Complete, AlreadySatisfied, Blocked, Failed };
struct InductiveSynthesisConfig { std::filesystem::path protectedProject, isolatedWorkspace, experienceStore; NativeBuildConfig nativeBuild; std::size_t maxCandidates{50}; };
struct InductiveSynthesisAttempt { std::filesystem::path source; std::string expression; bool compiled{}, testsPassed{}; };
struct InductiveSynthesisResult { InductiveSynthesisOutcome outcome{InductiveSynthesisOutcome::Blocked}; NativeBuildResult baseline; std::vector<InductiveSynthesisAttempt> attempts; std::filesystem::path developedProject, reportPath; bool originalUntouched{}, experienceUsed{}, experiencePersisted{}; std::string reason; };
InductiveSynthesisResult runInductiveSynthesis(const InductiveSynthesisConfig& config);
const char* toString(InductiveSynthesisOutcome value) noexcept;
} // namespace genomic::gpi
