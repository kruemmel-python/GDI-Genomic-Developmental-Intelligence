#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "cpp20/cpp20_lexicon.hpp"
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_types.hpp"
#include "cpp20/genomic_expression_grammar.hpp"

namespace genomic {

class GenomicRuleEngine;

class Cpp20Parser {
public:
    explicit Cpp20Parser(std::vector<Token> tokens);
    Cpp20Parser(std::vector<Token> tokens, const GenomicExpressionGrammar& grammar);
    Cpp20Parser(std::vector<Token> tokens, const GenomicRuleEngine& rules);

    std::shared_ptr<AstNode> parseTranslationUnit();
    static std::shared_ptr<AstNode> parseWithGenome(const std::string& source,
                                                     const GenomicRuleEngine& rules);

    // Specific entry points for testing & modular parsing
    std::shared_ptr<AstNode> parseDeclaration();
    std::shared_ptr<AstNode> parseStatement();
    std::shared_ptr<AstNode> parseExpression();

private:
    std::vector<Token> tokens_;
    size_t cursor_{0};
    GenomicExpressionGrammar grammar_;

    void requireDeclaration(const std::string& kind) const;
    void requireStatement(const std::string& kind) const;

    const Token& peek(size_t offset = 0) const;
    const Token& advance();
    bool match(TokenType type);
    bool matchKeyword(const std::string& kw);
    const Token& expect(TokenType type, const std::string& errorMsg);
    const Token& expectKeyword(const std::string& kw, const std::string& errorMsg);
    bool isAtEnd() const;

    // Declarations
    std::shared_ptr<AstNode> parseNamespaceDecl();
    std::shared_ptr<AstNode> parseFunctionOrVariableDecl();
    std::shared_ptr<AstNode> parseClassDecl();
    std::shared_ptr<AstNode> parseEnumDecl();
    std::shared_ptr<AstNode> parseTemplateDecl();
    std::shared_ptr<AstNode> parseConceptDecl();
    std::shared_ptr<AstNode> parseModuleDecl();
    std::shared_ptr<AstNode> parseImportDecl();

    // Statements
    std::shared_ptr<AstNode> parseCompoundStmt();
    std::shared_ptr<AstNode> parseIfStmt();
    std::shared_ptr<AstNode> parseForStmt();
    std::shared_ptr<AstNode> parseWhileStmt();
    std::shared_ptr<AstNode> parseDoWhileStmt();
    std::shared_ptr<AstNode> parseReturnStmt();
    std::shared_ptr<AstNode> parseTryCatchStmt();
    std::shared_ptr<AstNode> parseCoroutineStmt();

    // Expressions
    std::shared_ptr<AstNode> parseAssignmentExpr();
    std::shared_ptr<AstNode> parsePrecedenceExpr(int minimumPrecedence);
    std::shared_ptr<AstNode> parseUnaryExpr();
    std::shared_ptr<AstNode> parsePostfixExpr();
    std::shared_ptr<AstNode> parsePrimaryExpr();
    std::shared_ptr<AstNode> parseLambdaExpr();

    std::shared_ptr<CppType> parseType();
};

} // namespace genomic
