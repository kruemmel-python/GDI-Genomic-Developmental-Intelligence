#pragma once

#include <string>
#include <vector>
#include <memory>
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_semantics.hpp"

namespace genomic {

class GenomicRuleEngine;

struct ValidationGateResult {
    bool passed{true};
    std::string gateName;
    std::vector<std::string> errors;
};

struct ComprehensiveValidationResult {
    bool allGatesPassed{true};
    std::vector<ValidationGateResult> gates;
};

class SemanticValidator {
public:
    static ComprehensiveValidationResult validate(
        AstNode& root,
        double energyBudget = 1000.0,
        double memoryBudget = 1024.0 * 1024.0,
        const GenomicRuleEngine* ruleEngine = nullptr
    );
};

} // namespace genomic
