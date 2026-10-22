#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "genome/genome_reference.hpp"

namespace genomic {

struct ProgramRequirement {
    GenomeSymbolId desiredCapability{0};
    std::string capabilityName;
    std::vector<GenomeSymbolId> requiredInputs;
    std::vector<GenomeSymbolId> requiredOutputs;
    double energyBudget{1000.0};
    double memoryBudget{1024.0 * 1024.0};

    bool operator==(const ProgramRequirement& other) const noexcept {
        return desiredCapability == other.desiredCapability &&
               capabilityName == other.capabilityName &&
               requiredInputs == other.requiredInputs &&
               requiredOutputs == other.requiredOutputs &&
               energyBudget == other.energyBudget &&
               memoryBudget == other.memoryBudget;
    }
};

} // namespace genomic
