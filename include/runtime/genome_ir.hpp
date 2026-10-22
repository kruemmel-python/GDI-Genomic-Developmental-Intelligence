#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_ast.hpp"

namespace genomic {

enum class IrOpcode : uint8_t {
    Nop,
    ConstInt,
    ConstFloat,
    ConstBool,
    ConstString,
    Alloca,
    Load,
    Store,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    CmpEQ,
    CmpNE,
    CmpLT,
    CmpLE,
    CmpGT,
    CmpGE,
    Branch,
    BranchCond,
    Call,
    Return,
    Suspend,
    Resume,
    EmitMolecule,
    ReadMolecule,
    CheckCapability,
    Print,
    BitAnd,
    BitOr,
    BitXor,
    BitShl,
    BitShr,
    Trap,
    Halt
};

inline std::string irOpcodeToString(IrOpcode op) {
    switch (op) {
        case IrOpcode::Nop: return "nop";
        case IrOpcode::ConstInt: return "const.i64";
        case IrOpcode::ConstFloat: return "const.f64";
        case IrOpcode::ConstBool: return "const.bool";
        case IrOpcode::ConstString: return "const.str";
        case IrOpcode::Alloca: return "alloca";
        case IrOpcode::Load: return "load";
        case IrOpcode::Store: return "store";
        case IrOpcode::Add: return "add";
        case IrOpcode::Sub: return "sub";
        case IrOpcode::Mul: return "mul";
        case IrOpcode::Div: return "div";
        case IrOpcode::Mod: return "mod";
        case IrOpcode::BitAnd: return "bit.and";
        case IrOpcode::BitOr: return "bit.or";
        case IrOpcode::BitXor: return "bit.xor";
        case IrOpcode::BitShl: return "bit.shl";
        case IrOpcode::BitShr: return "bit.shr";
        case IrOpcode::Trap: return "trap";
        case IrOpcode::CmpEQ: return "cmp.eq";
        case IrOpcode::CmpNE: return "cmp.ne";
        case IrOpcode::CmpLT: return "cmp.lt";
        case IrOpcode::CmpLE: return "cmp.le";
        case IrOpcode::CmpGT: return "cmp.gt";
        case IrOpcode::CmpGE: return "cmp.ge";
        case IrOpcode::Branch: return "br";
        case IrOpcode::BranchCond: return "br.cond";
        case IrOpcode::Call: return "call";
        case IrOpcode::Return: return "ret";
        case IrOpcode::Suspend: return "suspend";
        case IrOpcode::Resume: return "resume";
        case IrOpcode::EmitMolecule: return "emit.mol";
        case IrOpcode::ReadMolecule: return "read.mol";
        case IrOpcode::CheckCapability: return "check.cap";
        case IrOpcode::Print: return "print";
        case IrOpcode::Halt: return "halt";
    }
    return "unknown";
}

using IrRegister = uint32_t;

struct IrInstruction {
    IrOpcode opcode{IrOpcode::Nop};
    IrRegister destReg{0};
    IrRegister srcReg1{0};
    IrRegister srcReg2{0};
    int64_t immediateInt{0};
    double immediateFloat{0.0};
    std::string immediateStr;
    std::string labelTarget;
    GenomeProvenance provenance;

    std::string toString() const;
};

struct BasicBlock {
    std::string label;
    std::vector<IrInstruction> instructions;
};

struct IrFunction {
    std::string name;
    std::shared_ptr<CppType> returnType;
    std::vector<std::pair<std::string, std::shared_ptr<CppType>>> parameters;
    std::vector<BasicBlock> blocks;
    uint32_t registerCount{0};
    GenomeProvenance provenance;
};

struct IrModule {
    std::string name;
    std::vector<IrFunction> functions;
    GenomeRegionId sourceRegion{0};
    uint64_t irHash{0};

    uint64_t computeHash() const;
    std::string dump() const;
};

} // namespace genomic
