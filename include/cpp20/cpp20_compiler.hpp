#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "cpp20/cpp20_ast.hpp"
#include "runtime/genome_ir.hpp"

namespace genomic {

class GenomicRuleEngine;

class GenomeCompiler {
public:
    explicit GenomeCompiler(const GenomicRuleEngine* ruleEngine = nullptr);

    void setRuleEngine(const GenomicRuleEngine* ruleEngine) noexcept { ruleEngine_ = ruleEngine; }

    // Compiles validated C++20 AST into Typed Genome IR Module
    std::shared_ptr<IrModule> compile(const AstNode& root, const std::string& moduleName = "GenomicModule");

private:
    const GenomicRuleEngine* ruleEngine_{nullptr};
    std::shared_ptr<IrModule> currentModule_;
    IrFunction* currentFunction_{nullptr};
    BasicBlock* currentBlock_{nullptr};
    uint32_t nextRegister_{1};
    uint32_t nextLabelId_{1};

    std::unordered_map<std::string, IrRegister> varToRegMap_;
    std::vector<std::string> breakTargetStack_;
    std::vector<std::string> continueTargetStack_;

    IrRegister allocateRegister();
    std::string generateLabel(const std::string& prefix = "bb");
    void emit(IrInstruction inst);

    void compileNode(const AstNode& node);
    void compileFunction(const AstNode& node);
    IrRegister compileExpression(const AstNode& expr);
};

} // namespace genomic
