#include "cpp20/cpp20_templates.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <numeric>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace genomic {

namespace {
std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string stripConceptTemplateArgs(const std::string& raw) {
    std::string t = trim(raw);
    if (t.rfind("std::", 0) == 0) t = t.substr(5);
    size_t lt = t.find('<');
    if (lt != std::string::npos) {
        return trim(t.substr(0, lt));
    }
    return t;
}

std::vector<std::string> splitTopLevel(const std::string& expr, const std::string& delim) {
    std::vector<std::string> parts;
    int parenDepth = 0;
    int angleDepth = 0;
    size_t last = 0;
    for (size_t i = 0; i < expr.size(); ++i) {
        if (expr[i] == '(') parenDepth++;
        else if (expr[i] == ')' && parenDepth > 0) parenDepth--;
        else if (expr[i] == '<') angleDepth++;
        else if (expr[i] == '>' && angleDepth > 0) angleDepth--;
        else if (parenDepth == 0 && angleDepth == 0 &&
                 i + delim.size() <= expr.size() &&
                 expr.compare(i, delim.size(), delim) == 0) {
            parts.push_back(trim(expr.substr(last, i - last)));
            i += delim.size() - 1;
            last = i + 1;
        }
    }
    parts.push_back(trim(expr.substr(last)));
    return parts;
}
} // namespace

void TemplateEngine::registerConcept(const ConceptDefinition& conceptDef) {
    concepts_[conceptDef.name] = conceptDef;
}

bool TemplateEngine::checkConstraint(const std::string& conceptExpression,
                                     const std::vector<std::shared_ptr<CppType>>& types) const {
    std::string expr = trim(conceptExpression);
    if (expr.empty() || expr == "Any" || expr == "true") return true;
    if (expr == "false") return false;

    // Handle top-level disjunctions `A || B`
    auto disjParts = splitTopLevel(expr, "||");
    if (disjParts.size() > 1) {
        for (const auto& d : disjParts) {
            if (checkConstraint(d, types)) return true;
        }
        return false;
    }

    // Handle top-level conjunctions `A && B`
    auto conjParts = splitTopLevel(expr, "&&");
    if (conjParts.size() > 1) {
        for (const auto& c : conjParts) {
            if (!checkConstraint(c, types)) return false;
        }
        return true;
    }

    // Handle outer parentheses
    if (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')') {
        int depth = 0;
        bool wrapsAll = true;
        for (size_t i = 0; i + 1 < expr.size(); ++i) {
            if (expr[i] == '(') depth++;
            else if (expr[i] == ')') depth--;
            if (depth == 0) { wrapsAll = false; break; }
        }
        if (wrapsAll) {
            return checkConstraint(expr.substr(1, expr.size() - 2), types);
        }
    }

    // Negation `!Concept<T>`
    if (!expr.empty() && expr.front() == '!') {
        return !checkConstraint(expr.substr(1), types);
    }

    std::string conceptName = stripConceptTemplateArgs(expr);

    // Check registered custom concept definitions first
    auto it = concepts_.find(conceptName);
    if (it != concepts_.end()) {
        if (!it->second.rawConstraintExpression.empty()) {
            if (!checkConstraint(it->second.rawConstraintExpression, types)) return false;
        }
        if (it->second.predicate) {
            return it->second.predicate(types);
        }
        return true;
    }

    // Check genomic rule engine if connected
    if (!types.empty() && types[0]) {
        auto target = types[0]->removeCVRef();
        if (ruleEngine_ && ruleEngine_->checkConceptSatisfaction(conceptName, target->kind)) {
            return true;
        }
        // Standard C++20 <concepts> built-in evaluation
        if (conceptName == "integral" || conceptName == "Integral") {
            return target->isInteger();
        }
        if (conceptName == "signed_integral" || conceptName == "SignedIntegral") {
            return target->isSignedInteger();
        }
        if (conceptName == "unsigned_integral" || conceptName == "UnsignedIntegral") {
            return target->isUnsignedInteger() && target->kind != TypeKind::Bool;
        }
        if (conceptName == "floating_point" || conceptName == "FloatingPoint") {
            return target->isFloatingPoint();
        }
        if (conceptName == "arithmetic" || conceptName == "Arithmetic") {
            return target->isArithmetic();
        }
        if (conceptName == "same_as" && types.size() >= 2 && types[1]) {
            return types[0]->isEquivalentTo(*types[1]);
        }
        if (conceptName == "convertible_to" && types.size() >= 2 && types[1]) {
            return TypeSystem::canImplicitlyConvert(*types[0], *types[1]);
        }
        if (conceptName == "equality_comparable" || conceptName == "totally_ordered" ||
            conceptName == "regular" || conceptName == "semiregular" ||
            conceptName == "copy_constructible" || conceptName == "move_constructible" ||
            conceptName == "destructible" || conceptName == "default_initializable") {
            return !target->isVoid();
        }
    }

    return false;
}

