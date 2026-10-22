#include "gpi/task_ir.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace genomic::gpi {
namespace {
std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b-a+1);
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
struct Token { std::string text; enum class Kind { Number, Name, Operator, End } kind; };
class ExpressionParser {
public:
    ExpressionParser(std::string_view s, bool filter) : input_(s), filter_(filter) { next(); }
    std::unique_ptr<ExprNode> parse() {
        auto e = expression(0, 0);
        if (tok_.kind != Token::Kind::End) throw std::invalid_argument("Unexpected expression token: " + tok_.text);
        if (e->kind != (filter_ ? ValueKind::Boolean : ValueKind::Number))
            throw std::invalid_argument(filter_ ? "Filter must be a boolean predicate" : "Map must be numeric");
        return e;
    }
private:
    std::string_view input_;
    std::size_t pos_{0};
    std::size_t nodes_{0};
    bool filter_;
    Token tok_;
    void next() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        if (pos_ == input_.size()) { tok_ = {"", Token::Kind::End}; return; }
        const std::size_t begin = pos_;
        const char c = input_[pos_];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            bool digit = false;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) { ++pos_; digit = true; }
            if (pos_ < input_.size() && input_[pos_] == '.') {
                ++pos_;
                while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) { ++pos_; digit = true; }
            }
            if (!digit) throw std::invalid_argument("Malformed number");
            tok_ = {std::string(input_.substr(begin, pos_-begin)), Token::Kind::Number}; return;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) ++pos_;
            tok_ = {std::string(input_.substr(begin, pos_-begin)), Token::Kind::Name}; return;
        }
        if (pos_ + 1 < input_.size()) {
            const std::string pair(input_.substr(pos_,2));
            if (pair == "&&" || pair == "||" || pair == "==" || pair == "!=" || pair == "<=" || pair == ">=") {
                pos_ += 2; tok_ = {pair, Token::Kind::Operator}; return;
            }
        }
        if (std::string_view("+-*/%()<>!").find(c) == std::string_view::npos)
            throw std::invalid_argument("Forbidden expression character");
        ++pos_; tok_ = {std::string(1,c), Token::Kind::Operator};
    }
    static int priority(const std::string& op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/" || op == "%") return 6;
        return -1;
    }
    std::unique_ptr<ExprNode> make(std::string op, ValueKind kind,
                                   std::unique_ptr<ExprNode> left = {}, std::unique_ptr<ExprNode> right = {}) {
        if (++nodes_ > 128) throw std::invalid_argument("Expression exceeds 128 AST nodes");
        auto n = std::make_unique<ExprNode>(); n->op = std::move(op); n->kind = kind;
        n->left = std::move(left); n->right = std::move(right); return n;
    }
    std::unique_ptr<ExprNode> expression(int minPriority, int depth) {
        if (depth > 32) throw std::invalid_argument("Expression nesting exceeds 32");
        std::unique_ptr<ExprNode> lhs;
        if (tok_.text == "-" || tok_.text == "+" || tok_.text == "!") {
            const auto op = tok_.text; next();
            auto operand = expression(7, depth+1);
            if (operand->kind != (op == "!" ? ValueKind::Boolean : ValueKind::Number))
                throw std::invalid_argument("Unary operand has wrong type");
            lhs = make("unary" + op, op == "!" ? ValueKind::Boolean : ValueKind::Number, std::move(operand));
        } else if (tok_.text == "(") {
            next(); lhs = expression(0, depth+1);
            if (tok_.text != ")") throw std::invalid_argument("Missing closing parenthesis");
            next();
        } else if (tok_.kind == Token::Kind::Number) {
            const auto value = tok_.text;
            double number = 0;
            const auto [end, ec] = std::from_chars(value.data(), value.data()+value.size(), number);
            if (ec != std::errc{} || end != value.data()+value.size() || !std::isfinite(number))
                throw std::invalid_argument("Invalid finite numeric literal");
            lhs = make("literal", ValueKind::Number); lhs->literal = value; next();
        } else if (tok_.kind == Token::Kind::Name) {
            if (tok_.text != "x" && (!filter_ || tok_.text != "y"))
                throw std::invalid_argument("Only x (input) and y (mapped value in filter) are allowed");
            lhs = make(tok_.text, ValueKind::Number); next();
        } else throw std::invalid_argument("Expected numeric expression");
        while (true) {
            const auto op = tok_.text;
            const int p = priority(op);
            if (p < minPriority) break;
            next(); auto rhs = expression(p+1, depth+1);
            ValueKind kind = ValueKind::Number;
            if (op == "&&" || op == "||") {
                if (lhs->kind != ValueKind::Boolean || rhs->kind != ValueKind::Boolean)
                    throw std::invalid_argument("Logical operators require predicates");
                kind = ValueKind::Boolean;
            } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
                if (lhs->kind != ValueKind::Number || rhs->kind != ValueKind::Number)
                    throw std::invalid_argument("Comparison requires numbers");
                kind = ValueKind::Boolean;
            } else if (lhs->kind != ValueKind::Number || rhs->kind != ValueKind::Number) {
                throw std::invalid_argument("Arithmetic requires numbers");
            }
            lhs = make(op, kind, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }
};
std::string emit(const ExprNode& n) {
    if (n.op == "literal") return n.literal;
    if (n.op == "x" || n.op == "y") return n.op;
    if (n.op.rfind("unary",0) == 0) return "(" + n.op.substr(5) + emit(*n.left) + ")";
    if (n.op == "%") return "std::fmod(" + emit(*n.left) + ", " + emit(*n.right) + ")";
    return "(" + emit(*n.left) + " " + n.op + " " + emit(*n.right) + ")";
}
double eval(const ExprNode& n, double x, double y) {
    if (n.op == "literal") return std::stod(n.literal);
    if (n.op == "x") return x;
    if (n.op == "y") return y;
    if (n.op == "unary-") return -eval(*n.left,x,y);
    if (n.op == "unary+") return eval(*n.left,x,y);
    if (n.op == "unary!") return eval(*n.left,x,y) == 0 ? 1.0 : 0.0;
    const double a = eval(*n.left,x,y);
    if (n.op == "&&" && a == 0) return 0;
    if (n.op == "||" && a != 0) return 1;
    const double b = eval(*n.right,x,y);
    if (n.op == "+") return a+b;
    if (n.op == "-") return a-b;
    if (n.op == "*") return a*b;
    if (n.op == "/") return a/b;
    if (n.op == "%") return std::fmod(a,b);
    if (n.op == ">") return a>b;
    if (n.op == "<") return a<b;
    if (n.op == ">=") return a>=b;
    if (n.op == "<=") return a<=b;
    if (n.op == "==") return a==b;
    if (n.op == "!=") return a!=b;
    if (n.op == "&&") return b!=0;
    if (n.op == "||") return b!=0;
    throw std::logic_error("Unknown expression opcode");
}
std::vector<std::string> parts(const std::string& s, char sep) {
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= s.size()) {
        const auto end = s.find(sep, begin);
        result.push_back(trim(s.substr(begin, end == std::string::npos ? end : end-begin)));
        if (end == std::string::npos) break;
        begin = end+1;
    }
    return result;
}
} // namespace
TaskParseResult parseTask(const std::string& prompt) {
    TaskParseResult result;
    try {
        if (prompt.empty() || prompt.size() > 8192) throw std::invalid_argument("Task must contain 1..8192 bytes");
        TaskIR task;
        bool hasMap = false, hasReduce = false;
        std::vector<std::string> seen;
        for (const auto& part : parts(prompt, ';')) {
            if (part.empty()) continue;
            const auto eq = part.find('=');
            if (eq == std::string::npos) throw std::invalid_argument("Expected key=value; use map=..., filter=..., reduce=...");
            const auto key = lower(trim(part.substr(0,eq)));
            const auto value = trim(part.substr(eq+1));
            if (std::find(seen.begin(),seen.end(),key) != seen.end()) throw std::invalid_argument("Duplicate task key: " + key);
            seen.push_back(key);
            if (value.empty()) throw std::invalid_argument("Empty value for " + key);
            if (key == "map") {
                if (value.size() > 1024) throw std::invalid_argument("Map expression too long");
                task.map = ExpressionParser(value,false).parse(); task.mapText = value; hasMap = true;
            } else if (key == "filter") {
                if (value.size() > 1024) throw std::invalid_argument("Filter expression too long");
                task.filter = ExpressionParser(value,true).parse(); task.filterText = value;
            } else if (key == "reduce") {
                const auto r = lower(value);
                if (r != "sum" && r != "mean" && r != "min" && r != "max" && r != "count")
                    throw std::invalid_argument("Unsupported reduction: " + value);
                task.reduction = r; hasReduce = true;
            } else if (key == "samples") {
                std::size_t n = 0;
                const auto [end,ec] = std::from_chars(value.data(),value.data()+value.size(),n);
                if (ec != std::errc{} || end != value.data()+value.size() || n < 1 || n > 1000000)
                    throw std::invalid_argument("samples must be in 1..1000000");
                task.samples = n;
            } else if (key == "emit") {
                const auto v = lower(value);
                if (v != "true" && v != "false") throw std::invalid_argument("emit must be true or false");
                task.emitValues = v == "true";
            } else throw std::invalid_argument("Unknown task key: " + key);
        }
        if (!hasMap || !hasReduce) throw std::invalid_argument("Task requires map=expression and reduce=operation");
        task.plan.push_back("Map each input x using typed expression: " + task.mapText);
        if (task.filter) task.plan.push_back("Select mapped y using predicate: " + task.filterText);
        task.plan.push_back("Reduce selected values: " + task.reduction);
        task.plan.push_back(task.samples ? "Require exactly " + std::to_string(*task.samples) + " inputs" : "Read until EOF");
        result.task.emplace(std::move(task));
    } catch (const std::exception& e) { result.error = e.what(); }
    return result;
}
std::string emitExpression(const ExprNode& node) { return emit(node); }
double evaluateExpression(const ExprNode& node, double x, double y) { return eval(node,x,y); }
std::string generateTaskSource(const TaskIR& task, uint64_t genomeHash, bool includeMathHeader) {
    std::ostringstream out;
    out << "// GPI typed-task synthesis; genome hash=" << genomeHash << "\n"
        << "#include <iostream>\n#include <iomanip>\n#include <cstddef>\n";
    if (includeMathHeader) out << "#include <cmath>\n";
    out << "int main() {\n"
        << "  std::cout << std::fixed << std::setprecision(3);\n"
        << "  double x = 0.0;\n";
    if (task.reduction == "sum" || task.reduction == "mean") out << "  double total = 0.0;\n";
    if (task.reduction == "min" || task.reduction == "max") out << "  double extremum = 0.0;\n";
    out << "  std::size_t read = 0; std::size_t selected = 0;\n"
        << "  while (";
    if (task.samples) out << "read < " << *task.samples << " && ";
    out << "(std::cin >> x)) {\n"
        << "    if (!std::isfinite(x)) { std::cerr << \"NONFINITE_INPUT\\n\"; return 4; }\n"
        << "    ++read;\n"
        << "    const double y = " << emit(*task.map) << ";\n"
        << "    if (!std::isfinite(y)) { std::cerr << \"NONFINITE_RESULT\\n\"; return 5; }\n";
    if (task.filter) out << "    if (" << emit(*task.filter) << ") {\n";
    out << "      ++selected;\n";
    if (task.reduction == "sum" || task.reduction == "mean")
        out << "      total += y;\n"
            << "      if (!std::isfinite(total)) { std::cerr << \"NONFINITE_TOTAL\\n\"; return 5; }\n";
    if (task.reduction == "min" || task.reduction == "max") out << "      if (selected == 1) extremum = y;\n";
    if (task.reduction == "min") out << "      if (y < extremum) extremum = y;\n";
    if (task.reduction == "max") out << "      if (y > extremum) extremum = y;\n";
    if (task.emitValues) out << "      std::cout << \"VALUE=\" << y << '\\n';\n";
    if (task.filter) out << "    }\n";
    out << "  }\n";
    if (task.samples) out << "  if (read != " << *task.samples << ") { std::cerr << \"INSUFFICIENT_INPUT\\n\"; return 3; }\n";
    out << "  if (!std::cin.eof() && std::cin.fail()) { std::cerr << \"INVALID_INPUT\\n\"; return 4; }\n"
        << "  std::cout << \"READ=\" << read << '\\n';\n"
        << "  std::cout << \"SELECTED=\" << selected << '\\n';\n";
    if (task.reduction == "sum") out << "  std::cout << \"SUM=\" << total << '\\n';\n";
    if (task.reduction == "count") out << "  std::cout << \"COUNT=\" << selected << '\\n';\n";
    if (task.reduction == "mean") out << "  if (selected == 0) { std::cerr << \"EMPTY_REDUCTION\\n\"; return 6; }\n"
        << "  std::cout << \"MEAN=\" << (total / static_cast<double>(selected)) << '\\n';\n";
    if (task.reduction == "min" || task.reduction == "max") {
        out << "  if (selected == 0) { std::cerr << \"EMPTY_REDUCTION\\n\"; return 6; }\n";
        out << "  std::cout << \"" << (task.reduction == "min" ? "MIN" : "MAX") << "=\" << extremum << '\\n';\n";
    }
    out << "  return 0;\n}\n";
    return out.str();
}
std::string expectedOutputForInputs(const TaskIR& task, const std::vector<double>& inputs) {
    const auto count = task.samples.value_or(inputs.size());
    // Use a deterministic bounded smoke fixture even for large sample requirements.
    if (count > 32 || inputs.size() != count) throw std::invalid_argument("Oracle fixture requires exactly 1..32 inputs");
    double total=0, extremum=0;
    std::size_t selected=0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    for (std::size_t i=0; i<count; ++i) {
        const double x = inputs[i];
        const double y = eval(*task.map,x,0);
        if (!std::isfinite(y)) throw std::invalid_argument("Smoke oracle produced nonfinite map result");
        if (task.filter && eval(*task.filter,x,y) == 0) continue;
        ++selected; total+=y;
        if (!std::isfinite(total)) throw std::invalid_argument("Smoke oracle produced nonfinite total");
        if (selected==1) extremum=y;
        if (task.reduction=="min") extremum=std::min(extremum,y);
        if (task.reduction=="max") extremum=std::max(extremum,y);
        if (task.emitValues) out << "VALUE=" << y << '\n';
    }
    out << "READ=" << count << "\nSELECTED=" << selected << '\n';
    if (task.reduction=="sum") out << "SUM=" << total << '\n';
    if (task.reduction=="count") out << "COUNT=" << selected << '\n';
    if (task.reduction=="mean") {
        if (!selected) throw std::invalid_argument("Smoke fixture has empty reduction");
        out << "MEAN=" << total/static_cast<double>(selected) << '\n';
    }
    if (task.reduction=="min" || task.reduction=="max") {
        if (!selected) throw std::invalid_argument("Smoke fixture has empty reduction");
        out << (task.reduction=="min" ? "MIN=" : "MAX=") << extremum << '\n';
    }
    return out.str();
}
std::string expectedSmokeOutput(const TaskIR& task) {
    const auto count=task.samples.value_or(4);
    if (count>32) throw std::invalid_argument("Smoke test requires samples<=32");
    std::vector<double> values;
    for (std::size_t i=1;i<=count;++i) values.push_back(static_cast<double>(i));
    return expectedOutputForInputs(task,values);
}
} // namespace genomic::gpi
