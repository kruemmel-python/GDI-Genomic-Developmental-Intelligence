#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <chrono>

namespace genomic {

enum class CapabilityType : uint32_t {
    None           = 0,
    MemoryAccess   = 1 << 0,
    FileIO         = 1 << 1,
    NetworkIO      = 1 << 2,
    SensorInput    = 1 << 3,
    ActuatorOutput = 1 << 4,
    NativeBridge   = 1 << 5,
    All            = 0xFFFFFFFF
};

inline CapabilityType operator|(CapabilityType a, CapabilityType b) {
    return static_cast<CapabilityType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CapabilityType operator&(CapabilityType a, CapabilityType b) {
    return static_cast<CapabilityType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasCapability(CapabilityType mask, CapabilityType cap) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(cap)) == static_cast<uint32_t>(cap);
}

class SubstrateKernel {
public:
    explicit SubstrateKernel(size_t physicalMemoryBytes = 1024 * 1024,
                             uint64_t initialEnergyBudget = 1000000,
                             uint64_t rngSeed = 133742);

    // Deterministic Clock
    uint64_t tick();
    uint64_t currentTick() const noexcept;
    void resetClock(uint64_t tickValue = 0) noexcept;

    // Physical Memory Abstraction
    size_t memorySize() const noexcept;
    uint8_t readByte(size_t address) const;
    void writeByte(size_t address, uint8_t value);
    void readBuffer(size_t address, uint8_t* destination, size_t count) const;
    void writeBuffer(size_t address, const uint8_t* source, size_t count);

    // Resource and Gas Accounting
    bool consumeEnergy(uint64_t gasCost);
    void rechargeEnergy(uint64_t amount);
    uint64_t remainingEnergy() const noexcept;
    uint64_t maxEnergy() const noexcept;

    // Capability Sandbox
    void grantCapability(CapabilityType cap) noexcept;
    void revokeCapability(CapabilityType cap) noexcept;
    bool checkCapability(CapabilityType cap) const noexcept;
    CapabilityType currentCapabilities() const noexcept;

    // Deterministic PRNG
    uint64_t nextRandom();
    double nextRandomDouble(); // [0.0, 1.0)
    void reseed(uint64_t seed) noexcept;

    // Elementary bitwise & arithmetic operations for Stage-0
    static uint32_t crc32(const uint8_t* data, size_t length);
    static uint64_t fnv1a64(const uint8_t* data, size_t length);

private:
    std::vector<uint8_t> physicalMemory_;
    uint64_t clockTicks_{0};
    uint64_t currentEnergy_{1000000};
    uint64_t maxEnergy_{1000000};
    CapabilityType activeCapabilities_{CapabilityType::MemoryAccess | CapabilityType::SensorInput};
    uint64_t prngState_{133742};
};

} // namespace genomic