bool TemplateEngine::evaluateRequiresExpression(
    const std::vector<RequiresRequirement>& requirements,
    const std::shared_ptr<CppType>& targetType
) const {
    if (!targetType || targetType->isVoid()) return false;
    auto unqual = targetType->removeCVRef();

    for (const auto& req : requirements) {
        switch (req.kind) {
            case RequiresRequirement::Kind::Simple:
            case RequiresRequirement::Kind::Compound: {
                if ((req.expression.find('+') != std::string::npos ||
                     req.expression.find('-') != std::string::npos ||
                     req.expression.find('*') != std::string::npos) &&
                    !unqual->isArithmetic() && !unqual->isPointer() && !unqual->isRecord()) {
                    return false;
                }
                if (!req.returnConceptConstraint.empty()) {
                    if (!checkConstraint(req.returnConceptConstraint, {unqual})) {
                        return false;
                    }
                }
                break;
            }
            case RequiresRequirement::Kind::Type: {
                if (unqual->isVoid()) return false;
                break;
            }
            case RequiresRequirement::Kind::Nested: {
                if (!checkConstraint(req.expression, {unqual})) {
                    return false;
                }
                break;
            }
        }
    }
    return true;
}

NormalizedConstraint TemplateEngine::normalizeConstraint(const std::string& constraintExpr) const {
    NormalizedConstraint result;
    std::string expr = trim(constraintExpr);
    if (expr.empty() || expr == "Any" || expr == "true") {
        return result;
    }

    // Outer parens unwrap
    while (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')') {
        int depth = 0;
        bool wrapsAll = true;
        for (size_t i = 0; i + 1 < expr.size(); ++i) {
            if (expr[i] == '(') depth++;
            else if (expr[i] == ')') depth--;
            if (depth == 0) { wrapsAll = false; break; }
        }
        if (wrapsAll) expr = trim(expr.substr(1, expr.size() - 2));
        else break;
    }

    // 1. Disjunctions: Norm(P || Q) = Norm(P) U Norm(Q)
    auto disjParts = splitTopLevel(expr, "||");
    if (disjParts.size() > 1) {
        for (const auto& part : disjParts) {
            auto sub = normalizeConstraint(part);
            for (auto& clause : sub.disjunctionOfConjunctions) {
                result.disjunctionOfConjunctions.push_back(std::move(clause));
            }
        }
        return result;
    }

    // 2. Conjunctions: Norm(P && Q) = cross-product of conjunction clauses
    auto conjParts = splitTopLevel(expr, "&&");
    if (conjParts.size() > 1) {
        result.disjunctionOfConjunctions.push_back({});
        for (const auto& part : conjParts) {
            auto sub = normalizeConstraint(part);
            if (sub.disjunctionOfConjunctions.empty()) continue;
            std::vector<std::vector<AtomicConstraint>> combined;
            for (const auto& existing : result.disjunctionOfConjunctions) {
                for (const auto& nextClause : sub.disjunctionOfConjunctions) {
                    auto merged = existing;
                    merged.insert(merged.end(), nextClause.begin(), nextClause.end());
                    combined.push_back(std::move(merged));
                }
            }
            result.disjunctionOfConjunctions = std::move(combined);
        }
        return result;
    }

    // 3. Single concept or atomic constraint: expand concept definition if registered!
    bool isNeg = false;
    if (!expr.empty() && expr.front() == '!') {
        isNeg = true;
        expr = trim(expr.substr(1));
    }

    std::string conceptName = stripConceptTemplateArgs(expr);
    if (!isNeg) {
        auto it = concepts_.find(conceptName);
        if (it != concepts_.end() && !it->second.rawConstraintExpression.empty()) {
            return normalizeConstraint(it->second.rawConstraintExpression);
        }
        // Standard C++20 concept decompositions into atomic constraints:
        if (conceptName == "signed_integral" || conceptName == "SignedIntegral") {
            AtomicConstraint c1{"integral", {"T"}, false};
            AtomicConstraint c2{"signed_integral", {"T"}, false};
            result.disjunctionOfConjunctions.push_back({c1, c2});
            return result;
        }
        if (conceptName == "unsigned_integral" || conceptName == "UnsignedIntegral") {
            AtomicConstraint c1{"integral", {"T"}, false};
            AtomicConstraint c2{"unsigned_integral", {"T"}, false};
            result.disjunctionOfConjunctions.push_back({c1, c2});
            return result;
        }
    }

    AtomicConstraint atom;
    atom.conceptOrPredicate = conceptName;
    atom.paramArgs = {"T"};
    atom.isNegated = isNeg;
    result.disjunctionOfConjunctions.push_back({atom});
    return result;
}

