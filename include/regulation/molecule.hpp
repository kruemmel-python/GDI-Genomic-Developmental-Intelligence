#pragma once

#include <cstdint>
#include <string>

namespace genomic {

enum class MoleculeClass : uint8_t {
    Signal        = 0,
    Activator     = 1,
    Repressor     = 2,
    Transport     = 3,
    Resource      = 4,
    Structural    = 5,
    Modifier      = 6,
    Degradation   = 7,
    Lineage       = 8,
    Development   = 9
};

namespace MoleculeSignatures {
    constexpr uint64_t EnergyLow        = 0x1001ull;
    constexpr uint64_t ThreatDetected   = 0x1002ull;
    constexpr uint64_t NutrientRich     = 0x1003ull;
    constexpr uint64_t PathBlocked      = 0x1004ull;
    constexpr uint64_t RecoverySignal   = 0x1005ull;
    constexpr uint64_t MaintenanceAlert = 0x1006ull;

    constexpr uint64_t LineageAnterior  = 0x2001ull;
    constexpr uint64_t LineagePosterior = 0x2002ull;
    constexpr uint64_t LineageMetabolic = 0x2003ull;

    constexpr uint64_t DevNeonatal      = 0x3001ull;
    constexpr uint64_t DevJuvenile      = 0x3002ull;
    constexpr uint64_t DevMature        = 0x3003ull;
}

struct DigitalMolecule {
    uint64_t signature{0};
    std::string name;
    MoleculeClass molClass{MoleculeClass::Signal};
    double concentration{0.0};   // [0.0, 1.0]
    double affinityScale{1.0};
    double halfLife{100.0};       // half life in ticks
    uint32_t compartment{0};

    bool operator==(const DigitalMolecule& other) const noexcept {
        return signature == other.signature && compartment == other.compartment;
    }
};

} // namespace genomic
