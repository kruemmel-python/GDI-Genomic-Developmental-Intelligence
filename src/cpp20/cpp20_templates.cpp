#include "cpp20/cpp20_templates.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <numeric>

namespace genomic {

void TemplateEngine::registerConcept(const ConceptDefinition& conceptDef) {
    concepts_[conceptDef.name] = conceptDef;
}

bool TemplateEngine::checkConstraint(const std::string& conceptName,
                                    const std::vector<std::shared_ptr<CppType>>& types) const {
    if (!types.empty() && types[0]) {
        if (ruleEngine_) {
            if (ruleEngine_->checkConceptSatisfaction(conceptName, types[0]->kind)) {
                return true;
            }
        }
    }
    auto it = concepts_.find(conceptName);
    if (it == concepts_.end()) {
        return false;
    }
    if (it->second.predicate) {
        return it->second.predicate(types);
    }
    return true;
}

bool TemplateEngine::isMoreConstrained(const std::string& conceptA, const std::string& conceptB) const {
    if (ruleEngine_) {
        return ruleEngine_->subsumes(conceptA, conceptB);
    }
    // Fallback if no rule engine connected
    if (conceptA == "Integral" && conceptB == "Any") return true;
    if (conceptA == "SignedIntegral" && conceptB == "Integral") return true;
    if (conceptA == "FloatingPoint" && conceptB == "Arithmetic") return true;
    return false;
}

int64_t TemplateEngine::evaluateFoldSum(const std::vector<int64_t>& values) {
    // (... + pack)
    return std::accumulate(values.begin(), values.end(), 0LL);
}

bool TemplateEngine::evaluateFoldAll(const std::vector<bool>& values) {
    // (... && pack)
    for (bool b : values) {
        if (!b) return false;
    }
    return true;
}

TemplateEngine::InstantiationResult TemplateEngine::instantiateFunctionTemplate(
    const std::string& templateName,
    const std::vector<std::shared_ptr<CppType>>& templateParams,
    const std::vector<std::shared_ptr<CppType>>& argumentTypes
) {
    (void)templateName;
    InstantiationResult res;
    if (templateParams.empty() || argumentTypes.empty()) {
        res.success = false;
        res.substitutionError = "SFINAE: Empty template or argument parameters";
        return res;
    }

    // Deduction: T deduces to argumentTypes[0]
    auto deduced = argumentTypes[0];
    if (deduced->isVoid()) {
        res.success = false;
        res.substitutionError = "SFINAE: Deductions to void in arithmetic context rejected";
        return res;
    }

    res.success = true;
    res.deducedType = deduced;
    return res;
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

    for (const auto& spec : it->second) {
        if (spec.arguments.size() == args.size()) {
            bool match = true;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!spec.arguments[i]->isEquivalentTo(*args[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return &spec;
        }
    }
    return nullptr;
}

} // namespace genomic
