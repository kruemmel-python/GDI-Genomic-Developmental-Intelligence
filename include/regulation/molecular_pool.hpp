#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "regulation/molecule.hpp"

namespace genomic {

class MolecularPool {
public:
    MolecularPool() = default;

    void setConcentration(uint32_t compartment, uint64_t signature, double concentration,
                          const std::string& name = "", MoleculeClass mClass = MoleculeClass::Signal,
                          double halfLife = 100.0);

    void addConcentration(uint32_t compartment, uint64_t signature, double delta,
                          const std::string& name = "", MoleculeClass mClass = MoleculeClass::Signal);

    double getConcentration(uint32_t compartment, uint64_t signature) const noexcept;
    bool hasMolecule(uint32_t compartment, uint64_t signature) const noexcept;

    std::vector<DigitalMolecule> getMolecules(uint32_t compartment) const;
    std::vector<DigitalMolecule> getAllMolecules() const;

    // Simulation steps
    void updateDecay(double elapsedTicks = 1.0);
    void updateDiffusion(uint32_t compartmentA, uint32_t compartmentB, double diffusionRate);

    void clear();

private:
    struct MoleculeKey {
        uint32_t compartment{0};
        uint64_t signature{0};
        bool operator==(const MoleculeKey&) const noexcept = default;
    };
    struct MoleculeKeyHash {
        std::size_t operator()(const MoleculeKey& key) const noexcept {
            // Hash collisions are harmless because unordered_map compares both
            // complete fields with MoleculeKey::operator==.
            const auto mixed=key.signature^(static_cast<uint64_t>(key.compartment)*0x9E3779B97F4A7C15ULL);
            return static_cast<std::size_t>(mixed^(mixed>>32));
        }
    };

    std::unordered_map<MoleculeKey, DigitalMolecule, MoleculeKeyHash> pool_;
};

} // namespace genomic
