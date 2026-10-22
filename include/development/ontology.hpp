#pragma once

#include <cstdint>
#include <string>
#include "genome/genome_reference.hpp"

namespace genomic {

enum class EntityCategory : uint16_t {
    Neuron            = 1,
    Synapse           = 2,
    Assembly          = 3,
    Sensor            = 4,
    Organ             = 5,
    MemoryStore       = 6,
    Program           = 7,
    Service           = 8,
    Controller        = 9,
    Network           = 10,
    Signal            = 11,
    MoleculePrimitive = 12
};

struct GenomeEntityDefinition {
    GenomeSymbolId entityType{0};
    EntityCategory category{EntityCategory::Program};
    std::string name;
    GenomeRegionId compositionRule{0};
    GenomeRegionId developmentRule{0};
    GenomeRegionId destructionRule{0};
    GenomeRegionId repairRule{0};
};

} // namespace genomic
