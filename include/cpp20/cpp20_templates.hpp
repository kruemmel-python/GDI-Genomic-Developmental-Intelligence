#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_ast.hpp"

namespace genomic {

class GenomicRuleEngine;

struct TemplateParam {
    std::string name;
    bool isTypeParam{true};
    bool isParameterPack{false};
    std::shared_ptr<CppType> defaultType;
};

struct ConceptDefinition {
    std::string name;
    std::vector<std::string> templateParams;
    std::function<bool(const std::vector<std::shared_ptr<CppType>>&)> predicate;
};

struct TemplateSpecialization {
    std::vector<std::shared_ptr<CppType>> arguments;
    std::shared_ptr<AstNode> specializedBody;
    bool isPartial{false};
};

class TemplateEngine {
public:
    explicit TemplateEngine(const GenomicRuleEngine* ruleEngine = nullptr)
        : ruleEngine_(ruleEngine) {}

    void setRuleEngine(const GenomicRuleEngine* ruleEngine) noexcept { ruleEngine_ = ruleEngine; }

    void registerConcept(const ConceptDefinition& conceptDef);
    bool checkConstraint(const std::string& conceptName, const std::vector<std::shared_ptr<CppType>>& types) const;

    // Subsumption check (Section 4.11: more constrained concept wins)
    bool isMoreConstrained(const std::string& conceptA, const std::string& conceptB) const;

    // Fold expression evaluator (Section 4.10)
    static int64_t evaluateFoldSum(const std::vector<int64_t>& values);
    static bool evaluateFoldAll(const std::vector<bool>& values);

    // SFINAE instantiation helper
    struct InstantiationResult {
        bool success{false};
        std::shared_ptr<CppType> deducedType;
        std::string substitutionError;
    };

    InstantiationResult instantiateFunctionTemplate(
        const std::string& templateName,
        const std::vector<std::shared_ptr<CppType>>& templateParams,
        const std::vector<std::shared_ptr<CppType>>& argumentTypes
    );

    // Specializations
    void registerSpecialization(const std::string& templateName, const TemplateSpecialization& spec);
    const TemplateSpecialization* findBestSpecialization(
        const std::string& templateName,
        const std::vector<std::shared_ptr<CppType>>& args
    ) const;

private:
    const GenomicRuleEngine* ruleEngine_{nullptr};
    std::unordered_map<std::string, ConceptDefinition> concepts_;
    std::unordered_map<std::string, std::vector<TemplateSpecialization>> specializations_;
};

} // namespace genomic
