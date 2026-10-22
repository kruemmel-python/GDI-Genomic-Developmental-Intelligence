#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "genome/genome_base.hpp"
#include "genome/genome_region.hpp"
#include "genome/genome_integrity.hpp"

namespace genomic {

class GenomeStrand {
public:
    GenomeStrand();

    // Builder methods (allowed only before freeze)
    void appendBases(const std::vector<GenomeBase>& bases);
    void appendBasesFromString(const std::string& sequence);
    GenomeRegionId addRegion(RegionKind kind,
                             const std::string& name,
                             const std::vector<GenomeBase>& regionBases,
                             const std::vector<uint8_t>& payload = {},
                             const std::vector<GenomeRegionId>& references = {},
                             const std::string& description = "");

    void freeze();
    bool isFrozen() const noexcept;

    // Read-only queries (available anytime)
    size_t length() const noexcept;
    const std::vector<GenomeBase>& getBases() const noexcept;
    std::string getSequenceString() const;
    std::vector<Codon> getCodons() const;

    bool hasRegion(GenomeRegionId id) const noexcept;
    const GenomeRegion& getRegion(GenomeRegionId id) const;
    const std::vector<GenomeRegion>& getAllRegions() const noexcept;

    uint64_t getGenomeHash() const noexcept;
    IntegrityReport verifyIntegrity() const;

    // Export formats
    std::string toFasta(const std::string& header = "GENOME_C_IMMUTABLE_STRAND") const;

    // Experience guard: verifies invariant that experience cannot mutate genome
    void assertImmutable() const;

private:
    std::vector<GenomeBase> physicalStrand_;
    std::vector<GenomeRegion> regions_;
    std::unordered_map<GenomeRegionId, size_t> regionIndexMap_;
    uint64_t genomeHash_{0};
    bool frozen_{false};
    GenomeRegionId nextRegionId_{1};
};

} // namespace genomic
