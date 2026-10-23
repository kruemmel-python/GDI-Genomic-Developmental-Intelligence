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
    breakTargetStack_.clear();
    continueTargetStack_.clear();

    compileNode(root);

    currentModule_->irHash = currentModule_->computeHash();
    return currentModule_;
}

void GenomeCompiler::compileNode(const AstNode& node) {
    switch (node.kind) {
        case AstNodeKind::TranslationUnit:
        case AstNodeKind::NamespaceDecl:
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
        case AstNodeKind::SwitchStmt: {
            size_t condIdx = node.boolVal ? 1 : 0;
            if (node.boolVal && !node.children.empty() && node.children[0]) {
                compileNode(*node.children[0]);
            }
            if (condIdx >= node.children.size() || !node.children[condIdx]) break;

            IrRegister switchCondReg = compileExpression(*node.children[condIdx]);
            std::string endSwitchLabel = generateLabel("switchend");
            breakTargetStack_.push_back(endSwitchLabel);

            size_t branchCount = node.children.size() - (condIdx + 1);
            std::vector<std::string> caseLabels(branchCount);
            std::string defaultLabel = endSwitchLabel;

            for (size_t i = 0; i < branchCount; ++i) {
                const auto& branch = node.children[condIdx + 1 + i];
                if (!branch) continue;
                if (branch->kind == AstNodeKind::DefaultStmt) {
                    caseLabels[i] = generateLabel("switchdefault");
                    defaultLabel = caseLabels[i];
                } else {
                    caseLabels[i] = generateLabel("switchcase");
                }
            }

            // Emit dispatch comparisons for each case
            for (size_t i = 0; i < branchCount; ++i) {
                const auto& branch = node.children[condIdx + 1 + i];
                if (!branch || branch->kind != AstNodeKind::CaseStmt) continue;
                if (branch->children.empty() || !branch->children[0]) continue;

                IrRegister caseValReg = compileExpression(*branch->children[0]);
                IrRegister cmpReg = allocateRegister();
                IrInstruction cmpInst;
                cmpInst.opcode = IrOpcode::CmpEQ;
                cmpInst.destReg = cmpReg;
                cmpInst.srcReg1 = switchCondReg;
                cmpInst.srcReg2 = caseValReg;
                cmpInst.provenance = branch->provenance;
                emit(cmpInst);

                IrInstruction brCase;
                brCase.opcode = IrOpcode::BranchCond;
                brCase.srcReg1 = cmpReg;
                brCase.labelTarget = caseLabels[i];
                brCase.provenance = branch->provenance;
                emit(brCase);
            }

            // If no case matched, jump to defaultLabel (or endSwitchLabel)
            IrInstruction brDefault;
            brDefault.opcode = IrOpcode::Branch;
            brDefault.labelTarget = defaultLabel;
            brDefault.provenance = node.provenance;
            emit(brDefault);

            // Emit basic blocks for each case/default body with C++ fallthrough semantics
            for (size_t i = 0; i < branchCount; ++i) {
                const auto& branch = node.children[condIdx + 1 + i];
                if (!branch) continue;

                currentFunction_->blocks.push_back(BasicBlock{caseLabels[i], {}});
                currentBlock_ = &currentFunction_->blocks.back();

                size_t firstStmt = (branch->kind == AstNodeKind::CaseStmt) ? 1 : 0;
                for (size_t s = firstStmt; s < branch->children.size(); ++s) {
                    if (branch->children[s]) {
                        compileNode(*branch->children[s]);
                    }
                }

                // If block does not end with Return or Branch (e.g. from break), fall through to next case or switchend
                bool terminated = !currentBlock_->instructions.empty() &&
                                  (currentBlock_->instructions.back().opcode == IrOpcode::Return ||
                                   currentBlock_->instructions.back().opcode == IrOpcode::Branch);
                if (!terminated) {
                    IrInstruction ftBranch;
                    ftBranch.opcode = IrOpcode::Branch;
                    ftBranch.labelTarget = (i + 1 < branchCount) ? caseLabels[i + 1] : endSwitchLabel;
                    emit(ftBranch);
                }
            }

            breakTargetStack_.pop_back();
            currentFunction_->blocks.push_back(BasicBlock{endSwitchLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            break;
        }
        case AstNodeKind::WhileStmt: {
            if (node.children.size() < 2 || !node.children[0] || !node.children[1]) break;
            std::string condLabel = generateLabel("whilecond");
            std::string bodyLabel = generateLabel("whilebody");
            std::string endLabel = generateLabel("whileend");

            IrInstruction brCond0;
            brCond0.opcode = IrOpcode::Branch;
            brCond0.labelTarget = condLabel;
            emit(brCond0);

            currentFunction_->blocks.push_back(BasicBlock{condLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            IrRegister condReg = compileExpression(*node.children[0]);
            IrInstruction brTake;
            brTake.opcode = IrOpcode::BranchCond;
            brTake.srcReg1 = condReg;
            brTake.labelTarget = bodyLabel;
            emit(brTake);
            IrInstruction brExit;
            brExit.opcode = IrOpcode::Branch;
            brExit.labelTarget = endLabel;
            emit(brExit);

            breakTargetStack_.push_back(endLabel);
            continueTargetStack_.push_back(condLabel);

            currentFunction_->blocks.push_back(BasicBlock{bodyLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            compileNode(*node.children[1]);
            IrInstruction brLoop;
            brLoop.opcode = IrOpcode::Branch;
            brLoop.labelTarget = condLabel;
            emit(brLoop);

            breakTargetStack_.pop_back();
            continueTargetStack_.pop_back();

            currentFunction_->blocks.push_back(BasicBlock{endLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            break;
        }
        case AstNodeKind::ForStmt: {
            if (node.children.size() < 4) break;
            if (node.children[0]) compileNode(*node.children[0]);

            std::string condLabel = generateLabel("forcond");
            std::string bodyLabel = generateLabel("forbody");
            std::string stepLabel = generateLabel("forstep");
            std::string endLabel = generateLabel("forend");

            IrInstruction brCond0;
            brCond0.opcode = IrOpcode::Branch;
            brCond0.labelTarget = condLabel;
            emit(brCond0);

            currentFunction_->blocks.push_back(BasicBlock{condLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            if (node.children[1]) {
                IrRegister condReg = compileExpression(*node.children[1]);
                IrInstruction brTake;
                brTake.opcode = IrOpcode::BranchCond;
                brTake.srcReg1 = condReg;
                brTake.labelTarget = bodyLabel;
                emit(brTake);
                IrInstruction brExit;
                brExit.opcode = IrOpcode::Branch;
                brExit.labelTarget = endLabel;
                emit(brExit);
            } else {
                IrInstruction brTake;
                brTake.opcode = IrOpcode::Branch;
                brTake.labelTarget = bodyLabel;
                emit(brTake);
            }

            breakTargetStack_.push_back(endLabel);
            continueTargetStack_.push_back(stepLabel);

            currentFunction_->blocks.push_back(BasicBlock{bodyLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            if (node.children[3]) compileNode(*node.children[3]);
            IrInstruction brStep;
            brStep.opcode = IrOpcode::Branch;
            brStep.labelTarget = stepLabel;
            emit(brStep);

            currentFunction_->blocks.push_back(BasicBlock{stepLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            if (node.children[2]) compileExpression(*node.children[2]);
            IrInstruction brLoop;
            brLoop.opcode = IrOpcode::Branch;
            brLoop.labelTarget = condLabel;
            emit(brLoop);

            breakTargetStack_.pop_back();
            continueTargetStack_.pop_back();

            currentFunction_->blocks.push_back(BasicBlock{endLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            break;
        }
        case AstNodeKind::BreakStmt: {
            if (!breakTargetStack_.empty()) {
                IrInstruction br;
                br.opcode = IrOpcode::Branch;
                br.labelTarget = breakTargetStack_.back();
                br.provenance = node.provenance;
                emit(br);
            }
            break;
        }
        case AstNodeKind::ContinueStmt: {
            if (!continueTargetStack_.empty()) {
                IrInstruction br;
                br.opcode = IrOpcode::Branch;
                br.labelTarget = continueTargetStack_.back();
                br.provenance = node.provenance;
                emit(br);
            }
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
    bool hasBody = !node.children.empty() && node.children.back() &&
                   (node.children.back()->kind == AstNodeKind::CompoundStmt ||
                    node.children.back()->kind == AstNodeKind::BinaryExpr ||
                    node.children.back()->kind == AstNodeKind::ReturnStmt ||
                    node.children.back()->kind == AstNodeKind::SwitchStmt ||
                    node.children.back()->kind == AstNodeKind::IfStmt);
    if (!hasBody && !node.children.empty() && node.children.back() &&
        node.children.back()->kind == AstNodeKind::VariableDecl) {
        return; // Prototype only
    }

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
        case AstNodeKind::UnaryExpr: {
            if (expr.children.empty() || !expr.children[0]) return outReg;
            IrRegister opndReg = compileExpression(*expr.children[0]);
            if (expr.op == "+") return opndReg;
            if (expr.op == "-") {
                IrRegister zeroReg = allocateRegister();
                IrInstruction zInst;
                zInst.opcode = IrOpcode::ConstInt;
                zInst.destReg = zeroReg;
                zInst.immediateInt = 0;
                emit(zInst);

                IrInstruction subInst;
                subInst.opcode = IrOpcode::Sub;
                subInst.destReg = outReg;
                subInst.srcReg1 = zeroReg;
                subInst.srcReg2 = opndReg;
                emit(subInst);
                return outReg;
            }
            if (expr.op == "++" || expr.op == "--") {
                IrRegister oneReg = allocateRegister();
                IrInstruction oneInst;
                oneInst.opcode = IrOpcode::ConstInt;
                oneInst.destReg = oneReg;
                oneInst.immediateInt = 1;
                emit(oneInst);

                IrInstruction stepInst;
                stepInst.opcode = (expr.op == "++") ? IrOpcode::Add : IrOpcode::Sub;
                stepInst.destReg = outReg;
                stepInst.srcReg1 = opndReg;
                stepInst.srcReg2 = oneReg;
                emit(stepInst);
                if (expr.children[0]->kind == AstNodeKind::IdentifierExpr) {
                    varToRegMap_[expr.children[0]->name] = outReg;
                }
                return expr.boolVal ? outReg : opndReg;
            }
            return opndReg;
        }
        case AstNodeKind::TernaryExpr: {
            if (expr.children.size() < 3) return outReg;
            IrRegister condReg = compileExpression(*expr.children[0]);
            std::string trueLabel = generateLabel("terntrue");
            std::string falseLabel = generateLabel("ternfalse");
            std::string endLabel = generateLabel("ternend");

            IrInstruction brCond;
            brCond.opcode = IrOpcode::BranchCond;
            brCond.srcReg1 = condReg;
            brCond.labelTarget = trueLabel;
            emit(brCond);

            IrInstruction brFalse;
            brFalse.opcode = IrOpcode::Branch;
            brFalse.labelTarget = falseLabel;
            emit(brFalse);

            currentFunction_->blocks.push_back(BasicBlock{trueLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            IrRegister rTrue = compileExpression(*expr.children[1]);
            IrInstruction movTrue;
            movTrue.opcode = IrOpcode::Add;
            movTrue.destReg = outReg;
            movTrue.srcReg1 = rTrue;
            movTrue.srcReg2 = 0;
            emit(movTrue);
            IrInstruction brEnd1;
            brEnd1.opcode = IrOpcode::Branch;
            brEnd1.labelTarget = endLabel;
            emit(brEnd1);

            currentFunction_->blocks.push_back(BasicBlock{falseLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            IrRegister rFalse = compileExpression(*expr.children[2]);
            IrInstruction movFalse;
            movFalse.opcode = IrOpcode::Add;
            movFalse.destReg = outReg;
            movFalse.srcReg1 = rFalse;
            movFalse.srcReg2 = 0;
            emit(movFalse);
            IrInstruction brEnd2;
            brEnd2.opcode = IrOpcode::Branch;
            brEnd2.labelTarget = endLabel;
            emit(brEnd2);

            currentFunction_->blocks.push_back(BasicBlock{endLabel, {}});
            currentBlock_ = &currentFunction_->blocks.back();
            return outReg;
        }
        case AstNodeKind::BinaryExpr: {
            if (expr.op == "=") {
                IrRegister rRight = compileExpression(*expr.children[1]);
                if (!expr.children.empty() && expr.children[0]->kind == AstNodeKind::IdentifierExpr) {
                    const std::string& varName = expr.children[0]->name;
                    auto it = varToRegMap_.find(varName);
                    if (it != varToRegMap_.end()) {
                        // Emit into existing register so dominance across switch/if blocks holds!
                        IrInstruction mov;
                        mov.opcode = IrOpcode::Add;
                        mov.destReg = it->second;
                        mov.srcReg1 = rRight;
                        mov.srcReg2 = 0;
                        mov.provenance = expr.provenance;
                        emit(mov);
                        return it->second;
                    }
                    varToRegMap_[varName] = rRight;
                }
                return rRight;
            }
            if (expr.op == "+=" || expr.op == "-=" || expr.op == "*=" || expr.op == "/=") {
                IrRegister rLeft = compileExpression(*expr.children[0]);
                IrRegister rRight = compileExpression(*expr.children[1]);
                IrInstruction opInst;
                opInst.destReg = rLeft;
                opInst.srcReg1 = rLeft;
                opInst.srcReg2 = rRight;
                if (expr.op == "+=") opInst.opcode = IrOpcode::Add;
                else if (expr.op == "-=") opInst.opcode = IrOpcode::Sub;
                else if (expr.op == "*=") opInst.opcode = IrOpcode::Mul;
                else opInst.opcode = IrOpcode::Div;
                emit(opInst);
                return rLeft;
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
                else if (expr.op == "&" || expr.op == "&&") opCode = IrOpcode::BitAnd;
                else if (expr.op == "|" || expr.op == "||") opCode = IrOpcode::BitOr;
                else if (expr.op == "^") opCode = IrOpcode::BitXor;
                else if (expr.op == "<<") opCode = IrOpcode::BitShl;
                else if (expr.op == ">>") opCode = IrOpcode::BitShr;
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
