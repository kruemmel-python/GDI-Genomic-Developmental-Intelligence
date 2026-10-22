#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "genome/genome_reference.hpp"
#include "regulation/binding_site.hpp"

namespace genomic {

struct GenomeEnhancer {
    GenomeRegionId id{0};
    GenomeRegionId targetRegion{0};
    std::string name;
    std::vector<GenomeBindingSite> sites;
    double activationThreshold{0.5};
    double cooperativity{2.0}; // Hill coefficient
    int32_t maxBindingDistance{50}; // Maximum distance for cooperative interaction

    bool operator==(const GenomeEnhancer& other) const noexcept {
        return id == other.id &&
               targetRegion == other.targetRegion &&
               activationThreshold == other.activationThreshold &&
               cooperativity == other.cooperativity;
    }
};

} // namespace genomic
