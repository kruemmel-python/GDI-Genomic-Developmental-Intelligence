#include "cpp20/cpp20_parser.hpp"
#include "genome/genomic_rule_engine.hpp"
#include <iostream>
#include <cctype>

namespace genomic {

Cpp20Parser::Cpp20Parser(std::vector<Token> tokens)
    : Cpp20Parser(std::move(tokens), GenomicExpressionGrammar::standardCpp20()) {}

Cpp20Parser::Cpp20Parser(std::vector<Token> tokens, const GenomicExpressionGrammar& grammar)
    : tokens_(std::move(tokens)), cursor_(0), grammar_(grammar) {
    if (tokens_.empty()) tokens_.push_back(Token{});
}

Cpp20Parser::Cpp20Parser(std::vector<Token> tokens, const GenomicRuleEngine& rules)
    : Cpp20Parser(std::move(tokens), rules.getExpressionGrammar()) {}

std::shared_ptr<AstNode> Cpp20Parser::parseWithGenome(const std::string& source,
                                                        const GenomicRuleEngine& rules) {
    Cpp20Parser parser(Cpp20Lexicon::tokenize(source, rules.getExpressionGrammar()), rules);
    return parser.parseTranslationUnit();
}

void Cpp20Parser::requireDeclaration(const std::string& kind) const {
    if (!grammar_.declarationEnabled(kind))
        throw std::runtime_error("Genomic grammar prohibits declaration: " + kind);
}

void Cpp20Parser::requireStatement(const std::string& kind) const {
    if (!grammar_.statementEnabled(kind))
        throw std::runtime_error("Genomic grammar prohibits statement: " + kind);
}


const Token& Cpp20Parser::peek(size_t offset) const {
    if (cursor_ + offset >= tokens_.size()) {
        return tokens_.back(); // EndOfFile
    }
    return tokens_[cursor_ + offset];
}

const Token& Cpp20Parser::advance() {
    if (!isAtEnd()) {
        cursor_++;
    }
    return tokens_[cursor_ - 1];
}

bool Cpp20Parser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

bool Cpp20Parser::matchKeyword(const std::string& kw) {
    if (peek().type == TokenType::Keyword && peek().text == kw) {
        advance();
        return true;
    }
    return false;
}

const Token& Cpp20Parser::expect(TokenType type, const std::string& errorMsg) {
    if (peek().type == type) {
        return advance();
    }
    throw std::runtime_error("Parse error at line " + std::to_string(peek().line) +
                             ", col " + std::to_string(peek().column) + ": " + errorMsg +
                             " (got '" + peek().text + "')");
}

const Token& Cpp20Parser::expectKeyword(const std::string& kw, const std::string& errorMsg) {
    if (peek().type == TokenType::Keyword && peek().text == kw) {
        return advance();
    }
    throw std::runtime_error("Parse error at line " + std::to_string(peek().line) +
                             ", col " + std::to_string(peek().column) + ": " + errorMsg);
}

bool Cpp20Parser::isAtEnd() const {
    return cursor_ >= tokens_.size() || peek().type == TokenType::EndOfFile;
}

std::shared_ptr<AstNode> Cpp20Parser::parseTranslationUnit() {
    auto tu = AstNode::makeTranslationUnit();
    while (!isAtEnd()) {
        if (peek().type == TokenType::Semicolon) {
            advance();
            continue;
        }
        auto decl = parseDeclaration();
        if (decl) {
            tu->children.push_back(decl);
        } else {
            if (grammar_.strict())
                throw std::runtime_error("Unsupported translation-unit token: " + peek().text);
            advance(); // Legacy parser recovery
        }
    }
    return tu;
}

std::shared_ptr<AstNode> Cpp20Parser::parseDeclaration() {
    if (matchKeyword("namespace")) {
        requireDeclaration("namespace_decl");
        return parseNamespaceDecl();
    }
    if (matchKeyword("template")) {
        requireDeclaration("template_decl");
        return parseTemplateDecl();
    }
    if (matchKeyword("concept")) {
        requireDeclaration("concept_decl");
        return parseConceptDecl();
    }
    if (matchKeyword("class") || matchKeyword("struct") || matchKeyword("union")) {
        requireDeclaration("class_decl");
        return parseClassDecl();
    }
    if (matchKeyword("enum")) {
        requireDeclaration("enum_decl");
        return parseEnumDecl();
    }
    if (matchKeyword("export")) {
        requireDeclaration("export_decl");
        if (matchKeyword("module")) {
            requireDeclaration("module_decl");
            return parseModuleDecl();
        }
        // Exported declaration
        return parseDeclaration();
    }
    if (matchKeyword("import")) {
        requireDeclaration("import_decl");
        return parseImportDecl();
    }
    if (matchKeyword("module")) {
        requireDeclaration("module_decl");
        return parseModuleDecl();
    }
    return parseFunctionOrVariableDecl();
}

std::shared_ptr<AstNode> Cpp20Parser::parseNamespaceDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected namespace name");
    expect(TokenType::OpenBrace, "Expected '{' after namespace name");
    auto nsNode = std::make_shared<AstNode>();
    nsNode->kind = AstNodeKind::NamespaceDecl;
    nsNode->name = nameTok.text;

