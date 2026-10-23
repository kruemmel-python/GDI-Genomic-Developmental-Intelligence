#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_ast.hpp"

namespace genomic {

class GenomicRuleEngine;

struct TemplateParam {
    std::string name{};
    bool isTypeParam{true};
    bool isParameterPack{false};
    std::shared_ptr<CppType> defaultType{};
    int64_t defaultNonTypeValue{0};
    bool hasDefaultNonType{false};
    std::string conceptConstraint{}; // e.g. `template<std::integral T>`
};

struct AtomicConstraint {
    std::string conceptOrPredicate{}; // e.g. "integral", "signed_integral", "sizeof(T)>=4"
    std::vector<std::string> paramArgs{};
    bool isNegated{false};

    bool operator==(const AtomicConstraint& other) const noexcept {
        return conceptOrPredicate == other.conceptOrPredicate &&
               paramArgs == other.paramArgs &&
               isNegated == other.isNegated;
    }
};

// Disjunctive Normal Form (DNF) of atomic constraints:
// (A1 && A2) || (B1 && B2)
struct NormalizedConstraint {
    std::vector<std::vector<AtomicConstraint>> disjunctionOfConjunctions;

    bool isUnconstrained() const noexcept { return disjunctionOfConjunctions.empty(); }
};

struct ConceptDefinition {
    std::string name{};
    std::vector<std::string> templateParams{};
    std::string rawConstraintExpression{}; // e.g. "integral<T> && Signed<T>"
    std::function<bool(const std::vector<std::shared_ptr<CppType>>&)> predicate{};
};

struct RequiresRequirement {
    enum class Kind : uint8_t {
        Simple,   // expr;
        Type,     // typename T::value_type;
        Compound, // { expr } noexcept -> Concept;
        Nested    // requires BooleanConstraint;
    } kind{Kind::Simple};
    std::string expression{};
    bool requireNoexcept{false};
    std::string returnConceptConstraint{};
};

struct TemplateSpecialization {
    std::vector<TemplateParam> templateParams{};
    std::vector<std::shared_ptr<CppType>> arguments{}; // Pattern arguments (e.g. `T*`, `std::pair<T, U>`, `int`)
    std::shared_ptr<AstNode> specializedBody{};
    bool isPartial{false};
    std::string constraintClause{};
};

struct DeductionGuide {
    std::string className{};
    std::vector<TemplateParam> templateParams{};
    std::vector<std::shared_ptr<CppType>> constructorParamPatterns{};
    std::vector<std::shared_ptr<CppType>> deducedClassTemplateArgs{};
};

class TemplateEngine {
public:
    explicit TemplateEngine(const GenomicRuleEngine* ruleEngine = nullptr)
        : ruleEngine_(ruleEngine) {}

    void setRuleEngine(const GenomicRuleEngine* ruleEngine) noexcept { ruleEngine_ = ruleEngine; }
    const GenomicRuleEngine* getRuleEngine() const noexcept { return ruleEngine_; }

    void registerConcept(const ConceptDefinition& conceptDef);
    bool checkConstraint(const std::string& conceptExpression, const std::vector<std::shared_ptr<CppType>>& types) const;
    bool evaluateRequiresExpression(const std::vector<RequiresRequirement>& requirements,
                                    const std::shared_ptr<CppType>& targetType) const;

    // C++20 Concept Normalization & Subsumption ([temp.constr.order]):
    // Normalizes arbitrary conjunctions (&&) and disjunctions (||) of concepts into DNF/CNF
    // and checks whether constraintA logically implies (subsumes) constraintB:
    // Every conjunction clause of A must imply at least one conjunction clause of B.
    NormalizedConstraint normalizeConstraint(const std::string& constraintExpr) const;
    bool isMoreConstrained(const std::string& constraintA, const std::string& constraintB) const;

    // Fold expression evaluators (Section 4.10)
    static int64_t evaluateFoldSum(const std::vector<int64_t>& values);
    static int64_t evaluateFoldProduct(const std::vector<int64_t>& values);
    static bool evaluateFoldAll(const std::vector<bool>& values);
    static bool evaluateFoldAny(const std::vector<bool>& values);

