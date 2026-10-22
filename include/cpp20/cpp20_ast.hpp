#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "cpp20/cpp20_types.hpp"
#include "genome/genome_reference.hpp"

namespace genomic {

enum class AstNodeKind : uint16_t {
    TranslationUnit,
    NamespaceDecl,
    FunctionDecl,
    VariableDecl,
    RecordDecl,
    TemplateDecl,
    ConceptDecl,
    CompoundStmt,
    IfStmt,
    ForStmt,
    WhileStmt,
    DoWhileStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    ExprStmt,
    CallExpr,
    BinaryExpr,
    UnaryExpr,
    LiteralExpr,
    IdentifierExpr,
    MemberAccessExpr,
    CastExpr,
    LambdaExpr,
    CoroutineExpr,
    EmitMoleculeExpr
};

struct GenomeProvenance {
    GenomeRegionId sourceRegion{0};
    GenomeRegionId enhancer{0};
    std::vector<uint64_t> triggeringMolecules;
    uint64_t expressionEvent{0};

    bool operator==(const GenomeProvenance& other) const noexcept {
        return sourceRegion == other.sourceRegion &&
               enhancer == other.enhancer &&
               triggeringMolecules == other.triggeringMolecules &&
               expressionEvent == other.expressionEvent;
    }
};

struct AstNode {
    AstNodeKind kind{AstNodeKind::CompoundStmt};
    std::string name;
    std::string op;
    std::shared_ptr<CppType> evaluatedType;
    ValueCategory valueCategory{ValueCategory::PRValue};
    GenomeProvenance provenance;

    // Literal payload
    int64_t intVal{0};
    double floatVal{0.0};
    std::string strVal;
    bool boolVal{false};

    // Sub-components
    std::vector<std::shared_ptr<AstNode>> children;

    // Helper builders
    static std::shared_ptr<AstNode> makeTranslationUnit();
    static std::shared_ptr<AstNode> makeFunctionDecl(
        const std::string& name,
        std::shared_ptr<CppType> returnType,
        std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params,
        std::shared_ptr<AstNode> body,
        bool isConstexpr = false,
        bool isCoroutine = false
    );
    static std::shared_ptr<AstNode> makeVariableDecl(
        const std::string& name,
        std::shared_ptr<CppType> type,
        std::shared_ptr<AstNode> initializer = nullptr,
        bool isConstexpr = false
    );
    static std::shared_ptr<AstNode> makeBinaryExpr(
        const std::string& op,
        std::shared_ptr<AstNode> lhs,
        std::shared_ptr<AstNode> rhs
    );
    static std::shared_ptr<AstNode> makeUnaryExpr(
        const std::string& op,
        std::shared_ptr<AstNode> operand,
        bool isPrefix = true
    );
    static std::shared_ptr<AstNode> makeCallExpr(
        const std::string& funcName,
        std::vector<std::shared_ptr<AstNode>> args
    );
    static std::shared_ptr<AstNode> makeLiteralInt(int64_t val);
    static std::shared_ptr<AstNode> makeLiteralFloat(double val);
    static std::shared_ptr<AstNode> makeLiteralBool(bool val);
    static std::shared_ptr<AstNode> makeLiteralString(const std::string& val);
    static std::shared_ptr<AstNode> makeIdentifier(const std::string& name);
    static std::shared_ptr<AstNode> makeIfStmt(
        std::shared_ptr<AstNode> condition,
        std::shared_ptr<AstNode> thenBranch,
        std::shared_ptr<AstNode> elseBranch = nullptr,
        bool isConstexpr = false
    );
    static std::shared_ptr<AstNode> makeForStmt(
        std::shared_ptr<AstNode> init,
        std::shared_ptr<AstNode> condition,
        std::shared_ptr<AstNode> step,
        std::shared_ptr<AstNode> body
    );
    static std::shared_ptr<AstNode> makeWhileStmt(
        std::shared_ptr<AstNode> condition,
        std::shared_ptr<AstNode> body
    );
    static std::shared_ptr<AstNode> makeReturnStmt(std::shared_ptr<AstNode> expr = nullptr);
    static std::shared_ptr<AstNode> makeCompoundStmt(std::vector<std::shared_ptr<AstNode>> stmts = {});
    static std::shared_ptr<AstNode> makeEmitMolecule(uint64_t signature, double concentration);
    static std::shared_ptr<AstNode> makeCoroutineSuspend(std::shared_ptr<AstNode> expr = nullptr);
};

// C++20 Pretty-printer / Decompiler (Blueprint section 37)
class Cpp20Printer {
public:
    static std::string print(const AstNode& node, int indent = 0);
};

} // namespace genomic
