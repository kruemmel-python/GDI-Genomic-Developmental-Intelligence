#include "cpp20/cpp20_semantics.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <algorithm>
#include <unordered_set>

namespace genomic {

bool Scope::declare(const Symbol& sym) {
    if (isDeclaredLocally(sym.name)) {
        return false;
    }
    symbols_[sym.name] = sym;
    declarationOrder_.push_back(sym.name);
    return true;
}

bool Scope::updateSymbol(const Symbol& sym) {
    auto it = symbols_.find(sym.name);
    if (it != symbols_.end()) {
        it->second = sym;
        return true;
    }
    if (parent_) {
        return parent_->updateSymbol(sym);
    }
    return false;
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
    : ruleEngine_(ruleEngine), templateEngine_(ruleEngine) {
    currentScope_ = std::make_shared<Scope>(nullptr, 0);

    // Register built-in functions
    registerFunction(FunctionSignature{"emitMolecule", CppType::makeVoid(), {CppType::makeUInt64(), CppType::makeFloat64()}});
    registerFunction(FunctionSignature{"print", CppType::makeVoid(), {CppType::makeInt64()}});
}

void SemanticAnalyzer::registerFunction(const FunctionSignature& fn) {
    functionTable_.push_back(fn);
}

void SemanticAnalyzer::registerFunctionTemplate(const FunctionSignature& fnTemplate) {
    auto copy = fnTemplate;
    copy.isTemplate = true;
    if (copy.templateParams.empty()) {
        copy.templateParams.push_back(TemplateParam{"T", true, false, nullptr, 0, false, ""});
    }
    functionTable_.push_back(std::move(copy));
}

void SemanticAnalyzer::registerUserDefinedConversion(const std::string& fromType, const std::string& toType, bool isExplicit) {
    userConversions_.emplace_back(fromType, toType, isExplicit);
}

bool SemanticAnalyzer::canUserConvert(const CppType& from, const CppType& to) const {
    auto uFrom = from.removeCVRef();
    auto uTo = to.removeCVRef();
    std::string fromName = uFrom->isRecord() ? uFrom->name : uFrom->toString();
    std::string toName = uTo->isRecord() ? uTo->name : uTo->toString();
    for (const auto& [src, dst, isExp] : userConversions_) {
        if (!isExp && src == fromName && dst == toName) {
            return true;
        }
    }
    return false;
}

std::optional<FunctionSignature> SemanticAnalyzer::resolveOverload(
    const std::string& name,
    const std::vector<std::shared_ptr<CppType>>& argTypes,
    const std::vector<ValueCategory>& argValueCategories,
    const std::vector<std::shared_ptr<CppType>>& explicitTemplateArgs
) {
    struct ViableCandidate {
        FunctionSignature instantiatedSig;
        const FunctionSignature* originTemplate{nullptr};
        std::vector<ImplicitConversionSequence> conversions;
    };

    std::vector<ViableCandidate> viableCandidates;

    for (const auto& fn : functionTable_) {
        if (fn.name != name) continue;

        FunctionSignature activeSig = fn;
        const FunctionSignature* originPtr = &fn;

        // 1. If candidate is a function template, perform template argument deduction & SFINAE
        if (fn.isTemplate) {
            auto inst = templateEngine_.deduceAndSubstituteFunctionTemplate(
                fn.name, fn.templateParams, fn.paramTypes, fn.returnType,
                argTypes, argValueCategories, explicitTemplateArgs, fn.constraintClause);
            if (!inst.success) {
                continue; // SFINAE / constraint failure removes candidate from overload set cleanly
            }
            activeSig.paramTypes = std::move(inst.substitutedParamTypes);
            activeSig.returnType = std::move(inst.substitutedReturnType);
        } else {
            // Non-template function cannot be called with explicit template arguments `fn<int>(...)`
            if (!explicitTemplateArgs.empty()) continue;
            // Check non-template requires clause if present
            if (!fn.constraintClause.empty()) {
                if (!templateEngine_.checkConstraint(fn.constraintClause, argTypes)) {
                    continue;
                }
            }
        }

        if (!activeSig.isVariadicEllipsis && activeSig.paramTypes.size() != argTypes.size()) {
            continue;
        }
        if (activeSig.isVariadicEllipsis && argTypes.size() < activeSig.paramTypes.size()) {
            continue;
        }

        // 2. Compute Implicit Conversion Sequence (ICS) for each argument
        ViableCandidate cand;
        cand.instantiatedSig = activeSig;
        cand.originTemplate = originPtr;
        bool viable = true;

        for (size_t i = 0; i < argTypes.size(); ++i) {
            if (!argTypes[i]) { viable = false; break; }
            ValueCategory cat = (i < argValueCategories.size()) ? argValueCategories[i] : ValueCategory::PRValue;

            if (i >= activeSig.paramTypes.size()) {
                // Variadic ellipsis match `...`
                ImplicitConversionSequence ellSeq;
                ellSeq.rank = ConversionRank::EllipsisConversion;
                ellSeq.subRankCost = 10;
                cand.conversions.push_back(ellSeq);
                continue;
            }

            if (!activeSig.paramTypes[i]) { viable = false; break; }
            auto seq = TypeSystem::computeImplicitConversionSequence(
                *argTypes[i], cat, *activeSig.paramTypes[i], ruleEngine_, &classRegistry_);

            // Check registered user-defined conversion if standard conversion failed
            if (!seq.isValid() && canUserConvert(*argTypes[i], *activeSig.paramTypes[i])) {
                seq.rank = ConversionRank::UserDefinedConversion;
                seq.subRankCost = 3;
            }

            if (!seq.isValid()) {
                viable = false;
                break;
            }
            cand.conversions.push_back(seq);
        }

        if (viable) {
            viableCandidates.push_back(std::move(cand));
        }
    }

    if (viableCandidates.empty()) {
        return std::nullopt;
    }

    // 3. Full ISO C++20 Overload Resolution Tie-Breaker Cascade ([over.match.best]):
    // Returns true if candidate A is strictly better than candidate B.
    auto isBetterCandidate = [this](const ViableCandidate& a, const ViableCandidate& b) -> bool {
        // Step 3.1: Argument conversion sequence dominance
        bool hasStrictlyBetterArg = false;
        for (size_t i = 0; i < a.conversions.size() && i < b.conversions.size(); ++i) {
            const CppType& pA = (i < a.instantiatedSig.paramTypes.size() && a.instantiatedSig.paramTypes[i])
                ? *a.instantiatedSig.paramTypes[i] : *CppType::makeVoid();
            const CppType& pB = (i < b.instantiatedSig.paramTypes.size() && b.instantiatedSig.paramTypes[i])
                ? *b.instantiatedSig.paramTypes[i] : *CppType::makeVoid();
            int cmp = TypeSystem::compareConversionSequences(a.conversions[i], b.conversions[i], pA, pB);
            if (cmp > 0) {
                return false; // A is worse on argument i -> A cannot dominate B by conversions
            }
            if (cmp < 0) {
                hasStrictlyBetterArg = true;
            }
        }
        if (hasStrictlyBetterArg) {
            return true;
        }

        //Verify that B is not strictly better on any argument before applying tie-breakers
        for (size_t i = 0; i < a.conversions.size() && i < b.conversions.size(); ++i) {
            const CppType& pA = (i < a.instantiatedSig.paramTypes.size() && a.instantiatedSig.paramTypes[i])
                ? *a.instantiatedSig.paramTypes[i] : *CppType::makeVoid();
            const CppType& pB = (i < b.instantiatedSig.paramTypes.size() && b.instantiatedSig.paramTypes[i])
                ? *b.instantiatedSig.paramTypes[i] : *CppType::makeVoid();
            if (TypeSystem::compareConversionSequences(a.conversions[i], b.conversions[i], pA, pB) != 0) {
                return false;
            }
        }

        // Step 3.2: Non-template function beats template specialization ([over.match.best.general]/2.4)
        bool preferNonTemplate = ruleEngine_ ? ruleEngine_->getOverloadRule().preferNonTemplateOverSpecialization : true;
        if (preferNonTemplate && a.originTemplate->isTemplate != b.originTemplate->isTemplate) {
            return !a.originTemplate->isTemplate;
        }

        // Step 3.3: Partial ordering of function templates ([temp.func.order])
        if (a.originTemplate->isTemplate && b.originTemplate->isTemplate) {
            int order = templateEngine_.compareFunctionTemplateSpecialization(
                a.originTemplate->templateParams, a.originTemplate->paramTypes, a.originTemplate->constraintClause,
                b.originTemplate->templateParams, b.originTemplate->paramTypes, b.originTemplate->constraintClause);
            if (order == -1) return true;
            if (order == 1) return false;
        }

        // Step 3.4: Concept subsumption for constrained functions/templates ([over.match.best.general]/2.6)
        bool preferConstrained = ruleEngine_ ? ruleEngine_->getOverloadRule().preferMoreConstrained : true;
        if (preferConstrained) {
            if (templateEngine_.isMoreConstrained(a.originTemplate->constraintClause, b.originTemplate->constraintClause)) {
                return true;
            }
        }

        return false;
    };

    const ViableCandidate* winner = nullptr;
    size_t nondominatedCount = 0;
    for (size_t i = 0; i < viableCandidates.size(); ++i) {
        bool dominated = false;
        for (size_t j = 0; j < viableCandidates.size(); ++j) {
            if (i != j && isBetterCandidate(viableCandidates[j], viableCandidates[i])) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            winner = &viableCandidates[i];
            ++nondominatedCount;
        }
    }

    if (nondominatedCount != 1 || !winner) {
        error("Ambiguous function call overload for " + name, name);
        return std::nullopt;
    }
    if (winner->instantiatedSig.isDeleted) {
        error("Call to deleted function '" + name + "'", name);
        return std::nullopt;
    }
    return winner->instantiatedSig;
}

void SemanticAnalyzer::enterScope() {
    currentScopeDepth_++;
    currentScope_ = std::make_shared<Scope>(currentScope_, currentScopeDepth_);
}

void SemanticAnalyzer::exitScope() {
    if (!currentScope_) return;
    // Emit deterministic reverse-order RAII destructors for variables & extended temporaries in this scope
    const auto& declOrder = currentScope_->getDeclarationOrder();
    const auto& localSyms = currentScope_->getLocalSymbols();
    for (auto it = declOrder.rbegin(); it != declOrder.rend(); ++it) {
        const auto symIt = localSyms.find(*it);
        if (symIt == localSyms.end()) continue;
        const auto& sym = symIt->second;
        if (sym.extendsTemporaryLifetime) {
            lifetimeTrace_.push_back(LifetimeEvent{
                LifetimeEvent::Kind::DestroyExtendedTemporaryAtScopeExit,
                sym.boundTargetName,
                sym.type ? sym.type->toString() : "auto",
                currentScopeDepth_
            });
        } else if (sym.storageDuration == StorageDuration::Automatic && sym.type && !sym.type->isReference()) {
            lifetimeTrace_.push_back(LifetimeEvent{
                LifetimeEvent::Kind::DestroyLocalAtScopeExit,
                sym.name,
                sym.type->toString(),
                currentScopeDepth_
            });
        }
    }

    if (currentScope_->getParent()) {
        currentScope_ = currentScope_->getParent();
        currentScopeDepth_--;
    }
}

void SemanticAnalyzer::emitScopeDestructorsUpTo(uint32_t targetScopeDepth) {
    auto sc = currentScope_;
    while (sc && sc->getDepth() >= targetScopeDepth) {
        const auto& declOrder = sc->getDeclarationOrder();
        const auto& localSyms = sc->getLocalSymbols();
        for (auto it = declOrder.rbegin(); it != declOrder.rend(); ++it) {
            const auto symIt = localSyms.find(*it);
            if (symIt == localSyms.end()) continue;
            const auto& sym = symIt->second;
            if (sym.extendsTemporaryLifetime) {
                lifetimeTrace_.push_back(LifetimeEvent{
                    LifetimeEvent::Kind::DestroyExtendedTemporaryAtScopeExit,
                    sym.boundTargetName,
                    sym.type ? sym.type->toString() : "auto",
                    sc->getDepth()
                });
            } else if (sym.storageDuration == StorageDuration::Automatic && sym.type && !sym.type->isReference()) {
                lifetimeTrace_.push_back(LifetimeEvent{
                    LifetimeEvent::Kind::DestroyLocalAtScopeExit,
                    sym.name,
                    sym.type->toString(),
                    sc->getDepth()
                });
            }
        }
        sc = sc->getParent();
    }
}

void SemanticAnalyzer::error(const std::string& msg, const std::string& ctx) {
    diagnostics_.push_back(SemanticDiagnostic{true, msg, ctx});
}

bool SemanticAnalyzer::analyze(AstNode& root) {
    diagnostics_.clear();
    lifetimeTrace_.clear();
    return analyzeNode(root) && diagnostics_.empty();
}

bool SemanticAnalyzer::analyzeNode(AstNode& node) {
    switch (node.kind) {
        case AstNodeKind::TranslationUnit:
        case AstNodeKind::NamespaceDecl: {
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
        case AstNodeKind::RecordDecl:
            return checkRecordDecl(node);
        case AstNodeKind::TemplateDecl:
            return checkTemplateDecl(node);
        case AstNodeKind::ConceptDecl:
            return checkConceptDecl(node);
        case AstNodeKind::StaticAssertDecl: {
            if (!node.children.empty() && node.children[0]) {
                if (!analyzeNode(*node.children[0])) return false;
                auto val = evaluateConstexprInt(*node.children[0], *currentScope_);
                if (val && *val == 0) {
                    error("Static assertion failed: " + node.strVal, "static_assert");
                    return false;
                }
            }
            return true;
        }
        case AstNodeKind::UsingDecl:
            return true;
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
        case AstNodeKind::SwitchStmt:
            return checkSwitchStmt(node);
        case AstNodeKind::CaseStmt: {
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            return ok;
        }
        case AstNodeKind::DefaultStmt: {
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            return ok;
        }
        case AstNodeKind::BreakStmt:
        case AstNodeKind::ContinueStmt: {
            if (loopOrSwitchDepth_ <= 0) {
                error("break/continue statement not within loop or switch",
                      node.kind == AstNodeKind::BreakStmt ? "break" : "continue");
                return false;
            }
            return true;
        }
        case AstNodeKind::BinaryExpr:
            return checkBinaryExpr(node);
        case AstNodeKind::UnaryExpr:
            return checkUnaryExpr(node);
        case AstNodeKind::TernaryExpr:
            return checkTernaryExpr(node);
        case AstNodeKind::CallExpr:
            return checkCallExpr(node);
        case AstNodeKind::MemberAccessExpr:
            return checkMemberAccessExpr(node);
        case AstNodeKind::SubscriptExpr: {
            if (node.children.size() < 2 || !node.children[0] || !node.children[1]) return false;
            if (!analyzeNode(*node.children[0]) || !analyzeNode(*node.children[1])) return false;
            auto baseT = node.children[0]->evaluatedType;
            if (baseT && (baseT->isPointer() || baseT->kind == TypeKind::Array) && baseT->pointeeType) {
                node.evaluatedType = baseT->pointeeType;
            } else {
                node.evaluatedType = CppType::makeInt64();
            }
            node.valueCategory = ValueCategory::LValue;
            return true;
        }
        case AstNodeKind::CastExpr:
            return checkCastExpr(node);
        case AstNodeKind::LambdaExpr: {
            enterScope();
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            exitScope();
            if (!node.evaluatedType) node.evaluatedType = CppType::makeAuto();
            node.valueCategory = ValueCategory::PRValue;
            return ok;
        }
        case AstNodeKind::RequiresExpr: {
            node.evaluatedType = CppType::makeBool();
            node.valueCategory = ValueCategory::PRValue;
            return true;
        }
        case AstNodeKind::IdentifierExpr: {
            auto sym = currentScope_->lookup(node.name);
            if (!sym) {
                error("Use of undeclared identifier '" + node.name + "'", node.name);
                return false;
            }
            if (sym->isMovedFrom) {
                error("Lifetime/UB Guard violation: Use of moved-from variable '" + node.name + "'", node.name);
                return false;
            }
            if (!sym->isInitialized) {
                error("Lifetime/UB Guard violation: Read of uninitialized variable '" + node.name + "'", node.name);
                return false;
            }
            node.evaluatedType = sym->type ? sym->type->removeReference() : CppType::makeInt64();
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
        case AstNodeKind::ReturnStmt:
            return checkReturnStmt(node);
        case AstNodeKind::ForStmt: {
            enterScope();
            loopOrSwitchDepth_++;
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            loopOrSwitchDepth_--;
            exitScope();
            return ok;
        }
        case AstNodeKind::WhileStmt:
        case AstNodeKind::DoWhileStmt: {
            loopOrSwitchDepth_++;
            bool ok = true;
            for (auto& child : node.children) {
                if (child && !analyzeNode(*child)) ok = false;
            }
            loopOrSwitchDepth_--;
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

    Symbol sym;
    sym.name = node.name;
    sym.type = node.evaluatedType;
    sym.isConstexpr = node.boolVal;
    sym.isInitialized = !node.children.empty() || node.evaluatedType->isRecord();
    sym.scopeDepth = currentScopeDepth_;

    if (!node.children.empty() && node.children[0]) {
        if (!analyzeNode(*node.children[0])) return false;
        auto initType = node.children[0]->evaluatedType;
        auto initCat = node.children[0]->valueCategory;

        // Deduce `auto` or `const auto&` or `auto&&` from initializer
        if (node.evaluatedType->kind == TypeKind::Auto) {
            node.evaluatedType = initType ? initType->decay()->withCV(node.evaluatedType->isConst, node.evaluatedType->isVolatile)
                                          : CppType::makeInt64();
            sym.type = node.evaluatedType;
        } else if (node.evaluatedType->isReference() && node.evaluatedType->pointeeType &&
                   node.evaluatedType->pointeeType->kind == TypeKind::Auto) {
            auto baseT = initType ? initType->removeReference()->withCV(node.evaluatedType->pointeeType->isConst) : CppType::makeInt64();
            node.evaluatedType = node.evaluatedType->isLValueReference()
                ? CppType::makeLValueRef(baseT)
                : CppType::makeRValueRef(baseT);
            sym.type = node.evaluatedType;
        }

        if (initType) {
            auto seq = TypeSystem::computeImplicitConversionSequence(
                *initType, initCat, *node.evaluatedType, ruleEngine_, &classRegistry_);
            if (!seq.isValid() && !canUserConvert(*initType, *node.evaluatedType)) {
                error("Cannot convert initializer type " + initType->toString() +
                      " to variable type " + node.evaluatedType->toString(), node.name);
                return false;
            }
        }

        // Reference binding & Temporary Materialization / Lifetime Extension (C++20 [class.temporary]/6)
        if (node.evaluatedType->isReference()) {
            if (initCat == ValueCategory::PRValue) {
                // Temporary materialization conversion (PRValue -> XValue) + lifetime extension to reference's scope!
                std::string tempName = "<temp_for_" + node.name + ">";
                lifetimeTrace_.push_back(LifetimeEvent{
                    LifetimeEvent::Kind::MaterializeTemporary,
                    tempName,
                    initType ? initType->toString() : "auto",
                    currentScopeDepth_
                });
                lifetimeTrace_.push_back(LifetimeEvent{
                    LifetimeEvent::Kind::ExtendTemporaryLifetime,
                    tempName,
                    initType ? initType->toString() : "auto",
                    currentScopeDepth_
                });
                sym.extendsTemporaryLifetime = true;
                sym.boundTargetName = tempName;
                sym.boundTargetScopeDepth = currentScopeDepth_;
            } else if (node.children[0]->kind == AstNodeKind::IdentifierExpr) {
                auto targetSym = currentScope_->lookup(node.children[0]->name);
                if (targetSym) {
                    sym.boundTargetName = targetSym->name;
                    sym.boundTargetScopeDepth = targetSym->scopeDepth;
                }
            }
        } else if (node.evaluatedType->isPointer() &&
                   node.children[0]->kind == AstNodeKind::UnaryExpr &&
                   node.children[0]->op == "&" &&
                   !node.children[0]->children.empty() &&
                   node.children[0]->children[0]->kind == AstNodeKind::IdentifierExpr) {
            auto targetSym = currentScope_->lookup(node.children[0]->children[0]->name);
            if (targetSym) {
                sym.boundTargetName = targetSym->name;
                sym.boundTargetScopeDepth = targetSym->scopeDepth;
            }
        }
    } else if (node.evaluatedType->isReference()) {
        error("Reference variable '" + node.name + "' requires an initializer", node.name);
        return false;
    }

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

    if (!sym.type->isReference()) {
        lifetimeTrace_.push_back(LifetimeEvent{
            LifetimeEvent::Kind::ConstructLocal,
            sym.name,
            sym.type->toString(),
            currentScopeDepth_
        });
    }
    return true;
}

bool SemanticAnalyzer::checkFunctionDecl(AstNode& node) {
    FunctionSignature sig;
    sig.name = node.name;
    sig.returnType = node.evaluatedType ? node.evaluatedType : CppType::makeVoid();
    sig.isConstexpr = node.boolVal;
    sig.constraintClause = node.strVal;

    bool hasBody = !node.children.empty() && node.children.back() &&
                   node.children.back()->kind == AstNodeKind::CompoundStmt;
    size_t paramCount = hasBody ? (node.children.size() - 1) : node.children.size();
    for (size_t i = 0; i < paramCount; ++i) {
        const auto& p = node.children[i];
        sig.paramTypes.push_back(p->evaluatedType ? p->evaluatedType : CppType::makeInt64());
    }

    registerFunction(sig);

    if (!hasBody) return true;

    auto prevReturnType = currentFunctionReturnType_;
    currentFunctionReturnType_ = sig.returnType;

    enterScope();
    for (size_t i = 0; i < paramCount; ++i) {
        const auto& p = node.children[i];
        Symbol sym;
        sym.name = p->name;
        sym.type = p->evaluatedType ? p->evaluatedType : CppType::makeInt64();
        sym.isInitialized = true;
        sym.scopeDepth = currentScopeDepth_;
        currentScope_->declare(sym);
    }

    bool bodyOk = analyzeNode(*node.children.back());

    exitScope();
    currentFunctionReturnType_ = prevReturnType;
    return bodyOk;
}

bool SemanticAnalyzer::checkRecordDecl(AstNode& node) {
    if (node.name.rfind("enum_", 0) == 0) {
        // Register enum constants in scope and qualified scope
        std::string enumName = node.name.substr(5);
        for (const auto& enumerator : node.children) {
            if (!enumerator) continue;
            Symbol sym;
            sym.name = enumName + "::" + enumerator->name;
            sym.type = node.evaluatedType ? node.evaluatedType : CppType::makeEnum(enumName);
            sym.isConstexpr = true;
            sym.isInitialized = true;
            sym.constexprIntVal = enumerator->intVal;
            currentScope_->declare(sym);
            if (!node.boolVal) {
                // Unscoped enum also injects into enclosing scope
                Symbol unscoped = sym;
                unscoped.name = enumerator->name;
                currentScope_->declare(unscoped);
            }
        }
        return true;
    }

    ClassModel model;
    model.name = node.name;
    for (const auto& member : node.children) {
        if (!member) continue;
        if (member->kind == AstNodeKind::VariableDecl) {
            model.memberVariables.push_back(FieldDefinition{
                member->name,
                member->evaluatedType ? member->evaluatedType : CppType::makeInt64(),
                0,
                true
            });
        } else if (member->kind == AstNodeKind::FunctionDecl) {
            MethodDefinition meth;
            meth.name = member->name;
            meth.signature = member->evaluatedType ? member->evaluatedType : CppType::makeVoid();
            model.methods.push_back(meth);
        }
    }
    classRegistry_.registerClass(model);
    return true;
}

bool SemanticAnalyzer::checkTemplateDecl(AstNode& node) {
    if (node.children.empty() || !node.children[0]) return true;
    const auto& inner = node.children[0];
    if (inner->kind == AstNodeKind::FunctionDecl) {
        FunctionSignature sig;
        sig.name = inner->name;
        sig.returnType = inner->evaluatedType ? inner->evaluatedType : CppType::makeVoid();
        sig.isTemplate = true;
        sig.constraintClause = !node.strVal.empty() ? node.strVal : inner->strVal;

        // Parse template parameter names from node.name (e.g. "typename T")
        TemplateParam tp;
        tp.name = "T";
        tp.isTypeParam = true;
        sig.templateParams.push_back(tp);

        bool hasBody = !inner->children.empty() && inner->children.back() &&
                       inner->children.back()->kind == AstNodeKind::CompoundStmt;
        size_t paramCount = hasBody ? (inner->children.size() - 1) : inner->children.size();
        for (size_t i = 0; i < paramCount; ++i) {
            auto pType = inner->children[i]->evaluatedType;
            if (pType && pType->kind == TypeKind::Record && pType->name == tp.name) {
                pType->kind = TypeKind::TemplateParam;
            }
            sig.paramTypes.push_back(pType ? pType : CppType::makeTemplateParam(tp.name));
        }
        registerFunctionTemplate(sig);
    } else if (inner->kind == AstNodeKind::ConceptDecl) {
        return checkConceptDecl(*inner);
    }
    return true;
}

bool SemanticAnalyzer::checkConceptDecl(AstNode& node) {
    ConceptDefinition cDef;
    cDef.name = node.name;
    cDef.templateParams = {"T"};
    cDef.rawConstraintExpression = node.strVal;
    templateEngine_.registerConcept(cDef);
    return true;
}

bool SemanticAnalyzer::checkBinaryExpr(AstNode& node) {
    if (node.children.size() < 2 || !node.children[0] || !node.children[1]) {
        error("Binary expression missing operand");
        return false;
    }

    // For simple assignment `=`, the RHS is evaluated first and the LHS is the target lvalue
    // (which may be a moved-from or uninitialized variable being re-assigned!)
    if (node.op == "=") {
        if (!analyzeNode(*node.children[1])) return false;
        if (node.children[0]->kind == AstNodeKind::IdentifierExpr && currentScope_) {
            auto sym = currentScope_->lookup(node.children[0]->name);
            if (!sym) {
                error("Use of undeclared identifier '" + node.children[0]->name + "'", node.children[0]->name);
                return false;
            }
            node.children[0]->evaluatedType = sym->type ? sym->type->removeReference() : CppType::makeInt64();
            node.children[0]->valueCategory = ValueCategory::LValue;
        } else {
            if (!analyzeNode(*node.children[0])) return false;
        }
    } else {
        if (!analyzeNode(*node.children[0]) || !analyzeNode(*node.children[1])) {
            return false;
        }
    }

    auto t1 = node.children[0]->evaluatedType;
    auto t2 = node.children[1]->evaluatedType;

    if (!t1 || !t2) {
        error("Missing operand type in binary expression");
        return false;
    }

    // UB Guard: Division by zero check at compile-time if divisor is constant 0
    if ((node.op == "/" || node.op == "%") && currentScope_) {
        auto divisorVal = evaluateConstexprInt(*node.children[1], *currentScope_);
        if ((divisorVal && *divisorVal == 0) ||
            (node.children[1]->kind == AstNodeKind::LiteralExpr &&
             node.children[1]->intVal == 0 && node.children[1]->floatVal == 0.0)) {
            error("UB Guard violation: Division by zero detected in expression", node.op);
            return false;
        }
    }

    // UB Guard: Invalid shift amount check
    if ((node.op == "<<" || node.op == ">>") && currentScope_) {
        auto shiftVal = evaluateConstexprInt(*node.children[1], *currentScope_);
        if (shiftVal && (*shiftVal < 0 || *shiftVal >= 64)) {
            error("UB Guard violation: Shift amount out of range [0, 63]", node.op);
            return false;
        }
    }

    if (node.op == "=" || node.op == "+=" || node.op == "-=" || node.op == "*=" || node.op == "/=") {
        if (node.children[0]->valueCategory != ValueCategory::LValue) {
            error("Assignment requires modifiable lvalue as left operand", node.op);
            return false;
        }
        if (t1->isConst) {
            error("Cannot assign to const-qualified lvalue", node.op);
            return false;
        }
        if (!TypeSystem::canImplicitlyConvert(*t2, *t1) && !canUserConvert(*t2, *t1)) {
            error("Cannot assign type " + t2->toString() + " to " + t1->toString());
            return false;
        }
        // Mark variable as initialized (and un-moved if re-assigned)
        if (node.children[0]->kind == AstNodeKind::IdentifierExpr && currentScope_) {
            auto sym = currentScope_->lookup(node.children[0]->name);
            if (sym) {
                sym->isInitialized = true;
                sym->isMovedFrom = false;
                currentScope_->updateSymbol(*sym);
            }
        }
        node.evaluatedType = t1;
        node.valueCategory = ValueCategory::LValue;
        return true;
    }

    // C++20 Rewritten equality / spaceship operator synthesis (`<=>`, `==`, `!=`)
    if (t1->isRecord() && t2->isRecord() && t1->name == t2->name) {
        if (node.op == "==" || node.op == "!=") {
            // Check if operator== is registered for this record
            auto eqMatch = resolveOverload("operator==", {t1, t2}, {node.children[0]->valueCategory, node.children[1]->valueCategory});
            if (eqMatch) {
                node.evaluatedType = CppType::makeBool();
                node.valueCategory = ValueCategory::PRValue;
                return true;
            }
        }
        if (node.op == "<=>" || node.op == "<" || node.op == "<=" || node.op == ">" || node.op == ">=") {
            auto cmpMatch = resolveOverload("operator<=>", {t1, t2}, {node.children[0]->valueCategory, node.children[1]->valueCategory});
            if (cmpMatch) {
                node.evaluatedType = (node.op == "<=>") ? cmpMatch->returnType : CppType::makeBool();
                node.valueCategory = ValueCategory::PRValue;
                return true;
            }
        }
    }

    // Query Genomic Rule Engine directly from the immutable genome!
    if (ruleEngine_) {
        TypeKind k1 = (t1->kind == TypeKind::Enum) ? TypeKind::Int64 : t1->kind;
        TypeKind k2 = (t2->kind == TypeKind::Enum) ? TypeKind::Int64 : t2->kind;
        auto rule = ruleEngine_->resolveBinaryOp(node.op, k1, k2);
        if (rule) {
            node.evaluatedType = std::make_shared<CppType>(rule->resultKind);
            node.valueCategory = rule->resultValueCategory;
            return true;
        }
        error("No genomic semantic rule for binary operator '" + node.op + "'", node.op);
        return false;
    }

    if (node.op == "<=>") {
        node.evaluatedType = CppType::makeInt8();
    } else if (node.op == "==" || node.op == "!=" || node.op == "<" ||
               node.op == "<=" || node.op == ">" || node.op == ">=" ||
               node.op == "&&" || node.op == "||") {
        node.evaluatedType = CppType::makeBool();
    } else {
        node.evaluatedType = TypeSystem::usualArithmeticConversion(*t1, *t2);
    }

    node.valueCategory = ValueCategory::PRValue;
    return true;
}

bool SemanticAnalyzer::checkUnaryExpr(AstNode& node) {
    if (node.children.empty() || !node.children[0]) {
        error("Unary expression missing operand");
        return false;
    }
    if (!analyzeNode(*node.children[0])) return false;

    auto opType = node.children[0]->evaluatedType;
    if (!opType) return false;

    // Address-of `&expr`
    if (node.op == "&" && node.boolVal) {
        if (node.children[0]->valueCategory != ValueCategory::LValue) {
            error("Cannot take address of rvalue/temporary expression", "&");
            return false;
        }
        node.evaluatedType = CppType::makePointer(opType);
        node.valueCategory = ValueCategory::PRValue;
        return true;
    }

    // Dereference `*ptr`
    if (node.op == "*" && node.boolVal) {
        if (opType->kind == TypeKind::Nullptr ||
            (node.children[0]->kind == AstNodeKind::LiteralExpr && node.children[0]->intVal == 0)) {
            error("UB Guard violation: Null pointer dereference", "*");
            return false;
        }
        if (!opType->isPointer() || !opType->pointeeType) {
            error("Indirection requires pointer operand", "*");
            return false;
        }
        node.evaluatedType = opType->pointeeType;
        node.valueCategory = ValueCategory::LValue;
        return true;
    }

    if (node.op == "!") {
        node.evaluatedType = CppType::makeBool();
        node.valueCategory = ValueCategory::PRValue;
        return true;
    }

    if (node.op == "++" || node.op == "--") {
        if (node.children[0]->valueCategory != ValueCategory::LValue || opType->isConst) {
            error("Increment/decrement requires modifiable lvalue operand", node.op);
            return false;
        }
        node.evaluatedType = opType;
        node.valueCategory = node.boolVal ? ValueCategory::LValue : ValueCategory::PRValue;
        return true;
    }

    node.evaluatedType = opType;
    node.valueCategory = ValueCategory::PRValue;
    return true;
}

bool SemanticAnalyzer::checkTernaryExpr(AstNode& node) {
    if (node.children.size() < 3 || !node.children[0] || !node.children[1] || !node.children[2]) {
        error("Ternary expression missing operand");
        return false;
    }
    if (!analyzeNode(*node.children[0]) || !analyzeNode(*node.children[1]) || !analyzeNode(*node.children[2])) {
        return false;
    }
    auto tTrue = node.children[1]->evaluatedType;
    auto tFalse = node.children[2]->evaluatedType;
    if (!tTrue || !tFalse) return false;

    if (tTrue->isEquivalentTo(*tFalse)) {
        node.evaluatedType = tTrue;
        node.valueCategory = (node.children[1]->valueCategory == ValueCategory::LValue &&
                              node.children[2]->valueCategory == ValueCategory::LValue)
            ? ValueCategory::LValue : ValueCategory::PRValue;
        return true;
    }
    if (tTrue->isArithmetic() && tFalse->isArithmetic()) {
        node.evaluatedType = TypeSystem::usualArithmeticConversion(*tTrue, *tFalse);
        node.valueCategory = ValueCategory::PRValue;
        return true;
    }
    error("Incompatible operand types in conditional ?: expression: " +
          tTrue->toString() + " and " + tFalse->toString());
    return false;
}

bool SemanticAnalyzer::checkCallExpr(AstNode& node) {
    // Special handling for `std::move(x)`: produces XValue and marks local object as moved-from!
    if ((node.name == "std::move" || node.name == "move") && node.children.size() == 1 && node.children[0]) {
        if (!analyzeNode(*node.children[0])) return false;
        auto innerType = node.children[0]->evaluatedType ? node.children[0]->evaluatedType->removeReference() : CppType::makeInt64();
        node.evaluatedType = CppType::makeRValueRef(innerType);
        node.valueCategory = ValueCategory::XValue;
        if (node.children[0]->kind == AstNodeKind::IdentifierExpr && currentScope_) {
            auto sym = currentScope_->lookup(node.children[0]->name);
            if (sym) {
                sym->isMovedFrom = true;
                currentScope_->updateSymbol(*sym);
                lifetimeTrace_.push_back(LifetimeEvent{
                    LifetimeEvent::Kind::MoveFromObject,
                    sym->name,
                    innerType->toString(),
                    currentScopeDepth_
                });
            }
        }
        return true;
    }

    std::vector<std::shared_ptr<CppType>> argTypes;
    std::vector<ValueCategory> argCategories;
    for (auto& child : node.children) {
        if (!analyzeNode(*child)) return false;
        argTypes.push_back(child->evaluatedType);
        argCategories.push_back(child->valueCategory);
    }

    // Check if this is aggregate construction `Record{...}` or CTAD
    if (node.op == "{}" || classRegistry_.lookupClass(node.name) != nullptr) {
        node.evaluatedType = CppType::makeRecord(node.name);
        node.valueCategory = ValueCategory::PRValue;
        return true;
    }

    auto matchedFn = resolveOverload(node.name, argTypes, argCategories);
    if (!matchedFn) {
        error("No matching overload found for function call '" + node.name + "'", node.name);
        return false;
    }

    node.evaluatedType = matchedFn->returnType ? matchedFn->returnType->removeReference() : CppType::makeVoid();
    if (matchedFn->returnType && matchedFn->returnType->isLValueReference()) {
        node.valueCategory = ValueCategory::LValue;
    } else if (matchedFn->returnType && matchedFn->returnType->isRValueReference()) {
        node.valueCategory = ValueCategory::XValue;
    } else {
        node.valueCategory = ValueCategory::PRValue;
    }
    return true;
}

bool SemanticAnalyzer::checkMemberAccessExpr(AstNode& node) {
    if (node.children.empty() || !node.children[0]) return false;
    if (!analyzeNode(*node.children[0])) return false;

    auto objType = node.children[0]->evaluatedType;
    if (!objType) return false;
    if (node.op == "->" && objType->isPointer() && objType->pointeeType) {
        objType = objType->pointeeType;
    }
    objType = objType->removeCVRef();

    if (objType->isRecord()) {
        auto cls = classRegistry_.lookupClass(objType->name);
        if (cls) {
            for (const auto& f : cls->memberVariables) {
                if (f.name == node.name) {
                    node.evaluatedType = f.type;
                    node.valueCategory = ValueCategory::LValue;
                    return true;
                }
            }
            for (const auto& m : cls->methods) {
                if (m.name == node.name) {
                    node.evaluatedType = m.signature;
                    node.valueCategory = ValueCategory::PRValue;
                    return true;
                }
            }
        }
        for (const auto& f : objType->fields) {
            if (f.name == node.name) {
                node.evaluatedType = f.type;
                node.valueCategory = ValueCategory::LValue;
                return true;
            }
        }
    }

    node.evaluatedType = CppType::makeInt64();
    node.valueCategory = ValueCategory::LValue;
    return true;
}

bool SemanticAnalyzer::checkCastExpr(AstNode& node) {
    if (node.children.empty() || !node.children[0]) return false;
    if (!analyzeNode(*node.children[0])) return false;

    if (!node.evaluatedType) {
        node.evaluatedType = node.children[0]->evaluatedType;
    }
    if (node.evaluatedType && node.evaluatedType->isRValueReference()) {
        node.valueCategory = ValueCategory::XValue;
    } else if (node.evaluatedType && node.evaluatedType->isLValueReference()) {
        node.valueCategory = ValueCategory::LValue;
    } else {
        node.valueCategory = ValueCategory::PRValue;
    }
    return true;
}

bool SemanticAnalyzer::checkIfStmt(AstNode& node) {
    if (node.children.empty() || !node.children[0]) {
        error("If statement missing condition");
        return false;
    }

    if (!analyzeNode(*node.children[0])) return false;

    // If `if constexpr (cond)`, evaluate condition at compile time and only require the active branch!
    if (node.boolVal && currentScope_) {
        auto cVal = evaluateConstexprInt(*node.children[0], *currentScope_);
        if (cVal.has_value()) {
            if (*cVal != 0) {
                if (node.children.size() > 1 && node.children[1]) {
                    return analyzeNode(*node.children[1]);
                }
                return true;
            } else {
                if (node.children.size() > 2 && node.children[2]) {
                    return analyzeNode(*node.children[2]);
                }
                return true;
            }
        }
    }

    bool ok = true;
    if (node.children.size() > 1 && node.children[1]) {
        if (!analyzeNode(*node.children[1])) ok = false;
    }
    if (node.children.size() > 2 && node.children[2]) {
        if (!analyzeNode(*node.children[2])) ok = false;
    }
    return ok;
}

bool SemanticAnalyzer::checkSwitchStmt(AstNode& node) {
    enterScope();
    loopOrSwitchDepth_++;

    size_t condIdx = node.boolVal ? 1 : 0;
    if (node.boolVal && !node.children.empty() && node.children[0]) {
        if (!analyzeNode(*node.children[0])) {
            loopOrSwitchDepth_--;
            exitScope();
            return false;
        }
    }

    if (condIdx >= node.children.size() || !node.children[condIdx]) {
        error("Switch statement missing condition expression", "switch");
        loopOrSwitchDepth_--;
        exitScope();
        return false;
    }

    if (!analyzeNode(*node.children[condIdx])) {
        loopOrSwitchDepth_--;
        exitScope();
        return false;
    }

    auto condType = node.children[condIdx]->evaluatedType;
    if (!condType || !condType->isSwitchConditionType()) {
        error("Switch condition must be of integral or enumeration type (got '" +
              (condType ? condType->toString() : "null") + "')", "switch");
        loopOrSwitchDepth_--;
        exitScope();
        return false;
    }

    std::unordered_set<int64_t> seenCaseValues;
    bool hasDefault = false;
    bool ok = true;

    for (size_t i = condIdx + 1; i < node.children.size(); ++i) {
        auto& branch = node.children[i];
        if (!branch) continue;

        if (branch->kind == AstNodeKind::CaseStmt) {
            if (branch->children.empty() || !branch->children[0]) {
                error("Case label missing constant expression", "case");
                ok = false;
                continue;
            }
            if (!analyzeNode(*branch->children[0])) {
                ok = false;
                continue;
            }
            auto caseType = branch->children[0]->evaluatedType;
            if (!caseType || !caseType->isSwitchConditionType()) {
                error("Case label expression must be of integral or enumeration type", "case");
                ok = false;
                continue;
            }
            auto constVal = evaluateConstexprInt(*branch->children[0], *currentScope_);
            if (!constVal.has_value()) {
                error("Case label expression is not a compile-time constant expression", "case");
                ok = false;
                continue;
            }
            if (!seenCaseValues.insert(*constVal).second) {
                error("Duplicate case value '" + std::to_string(*constVal) + "' in switch statement", "case");
                ok = false;
            }
            branch->intVal = *constVal;

            for (size_t s = 1; s < branch->children.size(); ++s) {
                if (branch->children[s] && !analyzeNode(*branch->children[s])) {
                    ok = false;
                }
            }
        } else if (branch->kind == AstNodeKind::DefaultStmt) {
            if (hasDefault) {
                error("Multiple 'default' labels in one switch statement", "default");
                ok = false;
            }
            hasDefault = true;
            for (auto& st : branch->children) {
                if (st && !analyzeNode(*st)) {
                    ok = false;
                }
            }
        } else {
            if (!analyzeNode(*branch)) ok = false;
        }
    }

    loopOrSwitchDepth_--;
    exitScope();
    return ok;
}

bool SemanticAnalyzer::checkReturnStmt(AstNode& node) {
    if (!node.children.empty() && node.children[0]) {
        if (!analyzeNode(*node.children[0])) return false;
        node.evaluatedType = node.children[0]->evaluatedType;

        // Lifetime & Dangling Reference/Pointer Guard (C++20 [class.temporary]/6.11):
        // Returning a reference or pointer to a local automatic variable or temporary is always a dangling reference!
        if (currentFunctionReturnType_ &&
            (currentFunctionReturnType_->isReference() || currentFunctionReturnType_->isPointer())) {
            const auto& retExpr = *node.children[0];

            // 1. Returning a pure temporary (PRValue) from a function returning T& / const T& / T&&
            if (currentFunctionReturnType_->isReference() && retExpr.valueCategory == ValueCategory::PRValue) {
                error("Lifetime/Borrow violation: Returning a reference to a temporary object (dangling reference)", "return");
                return false;
            }

            // 2. Returning a local automatic variable by reference
            if (currentFunctionReturnType_->isReference() && retExpr.kind == AstNodeKind::IdentifierExpr) {
                auto sym = currentScope_->lookup(retExpr.name);
                if (sym && sym->storageDuration == StorageDuration::Automatic &&
                    sym->scopeDepth >= 1 && !sym->type->isReference()) {
                    error("Lifetime/Borrow violation: Returning a reference to local variable '" +
                          retExpr.name + "' (dangling reference after function return)", retExpr.name);
                    return false;
                }
                if (sym && sym->extendsTemporaryLifetime) {
                    error("Lifetime/Borrow violation: Returning reference '" + retExpr.name +
                          "' bound to a local lifetime-extended temporary", retExpr.name);
                    return false;
                }
            }

            // 3. Returning `&localVar` as pointer
            if (currentFunctionReturnType_->isPointer() &&
                retExpr.kind == AstNodeKind::UnaryExpr && retExpr.op == "&" &&
                !retExpr.children.empty() && retExpr.children[0]->kind == AstNodeKind::IdentifierExpr) {
                auto sym = currentScope_->lookup(retExpr.children[0]->name);
                if (sym && sym->storageDuration == StorageDuration::Automatic && sym->scopeDepth >= 1) {
                    error("Lifetime/Borrow violation: Returning address of local variable '" +
                          sym->name + "' (dangling pointer after function return)", sym->name);
                    return false;
                }
            }
        }
    } else {
        node.evaluatedType = CppType::makeVoid();
    }

    // Emit RAII destructors for active function scopes upon return
    emitScopeDestructorsUpTo(1);
    return true;
}

std::optional<int64_t> SemanticAnalyzer::evaluateConstexprInt(const AstNode& expr, const Scope& scope) {
    if (expr.kind == AstNodeKind::LiteralExpr && expr.evaluatedType &&
        (expr.evaluatedType->isInteger() || expr.evaluatedType->isEnum())) {
        return expr.intVal;
    }
    if (expr.kind == AstNodeKind::IdentifierExpr) {
        auto sym = scope.lookup(expr.name);
        if (sym && sym->isConstexpr) {
            return sym->constexprIntVal;
        }
        return std::nullopt;
    }
    if (expr.kind == AstNodeKind::UnaryExpr && !expr.children.empty() && expr.children[0]) {
        auto val = evaluateConstexprInt(*expr.children[0], scope);
        if (!val) return std::nullopt;
        if (expr.op == "+") return *val;
        if (expr.op == "-") return -(*val);
        if (expr.op == "~") return ~(*val);
        if (expr.op == "!") return (*val == 0) ? 1 : 0;
    }
    if (expr.kind == AstNodeKind::TernaryExpr && expr.children.size() == 3) {
        auto cond = evaluateConstexprInt(*expr.children[0], scope);
        if (!cond) return std::nullopt;
        return (*cond != 0) ? evaluateConstexprInt(*expr.children[1], scope)
                            : evaluateConstexprInt(*expr.children[2], scope);
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
        if (expr.op == "&") return *left & *right;
        if (expr.op == "|") return *left | *right;
        if (expr.op == "^") return *left ^ *right;
        if (expr.op == "<<") return (*right >= 0 && *right < 64) ? std::optional<int64_t>(*left << *right) : std::nullopt;
        if (expr.op == ">>") return (*right >= 0 && *right < 64) ? std::optional<int64_t>(*left >> *right) : std::nullopt;
        if (expr.op == "==") return (*left == *right) ? 1 : 0;
        if (expr.op == "!=") return (*left != *right) ? 1 : 0;
        if (expr.op == "<") return (*left < *right) ? 1 : 0;
        if (expr.op == "<=") return (*left <= *right) ? 1 : 0;
        if (expr.op == ">") return (*left > *right) ? 1 : 0;
        if (expr.op == ">=") return (*left >= *right) ? 1 : 0;
        if (expr.op == "&&") return (*left != 0 && *right != 0) ? 1 : 0;
        if (expr.op == "||") return (*left != 0 || *right != 0) ? 1 : 0;
    }
    return std::nullopt;
}

} // namespace genomic
