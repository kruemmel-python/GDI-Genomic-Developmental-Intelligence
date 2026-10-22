#include "cpp20/cpp20_semantics.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <algorithm>

namespace genomic {

bool Scope::declare(const Symbol& sym) {
    if (isDeclaredLocally(sym.name)) {
        return false;
    }
    symbols_[sym.name] = sym;
    return true;
}

std::optional<Symbol> Scope::lookup(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->lookup(name);
    }
    return std::nullopt;
}

bool Scope::isDeclaredLocally(const std::string& name) const {
    return symbols_.find(name) != symbols_.end();
}

SemanticAnalyzer::SemanticAnalyzer(const GenomicRuleEngine* ruleEngine)
    : ruleEngine_(ruleEngine) {
    currentScope_ = std::make_shared<Scope>();

    // Register built-in functions
    registerFunction(FunctionSignature{"emitMolecule", CppType::makeVoid(), {CppType::makeUInt64(), CppType::makeFloat64()}});
    registerFunction(FunctionSignature{"print", CppType::makeVoid(), {CppType::makeInt64()}});
}

void SemanticAnalyzer::registerFunction(const FunctionSignature& fn) {
    functionTable_.push_back(fn);
}

std::optional<FunctionSignature> SemanticAnalyzer::resolveOverload(
    const std::string& name,
    const std::vector<std::shared_ptr<CppType>>& argTypes
) {
    struct Candidate {
        const FunctionSignature* signature;
        std::vector<int> conversionRanks;
    };
    std::vector<Candidate> candidates;
    for (const auto& fn : functionTable_) {
        if (fn.name != name || fn.paramTypes.size() != argTypes.size()) continue;
        Candidate candidate{&fn, {}};
        bool viable = true;
        for (size_t i = 0; i < argTypes.size(); ++i) {
            if (!argTypes[i] || !fn.paramTypes[i]) { viable = false; break; }
            int rank = 999;
            if (argTypes[i]->isEquivalentTo(*fn.paramTypes[i])) {
                rank = 0;
            } else if (ruleEngine_) {
                rank = ruleEngine_->getConversionCost(argTypes[i]->kind, fn.paramTypes[i]->kind);
            } else if (TypeSystem::canImplicitlyConvert(*argTypes[i], *fn.paramTypes[i])) {
                rank = 2;
            }
            if (rank >= 999) { viable = false; break; }
            candidate.conversionRanks.push_back(rank);
        }
        if (viable) candidates.push_back(std::move(candidate));
    }
    if (candidates.empty()) return std::nullopt;

    // A viable overload is better only when it is no worse on EVERY
    // argument and strictly better on at least one. Summing ranks is invalid.
    auto dominates = [](const Candidate& a, const Candidate& b) {
        bool strictlyBetter = false;
        for (size_t i = 0; i < a.conversionRanks.size(); ++i) {
            if (a.conversionRanks[i] > b.conversionRanks[i]) return false;
            if (a.conversionRanks[i] < b.conversionRanks[i]) strictlyBetter = true;
        }
        return strictlyBetter;
    };
    const FunctionSignature* winner = nullptr;
    size_t nondominated = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        bool dominated = false;
        for (size_t j = 0; j < candidates.size(); ++j) {
            if (i != j && dominates(candidates[j], candidates[i])) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            winner = candidates[i].signature;
            ++nondominated;
        }
    }
    if (nondominated != 1) {
        error("Ambiguous function call overload for " + name);
        return std::nullopt;
    }
    return *winner;
}

void SemanticAnalyzer::enterScope() {
    currentScope_ = std::make_shared<Scope>(currentScope_);
}

void SemanticAnalyzer::exitScope() {
    if (currentScope_->getParent()) {
        currentScope_ = currentScope_->getParent();
    }
}

void SemanticAnalyzer::error(const std::string& msg, const std::string& ctx) {
    diagnostics_.push_back(SemanticDiagnostic{true, msg, ctx});
}

bool SemanticAnalyzer::analyze(AstNode& root) {
    diagnostics_.clear();
    return analyzeNode(root) && diagnostics_.empty();
}