bool TemplateEngine::atomicImplies(const AtomicConstraint& a, const AtomicConstraint& b) const {
    if (a.isNegated != b.isNegated) return false;
    if (a.conceptOrPredicate == b.conceptOrPredicate) return true;
    if (a.isNegated) return false;

    if (ruleEngine_ && ruleEngine_->subsumes(a.conceptOrPredicate, b.conceptOrPredicate)) {
        return true;
    }

    // Standard C++20 concept hierarchy implications (case-insensitive / alias-aware)
    const auto& ca = a.conceptOrPredicate;
    const auto& cb = b.conceptOrPredicate;
    if ((ca == "integral" && cb == "Integral") || (ca == "Integral" && cb == "integral")) return true;
    if ((ca == "arithmetic" && cb == "Arithmetic") || (ca == "Arithmetic" && cb == "arithmetic")) return true;
    if ((ca == "floating_point" && cb == "FloatingPoint") || (ca == "FloatingPoint" && cb == "floating_point")) return true;
    if ((ca == "signed_integral" || ca == "unsigned_integral" || ca == "SignedIntegral" || ca == "UnsignedIntegral") &&
        (cb == "integral" || cb == "arithmetic" || cb == "Integral" || cb == "Arithmetic")) return true;
    if ((ca == "integral" || ca == "floating_point" || ca == "Integral" || ca == "FloatingPoint") &&
        (cb == "arithmetic" || cb == "Arithmetic")) return true;
    if (ca == "totally_ordered" && cb == "equality_comparable") return true;
    if (ca == "regular" && (cb == "semiregular" || cb == "equality_comparable" || cb == "copy_constructible" || cb == "default_initializable")) return true;
    return false;
}

bool TemplateEngine::conjunctionImplies(const std::vector<AtomicConstraint>& conjA,
                                        const std::vector<AtomicConstraint>& conjB) const {
    // Conjunction A implies Conjunction B iff for every atomic b in B, there is some atomic a in A that implies b
    for (const auto& b : conjB) {
        bool covered = false;
        for (const auto& a : conjA) {
            if (atomicImplies(a, b)) {
                covered = true;
                break;
            }
        }
        if (!covered) return false;
    }
    return true;
}

bool TemplateEngine::isMoreConstrained(const std::string& constraintA, const std::string& constraintB) const {
    std::string cleanA = trim(constraintA);
    std::string cleanB = trim(constraintB);
    if (cleanA == cleanB) return false;
    if (!cleanA.empty() && cleanA != "Any" && (cleanB.empty() || cleanB == "Any")) {
        return true; // Constrained template always subsumes unconstrained template
    }
    if (cleanA.empty() || cleanA == "Any") {
        return false;
    }

    // Direct genomic rule engine check for single-concept names
    if (ruleEngine_ && cleanA.find("&&") == std::string::npos && cleanA.find("||") == std::string::npos &&
        cleanB.find("&&") == std::string::npos && cleanB.find("||") == std::string::npos) {
        std::string bareA = stripConceptTemplateArgs(cleanA);
        std::string bareB = stripConceptTemplateArgs(cleanB);
        if (bareA != bareB && ruleEngine_->subsumes(bareA, bareB) && !ruleEngine_->subsumes(bareB, bareA)) {
            return true;
        }
    }

    auto normA = normalizeConstraint(cleanA);
    auto normB = normalizeConstraint(cleanB);
    if (normA.isUnconstrained()) return false;
    if (normB.isUnconstrained()) return true;

    // A subsumes B iff EVERY conjunction clause of A implies AT LEAST ONE conjunction clause of B
    auto subsumesDNF = [this](const NormalizedConstraint& lhs, const NormalizedConstraint& rhs) {
        for (const auto& conjA : lhs.disjunctionOfConjunctions) {
            bool impliesSomeB = false;
            for (const auto& conjB : rhs.disjunctionOfConjunctions) {
                if (conjunctionImplies(conjA, conjB)) {
                    impliesSomeB = true;
                    break;
                }
            }
            if (!impliesSomeB) return false;
        }
        return true;
    };

    bool aSubsumesB = subsumesDNF(normA, normB);
    bool bSubsumesA = subsumesDNF(normB, normA);
    return aSubsumesB && !bSubsumesA;
}

int64_t TemplateEngine::evaluateFoldSum(const std::vector<int64_t>& values) {
    return std::accumulate(values.begin(), values.end(), 0LL);
}

int64_t TemplateEngine::evaluateFoldProduct(const std::vector<int64_t>& values) {
    return std::accumulate(values.begin(), values.end(), 1LL, std::multiplies<int64_t>());
}

bool TemplateEngine::evaluateFoldAll(const std::vector<bool>& values) {
    for (bool b : values) {
        if (!b) return false;
    }
    return true;
}

bool TemplateEngine::evaluateFoldAny(const std::vector<bool>& values) {
    for (bool b : values) {
        if (b) return true;
    }
    return false;
}

