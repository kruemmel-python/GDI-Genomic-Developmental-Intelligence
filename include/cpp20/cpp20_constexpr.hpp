#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include "cpp20/cpp20_ast.hpp"

namespace genomic {

struct ConstexprValue {
    enum class Kind : uint8_t { Int, Float, Bool, String } kind{Kind::Int};
    int64_t iVal{0};
    double fVal{0.0};
    bool bVal{false};
    std::string sVal;

    static ConstexprValue makeInt(int64_t v) { ConstexprValue val; val.kind = Kind::Int; val.iVal = v; return val; }
    static ConstexprValue makeFloat(double v) { ConstexprValue val; val.kind = Kind::Float; val.fVal = v; return val; }
    static ConstexprValue makeBool(bool v) { ConstexprValue val; val.kind = Kind::Bool; val.bVal = v; return val; }
    static ConstexprValue makeString(const std::string& v) { ConstexprValue val; val.kind = Kind::String; val.sVal = v; return val; }
};

class ConstexprEngine {
public:
    ConstexprEngine() = default;

    void registerConstexprFunction(const std::string& name, std::shared_ptr<AstNode> functionDecl);

    std::optional<ConstexprValue> evaluateExpression(
        const AstNode& expr,
        const std::unordered_map<std::string, ConstexprValue>& localContext = {}
    );

    // Static assert verification
    bool evaluateStaticAssert(const AstNode& condExpr, std::string& outMessage);

private:
    std::unordered_map<std::string, std::shared_ptr<AstNode>> constexprFunctions_;

    std::optional<ConstexprValue> executeFunctionBody(
        const AstNode& body,
        std::unordered_map<std::string, ConstexprValue>& env
    );
};

} // namespace genomic
