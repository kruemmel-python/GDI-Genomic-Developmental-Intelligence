#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "genome/genome_reference.hpp"

namespace genomic {

struct ExpressionTrace {
    uint64_t expressionEventId{0};
    uint64_t genomeHash{0};
    std::string triggerName;
    std::vector<std::pair<uint64_t, double>> boundMolecules;
    std::vector<GenomeRegionId> activeEnhancers;
    std::vector<GenomeRegionId> activeRegions;
    std::vector<std::string> splicedExonNames;
    std::string generatedCppSource;
    uint64_t astHash{0};
    uint64_t irHash{0};
    bool semanticValidationPassed{false};
    bool executionActive{false};
};

} // namespace genomic