bool TemplateEngine::deduceFromTypePattern(
    const CppType& pattern,
    const CppType& argType,
    ValueCategory argCat,
    const std::unordered_set<std::string>& templateParamNames,
    std::unordered_map<std::string, std::shared_ptr<CppType>>& deducedTypes,
    std::unordered_map<std::string, int64_t>& deducedNonTypes,
    std::string& outError,
    bool isTopLevelParam
) {
    // Non-deduced context check (e.g. std::type_identity_t<T>)
    if (pattern.isNonDeduced) {
        return true; // Succeeds without deducing anything from this argument
    }

    auto bindConsistent = [&](const std::string& paramName, std::shared_ptr<CppType> candidate) -> bool {
        if (!candidate) return false;
        auto it = deducedTypes.find(paramName);
        if (it != deducedTypes.end() && it->second) {
            if (!it->second->isEquivalentTo(*candidate)) {
                outError = "SFINAE: Conflicting types deduced for template parameter '" + paramName +
                           "': '" + it->second->toString() + "' vs '" + candidate->toString() + "'";
                return false;
            }
            return true;
        }
        deducedTypes[paramName] = std::move(candidate);
        return true;
    };

    // Case 1: Pattern is a reference type (`P&` or `P&&`)
    if (pattern.isReference() && pattern.pointeeType) {
        const CppType& innerPattern = *pattern.pointeeType;
        auto unwrappedArg = argType.removeReference();

        // Check for Forwarding Reference (`T&&` where `T` is an cv-unqualified template parameter)
        bool isForwarding = pattern.isRValueReference() &&
                            (innerPattern.kind == TypeKind::TemplateParam || templateParamNames.contains(innerPattern.name)) &&
                            !innerPattern.isConst && !innerPattern.isVolatile;
        if (isForwarding) {
            // C++20 [temp.deduct.call]/3: If the argument is an lvalue, `T` is deduced as `A&`!
            // By reference collapsing, `A& &&` becomes `A&`. If rvalue, `T` is deduced as `A`.
            bool isLValueArg = (argCat == ValueCategory::LValue) || argType.isLValueReference();
            std::shared_ptr<CppType> deduced = isLValueArg ? CppType::makeLValueRef(unwrappedArg) : unwrappedArg;
            return bindConsistent(innerPattern.name, deduced);
        }

        // Standard lvalue or rvalue reference pattern (`T&`, `const T&`, `const T&&`)
        if (pattern.isRValueReference() && argCat == ValueCategory::LValue && !argType.isRValueReference()) {
            outError = "SFINAE: Cannot bind rvalue reference pattern to lvalue argument";
            return false;
        }
        if (pattern.isLValueReference() && !innerPattern.isConst && argCat != ValueCategory::LValue && !argType.isLValueReference()) {
            outError = "SFINAE: Cannot bind non-const lvalue reference pattern to rvalue argument";
            return false;
        }
        // If innerPattern is `const T`, strip `const` from unwrappedArg when deducing `T`
        if ((innerPattern.kind == TypeKind::TemplateParam || templateParamNames.contains(innerPattern.name))) {
            if (!innerPattern.isConst && unwrappedArg->isConst && pattern.isLValueReference()) {
                // `T&` bound to `const int` -> `T` is deduced as `const int`!
                return bindConsistent(innerPattern.name, unwrappedArg);
            }
            auto stripped = std::make_shared<CppType>(*unwrappedArg);
            if (innerPattern.isConst) stripped->isConst = false;
            if (innerPattern.isVolatile) stripped->isVolatile = false;
            return bindConsistent(innerPattern.name, stripped);
        }
        return deduceFromTypePattern(innerPattern, *unwrappedArg, argCat, templateParamNames, deducedTypes, deducedNonTypes, outError, false);
    }

    // Case 2: Pattern is a bare template parameter `T` or `const T`
    if (pattern.kind == TypeKind::TemplateParam || templateParamNames.contains(pattern.name)) {
        auto candidate = isTopLevelParam ? argType.decay() : argType.removeReference();
        if (!isTopLevelParam) {
            if (pattern.isConst && !candidate->isConst) {
                outError = "SFINAE: Argument lacks required const qualifier for pattern";
                return false;
            }
            auto stripped = std::make_shared<CppType>(*candidate);
            if (pattern.isConst) stripped->isConst = false;
            if (pattern.isVolatile) stripped->isVolatile = false;
            candidate = stripped;
        }
        return bindConsistent(pattern.name, candidate);
    }

    // Case 3: Pattern is a pointer `P*`
    if (pattern.isPointer() && pattern.pointeeType) {
        auto decayedArg = isTopLevelParam ? argType.decay() : argType.removeReference();
        if (!decayedArg->isPointer() || !decayedArg->pointeeType) {
            outError = "SFINAE: Expected pointer argument for pointer pattern '" + pattern.toString() + "'";
            return false;
        }
        return deduceFromTypePattern(*pattern.pointeeType, *decayedArg->pointeeType, ValueCategory::PRValue,
                                     templateParamNames, deducedTypes, deducedNonTypes, outError, false);
    }

    // Case 4: Pattern is an array `P[N]`
    if (pattern.kind == TypeKind::Array && pattern.pointeeType) {
        auto nonRefArg = argType.removeReference();
        if (nonRefArg->kind != TypeKind::Array || !nonRefArg->pointeeType) {
            outError = "SFINAE: Expected array argument for array pattern";
            return false;
        }
        if (pattern.arraySize != 0 && nonRefArg->arraySize != 0 && pattern.arraySize != nonRefArg->arraySize) {
            outError = "SFINAE: Array bound mismatch";
            return false;
        }
        if (nonRefArg->arraySize != 0) {
            deducedNonTypes["N"] = static_cast<int64_t>(nonRefArg->arraySize);
        }
        return deduceFromTypePattern(*pattern.pointeeType, *nonRefArg->pointeeType, ValueCategory::PRValue,
                                     templateParamNames, deducedTypes, deducedNonTypes, outError, false);
    }

    // Case 5: Pattern is a template specialization `Record<P1, P2, ...>`
    if (pattern.kind == TypeKind::Record && !pattern.templateArgs.empty()) {
        auto nonRefArg = argType.removeCVRef();
        if (nonRefArg->kind != TypeKind::Record || nonRefArg->name != pattern.name ||
            nonRefArg->templateArgs.size() != pattern.templateArgs.size()) {
            outError = "SFINAE: Template specialization mismatch for '" + pattern.name + "'";
            return false;
        }
        for (size_t i = 0; i < pattern.templateArgs.size(); ++i) {
            if (!pattern.templateArgs[i] || !nonRefArg->templateArgs[i]) return false;
            if (!deduceFromTypePattern(*pattern.templateArgs[i], *nonRefArg->templateArgs[i], ValueCategory::PRValue,
                                       templateParamNames, deducedTypes, deducedNonTypes, outError, false)) {
                return false;
            }
        }
        return true;
    }

    // Case 6: Concrete non-dependent parameter type (must be convertible or identical)
    return true;
}

