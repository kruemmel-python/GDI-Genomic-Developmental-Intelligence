#include "runtime/genome_ir.hpp"
#include "runtime/substrate_kernel.hpp"
#include <sstream>

namespace genomic {

std::string IrInstruction::toString() const {
    std::ostringstream ss;
    ss << irOpcodeToString(opcode) << " ";

    switch (opcode) {
        case IrOpcode::ConstInt:
            ss << "r" << destReg << " = " << immediateInt;
            break;
        case IrOpcode::ConstFloat:
            ss << "r" << destReg << " = " << immediateFloat;
            break;
        case IrOpcode::ConstBool:
            ss << "r" << destReg << " = " << (immediateInt != 0 ? "true" : "false");
            break;
        case IrOpcode::ConstString:
            ss << "r" << destReg << " = \"" << immediateStr << "\"";
            break;
        case IrOpcode::Alloca:
            ss << "r" << destReg;
            break;
        case IrOpcode::Load:
            ss << "r" << destReg << " = *r" << srcReg1;
            break;
        case IrOpcode::Store:
            ss << "*r" << destReg << " = r" << srcReg1;
            break;
        case IrOpcode::Add:
        case IrOpcode::Sub:
        case IrOpcode::Mul:
        case IrOpcode::Div:
        case IrOpcode::Mod:
        case IrOpcode::CmpEQ:
        case IrOpcode::CmpNE:
        case IrOpcode::CmpLT:
        case IrOpcode::CmpLE:
        case IrOpcode::CmpGT:
        case IrOpcode::CmpGE:
            ss << "r" << destReg << " = r" << srcReg1 << ", r" << srcReg2;
            break;
        case IrOpcode::Branch:
            ss << labelTarget;
            break;
        case IrOpcode::BranchCond:
            ss << "r" << srcReg1 << " ? " << labelTarget << " : next";
            break;
        case IrOpcode::Call:
            ss << "r" << destReg << " = " << immediateStr << "()";
            break;
        case IrOpcode::Return:
            ss << "r" << srcReg1;
            break;
        case IrOpcode::EmitMolecule:
            ss << "sig=0x" << std::hex << immediateInt << std::dec << " conc=" << immediateFloat;
            break;
        case IrOpcode::ReadMolecule:
            ss << "r" << destReg << " = sig=0x" << std::hex << immediateInt << std::dec;
            break;
        case IrOpcode::Print:
            ss << "r" << srcReg1;
            break;
        default:
            break;
    }
    return ss.str();
}

uint64_t IrModule::computeHash() const {
    std::string text = dump();
    return SubstrateKernel::fnv1a64(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::string IrModule::dump() const {
    std::ostringstream ss;
    ss << "; Module: " << name << "\n";
    for (const auto& fn : functions) {
        ss << "define " << (fn.returnType ? fn.returnType->toString() : "void")
           << " @" << fn.name << "(";
        for (size_t i = 0; i < fn.parameters.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << fn.parameters[i].second->toString() << " %" << fn.parameters[i].first;
        }
        ss << ") {\n";
        for (const auto& bb : fn.blocks) {
            ss << bb.label << ":\n";
            for (const auto& inst : bb.instructions) {
                ss << "    " << inst.toString() << "\n";
            }
        }
        ss << "}\n\n";
    }
    return ss.str();
}

} // namespace genomic
