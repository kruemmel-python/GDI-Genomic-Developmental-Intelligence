#include "runtime/genome_vm.hpp"
#include <algorithm>

namespace genomic {

GenomeVm::GenomeVm(SubstrateKernel* kernel,
                   MolecularPool* molecularPool,
                   HostCapability grantedCapabilities)
    : kernel_(kernel),
      molecularPool_(molecularPool),
      capabilities_(grantedCapabilities) {}

void GenomeVm::grantCapability(HostCapability cap) noexcept {
    capabilities_ = capabilities_ | cap;
}

void GenomeVm::revokeCapability(HostCapability cap) noexcept {
    capabilities_ = static_cast<HostCapability>(
        static_cast<uint32_t>(capabilities_) & ~static_cast<uint32_t>(cap)
    );
}

VmExecutionResult GenomeVm::execute(const IrModule& module,
                                   const std::string& entryFunction,
                                   const std::vector<VmValue>& arguments,
                                   uint64_t gasLimit) {
    const IrFunction* targetFn = nullptr;
    for (const auto& fn : module.functions) {
        if (fn.name == entryFunction) {
            targetFn = &fn;
            break;
        }
    }

    if (!targetFn && !module.functions.empty()) {
        targetFn = &module.functions.front();
    }

    if (!targetFn) {
        VmExecutionResult res;
        res.status = VmExecutionStatus::RuntimeError;
        res.errorMessage = "Entry function '" + entryFunction + "' not found in module";
        return res;
    }

    VmFrame frame;
    frame.functionName = targetFn->name;
    frame.currentBlockIndex = 0;
    frame.currentInstructionIndex = 0;

    // Load arguments into registers 1..N
    for (size_t i = 0; i < arguments.size(); ++i) {
        frame.registers[static_cast<IrRegister>(i + 1)] = arguments[i];
    }

    return runFrame(module, *targetFn, frame, gasLimit);
}

VmExecutionResult GenomeVm::resume(const IrModule& module, VmFrame& suspendedFrame, uint64_t gasLimit) {
    const IrFunction* targetFn = nullptr;
    for (const auto& fn : module.functions) {
        if (fn.name == suspendedFrame.functionName) {
            targetFn = &fn;
            break;
        }
    }

    if (!targetFn) {
        VmExecutionResult res;
        res.status = VmExecutionStatus::RuntimeError;
        res.errorMessage = "Function '" + suspendedFrame.functionName + "' not found to resume";
        return res;
    }

    suspendedFrame.isSuspended = false;
    // Step past the suspend instruction
    suspendedFrame.currentInstructionIndex++;

    return runFrame(module, *targetFn, suspendedFrame, gasLimit);
}

VmExecutionResult GenomeVm::runFrame(const IrModule& module, const IrFunction& fn, VmFrame& frame, uint64_t gasLimit) {
    (void)module;
    VmExecutionResult result;
    uint64_t gasRemaining = gasLimit;

    while (frame.currentBlockIndex < fn.blocks.size()) {
        const auto& block = fn.blocks[frame.currentBlockIndex];

        while (frame.currentInstructionIndex < block.instructions.size()) {
            if (gasRemaining == 0 || (kernel_ && !kernel_->consumeEnergy(1))) {
                result.status = VmExecutionStatus::GasExhausted;
                result.errorMessage = "Execution gas limit exhausted";
                return result;
            }
            gasRemaining--;
            result.cyclesExecuted++;

            const auto& inst = block.instructions[frame.currentInstructionIndex];
            result.executionTrace.push_back(inst.toString());

            switch (inst.opcode) {
                case IrOpcode::Nop:
                    break;
                case IrOpcode::ConstInt:
                    frame.registers[inst.destReg] = VmValue::makeInt(inst.immediateInt);
                    break;
                case IrOpcode::ConstFloat:
                    frame.registers[inst.destReg] = VmValue::makeFloat(inst.immediateFloat);
                    break;
                case IrOpcode::ConstBool:
                    frame.registers[inst.destReg] = VmValue::makeBool(inst.immediateInt != 0);
                    break;
                case IrOpcode::ConstString:
                    frame.registers[inst.destReg] = VmValue::makeString(inst.immediateStr);
                    break;
                case IrOpcode::Add: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    if (v1.kind == VmValue::Kind::Float || v2.kind == VmValue::Kind::Float) {
                        double f1 = (v1.kind == VmValue::Kind::Float) ? v1.fVal : static_cast<double>(v1.iVal);
                        double f2 = (v2.kind == VmValue::Kind::Float) ? v2.fVal : static_cast<double>(v2.iVal);
                        frame.registers[inst.destReg] = VmValue::makeFloat(f1 + f2);
                    } else {
                        frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal + v2.iVal);
                    }
                    break;
                }
                case IrOpcode::Sub: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    if (v1.kind == VmValue::Kind::Float || v2.kind == VmValue::Kind::Float) {
                        double f1 = (v1.kind == VmValue::Kind::Float) ? v1.fVal : static_cast<double>(v1.iVal);
                        double f2 = (v2.kind == VmValue::Kind::Float) ? v2.fVal : static_cast<double>(v2.iVal);
                        frame.registers[inst.destReg] = VmValue::makeFloat(f1 - f2);
                    } else {
                        frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal - v2.iVal);
                    }
                    break;
                }
                case IrOpcode::Mul: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    if (v1.kind == VmValue::Kind::Float || v2.kind == VmValue::Kind::Float) {
                        double f1 = (v1.kind == VmValue::Kind::Float) ? v1.fVal : static_cast<double>(v1.iVal);
                        double f2 = (v2.kind == VmValue::Kind::Float) ? v2.fVal : static_cast<double>(v2.iVal);
                        frame.registers[inst.destReg] = VmValue::makeFloat(f1 * f2);
                    } else {
                        frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal * v2.iVal);
                    }
                    break;
                }
                case IrOpcode::Div: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    if (v1.kind == VmValue::Kind::Float || v2.kind == VmValue::Kind::Float) {
                        double f1 = (v1.kind == VmValue::Kind::Float) ? v1.fVal : static_cast<double>(v1.iVal);
                        double f2 = (v2.kind == VmValue::Kind::Float) ? v2.fVal : static_cast<double>(v2.iVal);
                        if (f2 == 0.0) {
                            result.status = VmExecutionStatus::DivisionByZero;
                            result.errorMessage = "Runtime error: Division by zero (floating point)";
                            return result;
                        }
                        frame.registers[inst.destReg] = VmValue::makeFloat(f1 / f2);
                    } else {
                        if (v2.iVal == 0) {
                            result.status = VmExecutionStatus::DivisionByZero;
                            result.errorMessage = "Runtime error: Division by zero (integer)";
                            return result;
                        }
                        frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal / v2.iVal);
                    }
                    break;
                }
                case IrOpcode::Mod: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    if (v2.iVal == 0) {
                        result.status = VmExecutionStatus::DivisionByZero;
                        result.errorMessage = "Runtime error: Modulo by zero";
                        return result;
                    }
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal % v2.iVal);
                    break;
                }
                case IrOpcode::BitAnd: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal & v2.iVal);
                    break;
                }
                case IrOpcode::BitOr: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal | v2.iVal);
                    break;
                }
                case IrOpcode::BitXor: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal ^ v2.iVal);
                    break;
                }
                case IrOpcode::BitShl: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal << v2.iVal);
                    break;
                }
                case IrOpcode::BitShr: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeInt(v1.iVal >> v2.iVal);
                    break;
                }
                case IrOpcode::Trap: {
                    result.status = VmExecutionStatus::RuntimeError;
                    result.errorMessage = "Genomic VM Trap instruction triggered";
                    return result;
                }
                case IrOpcode::CmpEQ: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal == v2.iVal);
                    break;
                }
                case IrOpcode::CmpNE: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal != v2.iVal);
                    break;
                }
                case IrOpcode::CmpLT: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal < v2.iVal);
                    break;
                }
                case IrOpcode::CmpLE: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal <= v2.iVal);
                    break;
                }
                case IrOpcode::CmpGT: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal > v2.iVal);
                    break;
                }
                case IrOpcode::CmpGE: {
                    auto v1 = frame.registers[inst.srcReg1];
                    auto v2 = frame.registers[inst.srcReg2];
                    frame.registers[inst.destReg] = VmValue::makeBool(v1.iVal >= v2.iVal);
                    break;
                }
                case IrOpcode::Branch: {
                    // Find block by label
                    size_t targetIdx = fn.blocks.size();
                    for (size_t b = 0; b < fn.blocks.size(); ++b) {
                        if (fn.blocks[b].label == inst.labelTarget) {
                            targetIdx = b;
                            break;
                        }
                    }
                    if (targetIdx < fn.blocks.size()) {
                        frame.currentBlockIndex = targetIdx;
                        frame.currentInstructionIndex = 0;
                        goto next_instruction_loop;
                    }
                    break;
                }
                case IrOpcode::BranchCond: {
                    auto condVal = frame.registers[inst.srcReg1];
                    bool take = (condVal.kind == VmValue::Kind::Bool) ? condVal.bVal : (condVal.iVal != 0);
                    if (take) {
                        size_t targetIdx = fn.blocks.size();
                        for (size_t b = 0; b < fn.blocks.size(); ++b) {
                            if (fn.blocks[b].label == inst.labelTarget) {
                                targetIdx = b;
                                break;
                            }
                        }
                        if (targetIdx < fn.blocks.size()) {
                            frame.currentBlockIndex = targetIdx;
                            frame.currentInstructionIndex = 0;
                            goto next_instruction_loop;
                        }
                    }
                    break;
                }
                case IrOpcode::Return:
                    result.returnValue = frame.registers[inst.srcReg1];
                    result.status = VmExecutionStatus::Success;
                    return result;

                case IrOpcode::Suspend:
                    frame.isSuspended = true;
                    result.returnValue = frame.registers[inst.srcReg1];
                    result.status = VmExecutionStatus::Suspended;
                    return result;

                case IrOpcode::EmitMolecule: {
                    if (!hasHostCapability(capabilities_, HostCapability::EmitMolecules)) {
                        result.status = VmExecutionStatus::CapabilityDenied;
                        result.errorMessage = "Capability violation: EmitMolecules denied";
                        return result;
                    }
                    uint64_t sig = static_cast<uint64_t>(inst.immediateInt);
                    double conc = inst.immediateFloat;
                    result.emittedMolecules.push_back({sig, conc});
                    if (molecularPool_) {
                        molecularPool_->addConcentration(0, sig, conc, "EmittedByProgram");
                    }
                    break;
                }
                case IrOpcode::ReadMolecule: {
                    uint64_t sig = static_cast<uint64_t>(inst.immediateInt);
                    double conc = molecularPool_ ? molecularPool_->getConcentration(0, sig) : 0.0;
                    frame.registers[inst.destReg] = VmValue::makeFloat(conc);
                    break;
                }
                case IrOpcode::CheckCapability: {
                    auto capVal = static_cast<HostCapability>(inst.immediateInt);
                    bool ok = hasHostCapability(capabilities_, capVal);
                    frame.registers[inst.destReg] = VmValue::makeBool(ok);
                    break;
                }
                case IrOpcode::Print:
                    // In-memory trace only
                    break;
                case IrOpcode::Halt:
                    result.status = VmExecutionStatus::Success;
                    return result;
                default:
                    break;
            }

            frame.currentInstructionIndex++;
        }

        frame.currentBlockIndex++;
        frame.currentInstructionIndex = 0;

        next_instruction_loop:;
    }

    result.status = VmExecutionStatus::Success;
    return result;
}

} // namespace genomic