std::shared_ptr<CppType> TemplateEngine::substituteTypePattern(
    const std::shared_ptr<CppType>& pattern,
    const std::unordered_map<std::string, std::shared_ptr<CppType>>& typeMap,
    const std::unordered_map<std::string, int64_t>& nonTypeMap
) {
    if (!pattern) return CppType::makeVoid();

    if (pattern->kind == TypeKind::TemplateParam || typeMap.contains(pattern->name)) {
        auto it = typeMap.find(pattern->name);
        if (it != typeMap.end() && it->second) {
            auto sub = it->second;
            if (pattern->isConst || pattern->isVolatile) {
                if (sub->isReference() && sub->pointeeType) {
                    // `const T` where `T = int&` remains `int&` in C++20!
                    return sub;
                }
                return sub->withCV(pattern->isConst, pattern->isVolatile);
            }
            return sub;
        }
    }

    if (pattern->isLValueReference() && pattern->pointeeType) {
        auto subTarget = substituteTypePattern(pattern->pointeeType, typeMap, nonTypeMap);
        return CppType::collapseReference(subTarget, true);
    }
    if (pattern->isRValueReference() && pattern->pointeeType) {
        auto subTarget = substituteTypePattern(pattern->pointeeType, typeMap, nonTypeMap);
        return CppType::collapseReference(subTarget, false);
    }
    if (pattern->isPointer() && pattern->pointeeType) {
        auto subTarget = substituteTypePattern(pattern->pointeeType, typeMap, nonTypeMap);
        return CppType::makePointer(subTarget, pattern->isConst);
    }
    if (pattern->kind == TypeKind::Array && pattern->pointeeType) {
        auto subElem = substituteTypePattern(pattern->pointeeType, typeMap, nonTypeMap);
        size_t len = pattern->arraySize;
        if (len == 0 && nonTypeMap.contains("N")) {
            len = static_cast<size_t>(nonTypeMap.at("N"));
        }
        return CppType::makeArray(subElem, len);
    }
    if (pattern->kind == TypeKind::Record && !pattern->templateArgs.empty()) {
        std::vector<std::shared_ptr<CppType>> subArgs;
        for (const auto& tArg : pattern->templateArgs) {
            subArgs.push_back(substituteTypePattern(tArg, typeMap, nonTypeMap));
        }
        auto rec = CppType::makeRecord(pattern->name, std::move(subArgs));
        rec->isConst = pattern->isConst;
        rec->isVolatile = pattern->isVolatile;
        return rec;
    }

    return std::make_shared<CppType>(*pattern);
}