bool SemanticAnalyzer::analyzeNode(AstNode& node) {
    switch (node.kind) {
        case AstNodeKind::TranslationUnit: {
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            return ok;
        }
        case AstNodeKind::FunctionDecl:
            return checkFunctionDecl(node);
        case AstNodeKind::VariableDecl:
            return checkVariableDecl(node);
        case AstNodeKind::CompoundStmt: {
            enterScope();
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            exitScope();
            return ok;
        }
        case AstNodeKind::IfStmt:
            return checkIfStmt(node);
        case AstNodeKind::BinaryExpr:
            return checkBinaryExpr(node);
        case AstNodeKind::CallExpr:
            return checkCallExpr(node);
        case AstNodeKind::IdentifierExpr: {
            auto sym = currentScope_->lookup(node.name);
            if (!sym) {
                error("Use of undeclared identifier '" + node.name + "'", node.name);
                return false;
            }
            node.evaluatedType = sym->type;
            node.valueCategory = ValueCategory::LValue;
            return true;
        }
        case AstNodeKind::LiteralExpr: {
            if (!node.evaluatedType) {
                node.evaluatedType = CppType::makeInt64();
            }
            node.valueCategory = ValueCategory::PRValue;
            return true;
        }
        case AstNodeKind::ReturnStmt: {
            if (!node.children.empty() && node.children[0]) {
                if (!analyzeNode(*node.children[0])) return false;
                node.evaluatedType = node.children[0]->evaluatedType;
            } else {
                node.evaluatedType = CppType::makeVoid();
            }
            return true;
        }
        case AstNodeKind::ForStmt: {
            enterScope();
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            exitScope();
            return ok;
        }
        case AstNodeKind::WhileStmt: {
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            return ok;
        }
        case AstNodeKind::EmitMoleculeExpr: {
            node.evaluatedType = CppType::makeVoid();
            return true;
        }
        case AstNodeKind::CoroutineExpr: {
            if (!node.children.empty() && node.children[0]) {
                if (!analyzeNode(*node.children[0])) return false;
                node.evaluatedType = node.children[0]->evaluatedType;
            } else {
                node.evaluatedType = CppType::makeVoid();
            }
            return true;
        }
        default:
            return true;
    }
}

bool SemanticAnalyzer::checkVariableDecl(AstNode& node) {
    if (!node.evaluatedType) {
        node.evaluatedType = CppType::makeInt64();
    }

    if (!node.children.empty() && node.children[0]) {
        if (!analyzeNode(*node.children[0])) return false;
        auto initType = node.children[0]->evaluatedType;
        if (initType && !TypeSystem::canImplicitlyConvert(*initType, *node.evaluatedType)) {
            error("Cannot convert initializer type " + initType->toString() +
                  " to variable type " + node.evaluatedType->toString(), node.name);
            return false;
        }
    }

    Symbol sym;
    sym.name = node.name;
    sym.type = node.evaluatedType;
    sym.isConstexpr = node.boolVal;
    sym.isInitialized = !node.children.empty();

    if (sym.isConstexpr && !node.children.empty() && node.children[0]) {
        auto val = evaluateConstexprInt(*node.children[0], *currentScope_);
        if (val) {
            sym.constexprIntVal = *val;
        }
    }

    if (!currentScope_->declare(sym)) {
        error("Redeclaration of variable '" + node.name + "' in the same scope", node.name);
        return false;
    }
    return true;
}

bool SemanticAnalyzer::checkFunctionDecl(AstNode& node) {
    FunctionSignature sig;
    sig.name = node.name;
    sig.returnType = node.evaluatedType ? node.evaluatedType : CppType::makeVoid();
    sig.isConstexpr = node.boolVal;

    size_t paramCount = (node.children.empty() ? 0 : node.children.size() - 1);
    for (size_t i = 0; i < paramCount; ++i) {
        const auto& p = node.children[i];
        sig.paramTypes.push_back(p->evaluatedType ? p->evaluatedType : CppType::makeInt64());
    }

    registerFunction(sig);

    enterScope();
    for (size_t i = 0; i < paramCount; ++i) {
        const auto& p = node.children[i];
        Symbol sym;
        sym.name = p->name;
        sym.type = p->evaluatedType ? p->evaluatedType : CppType::makeInt64();
        sym.isInitialized = true;
        currentScope_->declare(sym);
    }

    bool bodyOk = true;
    if (!node.children.empty() && node.children.back()) {
        bodyOk = analyzeNode(*node.children.back());
    }

    exitScope();
    return bodyOk;
}

