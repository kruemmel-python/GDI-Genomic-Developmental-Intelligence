#include "cpp20/cpp20_memory_model.hpp"
#include <climits>

namespace genomic {

void MemoryModelEngine::recordAccess(const MemoryAccessRecord& access) {
    accessHistory_.push_back(access);
}

bool MemoryModelEngine::checkDataRace(const MemoryAccessRecord& access, std::string& outRaceDescription) const {
    for (const auto& past : accessHistory_) {
        // Concurrency: different threads, overlapping memory addresses
        if (past.threadId != access.threadId) {
            uint64_t pastEnd = past.memoryAddress + past.sizeBytes;
            uint64_t currentEnd = access.memoryAddress + access.sizeBytes;

            bool overlap = (past.memoryAddress < currentEnd && access.memoryAddress < pastEnd);
            if (overlap && (past.isWrite || access.isWrite)) {
                // If neither access is atomic or synchronized, data race!
                if (past.memoryOrder == CppMemoryOrder::Relaxed || access.memoryOrder == CppMemoryOrder::Relaxed) {
                    outRaceDescription = "Data race detected between thread " + std::to_string(past.threadId) +
                                         " and thread " + std::to_string(access.threadId) +
                                         " at address 0x" + std::to_string(access.memoryAddress);
                    return true;
                }
            }
        }
    }
    return false;
}

void MemoryModelEngine::recordSynchronizationBarrier(uint64_t threadFrom, uint64_t threadTo, uint64_t timestamp) {
    (void)threadFrom;
    syncClockVector_[threadTo] = timestamp;
}

void UbGuard::registerAllocation(uint64_t address, size_t size) {
    allocations_[address] = AllocationInfo{size, false};
}

void UbGuard::registerFree(uint64_t address) {
    auto it = allocations_.find(address);
    if (it != allocations_.end()) {
        it->second.isFreed = true;
    }
}

UbReport UbGuard::verifyAccess(uint64_t address, size_t size, bool isWrite) {
    (void)isWrite;
    UbReport rep;
    if (address == 0) {
        rep.hasViolation = true;
        rep.kind = UbViolationKind::NullDereference;
        rep.description = "UB Guard: Null pointer dereference";
        rep.offendingAddress = address;
        return rep;
    }

    // Check if within any known allocation
    for (const auto& [base, info] : allocations_) {
        if (address >= base && address < base + info.size) {
            if (info.isFreed) {
                rep.hasViolation = true;
                rep.kind = UbViolationKind::UseAfterFree;
                rep.description = "UB Guard: Use-after-free at address " + std::to_string(address);
                rep.offendingAddress = address;
                return rep;
            }
            if (address + size > base + info.size) {
                rep.hasViolation = true;
                rep.kind = UbViolationKind::OutOfBounds;
                rep.description = "UB Guard: Out-of-bounds access past allocated buffer";
                rep.offendingAddress = address;
                return rep;
            }
            return rep; // Valid
        }
    }

    return rep;
}

UbReport UbGuard::verifyArithmetic(int64_t a, int64_t b, const std::string& op) {
    UbReport rep;
    if (op == "+") {
        if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
            rep.hasViolation = true;
            rep.kind = UbViolationKind::SignedOverflow;
            rep.description = "UB Guard: Signed integer overflow in addition";
        }
    } else if (op == "-") {
        if ((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b)) {
            rep.hasViolation = true;
            rep.kind = UbViolationKind::SignedOverflow;
            rep.description = "UB Guard: Signed integer overflow in subtraction";
        }
    } else if (op == "*") {
        if (a > 0 && b > 0 && a > LLONG_MAX / b) {
            rep.hasViolation = true;
            rep.kind = UbViolationKind::SignedOverflow;
            rep.description = "UB Guard: Signed integer overflow in multiplication";
        }
    }
    return rep;
}

UbReport UbGuard::verifyShift(int64_t value, int64_t shiftAmount) {
    (void)value;
    UbReport rep;
    if (shiftAmount < 0 || shiftAmount >= 64) {
        rep.hasViolation = true;
        rep.kind = UbViolationKind::InvalidShift;
        rep.description = "UB Guard: Invalid shift amount " + std::to_string(shiftAmount);
    }
    return rep;
}

} // namespace genomic
