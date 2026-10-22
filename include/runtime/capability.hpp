#pragma once

#include <cstdint>
#include <string>

namespace genomic {

enum class HostCapability : uint32_t {
    None           = 0,
    MemoryAccess   = 1 << 0,
    FileIO         = 1 << 1,
    NetworkIO      = 1 << 2,
    SensorInput    = 1 << 3,
    ActuatorOutput = 1 << 4,
    NativeBridge   = 1 << 5,
    EmitMolecules  = 1 << 6
};

inline HostCapability operator|(HostCapability a, HostCapability b) {
    return static_cast<HostCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline HostCapability operator&(HostCapability a, HostCapability b) {
    return static_cast<HostCapability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasHostCapability(HostCapability mask, HostCapability cap) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(cap)) == static_cast<uint32_t>(cap);
}

} // namespace genomic
