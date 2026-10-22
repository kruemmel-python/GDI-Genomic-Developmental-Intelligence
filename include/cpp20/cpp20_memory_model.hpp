#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace genomic {

enum class CppMemoryOrder : uint8_t {
    Relaxed,
    Consume,
    Acquire,
    Release,
    AcqRel,
    SeqCst
};

struct MemoryAccessRecord {
    uint64_t memoryAddress{0};
    size_t sizeBytes{0};
    bool isWrite{false};
    uint64_t threadId{0};
    uint64_t timestamp{0};
    CppMemoryOrder memoryOrder{CppMemoryOrder::SeqCst};
};

class MemoryModelEngine {
public:
    MemoryModelEngine() = default;

    void recordAccess(const MemoryAccessRecord& access);
    bool checkDataRace(const MemoryAccessRecord& access, std::string& outRaceDescription) const;

    // Synchronization barriers
    void recordSynchronizationBarrier(uint64_t threadFrom, uint64_t threadTo, uint64_t timestamp);

private:
    std::vector<MemoryAccessRecord> accessHistory_;
    std::unordered_map<uint64_t, uint64_t> syncClockVector_;
};

enum class UbViolationKind : uint16_t {
    None = 0,
    OutOfBounds,
    UseAfterFree,
    DoubleFree,
    NullDereference,
    UninitializedRead,
    SignedOverflow,
    InvalidShift,
    DanglingReference,
    DataRace
};

struct UbReport {
    bool hasViolation{false};
    UbViolationKind kind{UbViolationKind::None};
    std::string description;
    uint64_t offendingAddress{0};
};

class UbGuard {
public:
    UbGuard() = default;

    void registerAllocation(uint64_t address, size_t size);
    void registerFree(uint64_t address);

    UbReport verifyAccess(uint64_t address, size_t size, bool isWrite);
    UbReport verifyArithmetic(int64_t a, int64_t b, const std::string& op);
    UbReport verifyShift(int64_t value, int64_t shiftAmount);

private:
    struct AllocationInfo {
        size_t size{0};
        bool isFreed{false};
    };
    std::unordered_map<uint64_t, AllocationInfo> allocations_;
};

} // namespace genomic
