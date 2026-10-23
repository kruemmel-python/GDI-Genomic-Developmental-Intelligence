#include "cpp20/cpp20_ast.hpp"
#include <sstream>

namespace genomic {

std::shared_ptr<AstNode> AstNode::makeTranslationUnit() {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::TranslationUnit;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeFunctionDecl(
    const std::string& name,
    std::shared_ptr<CppType> returnType,
    std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params,
    std::shared_ptr<AstNode> body,
    bool isConstexpr,
    bool isCoroutine
) {
    (void)isCoroutine;
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::FunctionDecl;
    n->name = name;
    n->evaluatedType = returnType;
    n->boolVal = isConstexpr;

    // Children: [0..k-1] parameters as VariableDecl, [k] body
    for (const auto& [pName, pType] : params) {
        auto paramNode = std::make_shared<AstNode>();
        paramNode->kind = AstNodeKind::VariableDecl;
        paramNode->name = pName;
        paramNode->evaluatedType = pType;
        n->children.push_back(paramNode);
    }
    if (body) {
        n->children.push_back(body);
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeVariableDecl(
    const std::string& name,
    std::shared_ptr<CppType> type,
    std::shared_ptr<AstNode> initializer,
    bool isConstexpr
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::VariableDecl;
    n->name = name;
    n->evaluatedType = type;
    n->boolVal = isConstexpr;
    if (initializer) {
        n->children.push_back(initializer);
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeBinaryExpr(
    const std::string& op,
    std::shared_ptr<AstNode> lhs,
    std::shared_ptr<AstNode> rhs
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::BinaryExpr;
    n->op = op;
    n->children.push_back(lhs);
    n->children.push_back(rhs);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeUnaryExpr(
    const std::string& op,
    std::shared_ptr<AstNode> operand,
    bool isPrefix
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::UnaryExpr;
    n->op = op;
    n->boolVal = isPrefix;
    n->children.push_back(operand);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeCallExpr(
    const std::string& funcName,
    std::vector<std::shared_ptr<AstNode>> args
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::CallExpr;
    n->name = funcName;
    n->children = std::move(args);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeLiteralInt(int64_t val) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::LiteralExpr;
    n->evaluatedType = CppType::makeInt64();
    n->intVal = val;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeLiteralFloat(double val) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::LiteralExpr;
    n->evaluatedType = CppType::makeFloat64();
    n->floatVal = val;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeLiteralBool(bool val) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::LiteralExpr;
    n->evaluatedType = CppType::makeBool();
    n->boolVal = val;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeLiteralString(const std::string& val) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::LiteralExpr;
    n->evaluatedType = CppType::makePointer(std::make_shared<CppType>(TypeKind::Char, true));
    n->strVal = val;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeIdentifier(const std::string& name) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::IdentifierExpr;
    n->name = name;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeIfStmt(
    std::shared_ptr<AstNode> condition,
    std::shared_ptr<AstNode> thenBranch,
    std::shared_ptr<AstNode> elseBranch,
    bool isConstexpr
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::IfStmt;
    n->boolVal = isConstexpr;
    n->children.push_back(condition);
    n->children.push_back(thenBranch);
    if (elseBranch) {
        n->children.push_back(elseBranch);
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeSwitchStmt(
    std::shared_ptr<AstNode> condition,
    std::vector<std::shared_ptr<AstNode>> cases,
    std::shared_ptr<AstNode> initStmt
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::SwitchStmt;
    n->boolVal = (initStmt != nullptr);
    if (initStmt) {
        n->children.push_back(initStmt);
    }
    n->children.push_back(condition);
    for (auto& c : cases) {
        n->children.push_back(std::move(c));
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeCaseStmt(
    std::shared_ptr<AstNode> caseValue,
    std::vector<std::shared_ptr<AstNode>> statements,
    bool hasFallthroughAttr
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::CaseStmt;
    n->boolVal = hasFallthroughAttr;
    n->children.push_back(caseValue);
    for (auto& s : statements) {
        n->children.push_back(std::move(s));
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeDefaultStmt(
    std::vector<std::shared_ptr<AstNode>> statements
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::DefaultStmt;
    n->children = std::move(statements);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeTernaryExpr(
    std::shared_ptr<AstNode> condition,
    std::shared_ptr<AstNode> trueExpr,
    std::shared_ptr<AstNode> falseExpr
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::TernaryExpr;
    n->op = "?:";
    n->children.push_back(condition);
    n->children.push_back(trueExpr);
    n->children.push_back(falseExpr);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeBreakStmt() {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::BreakStmt;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeContinueStmt() {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::ContinueStmt;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeForStmt(
    std::shared_ptr<AstNode> init,
    std::shared_ptr<AstNode> condition,
    std::shared_ptr<AstNode> step,
    std::shared_ptr<AstNode> body
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::ForStmt;
    n->children.push_back(init);
    n->children.push_back(condition);
    n->children.push_back(step);
    n->children.push_back(body);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeWhileStmt(
    std::shared_ptr<AstNode> condition,
    std::shared_ptr<AstNode> body
) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::WhileStmt;
    n->children.push_back(condition);
    n->children.push_back(body);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeReturnStmt(std::shared_ptr<AstNode> expr) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::ReturnStmt;
    if (expr) {
        n->children.push_back(expr);
    }
    return n;
}

std::shared_ptr<AstNode> AstNode::makeCompoundStmt(std::vector<std::shared_ptr<AstNode>> stmts) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::CompoundStmt;
    n->children = std::move(stmts);
    return n;
}

std::shared_ptr<AstNode> AstNode::makeEmitMolecule(uint64_t signature, double concentration) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::EmitMoleculeExpr;
    n->intVal = static_cast<int64_t>(signature);
    n->floatVal = concentration;
    return n;
}

std::shared_ptr<AstNode> AstNode::makeCoroutineSuspend(std::shared_ptr<AstNode> expr) {
    auto n = std::make_shared<AstNode>();
    n->kind = AstNodeKind::CoroutineExpr;
    n->name = "co_await";
    if (expr) {
        n->children.push_back(expr);
    }
    return n;
}

std::string Cpp20Printer::print(const AstNode& node, int indent) {
    std::string ind(indent * 4, ' ');
    std::ostringstream ss;

    switch (node.kind) {
        case AstNodeKind::TranslationUnit: {
            for (const auto& child : node.children) {
                if (child) {
                    ss << print(*child, indent) << "\n";
                }
            }
            break;
        }
        case AstNodeKind::FunctionDecl: {
            ss << ind;
            if (node.boolVal) ss << "constexpr ";
            ss << (node.evaluatedType ? node.evaluatedType->toString() : "void") << " " << node.name << "(";
            size_t paramCount = (node.children.empty() ? 0 : node.children.size() - 1);
            for (size_t i = 0; i < paramCount; ++i) {
                if (i > 0) ss << ", ";
                const auto& p = node.children[i];
                ss << (p->evaluatedType ? p->evaluatedType->toString() : "auto") << " " << p->name;
            }
            ss << ")\n";
            if (!node.children.empty()) {
                ss << print(*node.children.back(), indent);
            } else {
                ss << ind << "{}\n";
            }
            break;
        }
        case AstNodeKind::VariableDecl: {
            ss << ind;
            if (node.boolVal) ss << "constexpr ";
            ss << (node.evaluatedType ? node.evaluatedType->toString() : "auto") << " " << node.name;
            if (!node.children.empty() && node.children[0]) {
                ss << " = " << print(*node.children[0], 0);
            }
            if (indent > 0) ss << ";";
            break;
        }
        case AstNodeKind::CompoundStmt: {
            ss << ind << "{\n";
            for (const auto& child : node.children) {
                if (child) {
                    ss << print(*child, indent + 1);
                    if (child->kind != AstNodeKind::IfStmt &&
                        child->kind != AstNodeKind::SwitchStmt &&
                        child->kind != AstNodeKind::CaseStmt &&
                        child->kind != AstNodeKind::DefaultStmt &&
                        child->kind != AstNodeKind::ForStmt &&
                        child->kind != AstNodeKind::WhileStmt &&
                        child->kind != AstNodeKind::CompoundStmt) {
                        ss << ";";
                    }
                    ss << "\n";
                }
            }
            ss << ind << "}";
            break;
        }
        case AstNodeKind::SwitchStmt: {
            ss << ind << "switch (";
            size_t condIdx = node.boolVal ? 1 : 0;
            if (node.boolVal && !node.children.empty() && node.children[0]) {
                ss << print(*node.children[0], 0) << "; ";
            }
            if (condIdx < node.children.size() && node.children[condIdx]) {
                ss << print(*node.children[condIdx], 0);
            }
            ss << ") {\n";
            for (size_t i = condIdx + 1; i < node.children.size(); ++i) {
                if (node.children[i]) {
                    ss << print(*node.children[i], indent + 1) << "\n";
                }
            }
            ss << ind << "}";
            break;
        }
        case AstNodeKind::CaseStmt: {
            ss << ind << "case ";
            if (!node.children.empty() && node.children[0]) {
                ss << print(*node.children[0], 0);
            }
            ss << ":\n";
            for (size_t i = 1; i < node.children.size(); ++i) {
                const auto& stmt = node.children[i];
                if (!stmt) continue;
                ss << print(*stmt, indent + 1);
                if (stmt->kind != AstNodeKind::CompoundStmt &&
                    stmt->kind != AstNodeKind::IfStmt &&
                    stmt->kind != AstNodeKind::SwitchStmt &&
                    stmt->kind != AstNodeKind::ForStmt &&
                    stmt->kind != AstNodeKind::WhileStmt) {
                    ss << ";";
                }
                if (i + 1 < node.children.size() || node.boolVal) ss << "\n";
            }
            if (node.boolVal) {
                ss << std::string((indent + 1) * 4, ' ') << "[[fallthrough]];";
            }
            break;
        }
        case AstNodeKind::DefaultStmt: {
            ss << ind << "default:\n";
            for (size_t i = 0; i < node.children.size(); ++i) {
                const auto& stmt = node.children[i];
                if (!stmt) continue;
                ss << print(*stmt, indent + 1);
                if (stmt->kind != AstNodeKind::CompoundStmt &&
                    stmt->kind != AstNodeKind::IfStmt &&
                    stmt->kind != AstNodeKind::SwitchStmt &&
                    stmt->kind != AstNodeKind::ForStmt &&
                    stmt->kind != AstNodeKind::WhileStmt) {
                    ss << ";";
                }
                if (i + 1 < node.children.size()) ss << "\n";
            }
            break;
        }
        case AstNodeKind::BreakStmt: {
            ss << ind << "break";
            break;
        }
        case AstNodeKind::ContinueStmt: {
            ss << ind << "continue";
            break;
        }
        case AstNodeKind::TernaryExpr: {
            if (node.children.size() >= 3 && node.children[0] && node.children[1] && node.children[2]) {
                ss << "(" << print(*node.children[0], 0) << " ? "
                   << print(*node.children[1], 0) << " : "
                   << print(*node.children[2], 0) << ")";
            }
            break;
        }
        case AstNodeKind::MemberAccessExpr: {
            if (!node.children.empty() && node.children[0]) {
                ss << print(*node.children[0], 0) << node.op << node.name;
            }
            break;
        }
        case AstNodeKind::SubscriptExpr: {
            if (node.children.size() >= 2 && node.children[0] && node.children[1]) {
                ss << print(*node.children[0], 0) << "[" << print(*node.children[1], 0) << "]";
            }
            break;
        }
        case AstNodeKind::CastExpr: {
            ss << node.name << "<" << (node.evaluatedType ? node.evaluatedType->toString() : "auto") << ">(";
            if (!node.children.empty() && node.children[0]) {
                ss << print(*node.children[0], 0);
            }
            ss << ")";
            break;
        }
        case AstNodeKind::IfStmt: {
            ss << ind << (node.boolVal ? "if constexpr (" : "if (");
            if (!node.children.empty() && node.children[0]) {
                ss << print(*node.children[0], 0);
            }
            ss << ") ";
            if (node.children.size() > 1 && node.children[1]) {
                if (node.children[1]->kind == AstNodeKind::CompoundStmt) {
                    ss << "\n" << print(*node.children[1], indent);
                } else {
                    ss << print(*node.children[1], 0) << ";";
                }
            }
            if (node.children.size() > 2 && node.children[2]) {
                ss << " else ";
                if (node.children[2]->kind == AstNodeKind::CompoundStmt) {
                    ss << "\n" << print(*node.children[2], indent);
                } else {
                    ss << print(*node.children[2], 0) << ";";
                }
            }
            break;
        }
        case AstNodeKind::ForStmt: {
            ss << ind << "for (";
            if (node.children.size() > 0 && node.children[0]) ss << print(*node.children[0], 0);
            ss << "; ";
            if (node.children.size() > 1 && node.children[1]) ss << print(*node.children[1], 0);
            ss << "; ";
            if (node.children.size() > 2 && node.children[2]) ss << print(*node.children[2], 0);
            ss << ") ";
            if (node.children.size() > 3 && node.children[3]) {
                ss << "\n" << print(*node.children[3], indent);
            }
            break;
        }
        case AstNodeKind::WhileStmt: {
            ss << ind << "while (";
            if (node.children.size() > 0 && node.children[0]) ss << print(*node.children[0], 0);
            ss << ") ";
            if (node.children.size() > 1 && node.children[1]) {
                ss << "\n" << print(*node.children[1], indent);
            }
            break;
        }
        case AstNodeKind::ReturnStmt: {
            ss << ind << "return";
            if (!node.children.empty() && node.children[0]) {
                ss << " " << print(*node.children[0], 0);
            }
            break;
        }
        case AstNodeKind::BinaryExpr: {
            if (node.children.size() >= 2 && node.children[0] && node.children[1]) {
                ss << "(" << print(*node.children[0], 0) << " " << node.op << " " << print(*node.children[1], 0) << ")";
            }
            break;
        }
        case AstNodeKind::UnaryExpr: {
            if (!node.children.empty() && node.children[0]) {
                if (node.boolVal) {
                    ss << node.op << print(*node.children[0], 0);
                } else {
                    ss << print(*node.children[0], 0) << node.op;
                }
            }
            break;
        }
        case AstNodeKind::CallExpr: {
            ss << node.name << "(";
            for (size_t i = 0; i < node.children.size(); ++i) {
                if (i > 0) ss << ", ";
                if (node.children[i]) ss << print(*node.children[i], 0);
            }
            ss << ")";
            break;
        }
        case AstNodeKind::IdentifierExpr: {
            ss << node.name;
            break;
        }
        case AstNodeKind::LiteralExpr: {
            if (node.evaluatedType) {
                if (node.evaluatedType->kind == TypeKind::Bool) {
                    ss << (node.boolVal ? "true" : "false");
                } else if (node.evaluatedType->isFloatingPoint()) {
                    ss << node.floatVal;
                } else if (node.evaluatedType->isInteger()) {
                    ss << node.intVal;
                } else if (node.evaluatedType->isPointer()) {
                    ss << "\"" << node.strVal << "\"";
                }
            } else {
                ss << node.intVal;
            }
            break;
        }
        case AstNodeKind::EmitMoleculeExpr: {
            ss << "emitMolecule(0x" << std::hex << node.intVal << std::dec << ", " << node.floatVal << ")";
            break;
        }
        case AstNodeKind::CoroutineExpr: {
            ss << node.name;
            if (!node.children.empty() && node.children[0]) {
                ss << " " << print(*node.children[0], 0);
            }
            break;
        }
        default:
            ss << "/* AST node " << static_cast<int>(node.kind) << " */";
            break;
    }

    return ss.str();
}

} // namespace genomic
