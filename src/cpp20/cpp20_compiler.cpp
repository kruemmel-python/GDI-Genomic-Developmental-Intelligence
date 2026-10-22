#include "cpp20/cpp20_compiler.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <stdexcept>

namespace genomic {

GenomeCompiler::GenomeCompiler(const GenomicRuleEngine* ruleEngine)
    : ruleEngine_(ruleEngine) {}

IrRegister GenomeCompiler::allocateRegister() {
    return nextRegister_++;
}

std::string GenomeCompiler::generateLabel(const std::string& prefix) {
    return prefix + "_" + std::to_string(nextLabelId_++);
}

void GenomeCompiler::emit(IrInstruction inst) {
    if (currentBlock_) {
        currentBlock_->instructions.push_back(std::move(inst));
    }
}

std::shared_ptr<IrModule> GenomeCompiler::compile(const AstNode& root, const std::string& moduleName) {
    currentModule_ = std::make_shared<IrModule>();
    currentModule_->name = moduleName;
    nextRegister_ = 1;
    nextLabelId_ = 1;

    compileNode(root);

    currentModule_->irHash = currentModule_->computeHash();
    return currentModule_;
}

void GenomeCompiler::compileNode(const AstNode& node) {
    switch (node.kind) {
        case AstNodeKind::TranslationUnit:
            for (const auto& child : node.children) {
                if (child) compileNode(*child);
            }
            break;
        case AstNodeKind::FunctionDecl:
            compileFunction(node);
            break;
        case AstNodeKind::VariableDecl: {
            IrRegister r = allocateRegister();
            varToRegMap_[node.name] = r;
            if (!node.children.empty() && node.children[0]) {
                IrRegister initReg = compileExpression(*node.children[0]);
                IrInstruction inst;
                inst.opcode = IrOpcode::Add; // r = initReg + 0
                inst.destReg = r;
                inst.srcReg1 = initReg;
                inst.srcReg2 = 0;
                inst.provenance = node.provenance;
                emit(inst);
            }
            break;
        }
        case AstNodeKind::CompoundStmt:
            for (const auto& child : node.children) {
                if (child) compileNode(*child);
            }
            break;
        case AstNodeKind::IfStmt: {
            if (node.children.size() < 2 || !node.children[0] || !node.children[1]) break;
            IrRegister condReg = compileExpression(*node.children[0]);

            std::string thenLabel = generateLabel("then");
            std::string elseLabel = generateLabel("else");
            std::string endLabel = generateLabel("ifend");

            IrInstruction brCond;
            brCond.opcode = IrOpcode::BranchCond;
            brCond.srcReg1 = condReg;
            brCond.labelTarget = thenLabel;
            brCond.provenance = node.provenance;
            emit(brCond);

            // Fallthrough to else or end
            IrInstruction brElse;
            brElse.opcode = IrOpcode::Branch;
            brElse.labelTarget = (node.children.size() > 2 && node.children[2]) ? elseLabel : endLabel;
            brElse.provenance = node.provenance;
            emit(brElse);

            // Then block
            currentFunction_->blocks.push_back(BasicBlock{thenLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            compileNode(*node.children[1]);
            IrInstruction brEnd;
            brEnd.opcode = IrOpcode::Branch;
            brEnd.labelTarget = endLabel;
            emit(brEnd);

            // Else block
            if (node.children.size() > 2 && node.children[2]) {
                currentFunction_->blocks.push_back(BasicBlock{elseLabel, {}});
                currentBlock_ = &currentFunction_->blocks.back();
                compileNode(*node.children[2]);
                IrInstruction brEnd2;
                brEnd2.opcode = IrOpcode::Branch;
                brEnd2.labelTarget = endLabel;
                emit(brEnd2);
            }

            // End block
            currentFunction_->blocks.push_back(BasicBlock{endLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            break;
        }
        case AstNodeKind::ReturnStmt: {
            IrInstruction ret;
            ret.opcode = IrOpcode::Return;
            ret.provenance = node.provenance;
            if (!node.children.empty() && node.children[0]) {
                ret.srcReg1 = compileExpression(*node.children[0]);
            }
            emit(ret);
            break;
        }
        case AstNodeKind::EmitMoleculeExpr: {
            IrInstruction em;
            em.opcode = IrOpcode::EmitMolecule;
            em.immediateInt = node.intVal;
            em.immediateFloat = node.floatVal;
            em.provenance = node.provenance;
            emit(em);
            break;
        }
        case AstNodeKind::CoroutineExpr: {
            IrInstruction susp;
            susp.opcode = IrOpcode::Suspend;
            susp.provenance = node.provenance;
            if (!node.children.empty() && node.children[0]) {
                susp.srcReg1 = compileExpression(*node.children[0]);
            }
            emit(susp);
            break;
        }
        default:
            compileExpression(node);
            break;
    }
}

void GenomeCompiler::compileFunction(const AstNode& node) {
    IrFunction fn;
    fn.name = node.name;
    fn.returnType = node.evaluatedType;
    fn.provenance = node.provenance;

    size_t paramCount = (node.children.empty() ? 0 : node.children.size() - 1);
    for (size_t i = 0; i < paramCount; ++i) {
        const auto& p = node.children[i];
        fn.parameters.push_back({p->name, p->evaluatedType});
    }

    currentModule_->functions.push_back(std::move(fn));
    currentFunction_ = &currentModule_->functions.back();

    varToRegMap_.clear();

    // Entry block
    currentFunction_->blocks.push_back(BasicBlock{"entry", {}});
    currentBlock_ = &currentFunction_->blocks.back();

    // Map parameters to registers
    for (size_t i = 0; i < paramCount; ++i) {
        IrRegister reg = allocateRegister();
        varToRegMap_[currentFunction_->parameters[i].first] = reg;
    }

    if (!node.children.empty() && node.children.back()) {
        compileNode(*node.children.back());
    }

    // Ensure terminal return in block if missing
    if (currentBlock_->instructions.empty() ||
        (currentBlock_->instructions.back().opcode != IrOpcode::Return &&
         currentBlock_->instructions.back().opcode != IrOpcode::Halt)) {
        IrInstruction ret;
        ret.opcode = IrOpcode::Return;
        ret.provenance = node.provenance;
        emit(ret);
    }

    currentFunction_->registerCount = nextRegister_;
}

IrRegister GenomeCompiler::compileExpression(const AstNode& expr) {
    IrRegister outReg = allocateRegister();

    switch (expr.kind) {
        case AstNodeKind::LiteralExpr: {
            IrInstruction inst;
            inst.destReg = outReg;
            inst.provenance = expr.provenance;
            if (expr.evaluatedType && expr.evaluatedType->isFloatingPoint()) {
                inst.opcode = IrOpcode::ConstFloat;
                inst.immediateFloat = expr.floatVal;
            } else if (expr.evaluatedType && expr.evaluatedType->kind == TypeKind::Bool) {
                inst.opcode = IrOpcode::ConstBool;
                inst.immediateInt = expr.boolVal ? 1 : 0;
            } else if (expr.evaluatedType && expr.evaluatedType->isPointer()) {
                inst.opcode = IrOpcode::ConstString;
                inst.immediateStr = expr.strVal;
            } else {
                inst.opcode = IrOpcode::ConstInt;
                inst.immediateInt = expr.intVal;
            }
            emit(inst);
            return outReg;
        }
        case AstNodeKind::IdentifierExpr: {
            auto it = varToRegMap_.find(expr.name);
            if (it != varToRegMap_.end()) {
                return it->second;
            }
            // Fallback: const 0
            IrInstruction inst;
            inst.opcode = IrOpcode::ConstInt;
            inst.destReg = outReg;
            inst.immediateInt = 0;
            inst.provenance = expr.provenance;
            emit(inst);
            return outReg;
        }
        case AstNodeKind::BinaryExpr: {
            if (expr.op == "=") {
                IrRegister rRight = compileExpression(*expr.children[1]);
                if (!expr.children.empty() && expr.children[0]->kind == AstNodeKind::IdentifierExpr) {
                    varToRegMap_[expr.children[0]->name] = rRight;
                }
                return rRight;
            }
            IrRegister rLeft = compileExpression(*expr.children[0]);
            IrRegister rRight = compileExpression(*expr.children[1]);

            IrInstruction inst;
            inst.destReg = outReg;
            inst.srcReg1 = rLeft;
            inst.srcReg2 = rRight;
            inst.provenance = expr.provenance;

            // Query GenomicRuleEngine loaded from the immutable genome strand!
            IrOpcode opCode = IrOpcode::Nop;
            if (ruleEngine_) {
                opCode = ruleEngine_->getLoweringOpcode("binop:" + expr.op);
                if (opCode == IrOpcode::Nop) {
                    TypeKind t1 = (expr.children[0] && expr.children[0]->evaluatedType) ? expr.children[0]->evaluatedType->kind : TypeKind::Int64;
                    TypeKind t2 = (expr.children[1] && expr.children[1]->evaluatedType) ? expr.children[1]->evaluatedType->kind : TypeKind::Int64;
                    auto rule = ruleEngine_->resolveBinaryOp(expr.op, t1, t2);
                    if (rule.has_value()) {
                        opCode = rule->loweringOpcode;
                    }
                }
            }

            // Genome mode must NEVER silently fall back to host operator semantics.
            // Legacy mode without a rule engine is retained for existing callers.
            if (opCode == IrOpcode::Nop && !ruleEngine_) {
                if (expr.op == "+") opCode = IrOpcode::Add;
                else if (expr.op == "-") opCode = IrOpcode::Sub;
                else if (expr.op == "*") opCode = IrOpcode::Mul;
                else if (expr.op == "/") opCode = IrOpcode::Div;
                else if (expr.op == "%") opCode = IrOpcode::Mod;
                else if (expr.op == "==") opCode = IrOpcode::CmpEQ;
                else if (expr.op == "!=") opCode = IrOpcode::CmpNE;
                else if (expr.op == "<") opCode = IrOpcode::CmpLT;
                else if (expr.op == "<=") opCode = IrOpcode::CmpLE;
                else if (expr.op == ">") opCode = IrOpcode::CmpGT;
                else if (expr.op == ">=") opCode = IrOpcode::CmpGE;
            }
            if (opCode == IrOpcode::Nop) {
                throw std::runtime_error("Missing genomic lowering rule for binary operator '" + expr.op + "'");
            }

            inst.opcode = opCode;
            emit(inst);
            return outReg;
        }
        case AstNodeKind::CallExpr: {
            if (expr.name == "emitMolecule") {
                IrInstruction inst;
                inst.opcode = IrOpcode::EmitMolecule;
                inst.provenance = expr.provenance;
                if (!expr.children.empty() && expr.children[0]->kind == AstNodeKind::LiteralExpr) {
                    inst.immediateInt = expr.children[0]->intVal;
                }
                if (expr.children.size() > 1 && expr.children[1]->kind == AstNodeKind::LiteralExpr) {
                    inst.immediateFloat = expr.children[1]->floatVal;
                }
                emit(inst);
                return outReg;
            }

            IrInstruction callInst;
            callInst.opcode = IrOpcode::Call;
            callInst.destReg = outReg;
            callInst.immediateStr = expr.name;
            callInst.provenance = expr.provenance;
            if (!expr.children.empty()) {
                callInst.srcReg1 = compileExpression(*expr.children[0]);
            }
            emit(callInst);
            return outReg;
        }
        case AstNodeKind::EmitMoleculeExpr: {
            IrInstruction em;
            em.opcode = IrOpcode::EmitMolecule;
            em.immediateInt = expr.intVal;
            em.immediateFloat = expr.floatVal;
            em.provenance = expr.provenance;
            emit(em);
            return outReg;
        }
        default:
            return outReg;
    }
}

} // namespace genomic