bool SemanticAnalyzer::checkBinaryExpr(AstNode& node) {
    if (node.children.size() < 2 || !node.children[0] || !node.children[1]) {
        error("Binary expression missing operand");
        return false;
    }

    if (!analyzeNode(*node.children[0]) || !analyzeNode(*node.children[1])) {
        return false;
    }

    auto t1 = node.children[0]->evaluatedType;
    auto t2 = node.children[1]->evaluatedType;

    if (!t1 || !t2) {
        error("Missing operand type in binary expression");
        return false;
    }

    // UB Guard: Division by zero check at compile-time if divisor is constant 0
    if ((node.op == "/" || node.op == "%") &&
        node.children[1]->kind == AstNodeKind::LiteralExpr &&
        node.children[1]->intVal == 0 && node.children[1]->floatVal == 0.0) {
        error("UB Guard violation: Division by zero detected in expression", node.op);
        return false;
    }

    if (node.op == "=") {
        if (!TypeSystem::canImplicitlyConvert(*t2, *t1)) {
            error("Cannot assign type " + t2->toString() + " to " + t1->toString());
            return false;
        }
        node.evaluatedType = t1;
        node.valueCategory = ValueCategory::LValue;
        return true;
    }

    // Query Genomic Rule Engine directly from the immutable genome!
    if (ruleEngine_) {
        auto rule = ruleEngine_->resolveBinaryOp(node.op, t1->kind, t2->kind);
        if (rule) {
            node.evaluatedType = std::make_shared<CppType>(rule->resultKind);
            node.valueCategory = rule->resultValueCategory;
            return true;
        }
        // A common arithmetic type is not proof that an operator is legal.
        error("No genomic semantic rule for binary operator '" + node.op + "'", node.op);
        return false;
    }

    if (node.op == "==" || node.op == "!=" || node.op == "<" ||
        node.op == "<=" || node.op == ">" || node.op == ">=") {
        node.evaluatedType = CppType::makeBool();
    } else {
        node.evaluatedType = TypeSystem::usualArithmeticConversion(*t1, *t2);
    }

    node.valueCategory = ValueCategory::PRValue;
    return true;
}

bool SemanticAnalyzer::checkCallExpr(AstNode& node) {
    std::vector<std::shared_ptr<CppType>> argTypes;
    for (auto& child : node.children) {
        if (!analyzeNode(*child)) return false;
        argTypes.push_back(child->evaluatedType);
    }

    auto matchedFn = resolveOverload(node.name, argTypes);
    if (!matchedFn) {
        error("No matching overload found for function call '" + node.name + "'", node.name);
        return false;
    }

    node.evaluatedType = matchedFn->returnType;
    node.valueCategory = ValueCategory::PRValue;
    return true;
}

bool SemanticAnalyzer::checkIfStmt(AstNode& node) {
    if (node.children.empty() || !node.children[0]) {
        error("If statement missing condition");
        return false;
    }

    if (!analyzeNode(*node.children[0])) return false;

    bool ok = true;
    if (node.children.size() > 1 && node.children[1]) {
        if (!analyzeNode(*node.children[1])) ok = false;
    }
    if (node.children.size() > 2 && node.children[2]) {
        if (!analyzeNode(*node.children[2])) ok = false;
    }
    return ok;
}

std::optional<int64_t> SemanticAnalyzer::evaluateConstexprInt(const AstNode& expr, const Scope& scope) {
    if (expr.kind == AstNodeKind::LiteralExpr && expr.evaluatedType && expr.evaluatedType->isInteger()) {
        return expr.intVal;
    }
    if (expr.kind == AstNodeKind::IdentifierExpr) {
        auto sym = scope.lookup(expr.name);
        if (sym && sym->isConstexpr) {
            return sym->constexprIntVal;
        }
        return std::nullopt;
    }
    if (expr.kind == AstNodeKind::BinaryExpr && expr.children.size() == 2) {
        auto left = evaluateConstexprInt(*expr.children[0], scope);
        auto right = evaluateConstexprInt(*expr.children[1], scope);
        if (!left || !right) return std::nullopt;

        if (expr.op == "+") return *left + *right;
        if (expr.op == "-") return *left - *right;
        if (expr.op == "*") return *left * *right;
        if (expr.op == "/") return (*right != 0) ? std::optional<int64_t>(*left / *right) : std::nullopt;
        if (expr.op == "%") return (*right != 0) ? std::optional<int64_t>(*left % *right) : std::nullopt;
        if (expr.op == "==") return (*left == *right) ? 1 : 0;
        if (expr.op == "!=") return (*left != *right) ? 1 : 0;
        if (expr.op == "<") return (*left < *right) ? 1 : 0;
        if (expr.op == "<=") return (*left <= *right) ? 1 : 0;
        if (expr.op == ">") return (*left > *right) ? 1 : 0;
        if (expr.op == ">=") return (*left >= *right) ? 1 : 0;
    }
    return std::nullopt;
}

} // namespace genomic
