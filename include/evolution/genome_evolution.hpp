#pragma once

#include <memory>
#include <vector>
#include <string>
#include "genome/genome_strand.hpp"
#include "genome/genome_integrity.hpp"

namespace genomic {

enum class MutationType : uint8_t {
    ParameterMutation,
    MotifMutation,
    BindingSiteMutation,
    RegulatoryMutation,
    FunctionMutation,
    TypeMutation,
    ModuleMutation,
    GrammarMutation,
    CompilerMutation
};

struct RecombinationReport {
    bool isViable{false};
    uint64_t offspringHash{0};
    std::vector<std::string> validationErrors;
};

class GenomeEvolution {
public:
    // Safe recombination between two parent genomes at semantic region boundaries
    static std::pair<std::shared_ptr<GenomeStrand>, RecombinationReport> recombine(
        const GenomeStrand& parentA,
        const GenomeStrand& parentB,
        uint64_t crossoverRegionId
    );

    // Explicit reproductive mutation (never during life!)
    static std::shared_ptr<GenomeStrand> mutate(
        const GenomeStrand& parent,
        MutationType type,
        GenomeRegionId targetRegionId,
        const std::vector<uint8_t>& mutatedPayload
    );

    // Validation gate for offspring genomes
    static RecombinationReport validateOffspring(const GenomeStrand& offspring);
};

} // namespace genomic
