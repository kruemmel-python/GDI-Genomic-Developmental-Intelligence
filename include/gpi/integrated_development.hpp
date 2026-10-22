#pragma once

#include "gpi/research_developer.hpp"
#include "regulation/molecular_pool.hpp"
#include "genome/genome_reference.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class CompartmentRole { Unified, Analysis, Synthesis, Research };
enum class DevelopmentSignalKind {
    GoalAccepted,
    GoalUncertain,
    CapabilityProbeRequested,
    ResearchAuthorized,
    EvidenceVerified,
    NegativeEvidence,
    ComponentReady,
    SynthesisCompleted
};
enum class IntegratedArchitecture { Single, Isolated, Communicating, Experienced };

struct TypedDevelopmentSignal {
    DevelopmentSignalKind kind{DevelopmentSignalKind::GoalUncertain};
    uint32_t source{0};
    uint32_t target{0};
    uint64_t signature{0};
    uint64_t contractHash{0};
    double concentration{0.0};
    std::string evidence;
    bool delivered{false};
};

struct CompartmentDefinition {
    uint32_t id{0};
    CompartmentRole role{CompartmentRole::Analysis};
    bool experienceAccess{false};
    std::vector<DevelopmentSignalKind> acceptedSignals;
    std::vector<GenomeRegionId> accessibleRegions;
};

struct CompartmentSnapshot {
    uint32_t id{0};
    CompartmentRole role{CompartmentRole::Analysis};
    bool experienceAccess{false};
    std::vector<DigitalMolecule> molecules;
    std::vector<GenomeRegionId> accessibleRegions;
    std::size_t receivedSignals{0};
    std::size_t emittedSignals{0};
};

class MultiCompartmentSystem {
public:
    MultiCompartmentSystem(uint64_t genomeHash, bool communicationEnabled);
    void addCompartment(const CompartmentDefinition& definition);
    bool exchange(TypedDevelopmentSignal signal);
    double concentration(uint32_t compartment, DevelopmentSignalKind kind) const noexcept;
    std::vector<CompartmentSnapshot> snapshots() const;
    const std::vector<TypedDevelopmentSignal>& signals() const noexcept { return signals_; }
    uint64_t genomeHash() const noexcept { return genomeHash_; }

    static uint64_t signature(DevelopmentSignalKind kind) noexcept;

private:
    struct State {
        CompartmentDefinition definition;
        std::size_t received{0};
        std::size_t emitted{0};
    };
    uint64_t genomeHash_{0};
    bool communicationEnabled_{false};
    MolecularPool molecules_;
    std::vector<State> compartments_;
    std::vector<TypedDevelopmentSignal> signals_;
};

struct IntegratedMetrics {
    bool goalFulfilled{false};
    bool contractVerified{false};
    std::size_t compiledCandidates{0};
    std::size_t semanticChecks{0};
    std::size_t decisionErrors{0};
    std::size_t successfulTransfers{0};
    uint64_t compileMilliseconds{0};
    uint64_t executionMilliseconds{0};
};

struct IntegratedResult {
    IntegratedArchitecture architecture{IntegratedArchitecture::Single};
    GoalContract contract;
    std::string capabilityDiagnosis;
    ResearchResult development;
    IntegratedMetrics metrics;
    std::vector<CompartmentSnapshot> compartments;
    std::vector<TypedDevelopmentSignal> signals;
    std::vector<std::string> events;
};

IntegratedResult runIntegratedDevelopment(const ResearchPlan& plan,const ResearchConfig& config,
                                          IntegratedArchitecture architecture,
                                          std::size_t autonomousCycles=3);
std::vector<IntegratedResult> runArchitectureComparison(const ResearchPlan& plan,const ResearchConfig& config,
                                                        std::size_t autonomousCycles=3);
std::string integratedArchitectureName(IntegratedArchitecture architecture);

} // namespace genomic::gpi
