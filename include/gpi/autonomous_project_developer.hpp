#pragma once

#include "gpi/project_verifier.hpp"
#include "gpi/project_experience.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class CompilerDiagnosticClass {
    Syntax,
    MissingDeclaration,
    TypeMismatch,
    ConstraintFailure,
    DeprecatedApi,
    UndefinedReference,
    DuplicateDefinition,
    Warning,
    Unknown
};

struct CompilerDiagnostic {
    std::filesystem::path file;
    std::size_t line{0};
    std::size_t column{0};
    std::string severity;
    CompilerDiagnosticClass classification{CompilerDiagnosticClass::Unknown};
    std::string message;
    std::string raw;
};

// Parses GCC/Clang-style location diagnostics and linker diagnostics into a
// stable model used by repair candidate generation and campaign evidence.
std::vector<CompilerDiagnostic> parseCompilerDiagnostics(const std::string& text);

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
    std::vector<CompilerDiagnostic> diagnostics;
    bool duplicateStrategySuppressed{false};
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
    std::string terminationReason;
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
