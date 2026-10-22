#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "genome/genome_reference.hpp"
#include "development/lineage_context.hpp"

namespace genomic {

enum class NeuralDevelopmentStage : uint8_t {
    Neonatal = 0,
    Juvenile = 1,
    Mature   = 2
};

inline std::string stageToString(NeuralDevelopmentStage stage) {
    switch (stage) {
        case NeuralDevelopmentStage::Neonatal: return "Neonatal";
        case NeuralDevelopmentStage::Juvenile: return "Juvenile";
        case NeuralDevelopmentStage::Mature: return "Mature";
    }
    return "Unknown";
}

struct DevelopmentConstraint {
    NeuralDevelopmentStage minimumStage{NeuralDevelopmentStage::Neonatal};
    TissueLineage requiredLineage{TissueLineage::AnteriorNervous};
    std::vector<uint64_t> requiredMolecules;
    double minimumResourceLevel{0.1};
};

struct DevelopmentRule {
    GenomeRegionId ruleId{0};
    GenomeSymbolId targetEntity{0};
    std::string name;
    DevelopmentConstraint constraint;
    std::string compositionScript;
};

} // namespace genomic
