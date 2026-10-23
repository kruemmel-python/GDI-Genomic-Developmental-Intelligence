#pragma once

#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/behavioral_induction.hpp"
#include "gpi/integrated_development.hpp"
#include "gpi/project_experience.hpp"
#include "runtime/genome_persistence.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

// A verified component (function, method, or user-defined type) learned during developmental ontogeny
// and persisted in a hash-chained ComponentExperienceStore so future tasks can reuse it across runs.
struct VerifiedComponentKnowledge {
    std::uint64_t genomeHash{0};
    std::uint64_t goalHash{0};
    std::uint64_t verifiedGraphHash{0};
    std::uint64_t evidenceHash{0};
    std::string kind; // "function" or "type"
    std::string name;
    std::string serializedGoalFragment;
};

class ComponentExperienceStore {
public:
    explicit ComponentExperienceStore(std::filesystem::path path) : path_(std::move(path)) {}
    std::vector<VerifiedComponentKnowledge> load() const;
    void appendGroup(const std::vector<VerifiedComponentKnowledge>& entries) const;
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

struct DevelopmentalTaskStage {
    std::string stageId;
    NeuralDevelopmentStage requiredStage{NeuralDevelopmentStage::Neonatal};
    BehavioralGoal goal;
};

struct DevelopmentalCurriculum {
    std::string curriculumName;
    std::string namespaceName{"generated"};
    std::vector<DevelopmentalTaskStage> stages;
};

struct DevelopmentalStageReport {
    std::size_t stageNumber{0};
    std::string stageId;
    NeuralDevelopmentStage organismStage{NeuralDevelopmentStage::Neonatal};
    std::uint64_t contractHash{0};
    std::uint64_t verifiedGraphHash{0};
    bool goalAccepted{false};
    bool regulatoryAuthorized{false};
    bool verified{false};
    std::size_t reusedComponents{0};
    std::size_t newlyInducedFunctions{0};
    std::size_t newlyInducedTypes{0};
    std::size_t totalSearchNodes{0};
    std::size_t verificationCycles{0};
    std::vector<std::string> reusedNames;
    std::vector<std::string> inducedNames;
    std::vector<std::string> causalChain;
    std::filesystem::path stageWorkspace;
    std::filesystem::path evidenceReport;
};

struct DevelopmentalIntelligenceConfig {
    std::filesystem::path workspace;
    std::filesystem::path compiler{"g++"};
    std::filesystem::path sandboxRunner;
    std::filesystem::path componentMemory;
    std::filesystem::path projectRepairMemory;
    std::size_t maxCyclesPerStage{8};
    bool requireSandbox{true};
};

struct DevelopmentalIntelligenceResult {
    bool success{false};
    std::uint64_t immutableGenomeHash{0};
    NeuralDevelopmentStage finalStage{NeuralDevelopmentStage::Neonatal};
    std::size_t totalStagesCompleted{0};
    std::size_t totalReusedComponents{0};
    std::size_t totalNewlyInducedComponents{0};
    std::vector<DevelopmentalStageReport> stageReports;
    std::vector<CompartmentSnapshot> compartments;
    std::vector<TypedDevelopmentSignal> signals;
    SystemSnapshot finalSnapshot;
    std::filesystem::path reportPath;
    std::filesystem::path snapshotPath;
};

// =========================================================================
// Autonomous Ontogenetic Developmental Planner (GDI Self-Directed Planning)
// =========================================================================

struct OntogeneticComponentRequirement {
    std::string id;          // e.g. "function:square", "type:Point2D", "method:Point2D::manhattan", "function:norm_sq", "function:point_energy", "function:store_geometry_record"
    std::string kind;        // "type", "method", "function"
    std::string moduleName;  // "math", "geometry", "analytics", "storage"
    std::vector<std::string> dependencies; // IDs of required components
    NeuralDevelopmentStage baselineStage{NeuralDevelopmentStage::Neonatal};
};

struct OntogeneticCapabilityAudit {
    std::string highLevelObjective;
    std::uint64_t objectiveSemanticHash{0};
    bool objectiveSupported{false};
    std::vector<std::string> activatedCapabilities;
    std::vector<OntogeneticComponentRequirement> requiredComponents;
    std::vector<std::string> satisfiedFromMemory;
    std::vector<std::string> knowledgeGaps;
    std::vector<std::string> diagnostics;
};

struct AutonomousOntogeneticPlan {
    OntogeneticCapabilityAudit audit;
    DevelopmentalCurriculum derivedCurriculum;
    std::string serializedCurriculum;
    std::string plannerRationale;
};

struct OntogeneticControlComparison {
    std::string highLevelObjective;
    AutonomousOntogeneticPlan naivePlan;
    DevelopmentalIntelligenceResult naiveResult;
    AutonomousOntogeneticPlan experiencedPlan;
    DevelopmentalIntelligenceResult experiencedResult;
    bool memoryAlteredDevelopmentalPath{false};
    std::size_t stagesSavedByExperience{0};
    std::size_t componentsReusedFromExperience{0};
    std::filesystem::path comparisonReportPath;
};

DevelopmentalCurriculum loadDevelopmentalCurriculum(const std::filesystem::path& file);
DevelopmentalCurriculum parseDevelopmentalCurriculumText(const std::string& text);
DevelopmentalCurriculum curriculumFromBehavioralGoal(const BehavioralGoal& goal);

// Analyzes a high-level goal against the immutable genome and existing ComponentExperienceStore,
// identifies capability gaps, and derives a custom ontogenetic DevelopmentalCurriculum.
AutonomousOntogeneticPlan planAutonomousOntogenyFromObjective(
    const std::string& highLevelObjective,
    const std::filesystem::path& componentMemoryPath = {});

// Executes a self-directed ontogenetic development run from a high-level objective.
DevelopmentalIntelligenceResult runAutonomousGoalDirectedOntogeny(
    const std::string& highLevelObjective,
    const DevelopmentalIntelligenceConfig& config);

// Executes a controlled scientific comparison: runs the exact same high-level objective once
// without prior component memory (naive organism) and once with component memory (experienced organism),
// proving that experience changes the derived developmental curriculum and ontogenetic path.
OntogeneticControlComparison runOntogeneticControlExperiment(
    const std::string& highLevelObjective,
    const DevelopmentalIntelligenceConfig& baseConfig,
    const std::filesystem::path& priorComponentMemory);

DevelopmentalIntelligenceResult runDevelopmentalProgrammingIntelligence(
    const DevelopmentalCurriculum& curriculum,
    const DevelopmentalIntelligenceConfig& config);

} // namespace genomic::gpi
