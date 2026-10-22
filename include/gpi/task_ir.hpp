#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class ValueKind { Number, Boolean };
struct ExprNode {
    std::string op;
    std::string literal;
    ValueKind kind{ValueKind::Number};
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
};
struct TaskIR {
    std::unique_ptr<ExprNode> map;
    std::unique_ptr<ExprNode> filter;
    std::string mapText;
    std::string filterText;
    std::string reduction{"sum"};
    std::optional<std::size_t> samples;
    bool emitValues{false};
    std::vector<std::string> plan;
};
struct TaskParseResult {
    std::optional<TaskIR> task;
    std::string error;
};
// Strict, bounded task specification: map=expression; filter=predicate;
// reduce=sum|mean|min|max|count; samples=N; emit=true|false.
// Expressions are parsed to typed trees, never interpolated from the prompt.
TaskParseResult parseTask(const std::string& prompt);
std::string emitExpression(const ExprNode& node);
double evaluateExpression(const ExprNode& node, double x, double y);
std::string generateTaskSource(const TaskIR& task, uint64_t genomeHash, bool includeMathHeader = true);
std::string expectedSmokeOutput(const TaskIR& task);
std::string expectedOutputForInputs(const TaskIR& task, const std::vector<double>& inputs);
} // namespace genomic::gpi
