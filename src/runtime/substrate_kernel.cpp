#include "runtime/substrate_kernel.hpp"
#include <cstring>
#include <algorithm>

namespace genomic {

SubstrateKernel::SubstrateKernel(size_t physicalMemoryBytes,
                                 uint64_t initialEnergyBudget,
                                 uint64_t rngSeed)
    : physicalMemory_(physicalMemoryBytes, 0),
      clockTicks_(0),
      currentEnergy_(initialEnergyBudget),
      maxEnergy_(initialEnergyBudget),
      activeCapabilities_(CapabilityType::MemoryAccess | CapabilityType::SensorInput),
      prngState_(rngSeed != 0 ? rngSeed : 133742) {}

uint64_t SubstrateKernel::tick() {
    return ++clockTicks_;
}

uint64_t SubstrateKernel::currentTick() const noexcept {
    return clockTicks_;
}

void SubstrateKernel::resetClock(uint64_t tickValue) noexcept {
    clockTicks_ = tickValue;
}

size_t SubstrateKernel::memorySize() const noexcept {
    return physicalMemory_.size();
}

uint8_t SubstrateKernel::readByte(size_t address) const {
    if (!checkCapability(CapabilityType::MemoryAccess)) {
        throw std::runtime_error("Capability violation: MemoryAccess denied");
    }
    if (address >= physicalMemory_.size()) {
        throw std::out_of_range("Memory read out of bounds at address " + std::to_string(address));
    }
    return physicalMemory_[address];
}

void SubstrateKernel::writeByte(size_t address, uint8_t value) {
    if (!checkCapability(CapabilityType::MemoryAccess)) {
        throw std::runtime_error("Capability violation: MemoryAccess denied");
    }
    if (address >= physicalMemory_.size()) {
        throw std::out_of_range("Memory write out of bounds at address " + std::to_string(address));
    }
    physicalMemory_[address] = value;
}

void SubstrateKernel::readBuffer(size_t address, uint8_t* destination, size_t count) const {
    if (!destination) {
        throw std::invalid_argument("Destination buffer is null");
    }
    if (!checkCapability(CapabilityType::MemoryAccess)) {
        throw std::runtime_error("Capability violation: MemoryAccess denied");
    }
    if (address + count > physicalMemory_.size() || address + count < address) {
        throw std::out_of_range("Memory buffer read out of bounds");
    }
    std::memcpy(destination, physicalMemory_.data() + address, count);
}

void SubstrateKernel::writeBuffer(size_t address, const uint8_t* source, size_t count) {
    if (!source) {
        throw std::invalid_argument("Source buffer is null");
    }
    if (!checkCapability(CapabilityType::MemoryAccess)) {
        throw std::runtime_error("Capability violation: MemoryAccess denied");
    }
    if (address + count > physicalMemory_.size() || address + count < address) {
        throw std::out_of_range("Memory buffer write out of bounds");
    }
    std::memcpy(physicalMemory_.data() + address, source, count);
}

bool SubstrateKernel::consumeEnergy(uint64_t gasCost) {
    if (gasCost > currentEnergy_) {
        currentEnergy_ = 0;
        return false;
    }
    currentEnergy_ -= gasCost;
    return true;
}

void SubstrateKernel::rechargeEnergy(uint64_t amount) {
    currentEnergy_ = std::min(maxEnergy_, currentEnergy_ + amount);
}

uint64_t SubstrateKernel::remainingEnergy() const noexcept {
    return currentEnergy_;
}

uint64_t SubstrateKernel::maxEnergy() const noexcept {
    return maxEnergy_;
}

void SubstrateKernel::grantCapability(CapabilityType cap) noexcept {
    activeCapabilities_ = activeCapabilities_ | cap;
}

void SubstrateKernel::revokeCapability(CapabilityType cap) noexcept {
    activeCapabilities_ = static_cast<CapabilityType>(
        static_cast<uint32_t>(activeCapabilities_) & ~static_cast<uint32_t>(cap)
    );
}

bool SubstrateKernel::checkCapability(CapabilityType cap) const noexcept {
    return hasCapability(activeCapabilities_, cap);
}

CapabilityType SubstrateKernel::currentCapabilities() const noexcept {
    return activeCapabilities_;
}

uint64_t SubstrateKernel::nextRandom() {
    // 64-bit XorShift deterministic PRNG
    uint64_t x = prngState_;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    prngState_ = x;
    return x;
}

double SubstrateKernel::nextRandomDouble() {
    // Uniform in [0.0, 1.0)
    return static_cast<double>(nextRandom() >> 11) * (1.0 / 9007199254740992.0);
}

void SubstrateKernel::reseed(uint64_t seed) noexcept {
    prngState_ = (seed != 0 ? seed : 133742);
}

uint32_t SubstrateKernel::crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
    }
    return ~crc;
}

uint64_t SubstrateKernel::fnv1a64(const uint8_t* data, size_t length) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace genomic
