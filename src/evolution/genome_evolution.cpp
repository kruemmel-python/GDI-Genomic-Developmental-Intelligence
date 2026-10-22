#include "evolution/genome_evolution.hpp"
#include "genome/genome_decoder.hpp"
#include <sstream>

namespace genomic {

std::pair<std::shared_ptr<GenomeStrand>, RecombinationReport> GenomeEvolution::recombine(
    const GenomeStrand& parentA,
    const GenomeStrand& parentB,
    uint64_t crossoverRegionId
) {
    auto offspring = std::make_shared<GenomeStrand>();
    bool takeFromA = true;

    // Recombine at region boundaries
    for (const auto& regA : parentA.getAllRegions()) {
        if (regA.id == crossoverRegionId) {
            takeFromA = false;
        }

        if (takeFromA) {
            offspring->addRegion(regA.kind, regA.name, regA.bases, regA.payload, regA.referencedRegions, regA.description);
        } else {
            // Find corresponding region in parent B if available, else keep A
            if (parentB.hasRegion(regA.id)) {
                const auto& regB = parentB.getRegion(regA.id);
                offspring->addRegion(regB.kind, regB.name, regB.bases, regB.payload, regB.referencedRegions, regB.description);
            } else {
                offspring->addRegion(regA.kind, regA.name, regA.bases, regA.payload, regA.referencedRegions, regA.description);
            }
        }
    }

    offspring->freeze();
    auto report = validateOffspring(*offspring);
    return {offspring, report};
}

std::shared_ptr<GenomeStrand> GenomeEvolution::mutate(
    const GenomeStrand& parent,
    MutationType type,
    GenomeRegionId targetRegionId,
    const std::vector<uint8_t>& mutatedPayload
) {
    (void)type;
    auto mutated = std::make_shared<GenomeStrand>();

    bool targetFound = parent.hasRegion(targetRegionId);
    GenomeRegionId actualTargetId = targetFound ? targetRegionId : (parent.getAllRegions().empty() ? 0 : parent.getAllRegions().front().id);

    for (const auto& reg : parent.getAllRegions()) {
        if (reg.id == actualTargetId) {
            auto mutatedBases = reg.bases;
            if (!mutatedPayload.empty()) {
                auto extraBases = GenomeDecoder::encodeBytesToBases(mutatedPayload.data(), mutatedPayload.size());
                mutatedBases.insert(mutatedBases.end(), extraBases.begin(), extraBases.end());
            } else if (!mutatedBases.empty()) {
                mutatedBases[0] = (mutatedBases[0] == GenomeBase::A) ? GenomeBase::G : GenomeBase::A;
            }
            mutated->addRegion(reg.kind, reg.name + "_mutated", mutatedBases, mutatedPayload, reg.referencedRegions, reg.description);
        } else {
            mutated->addRegion(reg.kind, reg.name, reg.bases, reg.payload, reg.referencedRegions, reg.description);
        }
    }

    mutated->freeze();
    return mutated;
}

RecombinationReport GenomeEvolution::validateOffspring(const GenomeStrand& offspring) {
    RecombinationReport rep;
    rep.offspringHash = offspring.getGenomeHash();
    auto integrity = offspring.verifyIntegrity();
    rep.isViable = integrity.passed;
    rep.validationErrors = integrity.errors;
    return rep;
}

} // namespace genomic
