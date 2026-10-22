#pragma once

#include <cstdint>
#include <string>

namespace genomic {

enum class TissueLineage : uint8_t {
    AnteriorNervous,
    PosteriorNervous,
    Cardiovascular,
    Respiratory,
    Metabolic,
    Immune,
    Reproductive,
    GenericComputational
};

struct LineageContext {
    TissueLineage lineage{TissueLineage::AnteriorNervous};
    uint32_t compartment{0};
    std::string anatomicalRegion{"AnteriorBrain"};
};

} // namespace genomic
