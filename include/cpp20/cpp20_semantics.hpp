#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_templates.hpp"
#include "cpp20/cpp20_classes.hpp"

namespace genomic {

class GenomicRuleEngine;

enum class StorageDuration : uint8_t {
    Automatic,
    Static,
    ThreadLocal,
    Dynamic,
    Temporary
};

struct Symbol {
    std::string name{};
    std::shared_ptr<CppType> type{};
    bool isConstexpr{false};
    bool isInitialized{false};
    int64_t constexprIntVal{0};
    StorageDuration storageDuration{StorageDuration::Automatic};
    uint32_t scopeDepth{0};
    bool isMovedFrom{false};
    bool extendsTemporaryLifetime{false};
    std::string boundTargetName{}; // If reference/pointer, name of referenced local variable or "<temporary>"
    uint32_t boundTargetScopeDepth{0};
};

struct FunctionSignature {
    std::string name{};
    std::shared_ptr<CppType> returnType{};
    std::vector<std::shared_ptr<CppType>> paramTypes{};
    bool isConstexpr{false};
    bool isTemplate{false};
    std::vector<TemplateParam> templateParams{};
    std::string constraintClause{}; // e.g. "std::integral<T>" or "SignedIntegral"
    bool isVariadicEllipsis{false};
    bool isDeleted{false};
    bool isExplicit{false};
};

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr, uint32_t depth = 0)
        : parent_(std::move(parent)), depth_(depth) {}

    bool declare(const Symbol& sym);
    bool updateSymbol(const Symbol& sym);
    std::optional<Symbol> lookup(const std::string& name) const;
    bool isDeclaredLocally(const std::string& name) const;

    std::shared_ptr<Scope> getParent() const noexcept { return parent_; }
    uint32_t getDepth() const noexcept { return depth_; }
    const std::vector<std::string>& getDeclarationOrder() const noexcept { return declarationOrder_; }
    const std::unordered_map<std::string, Symbol>& getLocalSymbols() const noexcept { return symbols_; }

private:
    std::shared_ptr<Scope> parent_;
    uint32_t depth_{0};
    std::unordered_map<std::string, Symbol> symbols_;
    std::vector<std::string> declarationOrder_;
};

struct SemanticDiagnostic {
    bool isError{true};
    std::string message;
    std::string context;
};

struct LifetimeEvent {
    enum class Kind : uint8_t {
        ConstructLocal,
        MaterializeTemporary,
        ExtendTemporaryLifetime,
        DestroyTemporaryAtFullExprEnd,
        DestroyExtendedTemporaryAtScopeExit,
        DestroyLocalAtScopeExit,
        MoveFromObject
    } kind{Kind::ConstructLocal};
    std::string objectName;
    std::string typeName;
    uint32_t scopeDepth{0};
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(const GenomicRuleEngine* ruleEngine = nullptr);

    void setRuleEngine(const GenomicRuleEngine* ruleEngine) noexcept {
        ruleEngine_ = ruleEngine;
        templateEngine_.setRuleEngine(ruleEngine);
    }
    const GenomicRuleEngine* getRuleEngine() const noexcept { return ruleEngine_; }

    TemplateEngine& getTemplateEngine() noexcept { return templateEngine_; }
    const TemplateEngine& getTemplateEngine() const noexcept { return templateEngine_; }

    ClassRegistry& getClassRegistry() noexcept { return classRegistry_; }
    const ClassRegistry& getClassRegistry() const noexcept { return classRegistry_; }

    void registerFunction(const FunctionSignature& fn);
    void registerFunctionTemplate(const FunctionSignature& fnTemplate);
    void registerUserDefinedConversion(const std::string& fromType, const std::string& toType, bool isExplicit = false);

    // Full C++20 Overload Resolution with value categories, templates, partial ordering, and concept subsumption
    std::optional<FunctionSignature> resolveOverload(
        const std::string& name,
        const std::vector<std::shared_ptr<CppType>>& argTypes,
        const std::vector<ValueCategory>& argValueCategories = {},
        const std::vector<std::shared_ptr<CppType>>& explicitTemplateArgs = {}
    );

    // Full type checking, name lookup, switch validation, lifetime analysis, and UB Guard
    bool analyze(AstNode& root);
    const std::vector<SemanticDiagnostic>& getDiagnostics() const noexcept { return diagnostics_; }
    const std::vector<LifetimeEvent>& getLifetimeTrace() const noexcept { return lifetimeTrace_; }

    // Constexpr compile-time evaluator
    static std::optional<int64_t> evaluateConstexprInt(const AstNode& expr, const Scope& scope);

private:
    const GenomicRuleEngine* ruleEngine_{nullptr};
    TemplateEngine templateEngine_;
    ClassRegistry classRegistry_;
    std::shared_ptr<Scope> currentScope_;
    uint32_t currentScopeDepth_{0};
    std::shared_ptr<CppType> currentFunctionReturnType_;
    int loopOrSwitchDepth_{0};

    std::vector<FunctionSignature> functionTable_;
    std::vector<std::tuple<std::string, std::string, bool>> userConversions_; // (from, to, isExplicit)
    std::vector<SemanticDiagnostic> diagnostics_;
    std::vector<LifetimeEvent> lifetimeTrace_;

    void enterScope();
    void exitScope();
    void emitScopeDestructorsUpTo(uint32_t targetScopeDepth);
    void error(const std::string& msg, const std::string& ctx = "");

    bool canUserConvert(const CppType& from, const CppType& to) const;

    bool analyzeNode(AstNode& node);
    bool checkBinaryExpr(AstNode& node);
    bool checkUnaryExpr(AstNode& node);
    bool checkTernaryExpr(AstNode& node);
    bool checkCallExpr(AstNode& node);
    bool checkMemberAccessExpr(AstNode& node);
    bool checkCastExpr(AstNode& node);
    bool checkVariableDecl(AstNode& node);
    bool checkFunctionDecl(AstNode& node);
    bool checkRecordDecl(AstNode& node);
    bool checkTemplateDecl(AstNode& node);
    bool checkConceptDecl(AstNode& node);
    bool checkIfStmt(AstNode& node);
    bool checkSwitchStmt(AstNode& node);
    bool checkReturnStmt(AstNode& node);
};

} // namespace genomic
