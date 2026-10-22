#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "genome/genome_reference.hpp"
#include "regulation/binding_site.hpp"

namespace genomic {

struct GenomeRepressor {
    GenomeRegionId id{0};
    GenomeRegionId targetRegion{0};
    std::string name;
    std::vector<GenomeBindingSite> sites;
    double repressionThreshold{0.4};
    double potency{1.5};

    bool operator==(const GenomeRepressor& other) const noexcept {
        return id == other.id &&
               targetRegion == other.targetRegion &&
               repressionThreshold == other.repressionThreshold &&
               potency == other.potency;
    }
};

} // namespace genomic
