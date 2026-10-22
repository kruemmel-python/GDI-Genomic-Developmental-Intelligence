#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace genomic { class GenomeStrand; }
namespace genomic::gpi {

enum class ResearchProblem { Sort, Unique, Median, Frequency, Composite, ArbitraryCpp };
enum class ResearchDevelopmentState { Explore, Control, Developed };

struct DevelopmentNode {
    std::string id;
    std::string operation;
    std::string inputType;
    std::string outputType;
    std::vector<std::string> dependencies;
};

struct OperationSpec {
    std::string operation;
    std::string inputType;
    std::string outputType;
    std::optional<int64_t> integerParameter;
};

struct GoalContract {
    std::string originalRequest;
    std::string normalizedIntent;
    std::string inputType;
    std::string outputType;
    std::vector<OperationSpec> requiredOperations;
    std::vector<std::string> unresolvedRequirements;
    std::vector<std::string> contradictions;
    uint64_t semanticHash{0};
    bool accepted{false};
};

struct GenomicStrategyActivation {
    std::string strategy;
    uint64_t regionId{0};
    uint64_t moleculeSignature{0};
    double activity{0.0};
    bool expressed{false};
    std::string developmentalCondition;
};

struct ComponentDecision {
    std::string component;
    std::string responsibility;
    std::string dataStructure;
    std::string algorithm;
};

struct ResearchPlan {
    bool supported{false};
    bool requiresUnsafeMode{false};
    ResearchProblem problem{ResearchProblem::Sort};
    std::string goal;
    std::string embeddedSource;
    std::vector<ComponentDecision> components;
    std::vector<DevelopmentNode> graph;
    std::vector<OperationSpec> operations;
    GoalContract contract;
    std::vector<std::string> strategies;
    std::vector<std::string> diagnostics;
};

struct ResearchConfig {
    std::filesystem::path workspace;
    std::string compiler{"g++"};
    std::filesystem::path fixtureInput;
    std::filesystem::path fixtureExpected;
    std::filesystem::path memory;
    // Required for arbitrary prompt-provided code. Contract:
    // runner --workspace DIR --timeout-seconds N -- COMMAND [ARGS...]
    std::filesystem::path sandboxRunner;
    const GenomeStrand* genome{nullptr};
    ResearchDevelopmentState developmentState{ResearchDevelopmentState::Explore};
    std::size_t maxCycles{3};
    bool unsafeMode{false};
};

struct ResearchCycle {
    std::size_t number{0};
    std::string strategy;
    bool compiled{false};
    bool behaviorVerified{false};
    std::size_t score{0};
    std::filesystem::path source;
    std::filesystem::path executable;
    std::string diagnostic;
    uint64_t genomeRegion{0};
    uint64_t moleculeSignature{0};
    double regulatoryActivity{0.0};
    std::size_t semanticChecks{0};
    uint64_t compileMilliseconds{0};
    uint64_t executionMilliseconds{0};
};

struct ResearchResult {
    bool success{false};
    bool behaviorVerified{false};
    std::size_t cycles{0};
    std::string selectedStrategy;
    std::filesystem::path selectedSource;
    std::vector<ResearchCycle> history;
    uint64_t genomeHash{0};
    std::string experienceSignature;
    std::string experienceSourceSignature;
    double experienceSimilarity{0.0};
    bool experienceTransferred{false};
    std::string developedComponent;
    std::string reusedComponent;
    std::filesystem::path componentArtifact;
    bool autonomousMode{false};
    std::size_t autonomousCycles{0};
    std::vector<std::string> capabilityGaps;
    std::vector<std::string> derivedGoals;
    std::string capabilityDiagnosis;
    bool baselineVerified{false};
    std::size_t baselineScore{0};
    std::size_t totalSemanticChecks{0};
    uint64_t totalCompileMilliseconds{0};
    uint64_t totalExecutionMilliseconds{0};
    std::size_t totalCompiledCandidates{0};
    std::size_t totalDecisionErrors{0};
    std::vector<GenomicStrategyActivation> regulation;
    std::vector<std::string> events;
};

ResearchPlan decomposeResearchTask(const std::string& prompt, bool unsafeMode);
ResearchResult runResearchDevelopment(const ResearchPlan& plan, const ResearchConfig& config);
ResearchResult runAutonomousDevelopment(const ResearchPlan& plan, const ResearchConfig& config,
                                        std::size_t maxAutonomousCycles);

} // namespace genomic::gpi