    while (peek().type != TokenType::CloseBrace && !isAtEnd()) {
        auto child = parseDeclaration();
        if (child) nsNode->children.push_back(child);
        else if (grammar_.strict()) throw std::runtime_error("Invalid namespace declaration");
    }
    expect(TokenType::CloseBrace, "Expected closing brace for namespace");
    return nsNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseTemplateDecl() {
    expect(TokenType::Less, "Expected '<' after 'template'");
    std::string paramList;
    int depth = 1;
    while (!isAtEnd()) {
        if (peek().type == TokenType::Less) depth++;
        else if (peek().type == TokenType::Greater) {
            depth--;
            if (depth == 0) {
                advance();
                break;
            }
        }
        paramList += advance().text + " ";
    }

    // Optional requires clause: requires Concept<T>
    std::string constraintExpr;
    if (matchKeyword("requires")) {
        while (!isAtEnd() && peek().type != TokenType::OpenBrace &&
               peek().type != TokenType::Semicolon &&
               peek().type != TokenType::Keyword) {
            constraintExpr += advance().text + " ";
        }
    }

    auto decl = parseDeclaration();
    auto tNode = std::make_shared<AstNode>();
    tNode->kind = AstNodeKind::TemplateDecl;
    tNode->name = paramList;
    tNode->strVal = constraintExpr;
    if (decl) tNode->children.push_back(decl);
    return tNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseConceptDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected concept name");
    expect(TokenType::Equal, "Expected '=' in concept definition");

    auto cNode = std::make_shared<AstNode>();
    cNode->kind = AstNodeKind::ConceptDecl;
    cNode->name = nameTok.text;

    // Expression or requires expression
    if (matchKeyword("requires")) {
        expect(TokenType::OpenParen, "Expected '(' after 'requires'");
        std::string reqArgs;
        while (!match(TokenType::CloseParen) && !isAtEnd()) {
            reqArgs += advance().text + " ";
        }
        expect(TokenType::OpenBrace, "Expected '{' for requires body");
        std::string reqBody;
        int depth = 1;
        while (!isAtEnd()) {
            if (peek().type == TokenType::OpenBrace) depth++;
            else if (peek().type == TokenType::CloseBrace) {
                depth--;
                if (depth == 0) {
                    advance();
                    break;
                }
            }
            reqBody += advance().text + " ";
        }
        cNode->strVal = "requires(" + reqArgs + ") { " + reqBody + " }";
    } else {
        auto expr = parseExpression();
        cNode->children.push_back(expr);
    }
    match(TokenType::Semicolon);
    return cNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseClassDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected class/struct name");
    auto classNode = std::make_shared<AstNode>();
    classNode->kind = AstNodeKind::RecordDecl;
    classNode->name = nameTok.text;

    // Base classes (: public Base)
    if (match(TokenType::Colon)) {
        while (!isAtEnd() && peek().type != TokenType::OpenBrace) {
            advance();
        }
    }

    if (match(TokenType::OpenBrace)) {
        while (!match(TokenType::CloseBrace) && !isAtEnd()) {
            if (matchKeyword("public") || matchKeyword("private") || matchKeyword("protected")) {
                expect(TokenType::Colon, "Expected ':' after access specifier");
                continue;
            }
            size_t beforeCursor = cursor_;
            auto member = parseDeclaration();
            if (member) {
                classNode->children.push_back(member);
            }
            if (cursor_ == beforeCursor && !isAtEnd()) {
                advance();
            }
        }
        match(TokenType::Semicolon);
    } else {
        match(TokenType::Semicolon); // forward declaration
    }
    return classNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseEnumDecl() {
    matchKeyword("class"); // optional 'class'
    auto nameTok = expect(TokenType::Identifier, "Expected enum name");
    auto enumNode = std::make_shared<AstNode>();
    enumNode->kind = AstNodeKind::RecordDecl;
    enumNode->name = "enum_" + nameTok.text;

    if (match(TokenType::OpenBrace)) {
        while (!match(TokenType::CloseBrace) && !isAtEnd()) {
            advance();
        }
        match(TokenType::Semicolon);
    }
    return enumNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseModuleDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected module name");
    expect(TokenType::Semicolon, "Expected ';' after module name");
    auto mNode = std::make_shared<AstNode>();
    mNode->kind = AstNodeKind::NamespaceDecl;
    mNode->name = "module_" + nameTok.text;
    return mNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseImportDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected imported module name");
    expect(TokenType::Semicolon, "Expected ';' after import");
    auto mNode = std::make_shared<AstNode>();
    mNode->kind = AstNodeKind::NamespaceDecl;
    mNode->name = "import_" + nameTok.text;
    return mNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseFunctionOrVariableDecl() {
    bool isConstexpr = false;
    bool isStatic = false;
    bool isVirtual = false;
    bool isInline = false;
    bool isExplicit = false;
    bool isFriend = false;

    while (true) {
        if (matchKeyword("constexpr") || matchKeyword("consteval") || matchKeyword("constinit")) {
            isConstexpr = true;
        } else if (matchKeyword("static")) {
            isStatic = true;
        } else if (matchKeyword("virtual")) {
            isVirtual = true;
        } else if (matchKeyword("inline")) {
            isInline = true;
        } else if (matchKeyword("explicit")) {
            isExplicit = true;
        } else if (matchKeyword("friend")) {
            isFriend = true;
        } else {
            break;
        }
    }
    (void)isStatic;
    (void)isVirtual;
    (void)isInline;
    (void)isExplicit;
    (void)isFriend;

    // Structured binding: auto [x, y] = ...;
    if (peek().type == TokenType::Keyword && peek().text == "auto" && peek(1).type == TokenType::OpenBracket) {
        advance(); // auto
        advance(); // [
        std::vector<std::string> idents;
        while (!match(TokenType::CloseBracket) && !isAtEnd()) {
            if (peek().type == TokenType::Identifier) {
                idents.push_back(advance().text);
            }
            match(TokenType::Comma);
        }
        expect(TokenType::Equal, "Expected '=' in structured binding");
        auto initExpr = parseExpression();
        expect(TokenType::Semicolon, "Expected ';' after structured binding");
        auto sbNode = std::make_shared<AstNode>();
        sbNode->kind = AstNodeKind::VariableDecl;
        sbNode->name = idents.empty() ? "structured_binding" : idents[0];
        sbNode->children.push_back(initExpr);
        requireDeclaration("variable_decl");
        return sbNode;
    }

    auto type = parseType();
    if (!type) return nullptr;

    auto nameTok = expect(TokenType::Identifier, "Expected identifier after type");

    // Function declaration / definition: name(...)
    if (match(TokenType::OpenParen)) {
        std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params;
        while (!match(TokenType::CloseParen) && !isAtEnd()) {
            auto pType = parseType();
            std::string pName;
            if (peek().type == TokenType::Identifier) {
                pName = advance().text;
            } else {
                pName = "param_" + std::to_string(params.size());
            }
            params.push_back({pName, pType});
            match(TokenType::Comma);
        }

        // Optional trailing return type or attributes
        if (match(TokenType::Arrow)) {
            type = parseType();
        }
        matchKeyword("noexcept");

        std::shared_ptr<AstNode> body = nullptr;
        if (peek().type == TokenType::OpenBrace) {
            body = parseCompoundStmt();
        } else {
            expect(TokenType::Semicolon, "Expected ';' or '{' after function prototype");
        }

        requireDeclaration("function_decl");
        return AstNode::makeFunctionDecl(nameTok.text, type, params, body, isConstexpr);
    }

    // Variable declaration: name = init;
    std::shared_ptr<AstNode> initializer = nullptr;
    if (match(TokenType::Equal)) {
        initializer = parseExpression();
    }
    expect(TokenType::Semicolon, "Expected ';' after variable declaration");
    requireDeclaration("variable_decl");
    return AstNode::makeVariableDecl(nameTok.text, type, initializer, isConstexpr);
}

std::shared_ptr<CppType> Cpp20Parser::parseType() {
    bool isConst = matchKeyword("const");
    bool isVolatile = matchKeyword("volatile");

    TypeKind kind = TypeKind::Int64;
    std::string typeName;

    if (matchKeyword("void")) kind = TypeKind::Void;
    else if (matchKeyword("bool")) kind = TypeKind::Bool;
    else if (matchKeyword("char")) kind = TypeKind::Char;
    else if (matchKeyword("char8_t")) kind = TypeKind::Char8;
    else if (matchKeyword("char16_t")) kind = TypeKind::Char16;
    else if (matchKeyword("char32_t")) kind = TypeKind::Char32;
    else if (matchKeyword("wchar_t")) kind = TypeKind::WChar;
    else if (matchKeyword("int")) kind = TypeKind::Int32;
    else if (matchKeyword("short")) kind = TypeKind::Int16;
    else if (matchKeyword("long")) {
        if (matchKeyword("long")) kind = TypeKind::Int64;
        else kind = TypeKind::Int64;
    }
    else if (matchKeyword("float")) kind = TypeKind::Float32;
    else if (matchKeyword("double")) kind = TypeKind::Float64;
    else if (matchKeyword("auto")) kind = TypeKind::Auto;
    else if (matchKeyword("decltype")) {
        expect(TokenType::OpenParen, "Expected '(' after decltype");
        if (matchKeyword("auto")) {
            expect(TokenType::CloseParen, "Expected ')' after decltype(auto)");
            kind = TypeKind::Auto;
        } else {
            while (!match(TokenType::CloseParen) && !isAtEnd()) advance();
            kind = TypeKind::Auto;
        }
    } else if (peek().type == TokenType::Identifier) {
        typeName = advance().text;
        kind = TypeKind::Record;
    } else {
        return nullptr;
    }

    auto type = std::make_shared<CppType>(kind, isConst, isVolatile, typeName);

    // Pointer or Reference modifiers (*, &, &&)
    while (!isAtEnd()) {
        if (match(TokenType::Star)) {
            type = CppType::makePointer(type, matchKeyword("const"));
        } else if (match(TokenType::AmpAmp)) {
            type = CppType::makeRValueRef(type);
        } else if (match(TokenType::Amp)) {
            type = CppType::makeLValueRef(type);
        } else {
            break;
        }
    }

    return type;
}

std::shared_ptr<AstNode> Cpp20Parser::parseStatement() {
    if (peek().type == TokenType::OpenBrace) {
        requireStatement("compound_stmt");
        return parseCompoundStmt();
    }
    if (matchKeyword("if")) {
        requireStatement("if_stmt");
        return parseIfStmt();
    }
    if (matchKeyword("for")) {
        requireStatement("for_stmt");
        return parseForStmt();
    }
    if (matchKeyword("while")) {
        requireStatement("while_stmt");
        return parseWhileStmt();
    }
    if (matchKeyword("do")) {
        requireStatement("do_stmt");
        return parseDoWhileStmt();
    }
    if (matchKeyword("return")) {
        requireStatement("return_stmt");
        return parseReturnStmt();
    }
    if (matchKeyword("try")) {
        requireStatement("try_stmt");
        return parseTryCatchStmt();
    }
    if (matchKeyword("co_await") || matchKeyword("co_yield") || matchKeyword("co_return")) {
        requireStatement("coroutine_stmt");
        return parseCoroutineStmt();
    }
    if (matchKeyword("break")) {
        requireStatement("break_stmt");
        expect(TokenType::Semicolon, "Expected ';' after break");
        auto bNode = std::make_shared<AstNode>();
        bNode->kind = AstNodeKind::BreakStmt;
        return bNode;
    }
    if (matchKeyword("continue")) {
        requireStatement("continue_stmt");
        expect(TokenType::Semicolon, "Expected ';' after continue");
        auto cNode = std::make_shared<AstNode>();
        cNode->kind = AstNodeKind::ContinueStmt;
        return cNode;
    }

    // Try parsing variable declaration statement, else expression statement
    size_t saved = cursor_;
    try {
        auto decl = parseFunctionOrVariableDecl();
        if (decl) return decl;
        cursor_ = saved;
    } catch (...) {
        cursor_ = saved;
    }

    requireStatement("expression_stmt");
    auto expr = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' after expression statement");
    return expr;
}

std::shared_ptr<AstNode> Cpp20Parser::parseCompoundStmt() {
    expect(TokenType::OpenBrace, "Expected '{'");
    std::vector<std::shared_ptr<AstNode>> stmts;
    while (peek().type != TokenType::CloseBrace && !isAtEnd()) {
        size_t beforeCursor = cursor_;
        auto s = parseStatement();
        if (s) stmts.push_back(s);
        if (cursor_ == beforeCursor && !isAtEnd()) {
            advance();
        }
    }
    expect(TokenType::CloseBrace, "Expected closing brace for compound statement");
    return AstNode::makeCompoundStmt(std::move(stmts));
}

std::shared_ptr<AstNode> Cpp20Parser::parseIfStmt() {
    bool isConstexpr = matchKeyword("constexpr");
    expect(TokenType::OpenParen, "Expected '(' after if");
    auto cond = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after if condition");
    auto thenBranch = parseStatement();
    std::shared_ptr<AstNode> elseBranch = nullptr;
    if (matchKeyword("else")) {
        elseBranch = parseStatement();
    }
    return AstNode::makeIfStmt(cond, thenBranch, elseBranch, isConstexpr);
}

std::shared_ptr<AstNode> Cpp20Parser::parseForStmt() {
    expect(TokenType::OpenParen, "Expected '(' after for");
    auto init = parseStatement();
    auto cond = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' in for loop");
    auto step = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after for clauses");
    auto body = parseStatement();
    return AstNode::makeForStmt(init, cond, step, body);
}

std::shared_ptr<AstNode> Cpp20Parser::parseWhileStmt() {
    expect(TokenType::OpenParen, "Expected '(' after while");
    auto cond = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after while condition");
    auto body = parseStatement();
    return AstNode::makeWhileStmt(cond, body);
}

std::shared_ptr<AstNode> Cpp20Parser::parseDoWhileStmt() {
    auto body = parseStatement();
    expectKeyword("while", "Expected 'while' after do body");
    expect(TokenType::OpenParen, "Expected '(' after while");
    auto cond = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after condition");
    expect(TokenType::Semicolon, "Expected ';' after do-while");
    auto dwNode = std::make_shared<AstNode>();
    dwNode->kind = AstNodeKind::DoWhileStmt;
    dwNode->children.push_back(cond);
    dwNode->children.push_back(body);
    return dwNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseReturnStmt() {
    std::shared_ptr<AstNode> retExpr = nullptr;
    if (peek().type != TokenType::Semicolon) {
        retExpr = parseExpression();
    }
    expect(TokenType::Semicolon, "Expected ';' after return");
    return AstNode::makeReturnStmt(retExpr);
}

std::shared_ptr<AstNode> Cpp20Parser::parseTryCatchStmt() {
    auto tryBlock = parseCompoundStmt();
    expectKeyword("catch", "Expected 'catch' after try block");
    expect(TokenType::OpenParen, "Expected '(' after catch");
    if (match(TokenType::DotStar) || (peek().type == TokenType::Dot && peek(1).type == TokenType::Dot)) {
        // catch(...)
        advance(); advance(); advance();
    } else {
        parseType();
        if (peek().type == TokenType::Identifier) advance();
    }
    expect(TokenType::CloseParen, "Expected ')' after catch parameter");
    auto catchBlock = parseCompoundStmt();
    auto tryNode = std::make_shared<AstNode>();
    tryNode->kind = AstNodeKind::CompoundStmt;
    tryNode->name = "try_catch";
    tryNode->children.push_back(tryBlock);
    tryNode->children.push_back(catchBlock);
    return tryNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseCoroutineStmt() {
    std::string kw = tokens_[cursor_ - 1].text;
    std::shared_ptr<AstNode> expr = nullptr;
    if (peek().type != TokenType::Semicolon) {
        expr = parseExpression();
    }
    expect(TokenType::Semicolon, "Expected ';' after coroutine statement");
    auto cNode = std::make_shared<AstNode>();
    cNode->kind = AstNodeKind::CoroutineExpr;
    cNode->name = kw;
    if (expr) cNode->children.push_back(expr);
    return cNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseExpression() {
    return parsePrecedenceExpr(1);
}

std::shared_ptr<AstNode> Cpp20Parser::parseAssignmentExpr() {
    return parsePrecedenceExpr(1);
}

// Precedence climbing: operator precedence and associativity are read from
// the genomic CppGrammar payload, not encoded in the parser control flow.
std::shared_ptr<AstNode> Cpp20Parser::parsePrecedenceExpr(int minimumPrecedence) {
    auto lhs = parseUnaryExpr();
    while (!isAtEnd()) {
        const auto rule = grammar_.binary(peek().text);
        if (!rule || rule->precedence < minimumPrecedence) break;
        const std::string op = advance().text;
        const int rhsPrecedence = rule->precedence + (rule->rightAssociative ? 0 : 1);
        auto rhs = parsePrecedenceExpr(rhsPrecedence);
        lhs = AstNode::makeBinaryExpr(op, lhs, rhs);
    }
    return lhs;
}

std::shared_ptr<AstNode> Cpp20Parser::parseUnaryExpr() {
    if (grammar_.prefix(peek().text)) {
        std::string op = advance().text;
        auto operand = parseUnaryExpr();
        return AstNode::makeUnaryExpr(op, operand, true);
    }
    return parsePostfixExpr();
}

std::shared_ptr<AstNode> Cpp20Parser::parsePostfixExpr() {
    auto expr = parsePrimaryExpr();

    while (!isAtEnd()) {
        if (match(TokenType::OpenParen)) {
            // Function call
            std::vector<std::shared_ptr<AstNode>> args;
            while (!match(TokenType::CloseParen) && !isAtEnd()) {
                args.push_back(parseAssignmentExpr());
                match(TokenType::Comma);
            }
            std::string calleeName = expr ? expr->name : "anonymous_fn";
            expr = AstNode::makeCallExpr(calleeName, std::move(args));
        } else if (match(TokenType::Dot) || match(TokenType::Arrow)) {
            std::string accessOp = tokens_[cursor_ - 1].text;
            auto memberTok = expect(TokenType::Identifier, "Expected member name");
            auto memNode = std::make_shared<AstNode>();
            memNode->kind = AstNodeKind::MemberAccessExpr;
            memNode->name = memberTok.text;
            memNode->op = accessOp;
            memNode->children.push_back(expr);
            expr = memNode;
        } else if (grammar_.postfix(peek().text)) {
            std::string op = advance().text;
            expr = AstNode::makeUnaryExpr(op, expr, false);
        } else {
            break;
        }
    }

    return expr;
}

std::shared_ptr<AstNode> Cpp20Parser::parsePrimaryExpr() {
    if (match(TokenType::OpenParen)) {
        auto expr = parseExpression();
        expect(TokenType::CloseParen, "Expected ')'");
        return expr;
    }

    if (peek().type == TokenType::OpenBracket) {
        return parseLambdaExpr();
    }

    if (match(TokenType::IntLiteral)) {
        try {
            int64_t val = std::stoll(tokens_[cursor_ - 1].text);
            return AstNode::makeLiteralInt(val);
        } catch (...) {
            return AstNode::makeLiteralInt(0);
        }
    }

    if (match(TokenType::FloatLiteral)) {
        try {
            double val = std::stod(tokens_[cursor_ - 1].text);
            return AstNode::makeLiteralFloat(val);
        } catch (...) {
            return AstNode::makeLiteralFloat(0.0);
        }
    }

    if (match(TokenType::BoolLiteral)) {
        bool val = (tokens_[cursor_ - 1].text == "true");
        return AstNode::makeLiteralBool(val);
    }

    if (match(TokenType::StringLiteral)) {
        return AstNode::makeLiteralString(tokens_[cursor_ - 1].text);
    }

    if (match(TokenType::Identifier)) {
        const auto& ident = tokens_[cursor_ - 1].text;
        if (grammar_.strict() && (ident.empty() ||
            !(std::isalpha(static_cast<unsigned char>(ident.front())) || ident.front() == '_')))
            throw std::runtime_error("Invalid genomic identifier: " + ident);
        return AstNode::makeIdentifier(ident);
    }

    // Cast expressions (static_cast<T>(expr), etc.)
    if (matchKeyword("static_cast") || matchKeyword("dynamic_cast") ||
        matchKeyword("reinterpret_cast") || matchKeyword("const_cast")) {
        std::string castKind = tokens_[cursor_ - 1].text;
        expect(TokenType::Less, "Expected '<' in cast");
        auto targetType = parseType();
        expect(TokenType::Greater, "Expected '>' in cast");
        expect(TokenType::OpenParen, "Expected '(' in cast");
        auto innerExpr = parseExpression();
        expect(TokenType::CloseParen, "Expected ')' in cast");
        auto castNode = std::make_shared<AstNode>();
        castNode->kind = AstNodeKind::CastExpr;
        castNode->name = castKind;
        castNode->evaluatedType = targetType;
        castNode->children.push_back(innerExpr);
        return castNode;
    }

    if (grammar_.strict())
        throw std::runtime_error("Unsupported primary expression at line " +
            std::to_string(peek().line) + ": " + peek().text);
    // Compatibility with the old standalone parser.
    return AstNode::makeLiteralInt(0);
}

std::shared_ptr<AstNode> Cpp20Parser::parseLambdaExpr() {
    expect(TokenType::OpenBracket, "Expected '[' for lambda capture");
    std::string captures;
    while (!match(TokenType::CloseBracket) && !isAtEnd()) {
        captures += advance().text + " ";
    }

    // Optional parameter list
    std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params;
    if (match(TokenType::OpenParen)) {
        while (!match(TokenType::CloseParen) && !isAtEnd()) {
            auto pType = parseType();
            std::string pName = "arg";
            if (peek().type == TokenType::Identifier) {
                pName = advance().text;
            }
            params.push_back({pName, pType});
            match(TokenType::Comma);
        }
    }

    matchKeyword("mutable");
    matchKeyword("constexpr");

    auto body = parseCompoundStmt();
    auto lambdaNode = std::make_shared<AstNode>();
    lambdaNode->kind = AstNodeKind::LambdaExpr;
    lambdaNode->name = captures;
    lambdaNode->children.push_back(body);
    return lambdaNode;
}

} // namespace genomic