TemplateEngine::InstantiationResult TemplateEngine::deduceAndSubstituteFunctionTemplate(
    const std::string& templateName,
    const std::vector<TemplateParam>& templateParams,
    const std::vector<std::shared_ptr<CppType>>& fnParamPatterns,
    const std::shared_ptr<CppType>& fnReturnPattern,
    const std::vector<std::shared_ptr<CppType>>& callArgTypes,
    const std::vector<ValueCategory>& callArgValueCategories,
    const std::vector<std::shared_ptr<CppType>>& explicitTemplateArgs,
    const std::string& requiresClause
) const {
    (void)templateName;
    InstantiationResult res;
    if (templateParams.empty()) {
        res.success = false;
        res.substitutionError = "SFINAE: Empty template parameter list";
        return res;
    }

    std::unordered_set<std::string> paramNames;
    bool hasParameterPack = false;
    std::string packParamName;
    for (const auto& tp : templateParams) {
        paramNames.insert(tp.name);
        if (tp.isParameterPack) {
            hasParameterPack = true;
            packParamName = tp.name;
        }
    }

    // 1. Bind explicit template arguments if provided (`fn<int>(x)`)
    for (size_t i = 0; i < explicitTemplateArgs.size() && i < templateParams.size(); ++i) {
        if (explicitTemplateArgs[i]) {
            res.deducedTypeMap[templateParams[i].name] = explicitTemplateArgs[i];
        }
    }

    // 2. Check arity (unless variadic parameter pack)
    if (!hasParameterPack && callArgTypes.size() != fnParamPatterns.size()) {
        res.success = false;
        res.substitutionError = "SFINAE: Argument count mismatch in function template call";
        return res;
    }

    // 3. Deduce template parameters from each function argument
    for (size_t i = 0; i < callArgTypes.size(); ++i) {
        if (!callArgTypes[i]) {
            res.success = false;
            res.substitutionError = "SFINAE: Null call argument type";
            return res;
        }
        ValueCategory cat = (i < callArgValueCategories.size()) ? callArgValueCategories[i] : ValueCategory::PRValue;

        if (i < fnParamPatterns.size() && fnParamPatterns[i]) {
            const auto& pat = *fnParamPatterns[i];
            if (pat.isPackExpansion && !packParamName.empty()) {
                res.deducedPackMap[packParamName].push_back(callArgTypes[i]);
                if (!res.deducedTypeMap.contains(packParamName)) {
                    res.deducedTypeMap[packParamName] = callArgTypes[i];
                }
                continue;
            }
            if (!deduceFromTypePattern(pat, *callArgTypes[i], cat, paramNames,
                                       res.deducedTypeMap, res.deducedNonTypeMap, res.substitutionError, true)) {
                res.success = false;
                return res;
            }
        } else if (hasParameterPack && !packParamName.empty()) {
            res.deducedPackMap[packParamName].push_back(callArgTypes[i]);
            if (!res.deducedTypeMap.contains(packParamName)) {
                res.deducedTypeMap[packParamName] = callArgTypes[i];
            }
        }
    }

    // 4. Fill in default template arguments for any undeduced parameters
    for (const auto& tp : templateParams) {
        if (!res.deducedTypeMap.contains(tp.name)) {
            if (tp.defaultType) {
                res.deducedTypeMap[tp.name] = tp.defaultType;
            } else if (tp.isParameterPack) {
                res.deducedPackMap[tp.name] = {};
            } else {
                res.success = false;
                res.substitutionError = "SFINAE: Could not deduce template parameter '" + tp.name + "'";
                return res;
            }
        }
        const auto it = res.deducedTypeMap.find(tp.name);
        if (it != res.deducedTypeMap.end() && it->second && it->second->isVoid()) {
            res.success = false;
            res.substitutionError = "SFINAE: Deduction to void is invalid in value parameter context";
            return res;
        }
        // Check type-parameter concept constraint (`template<std::integral T>`)
        if (!tp.conceptConstraint.empty() && it != res.deducedTypeMap.end() && it->second) {
            if (!checkConstraint(tp.conceptConstraint, {it->second})) {
                res.success = false;
                res.substitutionError = "SFINAE: Constraint '" + tp.conceptConstraint + "' not satisfied for '" + tp.name + "'";
                return res;
            }
        }
    }

    // 5. Check function-level requires clause (`requires Concept<T>`)
    if (!requiresClause.empty()) {
        std::vector<std::shared_ptr<CppType>> orderedArgs;
        for (const auto& tp : templateParams) {
            if (res.deducedTypeMap.contains(tp.name)) {
                orderedArgs.push_back(res.deducedTypeMap.at(tp.name));
            }
        }
        if (!checkConstraint(requiresClause, orderedArgs)) {
            res.success = false;
            res.substitutionError = "SFINAE: Associated requires-clause '" + requiresClause + "' not satisfied";
            return res;
        }
    }

    // 6. Substitute into parameter patterns and return type
    for (size_t i = 0; i < fnParamPatterns.size(); ++i) {
        if (fnParamPatterns[i] && fnParamPatterns[i]->isPackExpansion && !packParamName.empty()) {
            for (const auto& packElem : res.deducedPackMap[packParamName]) {
                res.substitutedParamTypes.push_back(packElem);
            }
        } else {
            auto subParam = substituteTypePattern(fnParamPatterns[i], res.deducedTypeMap, res.deducedNonTypeMap);
            if (subParam->isVoid()) {
                res.success = false;
                res.substitutionError = "SFINAE: Substituted function parameter type cannot be void";
                return res;
            }
            res.substitutedParamTypes.push_back(std::move(subParam));
        }
    }
    res.substitutedReturnType = fnReturnPattern
        ? substituteTypePattern(fnReturnPattern, res.deducedTypeMap, res.deducedNonTypeMap)
        : CppType::makeVoid();

    res.deducedType = res.deducedTypeMap.contains(templateParams[0].name)
        ? res.deducedTypeMap.at(templateParams[0].name)
        : nullptr;
    res.success = true;
    return res;
}

