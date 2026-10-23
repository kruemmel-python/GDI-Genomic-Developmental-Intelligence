#pragma once

#include "gpi/build_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class StrategyStatus { Candidate, Executable, Running, Succeeded, Failed, Rejected, Uncertain };
struct StrategyOperator {
    std::string id, operation;
    std::vector<std::string> prerequisites, dependsOn, expectedEffects, verification;
    bool mutatesSources{false};
};
struct DevelopmentStrategy {
    std::string id, version{"1"}, goal, origin;
    std::vector<std::string> triggers, prerequisites, targetState, verificationConditions,
        rollbackInformation, safetyRequirements, successfulContexts, failedContexts;
    std::vector<StrategyOperator> operators;
    std::string requiredBuildSystem, requiredCompilerFamily;
    StrategyStatus status{StrategyStatus::Candidate};
    double confidence{0.0};
};
struct StrategyExecutionStep {
    std::string operatorId, inputState, outputState, reason;
    bool prerequisitesMet{false}, succeeded{false};
    std::vector<std::filesystem::path> changedArtifacts;
    std::vector<BuildDiagnostic> diagnostics;
};
struct StrategyExecutionResult {
    bool success{false}, rolledBack{false};
    std::string terminationReason;
    DevelopmentStrategy strategy;
    NativeBuildResult build;
    std::vector<StrategyExecutionStep> steps;
    std::filesystem::path snapshotPath, reportPath;
    std::size_t nextOperator{0};
};
struct StrategyPlanningRequest {
    std::string goal;
    BuildContract contract;
    BuildBaselineStatus baseline{BuildBaselineStatus::BuildContractUnverified};
    std::vector<BuildDiagnostic> diagnostics;
    std::vector<DevelopmentStrategy> experience;
    std::size_t searchBudget{16};
};
struct StrategyPlan {
    bool executable{false};
    std::string rationale;
    std::vector<DevelopmentStrategy> candidates;
    DevelopmentStrategy selected;
};

class DevelopmentStrategyInductionEngine {
public:
    std::vector<DevelopmentStrategy> induce(const StrategyPlanningRequest& request) const;
};
class AutonomousStrategyPlanner {
public:
    StrategyPlan plan(const StrategyPlanningRequest& request) const;
};
class DevelopmentStrategyStore {
public:
    explicit DevelopmentStrategyStore(std::filesystem::path path):path_(std::move(path)){}
    std::vector<DevelopmentStrategy> load() const;
    void append(const DevelopmentStrategy& strategy) const;
    void replace(const std::vector<DevelopmentStrategy>& strategies) const;
private:
    std::filesystem::path path_;
};

double strategyTransferScore(const DevelopmentStrategy& strategy,const BuildContract& contract,
                             const std::vector<BuildDiagnostic>& diagnostics);
StrategyExecutionResult executeDevelopmentStrategy(const DevelopmentStrategy& strategy,
    const BuildContract& contract,const NativeBuildConfig& config);

} // namespace genomic::gpi
