#include "runtime/genome_persistence.hpp"
#include "runtime/substrate_kernel.hpp"
#include <sstream>

namespace genomic {

SystemSnapshot GenomePersistence::captureSnapshot(
    const GenomeStrand& strand,
    const MolecularPool& pool,
    const PhenotypeBuilder& builder,
    uint64_t currentTick
) {
    SystemSnapshot snap;
    snap.genomeHash = strand.getGenomeHash();
    snap.timestampTick = currentTick;
    snap.molecularPoolState = pool.getAllMolecules();
    snap.developmentalStage = builder.getStage();
    snap.resourceLevel = builder.getResourceLevel();
    snap.activeStructureCount = builder.getInstances().size();

    std::string text = serialize(snap);
    snap.stateHash = SubstrateKernel::fnv1a64(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return snap;
}

bool GenomePersistence::restoreSnapshot(
    const SystemSnapshot& snapshot,
    MolecularPool& outPool,
    PhenotypeBuilder& outBuilder
) {
    outPool.clear();
    for (const auto& mol : snapshot.molecularPoolState) {
        outPool.setConcentration(mol.compartment, mol.signature, mol.concentration, mol.name, mol.molClass, mol.halfLife);
    }
    outBuilder.setStage(snapshot.developmentalStage);
    outBuilder.setResourceLevel(snapshot.resourceLevel);
    return true;
}

std::string GenomePersistence::serialize(const SystemSnapshot& snapshot) {
    std::ostringstream ss;
    ss << "GENOMIC_SNAPSHOT_V1\n"
       << "genomeHash=" << snapshot.genomeHash << "\n"
       << "tick=" << snapshot.timestampTick << "\n"
       << "stage=" << static_cast<int>(snapshot.developmentalStage) << "\n"
       << "resource=" << snapshot.resourceLevel << "\n"
       << "structures=" << snapshot.activeStructureCount << "\n"
       << "molecules=" << snapshot.molecularPoolState.size() << "\n";

    for (const auto& mol : snapshot.molecularPoolState) {
        ss << mol.compartment << "," << mol.signature << "," << mol.concentration << ","
           << mol.halfLife << "," << mol.name << "\n";
    }
    return ss.str();
}

SystemSnapshot GenomePersistence::deserialize(const std::string& data) {
    SystemSnapshot snap;
    std::istringstream stream(data);
    std::string line;

    if (!std::getline(stream, line) || line.rfind("GENOMIC_SNAPSHOT", 0) != 0) {
        return snap;
    }

    size_t molCount = 0;
    while (std::getline(stream, line)) {
        if (line.rfind("genomeHash=", 0) == 0) snap.genomeHash = std::stoull(line.substr(11));
        else if (line.rfind("tick=", 0) == 0) snap.timestampTick = std::stoull(line.substr(5));
        else if (line.rfind("stage=", 0) == 0) snap.developmentalStage = static_cast<NeuralDevelopmentStage>(std::stoi(line.substr(6)));
        else if (line.rfind("resource=", 0) == 0) snap.resourceLevel = std::stod(line.substr(9));
        else if (line.rfind("structures=", 0) == 0) snap.activeStructureCount = std::stoull(line.substr(11));
        else if (line.rfind("molecules=", 0) == 0) {
            molCount = std::stoull(line.substr(10));
            break;
        }
    }

    for (size_t i = 0; i < molCount && std::getline(stream, line); ++i) {
        std::istringstream mStream(line);
        std::string compStr, sigStr, concStr, hlStr, name;
        if (std::getline(mStream, compStr, ',') &&
            std::getline(mStream, sigStr, ',') &&
            std::getline(mStream, concStr, ',') &&
            std::getline(mStream, hlStr, ',') &&
            std::getline(mStream, name)) {
            DigitalMolecule mol;
            mol.compartment = std::stoul(compStr);
            mol.signature = std::stoull(sigStr);
            mol.concentration = std::stod(concStr);
            mol.halfLife = std::stod(hlStr);
            mol.name = name;
            snap.molecularPoolState.push_back(mol);
        }
    }

    std::string text = serialize(snap);
    snap.stateHash = SubstrateKernel::fnv1a64(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return snap;
}

} // namespace genomic
