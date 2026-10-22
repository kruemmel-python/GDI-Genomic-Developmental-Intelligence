#pragma once

#include <string>
#include <vector>
#include <memory>
#include "genome/genome_strand.hpp"
#include "regulation/molecular_pool.hpp"
#include "development/phenotype_builder.hpp"

namespace genomic {

struct SystemSnapshot {
    uint64_t genomeHash{0};
    uint64_t timestampTick{0};
    std::vector<DigitalMolecule> molecularPoolState;
    NeuralDevelopmentStage developmentalStage{NeuralDevelopmentStage::Neonatal};
    double resourceLevel{1.0};
    size_t activeStructureCount{0};
    uint64_t stateHash{0};
};

class GenomePersistence {
public:
    static SystemSnapshot captureSnapshot(
        const GenomeStrand& strand,
        const MolecularPool& pool,
        const PhenotypeBuilder& builder,
        uint64_t currentTick
    );

    static bool restoreSnapshot(
        const SystemSnapshot& snapshot,
        MolecularPool& outPool,
        PhenotypeBuilder& outBuilder
    );

    // Deterministic serialization to/from text or binary
    static std::string serialize(const SystemSnapshot& snapshot);
    static SystemSnapshot deserialize(const std::string& data);
};

} // namespace genomic
