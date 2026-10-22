#include "regulation/molecular_pool.hpp"
#include <cmath>
#include <algorithm>

namespace genomic {

void MolecularPool::setConcentration(uint32_t compartment, uint64_t signature, double concentration,
                                     const std::string& name, MoleculeClass mClass, double halfLife) {
    const MoleculeKey key{compartment,signature};
    double clamped = std::clamp(concentration, 0.0, 1.0);
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        it->second.concentration = clamped;
        if (!name.empty()) it->second.name = name;
        if (halfLife > 0.0) it->second.halfLife = halfLife;
    } else {
        DigitalMolecule mol;
        mol.compartment = compartment;
        mol.signature = signature;
        mol.concentration = clamped;
        mol.name = name.empty() ? ("MOL_0x" + std::to_string(signature)) : name;
        mol.molClass = mClass;
        mol.halfLife = halfLife > 0.0 ? halfLife : 100.0;
        pool_[key] = mol;
    }
}

void MolecularPool::addConcentration(uint32_t compartment, uint64_t signature, double delta,
                                     const std::string& name, MoleculeClass mClass) {
    const MoleculeKey key{compartment,signature};
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        it->second.concentration = std::clamp(it->second.concentration + delta, 0.0, 1.0);
    } else {
        setConcentration(compartment, signature, delta, name, mClass);
    }
}

double MolecularPool::getConcentration(uint32_t compartment, uint64_t signature) const noexcept {
    const MoleculeKey key{compartment,signature};
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        return it->second.concentration;
    }
    return 0.0;
}

bool MolecularPool::hasMolecule(uint32_t compartment, uint64_t signature) const noexcept {
    const MoleculeKey key{compartment,signature};
    auto it = pool_.find(key);
    return it != pool_.end() && it->second.concentration > 1e-6;
}

std::vector<DigitalMolecule> MolecularPool::getMolecules(uint32_t compartment) const {
    std::vector<DigitalMolecule> result;
    for (const auto& [k, mol] : pool_) {
        if (mol.compartment == compartment && mol.concentration > 1e-6) {
            result.push_back(mol);
        }
    }
    return result;
}

std::vector<DigitalMolecule> MolecularPool::getAllMolecules() const {
    std::vector<DigitalMolecule> result;
    result.reserve(pool_.size());
    for (const auto& [k, mol] : pool_) {
        if (mol.concentration > 1e-6) {
            result.push_back(mol);
        }
    }
    return result;
}

void MolecularPool::updateDecay(double elapsedTicks) {
    // Exponential decay: C(t + dt) = C(t) * exp(-ln(2) * dt / halfLife)
    const double ln2 = 0.6931471805599453;
    for (auto& [k, mol] : pool_) {
        if (mol.halfLife > 0.0 && mol.concentration > 1e-7) {
            double decayFactor = std::exp(-ln2 * elapsedTicks / mol.halfLife);
            mol.concentration *= decayFactor;
            if (mol.concentration < 1e-6) {
                mol.concentration = 0.0;
            }
        }
    }
}

void MolecularPool::updateDiffusion(uint32_t compartmentA, uint32_t compartmentB, double diffusionRate) {
    if (compartmentA == compartmentB || diffusionRate <= 0.0) return;

    double d = std::clamp(diffusionRate, 0.0, 0.5);

    // Collect all signatures present in either compartment
    std::vector<uint64_t> signatures;
    for (const auto& [k, mol] : pool_) {
        if (mol.compartment == compartmentA || mol.compartment == compartmentB) {
            signatures.push_back(mol.signature);
        }
    }
    std::sort(signatures.begin(), signatures.end());
    signatures.erase(std::unique(signatures.begin(), signatures.end()), signatures.end());

    for (uint64_t sig : signatures) {
        double cA = getConcentration(compartmentA, sig);
        double cB = getConcentration(compartmentB, sig);
        double delta = (cA - cB) * d;

        setConcentration(compartmentA, sig, cA - delta);
        setConcentration(compartmentB, sig, cB + delta);
    }
}

void MolecularPool::clear() {
    pool_.clear();
}

} // namespace genomic