TemplateEngine::InstantiationResult TemplateEngine::instantiateFunctionTemplate(
    const std::string& templateName,
    const std::vector<std::shared_ptr<CppType>>& templateParams,
    const std::vector<std::shared_ptr<CppType>>& argumentTypes
) {
    if (templateParams.empty() || argumentTypes.empty()) {
        InstantiationResult res;
        res.success = false;
        res.substitutionError = "SFINAE: Empty template or argument parameters";
        return res;
    }

    std::vector<TemplateParam> tParams;
    std::vector<std::shared_ptr<CppType>> patterns;
    for (size_t i = 0; i < templateParams.size(); ++i) {
        std::string pName = (templateParams[i] && !templateParams[i]->name.empty())
            ? templateParams[i]->name
            : ("T" + std::to_string(i));
        tParams.push_back(TemplateParam{pName, true, false, nullptr, 0, false, ""});
        if (templateParams[i] && (templateParams[i]->isPointer() || templateParams[i]->isReference() ||
                                  templateParams[i]->kind == TypeKind::Array || templateParams[i]->kind == TypeKind::Record)) {
            patterns.push_back(templateParams[i]);
        } else {
            patterns.push_back(CppType::makeTemplateParam(pName));
        }
    }
    while (patterns.size() < argumentTypes.size()) {
        patterns.push_back(patterns.back());
    }

    return deduceAndSubstituteFunctionTemplate(
        templateName, tParams, patterns, patterns[0], argumentTypes);
}

int TemplateEngine::compareFunctionTemplateSpecialization(
    const std::vector<TemplateParam>& paramsA,
    const std::vector<std::shared_ptr<CppType>>& fnParamsA,
    const std::string& constraintA,
    const std::vector<TemplateParam>& paramsB,
    const std::vector<std::shared_ptr<CppType>>& fnParamsB,
    const std::string& constraintB
) const {
    // C++20 [temp.func.order] Partial Ordering of Function Templates:
    // Synthesize unique placeholder types for template A's parameters, substitute into A's parameter list,
    // and try to deduce template B's parameters from A's synthesized parameter types (and vice versa).
    auto canDeduceTargetFromSource = [&](const std::vector<TemplateParam>& srcTParams,
                                         const std::vector<std::shared_ptr<CppType>>& srcFnParams,
                                         const std::vector<TemplateParam>& dstTParams,
                                         const std::vector<std::shared_ptr<CppType>>& dstFnParams) -> bool {
        if (srcFnParams.size() != dstFnParams.size()) return false;
        std::unordered_map<std::string, std::shared_ptr<CppType>> synthMap;
        for (size_t i = 0; i < srcTParams.size(); ++i) {
            synthMap[srcTParams[i].name] = CppType::makeRecord("__UniqueSynthParam_" + std::to_string(i));
        }
        std::unordered_set<std::string> dstNames;
        for (const auto& tp : dstTParams) dstNames.insert(tp.name);

        std::unordered_map<std::string, std::shared_ptr<CppType>> deduced;
        std::unordered_map<std::string, int64_t> deducedNT;
        std::string err;
        for (size_t i = 0; i < srcFnParams.size(); ++i) {
            auto synthesizedArg = substituteTypePattern(srcFnParams[i], synthMap);
            // For partial ordering of reference parameters: if dst is non-reference or less-qualified reference
            if (dstFnParams[i]->isReference() && !synthesizedArg->isReference()) {
                return false;
            }
            if (dstFnParams[i]->isReference() && synthesizedArg->isReference() &&
                dstFnParams[i]->pointeeType && synthesizedArg->pointeeType) {
                if (dstFnParams[i]->pointeeType->isConst && !synthesizedArg->pointeeType->isConst) {
                    return false; // `const T&` cannot be deduced from non-const `U&` as more specialized
                }
            }
            ValueCategory cat = synthesizedArg->isLValueReference() ? ValueCategory::LValue : ValueCategory::PRValue;
            if (!deduceFromTypePattern(*dstFnParams[i], *synthesizedArg, cat, dstNames, deduced, deducedNT, err, false)) {
                return false;
            }
        }
        return true;
    };

    bool bDeducesFromA = canDeduceTargetFromSource(paramsA, fnParamsA, paramsB, fnParamsB);
    bool aDeducesFromB = canDeduceTargetFromSource(paramsB, fnParamsB, paramsA, fnParamsA);

    if (bDeducesFromA && !aDeducesFromB) {
        return -1; // A is strictly more specialized than B (e.g. `T*` vs `T`, or `const T&` vs `T&`)
    }
    if (aDeducesFromB && !bDeducesFromA) {
        return 1;  // B is strictly more specialized than A
    }

    // If neither is more specialized by type structure, C++20 [temp.func.order]/6 checks constraint subsumption!
    if (isMoreConstrained(constraintA, constraintB)) return -1;
    if (isMoreConstrained(constraintB, constraintA)) return 1;
    return 0;
}

void TemplateEngine::registerDeductionGuide(const DeductionGuide& guide) {
    deductionGuides_[guide.className].push_back(guide);
}

