#include "cpp20/cpp20_constexpr.hpp"

namespace genomic {

void ConstexprEngine::registerConstexprFunction(const std::string& name, std::shared_ptr<AstNode> functionDecl) {
    constexprFunctions_[name] = functionDecl;
}

std::optional<ConstexprValue> ConstexprEngine::evaluateExpression(
    const AstNode& expr,
    const std::unordered_map<std::string, ConstexprValue>& localContext
) {
    switch (expr.kind) {
        case AstNodeKind::LiteralExpr: {
            if (expr.evaluatedType && expr.evaluatedType->kind == TypeKind::Bool) {
                return ConstexprValue::makeBool(expr.boolVal);
            }
            if (expr.evaluatedType && expr.evaluatedType->isFloatingPoint()) {
                return ConstexprValue::makeFloat(expr.floatVal);
            }
            return ConstexprValue::makeInt(expr.intVal);
        }
        case AstNodeKind::IdentifierExpr: {
            auto it = localContext.find(expr.name);
            if (it != localContext.end()) {
                return it->second;
            }
            return std::nullopt;
        }
        case AstNodeKind::BinaryExpr: {
            if (expr.children.size() < 2) return std::nullopt;
            auto lhs = evaluateExpression(*expr.children[0], localContext);
            auto rhs = evaluateExpression(*expr.children[1], localContext);
            if (!lhs || !rhs) return std::nullopt;

            if (expr.op == "+") return ConstexprValue::makeInt(lhs->iVal + rhs->iVal);
            if (expr.op == "-") return ConstexprValue::makeInt(lhs->iVal - rhs->iVal);
            if (expr.op == "*") return ConstexprValue::makeInt(lhs->iVal * rhs->iVal);
            if (expr.op == "/") return (rhs->iVal != 0) ? std::optional<ConstexprValue>(ConstexprValue::makeInt(lhs->iVal / rhs->iVal)) : std::nullopt;
            if (expr.op == "%") return (rhs->iVal != 0) ? std::optional<ConstexprValue>(ConstexprValue::makeInt(lhs->iVal % rhs->iVal)) : std::nullopt;
            if (expr.op == "==") return ConstexprValue::makeBool(lhs->iVal == rhs->iVal);
            if (expr.op == "!=") return ConstexprValue::makeBool(lhs->iVal != rhs->iVal);
            if (expr.op == "<") return ConstexprValue::makeBool(lhs->iVal < rhs->iVal);
            if (expr.op == "<=") return ConstexprValue::makeBool(lhs->iVal <= rhs->iVal);
            if (expr.op == ">") return ConstexprValue::makeBool(lhs->iVal > rhs->iVal);
            if (expr.op == ">=") return ConstexprValue::makeBool(lhs->iVal >= rhs->iVal);
            break;
        }
        case AstNodeKind::UnaryExpr: {
            if (expr.children.empty()) return std::nullopt;
            auto opVal = evaluateExpression(*expr.children[0], localContext);
            if (!opVal) return std::nullopt;
            if (expr.op == "-") return ConstexprValue::makeInt(-opVal->iVal);
            if (expr.op == "!") return ConstexprValue::makeBool(!opVal->bVal);
            break;
        }
        case AstNodeKind::CallExpr: {
            auto it = constexprFunctions_.find(expr.name);
            if (it == constexprFunctions_.end() || !it->second) return std::nullopt;

            const auto& fnNode = it->second;
            std::unordered_map<std::string, ConstexprValue> callEnv;

            size_t paramCount = (fnNode->children.empty() ? 0 : fnNode->children.size() - 1);
            for (size_t i = 0; i < paramCount && i < expr.children.size(); ++i) {
                auto argVal = evaluateExpression(*expr.children[i], localContext);
                if (!argVal) return std::nullopt;
                callEnv[fnNode->children[i]->name] = *argVal;
            }

            if (!fnNode->children.empty() && fnNode->children.back()) {
                return executeFunctionBody(*fnNode->children.back(), callEnv);
            }
            break;
        }
        default:
            break;
    }
    return std::nullopt;
}

std::optional<ConstexprValue> ConstexprEngine::executeFunctionBody(
    const AstNode& body,
    std::unordered_map<std::string, ConstexprValue>& env
) {
    if (body.kind == AstNodeKind::CompoundStmt) {
        for (const auto& stmt : body.children) {
            if (!stmt) continue;
            if (stmt->kind == AstNodeKind::ReturnStmt) {
                if (!stmt->children.empty() && stmt->children[0]) {
                    return evaluateExpression(*stmt->children[0], env);
                }
                return ConstexprValue::makeInt(0);
            }
            if (stmt->kind == AstNodeKind::IfStmt) {
                if (!stmt->children.empty() && stmt->children[0]) {
                    auto cond = evaluateExpression(*stmt->children[0], env);
                    if (cond && cond->bVal) {
                        if (stmt->children.size() > 1 && stmt->children[1]) {
                            auto res = executeFunctionBody(*stmt->children[1], env);
                            if (res) return res;
                        }
                    } else if (stmt->children.size() > 2 && stmt->children[2]) {
                        auto res = executeFunctionBody(*stmt->children[2], env);
                        if (res) return res;
                    }
                }
            }
            if (stmt->kind == AstNodeKind::VariableDecl) {
                if (!stmt->children.empty() && stmt->children[0]) {
                    auto val = evaluateExpression(*stmt->children[0], env);
                    if (val) env[stmt->name] = *val;
                }
            }
        }
    }
    return std::nullopt;
}

bool ConstexprEngine::evaluateStaticAssert(const AstNode& condExpr, std::string& outMessage) {
    auto res = evaluateExpression(condExpr);
    if (!res) {
        outMessage = "Static assertion expression could not be evaluated to a constant";
        return false;
    }
    bool pass = (res->kind == ConstexprValue::Kind::Bool) ? res->bVal : (res->iVal != 0);
    if (!pass) {
        outMessage = "Static assertion failed: condition evaluated to false";
        return false;
    }
    return true;
}

} // namespace genomic
