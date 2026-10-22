#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace genomic {

class GenomeStrand;

// Declarative, immutable-at-runtime grammar configuration. The expression
// parser interprets this data instead of embedding operator precedence in C++.
// This is a supported subset of C++20 grammar, not an ISO-complete grammar.
class GenomicExpressionGrammar {
public:
    struct BinaryRule {
        int precedence{0};
        bool rightAssociative{false};
    };

    static GenomicExpressionGrammar fromGenome(const GenomeStrand& strand);
    static GenomicExpressionGrammar fromPayload(const std::string& payload);
    static GenomicExpressionGrammar standardCpp20();
    static const std::string& standardRulesPayload();

    std::optional<BinaryRule> binary(const std::string& op) const;
    bool keyword(const std::string& word) const;
    bool prefix(const std::string& op) const;
    bool postfix(const std::string& op) const;
    bool declarationEnabled(const std::string& name) const;
    bool statementEnabled(const std::string& name) const;
    bool strict() const noexcept { return strict_; }
    size_t binaryRuleCount() const noexcept { return binary_.size(); }

private:
    bool strict_{false};
    std::unordered_map<std::string, BinaryRule> binary_;
    std::unordered_set<std::string> keywords_;
    std::unordered_set<std::string> prefix_;
    std::unordered_set<std::string> postfix_;
    std::unordered_set<std::string> declarations_;
    std::unordered_set<std::string> statements_;
};

} // namespace genomic