TemplateEngine::InstantiationResult TemplateEngine::deduceClassTemplateArguments(
    const std::string& className,
    const std::vector<std::shared_ptr<CppType>>& ctorArgTypes,
    const std::vector<ValueCategory>& ctorArgCategories
) const {
    auto it = deductionGuides_.find(className);
    if (it != deductionGuides_.end()) {
        for (const auto& guide : it->second) {
            auto retPattern = CppType::makeRecord(className, guide.deducedClassTemplateArgs);
            auto res = deduceAndSubstituteFunctionTemplate(
                className, guide.templateParams, guide.constructorParamPatterns,
                retPattern, ctorArgTypes, ctorArgCategories);
            if (res.success) {
                res.deducedType = res.substitutedReturnType;
                return res;
            }
        }
    }

    // Implicit CTAD from aggregate/constructor arguments: ClassName<Arg0, Arg1, ...>
    InstantiationResult implicitRes;
    if (ctorArgTypes.empty()) {
        implicitRes.success = false;
        implicitRes.substitutionError = "CTAD: Cannot deduce class template arguments from empty argument list without default";
        return implicitRes;
    }
    std::vector<std::shared_ptr<CppType>> decayedArgs;
    for (const auto& arg : ctorArgTypes) {
        if (!arg || arg->isVoid()) {
            implicitRes.success = false;
            implicitRes.substitutionError = "CTAD: Invalid constructor argument type";
            return implicitRes;
        }
        decayedArgs.push_back(arg->decay());
    }
    implicitRes.success = true;
    implicitRes.substitutedReturnType = CppType::makeRecord(className, decayedArgs);
    implicitRes.deducedType = implicitRes.substitutedReturnType;
    return implicitRes;
}

void TemplateEngine::registerSpecialization(const std::string& templateName, const TemplateSpecialization& spec) {
    specializations_[templateName].push_back(spec);
}

const TemplateSpecialization* TemplateEngine::findBestSpecialization(
    const std::string& templateName,
    const std::vector<std::shared_ptr<CppType>>& args
) const {
    auto it = specializations_.find(templateName);
    if (it == specializations_.end()) return nullptr;

    // 1. Check full (explicit) specializations first — exact argument match
    for (const auto& spec : it->second) {
        if (!spec.isPartial && spec.arguments.size() == args.size()) {
            bool match = true;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!spec.arguments[i] || !args[i] || !spec.arguments[i]->isEquivalentTo(*args[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return &spec;
        }
    }

    // 2. Check partial specializations via pattern deduction and partial ordering
    std::vector<const TemplateSpecialization*> matchingPartials;
    for (const auto& spec : it->second) {
        if (!spec.isPartial || spec.arguments.size() != args.size()) continue;
        std::unordered_set<std::string> paramNames;
        for (const auto& tp : spec.templateParams) paramNames.insert(tp.name);
        if (paramNames.empty()) paramNames.insert("T");

        std::unordered_map<std::string, std::shared_ptr<CppType>> deduced;
        std::unordered_map<std::string, int64_t> deducedNT;
        std::string err;
        bool matches = true;
        for (size_t i = 0; i < args.size(); ++i) {
            if (!spec.arguments[i] || !args[i] ||
                !deduceFromTypePattern(*spec.arguments[i], *args[i], ValueCategory::PRValue,
                                       paramNames, deduced, deducedNT, err, false)) {
                matches = false;
                break;
            }
        }
        if (matches && !spec.constraintClause.empty()) {
            std::vector<std::shared_ptr<CppType>> deducedVec;
            for (const auto& [k, v] : deduced) deducedVec.push_back(v);
            if (!checkConstraint(spec.constraintClause, deducedVec)) {
                matches = false;
            }
        }
        if (matches) {
            matchingPartials.push_back(&spec);
        }
    }

    if (matchingPartials.empty()) return nullptr;
    if (matchingPartials.size() == 1) return matchingPartials[0];

    // Partial ordering among matching partial specializations
    const TemplateSpecialization* best = nullptr;
    for (size_t i = 0; i < matchingPartials.size(); ++i) {
        bool dominatesAll = true;
        std::vector<TemplateParam> pA = matchingPartials[i]->templateParams;
        if (pA.empty()) {
            TemplateParam defParam;
            defParam.name = "T";
            defParam.isTypeParam = true;
            pA.push_back(std::move(defParam));
        }
        for (size_t j = 0; j < matchingPartials.size(); ++j) {
            if (i == j) continue;
            std::vector<TemplateParam> pB = matchingPartials[j]->templateParams;
            if (pB.empty()) {
                TemplateParam defParam;
                defParam.name = "T";
                defParam.isTypeParam = true;
                pB.push_back(std::move(defParam));
            }
            int cmp = compareFunctionTemplateSpecialization(
                pA, matchingPartials[i]->arguments, matchingPartials[i]->constraintClause,
                pB, matchingPartials[j]->arguments, matchingPartials[j]->constraintClause);
            if (cmp != -1) {
                dominatesAll = false;
                break;
            }
        }
        if (dominatesAll) {
            best = matchingPartials[i];
            break;
        }
    }
    return best;
}

} // namespace genomic
