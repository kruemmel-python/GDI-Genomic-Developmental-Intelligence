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
        case AstNodeKind::TernaryExpr: {
            if (expr.children.size() < 3) return std::nullopt;
            auto cond = evaluateExpression(*expr.children[0], localContext);
            if (!cond) return std::nullopt;
            bool takeTrue = (cond->kind == ConstexprValue::Kind::Bool) ? cond->bVal : (cond->iVal != 0);
            return takeTrue ? evaluateExpression(*expr.children[1], localContext)
                            : evaluateExpression(*expr.children[2], localContext);
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
            if (expr.op == "&") return ConstexprValue::makeInt(lhs->iVal & rhs->iVal);
            if (expr.op == "|") return ConstexprValue::makeInt(lhs->iVal | rhs->iVal);
            if (expr.op == "^") return ConstexprValue::makeInt(lhs->iVal ^ rhs->iVal);
            if (expr.op == "<<") return ConstexprValue::makeInt(lhs->iVal << rhs->iVal);
            if (expr.op == ">>") return ConstexprValue::makeInt(lhs->iVal >> rhs->iVal);
            if (expr.op == "==") return ConstexprValue::makeBool(lhs->iVal == rhs->iVal);
            if (expr.op == "!=") return ConstexprValue::makeBool(lhs->iVal != rhs->iVal);
            if (expr.op == "<") return ConstexprValue::makeBool(lhs->iVal < rhs->iVal);
            if (expr.op == "<=") return ConstexprValue::makeBool(lhs->iVal <= rhs->iVal);
            if (expr.op == ">") return ConstexprValue::makeBool(lhs->iVal > rhs->iVal);
            if (expr.op == ">=") return ConstexprValue::makeBool(lhs->iVal >= rhs->iVal);
            if (expr.op == "<=>") {
                int64_t cmp = (lhs->iVal < rhs->iVal) ? -1 : ((lhs->iVal > rhs->iVal) ? 1 : 0);
                return ConstexprValue::makeInt(cmp);
            }
            if (expr.op == "&&") {
                bool b1 = (lhs->kind == ConstexprValue::Kind::Bool) ? lhs->bVal : (lhs->iVal != 0);
                bool b2 = (rhs->kind == ConstexprValue::Kind::Bool) ? rhs->bVal : (rhs->iVal != 0);
                return ConstexprValue::makeBool(b1 && b2);
            }
            if (expr.op == "||") {
                bool b1 = (lhs->kind == ConstexprValue::Kind::Bool) ? lhs->bVal : (lhs->iVal != 0);
                bool b2 = (rhs->kind == ConstexprValue::Kind::Bool) ? rhs->bVal : (rhs->iVal != 0);
                return ConstexprValue::makeBool(b1 || b2);
            }
            break;
        }
        case AstNodeKind::UnaryExpr: {
            if (expr.children.empty()) return std::nullopt;
            auto opVal = evaluateExpression(*expr.children[0], localContext);
            if (!opVal) return std::nullopt;
            if (expr.op == "+") return ConstexprValue::makeInt(+opVal->iVal);
            if (expr.op == "-") return ConstexprValue::makeInt(-opVal->iVal);
            if (expr.op == "~") return ConstexprValue::makeInt(~opVal->iVal);
            if (expr.op == "!") {
                bool b = (opVal->kind == ConstexprValue::Kind::Bool) ? opVal->bVal : (opVal->iVal != 0);
                return ConstexprValue::makeBool(!b);
            }
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
    auto execStmt = [&](const AstNode& stmt, bool& hitBreak, auto& selfRef) -> std::optional<ConstexprValue> {
        if (stmt.kind == AstNodeKind::ReturnStmt) {
            if (!stmt.children.empty() && stmt.children[0]) {
                return evaluateExpression(*stmt.children[0], env);
            }
            return ConstexprValue::makeInt(0);
        }
        if (stmt.kind == AstNodeKind::BreakStmt) {
            hitBreak = true;
            return std::nullopt;
        }
        if (stmt.kind == AstNodeKind::VariableDecl) {
            if (!stmt.children.empty() && stmt.children[0]) {
                auto val = evaluateExpression(*stmt.children[0], env);
                if (val) env[stmt.name] = *val;
            }
            return std::nullopt;
        }
        if (stmt.kind == AstNodeKind::BinaryExpr &&
            (stmt.op == "=" || stmt.op == "+=" || stmt.op == "-=" || stmt.op == "*=" || stmt.op == "/=") &&
            stmt.children.size() == 2 && stmt.children[0] && stmt.children[0]->kind == AstNodeKind::IdentifierExpr) {
            auto rhs = evaluateExpression(*stmt.children[1], env);
            if (rhs) {
                const std::string& vName = stmt.children[0]->name;
                if (stmt.op == "=") env[vName] = *rhs;
                else if (stmt.op == "+=") env[vName].iVal += rhs->iVal;
                else if (stmt.op == "-=") env[vName].iVal -= rhs->iVal;
                else if (stmt.op == "*=") env[vName].iVal *= rhs->iVal;
                else if (stmt.op == "/=" && rhs->iVal != 0) env[vName].iVal /= rhs->iVal;
            }
            return std::nullopt;
        }
        if (stmt.kind == AstNodeKind::IfStmt) {
            if (!stmt.children.empty() && stmt.children[0]) {
                auto cond = evaluateExpression(*stmt.children[0], env);
                bool take = cond && ((cond->kind == ConstexprValue::Kind::Bool) ? cond->bVal : (cond->iVal != 0));
                if (take) {
                    if (stmt.children.size() > 1 && stmt.children[1]) {
                        return selfRef(*stmt.children[1], hitBreak, selfRef);
                    }
                } else if (stmt.children.size() > 2 && stmt.children[2]) {
                    return selfRef(*stmt.children[2], hitBreak, selfRef);
                }
            }
            return std::nullopt;
        }
        if (stmt.kind == AstNodeKind::SwitchStmt) {
            size_t condIdx = stmt.boolVal ? 1 : 0;
            if (stmt.boolVal && !stmt.children.empty() && stmt.children[0]) {
                bool dummyBreak = false;
                auto initRes = selfRef(*stmt.children[0], dummyBreak, selfRef);
                if (initRes) return initRes;
            }
            if (condIdx >= stmt.children.size() || !stmt.children[condIdx]) return std::nullopt;
            auto condVal = evaluateExpression(*stmt.children[condIdx], env);
            if (!condVal) return std::nullopt;
            int64_t targetVal = (condVal->kind == ConstexprValue::Kind::Bool) ? (condVal->bVal ? 1 : 0) : condVal->iVal;

            size_t matchedIdx = stmt.children.size();
            size_t defaultIdx = stmt.children.size();
            for (size_t i = condIdx + 1; i < stmt.children.size(); ++i) {
                const auto& cNode = stmt.children[i];
                if (!cNode) continue;
                if (cNode->kind == AstNodeKind::CaseStmt && !cNode->children.empty() && cNode->children[0]) {
                    auto cVal = evaluateExpression(*cNode->children[0], env);
                    if (cVal && cVal->iVal == targetVal) {
                        matchedIdx = i;
                        break;
                    }
                } else if (cNode->kind == AstNodeKind::DefaultStmt && defaultIdx == stmt.children.size()) {
                    defaultIdx = i;
                }
            }

            size_t startIdx = (matchedIdx < stmt.children.size()) ? matchedIdx : defaultIdx;
            if (startIdx < stmt.children.size()) {
                bool switchBreak = false;
                for (size_t i = startIdx; i < stmt.children.size() && !switchBreak; ++i) {
                    const auto& cNode = stmt.children[i];
                    if (!cNode) continue;
                    size_t firstStmt = (cNode->kind == AstNodeKind::CaseStmt) ? 1 : 0;
                    for (size_t s = firstStmt; s < cNode->children.size(); ++s) {
                        if (!cNode->children[s]) continue;
                        auto res = selfRef(*cNode->children[s], switchBreak, selfRef);
                        if (res) return res;
                        if (switchBreak) break;
                    }
                }
            }
            return std::nullopt;
        }
        if (stmt.kind == AstNodeKind::CompoundStmt) {
            for (const auto& child : stmt.children) {
                if (!child) continue;
                auto res = selfRef(*child, hitBreak, selfRef);
                if (res) return res;
                if (hitBreak) break;
            }
            return std::nullopt;
        }
        return std::nullopt;
    };

    bool hitBreak = false;
    return execStmt(body, hitBreak, execStmt);
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
