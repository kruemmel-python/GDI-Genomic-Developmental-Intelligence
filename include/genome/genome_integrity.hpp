#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "genome/genome_region.hpp"

namespace genomic {

struct IntegrityReport {
    bool passed{true};
    uint64_t globalStrandHash{0};
    uint32_t regionCount{0};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class GenomeIntegrity {
public:
    static uint32_t computeCrc32(const std::vector<GenomeBase>& bases);
    static uint32_t computeCrc32(const std::vector<uint8_t>& data);

    static uint64_t computeHash64(const std::vector<GenomeBase>& bases);
    static uint64_t computeHash64(const std::vector<uint8_t>& data);
    static uint64_t computeRegionHash(const GenomeRegion& region);

    static IntegrityReport validateStrand(
        const std::vector<GenomeBase>& fullStrand,
        const std::vector<GenomeRegion>& regions,
        uint64_t expectedStrandHash
    );

    static bool validateReferenceGraph(
        const std::vector<GenomeRegion>& regions,
        std::vector<std::string>& outErrors
    );
};

} // namespace genomic
