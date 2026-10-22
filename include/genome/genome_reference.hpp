#pragma once

#include <cstdint>
#include <string>

namespace genomic {

using GenomeRegionId = std::uint64_t;
using GenomeSymbolId = std::uint64_t;
using GenomeFunctionId = std::uint64_t;

struct GenomeReference {
    GenomeRegionId targetRegion{0};
    GenomeSymbolId targetSymbol{0};
    std::string identifierName;

    bool operator==(const GenomeReference& other) const noexcept {
        return targetRegion == other.targetRegion &&
               targetSymbol == other.targetSymbol &&
               identifierName == other.identifierName;
    }
};

} // namespace genomic
