#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "runtime/genome_ir.hpp"
#include "runtime/substrate_kernel.hpp"
#include "runtime/capability.hpp"
#include "regulation/molecular_pool.hpp"

namespace genomic {

enum class VmExecutionStatus : uint8_t {
    Success,
    Suspended,
    GasExhausted,
    DivisionByZero,
    CapabilityDenied,
    RuntimeError
};

struct VmValue {
    enum class Kind : uint8_t { Int, Float, Bool, String } kind{Kind::Int};
    int64_t iVal{0};
    double fVal{0.0};
    bool bVal{false};
    std::string sVal;

    static VmValue makeInt(int64_t v) { VmValue val; val.kind = Kind::Int; val.iVal = v; return val; }
    static VmValue makeFloat(double v) { VmValue val; val.kind = Kind::Float; val.fVal = v; return val; }
    static VmValue makeBool(bool v) { VmValue val; val.kind = Kind::Bool; val.bVal = v; return val; }
    static VmValue makeString(const std::string& v) { VmValue val; val.kind = Kind::String; val.sVal = v; return val; }
};

struct VmExecutionResult {
    VmExecutionStatus status{VmExecutionStatus::Success};
    VmValue returnValue{VmValue::makeInt(0)};
    uint64_t cyclesExecuted{0};
    std::vector<std::pair<uint64_t, double>> emittedMolecules; // (sig, conc)
    std::vector<std::string> executionTrace;
    std::string errorMessage;
};

// VM execution frame for functions and coroutine suspensions
struct VmFrame {
    std::string functionName;
    size_t currentBlockIndex{0};
    size_t currentInstructionIndex{0};
    std::unordered_map<IrRegister, VmValue> registers;
    bool isSuspended{false};
};

class GenomeVm {
public:
    explicit GenomeVm(SubstrateKernel* kernel,
                      MolecularPool* molecularPool = nullptr,
                      HostCapability grantedCapabilities = HostCapability::MemoryAccess | HostCapability::EmitMolecules);

    VmExecutionResult execute(const IrModule& module,
                             const std::string& entryFunction = "main",
                             const std::vector<VmValue>& arguments = {},
                             uint64_t gasLimit = 100000);

    VmExecutionResult resume(const IrModule& module, VmFrame& suspendedFrame, uint64_t gasLimit = 100000);

    void grantCapability(HostCapability cap) noexcept;
    void revokeCapability(HostCapability cap) noexcept;

private:
    SubstrateKernel* kernel_;
    MolecularPool* molecularPool_;
    HostCapability capabilities_;

    VmExecutionResult runFrame(const IrModule& module, const IrFunction& fn, VmFrame& frame, uint64_t gasLimit);
};

} // namespace genomic
