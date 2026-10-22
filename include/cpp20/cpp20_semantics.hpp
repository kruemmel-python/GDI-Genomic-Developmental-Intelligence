#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_types.hpp"

namespace genomic {

class GenomicRuleEngine;

struct Symbol {
    std::string name;
    std::shared_ptr<CppType> type;
    bool isConstexpr{false};
    bool isInitialized{false};
    int64_t constexprIntVal{0};
};

struct FunctionSignature {
    std::string name;
    std::shared_ptr<CppType> returnType;
    std::vector<std::shared_ptr<CppType>> paramTypes;
    bool isConstexpr{false};
};

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr) : parent_(parent) {}

    bool declare(const Symbol& sym);
    std::optional<Symbol> lookup(const std::string& name) const;
    bool isDeclaredLocally(const std::string& name) const;

    std::shared_ptr<Scope> getParent() const noexcept { return parent_; }

private:
    std::shared_ptr<Scope> parent_;
    std::unordered_map<std::string, Symbol> symbols_;
};

struct SemanticDiagnostic {
    bool isError{true};
    std::string message;
    std::string context;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(const GenomicRuleEngine* ruleEngine = nullptr);

    void setRuleEngine(const GenomicRuleEngine* ruleEngine) noexcept { ruleEngine_ = ruleEngine; }
    const GenomicRuleEngine* getRuleEngine() const noexcept { return ruleEngine_; }

    void registerFunction(const FunctionSignature& fn);
    std::optional<FunctionSignature> resolveOverload(const std::string& name,
                                                    const std::vector<std::shared_ptr<CppType>>& argTypes);

    // Full type checking, name lookup, and UB Guard
    bool analyze(AstNode& root);
    const std::vector<SemanticDiagnostic>& getDiagnostics() const noexcept { return diagnostics_; }

    // Constexpr compile-time evaluator
    static std::optional<int64_t> evaluateConstexprInt(const AstNode& expr, const Scope& scope);

private:
    const GenomicRuleEngine* ruleEngine_{nullptr};
    std::shared_ptr<Scope> currentScope_;
    std::vector<FunctionSignature> functionTable_;
    std::vector<SemanticDiagnostic> diagnostics_;

    void enterScope();
    void exitScope();
    void error(const std::string& msg, const std::string& ctx = "");

    bool analyzeNode(AstNode& node);
    bool checkBinaryExpr(AstNode& node);
    bool checkCallExpr(AstNode& node);
    bool checkVariableDecl(AstNode& node);
    bool checkFunctionDecl(AstNode& node);
    bool checkIfStmt(AstNode& node);
};

} // namespace genomic
