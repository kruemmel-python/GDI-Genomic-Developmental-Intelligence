#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include "genome/genome_strand.hpp"
#include "cpp20/cpp20_types.hpp"
#include "cpp20/genomic_expression_grammar.hpp"
#include "runtime/genome_ir.hpp"

namespace genomic {

struct GenomicTypeInfo {
    TypeKind kind{TypeKind::Int64};
    std::string name{"int64_t"};
    uint32_t sizeBytes{8};
    uint32_t alignment{8};
    bool isIntegral{true};
    bool isFloatingPoint{false};
    bool isSigned{true};
    bool isPointer{false};
    bool isReference{false};
};

struct GenomicBinaryOpRule {
    std::string op;
    TypeKind lhsKind{TypeKind::Int64};
    TypeKind rhsKind{TypeKind::Int64};
    TypeKind resultKind{TypeKind::Int64};
    ValueCategory resultValueCategory{ValueCategory::PRValue};
    IrOpcode loweringOpcode{IrOpcode::Add};
    bool isConstexprLegal{true};
};

struct GenomicUnaryOpRule {
    std::string op;
    TypeKind operandKind{TypeKind::Int64};
    TypeKind resultKind{TypeKind::Int64};
    ValueCategory resultValueCategory{ValueCategory::PRValue};
    IrOpcode loweringOpcode{IrOpcode::Nop};
    bool isPrefix{true};
    bool isConstexprLegal{true};
};

struct GenomicConversionRule {
    TypeKind fromKind{TypeKind::Int32};
    TypeKind toKind{TypeKind::Int64};
    int conversionCost{1}; // 0=Identity, 1=Promotion, 2=Standard, 3=UserDefined
    bool isImplicitAllowed{true};
    std::string category{"numeric_conversion"};
};

struct GenomicArithmeticConversionRule {
    TypeKind lhsKind{TypeKind::Int64};
    TypeKind rhsKind{TypeKind::Int64};
    TypeKind commonKind{TypeKind::Int64};
};

struct GenomicLoweringRule {
    std::string construct; // e.g. "binop:+", "binop:-", "stmt:return"
    IrOpcode opcode{IrOpcode::Nop};
};

struct GenomicOverloadRule {
    bool preferNonTemplateOverSpecialization{true};
    bool preferMoreConstrained{true};
    int exactMatchScore{0};
    int promotionScore{1};
    int conversionScore{2};
    int userDefinedScore{3};
    int ellipsisScore{4};
    int templatePenalty{1};
    int constrainedBonus{-1};
};

struct GenomicConstraintRule {
    std::string conceptName;
    std::vector<TypeKind> satisfiedTypes;
    std::vector<std::string> subsumedConcepts;
    bool requiresCompleteType{true};
};

struct GenomicLookupRule {
    std::vector<std::string> scopeSearchOrder{
        "local", "block", "function", "class", "base_class", "namespace", "global", "adl"
    };
    bool allowShadowing{true};
    bool twoPhaseLookup{true};
};

struct GenomicLifetimeRule {
    bool enforceDestructorsOnScopeExit{true};
    bool trackTemporaryLifetimes{true};
    bool guardAgainstUseAfterFree{true};
    bool guardAgainstDanglingReferences{true};
    bool materializePRValue{true};
};

struct GenomicMemoryModelRule {
    bool detectDataRaces{true};
    bool enforceSequentialConsistencyByDefault{true};
    std::vector<std::string> validMemoryOrders{
        "relaxed", "consume", "acquire", "release", "acq_rel", "seq_cst"
    };
    std::string defaultOrder{"seq_cst"};
};

class GenomicRuleEngine {
public:
    GenomicRuleEngine() = default;

    // Ingests semantic rules directly from the immutable GenomeStrand
    void loadRulesFromGenome(const GenomeStrand& strand);

    // Queries powered by genomic rules
    std::optional<GenomicTypeInfo> getTypeInfo(TypeKind kind) const;
    std::optional<GenomicBinaryOpRule> resolveBinaryOp(const std::string& op, TypeKind lhs, TypeKind rhs) const;
    std::optional<GenomicUnaryOpRule> resolveUnaryOp(const std::string& op, TypeKind operand, bool isPrefix) const;
    std::optional<TypeKind> getCommonType(TypeKind lhs, TypeKind rhs) const;
    int getConversionCost(TypeKind from, TypeKind to) const;
    bool canConvert(TypeKind from, TypeKind to) const;
    bool checkConceptSatisfaction(const std::string& conceptName, TypeKind targetType) const;
    bool subsumes(const std::string& moreConstrained, const std::string& lessConstrained) const;
    IrOpcode getLoweringOpcode(const std::string& construct, IrOpcode defaultOp = IrOpcode::Nop) const;

    const GenomicExpressionGrammar& getExpressionGrammar() const noexcept { return expressionGrammar_; }

    const GenomicOverloadRule& getOverloadRule() const noexcept { return overloadRule_; }
    const GenomicLookupRule& getLookupRule() const noexcept { return lookupRule_; }
    const GenomicLifetimeRule& getLifetimeRule() const noexcept { return lifetimeRule_; }
    const GenomicMemoryModelRule& getMemoryModelRule() const noexcept { return memoryModelRule_; }

    size_t getRuleCount() const noexcept;

    // Serialization helpers to format rules as string/codon payloads for GenomeStrand
    static std::string serializeTypeInfo(const GenomicTypeInfo& info);
    static std::string serializeBinaryOpRule(const GenomicBinaryOpRule& rule);
    static std::string serializeUnaryOpRule(const GenomicUnaryOpRule& rule);
    static std::string serializeConversionRule(const GenomicConversionRule& rule);
    static std::string serializeArithmeticConversionRule(const GenomicArithmeticConversionRule& rule);
    static std::string serializeConstraintRule(const GenomicConstraintRule& rule);
    static std::string serializeLoweringRule(const GenomicLoweringRule& rule);

private:
    std::unordered_map<int, GenomicTypeInfo> typeDefinitions_;
    std::vector<GenomicBinaryOpRule> binaryOpRules_;
    std::vector<GenomicUnaryOpRule> unaryOpRules_;
    std::vector<GenomicConversionRule> conversionRules_;
    std::vector<GenomicArithmeticConversionRule> arithmeticConversionRules_;
    std::unordered_map<std::string, GenomicConstraintRule> constraintRules_;
    std::unordered_map<std::string, IrOpcode> loweringRules_;

    GenomicExpressionGrammar expressionGrammar_;
    GenomicOverloadRule overloadRule_;
    GenomicLookupRule lookupRule_;
    GenomicLifetimeRule lifetimeRule_;
    GenomicMemoryModelRule memoryModelRule_;

    void parseTypeRulePayload(const std::string& payload);
    void parseSemanticRulePayload(const std::string& payload);
    void parseUnaryRulePayload(const std::string& payload);
    void parseConversionRulePayload(const std::string& payload);
    void parseArithmeticConversionPayload(const std::string& payload);
    void parseConstraintRulePayload(const std::string& payload);
    void parseLoweringRulePayload(const std::string& payload);
    void parseLookupRulePayload(const std::string& payload);
    void parseOverloadRulePayload(const std::string& payload);
    void parseLifetimeRulePayload(const std::string& payload);
    void parseMemoryModelPayload(const std::string& payload);
};

} // namespace genomic
