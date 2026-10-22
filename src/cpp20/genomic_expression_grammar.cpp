#include "cpp20/genomic_expression_grammar.hpp"
#include "genome/genome_strand.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace genomic {
namespace {
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream stream(s);
    std::string part;
    while (std::getline(stream, part, delimiter)) parts.push_back(part);
    return parts;
}

void addNames(std::unordered_set<std::string>& out, const std::string& value, char separator) {
    for (const auto& name : split(value, separator)) {
        if (name.empty() || !out.insert(name).second)
            throw std::invalid_argument("Invalid or repeated genomic grammar alternative: " + name);
    }
}
} // namespace

const std::string& GenomicExpressionGrammar::standardRulesPayload() {
    // Serialized into CppGrammar at bootstrap; the host's legacy parser uses
    // this same table only when no genome was supplied by the caller.
    static const std::string rules =
        "precedence:=:1:right;precedence:+=:1:right;precedence:-=:1:right;"
        "precedence:*=:1:right;"
        "precedence:/=:1:right;precedence:%=:1:right;"
        "precedence:||:2:left;precedence:&&:3:left;"
        "precedence:|:4:left;precedence:^:5:left;precedence:&:6:left;"
        "precedence:==:7:left;precedence:!=:7:left;"
        "precedence:<:8:left;precedence:<=:8:left;precedence:>:8:left;precedence:>=:8:left;"
        "precedence:<=>:9:left;precedence:<<:10:left;precedence:>>:10:left;"
        "precedence:+:11:left;precedence:-:11:left;"
        "precedence:*:12:left;precedence:/:12:left;precedence:%:12:left;"
        "prefix:+;prefix:-;prefix:!;prefix:~;prefix:++;prefix:--;prefix:*;prefix:&;"
        "postfix:++;postfix:--;";
    return rules;
}

GenomicExpressionGrammar GenomicExpressionGrammar::fromPayload(const std::string& payload) {
    GenomicExpressionGrammar grammar;
    grammar.strict_ = true;
    for (const auto& entry : split(payload, ';')) {
        if (entry.rfind("precedence:", 0) == 0) {
            const auto fields = split(entry, ':');
            if (fields.size() != 4 || fields[1].empty() ||
                (fields[3] != "left" && fields[3] != "right"))
                throw std::invalid_argument("Invalid genomic precedence rule: " + entry);
            if (fields[1].find_first_of(" \t\n\r") != std::string::npos)
                throw std::invalid_argument("Whitespace in genomic operator: " + entry);
            size_t consumed = 0;
            int precedence = 0;
            try { precedence = std::stoi(fields[2], &consumed); }
            catch (const std::exception&) { throw std::invalid_argument("Invalid precedence number: " + entry); }
            if (consumed != fields[2].size() || precedence < 1 || precedence > 100)
                throw std::invalid_argument("Precedence out of range: " + entry);
            if (!grammar.binary_.emplace(fields[1], BinaryRule{precedence, fields[3] == "right"}).second)
                throw std::invalid_argument("Duplicate genomic precedence rule: " + entry);
        } else if (entry.rfind("prefix:", 0) == 0) {
            auto op = entry.substr(7);
            if (op.empty() || !grammar.prefix_.insert(op).second)
                throw std::invalid_argument("Invalid genomic prefix operator: " + entry);
        } else if (entry.rfind("postfix:", 0) == 0) {
            auto op = entry.substr(8);
            if (op.empty() || !grammar.postfix_.insert(op).second)
                throw std::invalid_argument("Invalid genomic postfix operator: " + entry);
        } else if (entry.rfind("grammar:declaration=", 0) == 0) {
            addNames(grammar.declarations_, entry.substr(20), '|');
        } else if (entry.rfind("grammar:statement=", 0) == 0) {
            addNames(grammar.statements_, entry.substr(18), '|');
        } else if (entry.rfind("grammar:keywords=", 0) == 0) {
            addNames(grammar.keywords_, entry.substr(17), ',');
        }
    }
    if (grammar.binary_.empty())
        throw std::invalid_argument("Genomic CppGrammar has no binary precedence rules");
    return grammar;
}

GenomicExpressionGrammar GenomicExpressionGrammar::standardCpp20() {
    auto grammar = fromPayload(standardRulesPayload());
    grammar.strict_ = false; // legacy standalone parser API
    return grammar;
}

GenomicExpressionGrammar GenomicExpressionGrammar::fromGenome(const GenomeStrand& strand) {
    std::string payload;
    for (const auto& region : strand.getAllRegions()) {
        if (region.kind == RegionKind::CppGrammar || region.kind == RegionKind::CppGrammarRule) {
            if (!region.payload.empty()) {
                payload.append(reinterpret_cast<const char*>(region.payload.data()), region.payload.size());
                payload.push_back(';');
            }
        }
    }
    if (payload.empty()) throw std::invalid_argument("Genome has no CppGrammar region");
    auto grammar = fromPayload(payload);
    if (grammar.declarations_.empty() || grammar.statements_.empty() || grammar.keywords_.empty())
        throw std::invalid_argument("Genomic CppGrammar lacks declaration or statement productions");
    return grammar;
}

std::optional<GenomicExpressionGrammar::BinaryRule>
GenomicExpressionGrammar::binary(const std::string& op) const {
    const auto it = binary_.find(op);
    if (it == binary_.end()) return std::nullopt;
    return it->second;
}

bool GenomicExpressionGrammar::keyword(const std::string& word) const {
    return !strict_ || keywords_.contains(word);
}
bool GenomicExpressionGrammar::prefix(const std::string& op) const { return prefix_.contains(op); }
bool GenomicExpressionGrammar::postfix(const std::string& op) const { return postfix_.contains(op); }
bool GenomicExpressionGrammar::declarationEnabled(const std::string& name) const {
    return !strict_ || declarations_.contains(name);
}
bool GenomicExpressionGrammar::statementEnabled(const std::string& name) const {
    return !strict_ || statements_.contains(name);
}
} // namespace genomic