    // Full C++20 Template Argument Deduction & SFINAE Substitution Result
    struct InstantiationResult {
        bool success{false};
        std::shared_ptr<CppType> deducedType; // Primary deduced type (for 1st template param)
        std::unordered_map<std::string, std::shared_ptr<CppType>> deducedTypeMap;
        std::unordered_map<std::string, std::vector<std::shared_ptr<CppType>>> deducedPackMap;
        std::unordered_map<std::string, int64_t> deducedNonTypeMap;
        std::vector<std::shared_ptr<CppType>> substitutedParamTypes;
        std::shared_ptr<CppType> substitutedReturnType;
        std::string substitutionError;
    };

    // Legacy 3-argument overload kept for full backwards compatibility
    InstantiationResult instantiateFunctionTemplate(
        const std::string& templateName,
        const std::vector<std::shared_ptr<CppType>>& templateParams,
        const std::vector<std::shared_ptr<CppType>>& argumentTypes
    );

    // Full C++20 multi-parameter pattern deduction with value categories, explicit args, defaults, and constraints
    InstantiationResult deduceAndSubstituteFunctionTemplate(
        const std::string& templateName,
        const std::vector<TemplateParam>& templateParams,
        const std::vector<std::shared_ptr<CppType>>& fnParamPatterns,
        const std::shared_ptr<CppType>& fnReturnPattern,
        const std::vector<std::shared_ptr<CppType>>& callArgTypes,
        const std::vector<ValueCategory>& callArgValueCategories = {},
        const std::vector<std::shared_ptr<CppType>>& explicitTemplateArgs = {},
        const std::string& requiresClause = ""
    ) const;

    // Substitute deduced template arguments into a type pattern
    static std::shared_ptr<CppType> substituteTypePattern(
        const std::shared_ptr<CppType>& pattern,
        const std::unordered_map<std::string, std::shared_ptr<CppType>>& typeMap,
        const std::unordered_map<std::string, int64_t>& nonTypeMap = {}
    );

    // Partial Ordering of Function Templates (C++20 [temp.func.order]):
    // Returns -1 if template A is more specialized than template B,
    // +1 if template B is more specialized than template A, 0 if ambiguous/neither.
    int compareFunctionTemplateSpecialization(
        const std::vector<TemplateParam>& paramsA,
        const std::vector<std::shared_ptr<CppType>>& fnParamsA,
        const std::string& constraintA,
        const std::vector<TemplateParam>& paramsB,
        const std::vector<std::shared_ptr<CppType>>& fnParamsB,
        const std::string& constraintB
    ) const;

    // Class Template Argument Deduction (CTAD - C++17/C++20 [over.match.class.deduct])
    void registerDeductionGuide(const DeductionGuide& guide);
    InstantiationResult deduceClassTemplateArguments(
        const std::string& className,
        const std::vector<std::shared_ptr<CppType>>& ctorArgTypes,
        const std::vector<ValueCategory>& ctorArgCategories = {}
    ) const;

    // Specializations (full and partial specializations with partial ordering)
    void registerSpecialization(const std::string& templateName, const TemplateSpecialization& spec);
    const TemplateSpecialization* findBestSpecialization(
        const std::string& templateName,
        const std::vector<std::shared_ptr<CppType>>& args
    ) const;

private:
    const GenomicRuleEngine* ruleEngine_{nullptr};
    std::unordered_map<std::string, ConceptDefinition> concepts_;
    std::unordered_map<std::string, std::vector<TemplateSpecialization>> specializations_;
    std::unordered_map<std::string, std::vector<DeductionGuide>> deductionGuides_;

    static bool deduceFromTypePattern(
        const CppType& pattern,
        const CppType& argType,
        ValueCategory argCat,
        const std::unordered_set<std::string>& templateParamNames,
        std::unordered_map<std::string, std::shared_ptr<CppType>>& deducedTypes,
        std::unordered_map<std::string, int64_t>& deducedNonTypes,
        std::string& outError,
        bool isTopLevelParam = true
    );

    bool atomicImplies(const AtomicConstraint& a, const AtomicConstraint& b) const;
    bool conjunctionImplies(const std::vector<AtomicConstraint>& conjA,
                            const std::vector<AtomicConstraint>& conjB) const;
};

} // namespace genomic
