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
    // Skip C++20 attributes [[nodiscard]], [[maybe_unused]], etc.
    while (peek().type == TokenType::OpenBracket && peek(1).type == TokenType::OpenBracket) {
        advance(); advance();
        while (!isAtEnd() && !(peek().type == TokenType::CloseBracket && peek(1).type == TokenType::CloseBracket)) {
            advance();
        }
        if (peek().type == TokenType::CloseBracket && peek(1).type == TokenType::CloseBracket) {
            advance(); advance();
        }
    }

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
    if (matchKeyword("static_assert")) {
        return parseStaticAssertDecl();
    }
    if (matchKeyword("using")) {
        return parseUsingDecl();
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
    std::string nsName;
    if (peek().type == TokenType::Identifier) {
        nsName = advance().text;
        while (match(TokenType::ColonColon)) {
            nsName += "::" + expect(TokenType::Identifier, "Expected nested namespace name").text;
        }
    }
    expect(TokenType::OpenBrace, "Expected '{' after namespace name");
    auto nsNode = std::make_shared<AstNode>();
    nsNode->kind = AstNodeKind::NamespaceDecl;
    nsNode->name = nsName;

    while (peek().type != TokenType::CloseBrace && !isAtEnd()) {
        if (peek().type == TokenType::Semicolon) {
            advance();
            continue;
        }
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

    // Optional requires clause: requires Concept<T> && ...
    std::string constraintExpr;
    if (matchKeyword("requires")) {
        int parenDepth = 0;
        int braceDepth = 0;
        while (!isAtEnd()) {
            if (peek().type == TokenType::OpenParen) parenDepth++;
            else if (peek().type == TokenType::CloseParen && parenDepth > 0) parenDepth--;
            else if (peek().type == TokenType::OpenBrace) {
                if (parenDepth == 0 && constraintExpr.find("requires") == std::string::npos) break;
                braceDepth++;
            } else if (peek().type == TokenType::CloseBrace && braceDepth > 0) {
                braceDepth--;
            } else if (parenDepth == 0 && braceDepth == 0) {
                if (peek().type == TokenType::Semicolon) break;
                if (peek().type == TokenType::Keyword &&
                    peek().text != "requires" && peek().text != "typename" &&
                    peek().text != "true" && peek().text != "false" &&
                    peek().text != "and" && peek().text != "or" && peek().text != "not") {
                    break;
                }
                // Check if this identifier is the return type of a declaration followed by another identifier
                if (peek().type == TokenType::Identifier &&
                    (peek(1).type == TokenType::Identifier ||
                     (peek(1).type == TokenType::Star && peek(2).type == TokenType::Identifier) ||
                     (peek(1).type == TokenType::Amp && peek(2).type == TokenType::Identifier))) {
                    break;
                }
            }
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

    if (matchKeyword("requires")) {
        auto reqExpr = parseRequiresExpr();
        cNode->strVal = reqExpr->strVal;
        cNode->children.push_back(reqExpr);
    } else {
        // Parse constraint expression up to ';'
        std::string rawConstraint;
        size_t startCursor = cursor_;
        while (!isAtEnd() && peek().type != TokenType::Semicolon) {
            rawConstraint += advance().text + " ";
        }
        cNode->strVal = rawConstraint;
        // Also attempt expression parse if simple
        cursor_ = startCursor;
        try {
            auto expr = parseExpression();
            if (peek().type == TokenType::Semicolon) {
                cNode->children.push_back(expr);
            } else {
                while (!isAtEnd() && peek().type != TokenType::Semicolon) advance();
            }
        } catch (...) {
            while (!isAtEnd() && peek().type != TokenType::Semicolon) advance();
        }
    }
    match(TokenType::Semicolon);
    return cNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseRequiresExpr() {
    auto reqNode = std::make_shared<AstNode>();
    reqNode->kind = AstNodeKind::RequiresExpr;

    std::string reqArgs;
    if (match(TokenType::OpenParen)) {
        while (!match(TokenType::CloseParen) && !isAtEnd()) {
            reqArgs += advance().text + " ";
        }
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
    reqNode->name = reqArgs;
    reqNode->strVal = "requires(" + reqArgs + ") { " + reqBody + " }";
    reqNode->evaluatedType = CppType::makeBool();
    return reqNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseStaticAssertDecl() {
    expect(TokenType::OpenParen, "Expected '(' after static_assert");
    auto condExpr = parseExpression();
    std::string msg;
    if (match(TokenType::Comma)) {
        if (peek().type == TokenType::StringLiteral) {
            msg = advance().text;
        }
    }
    expect(TokenType::CloseParen, "Expected ')' after static_assert");
    expect(TokenType::Semicolon, "Expected ';' after static_assert");
    auto saNode = std::make_shared<AstNode>();
    saNode->kind = AstNodeKind::StaticAssertDecl;
    saNode->strVal = msg;
    saNode->children.push_back(condExpr);
    return saNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseUsingDecl() {
    auto uNode = std::make_shared<AstNode>();
    uNode->kind = AstNodeKind::UsingDecl;
    if (matchKeyword("namespace")) {
        uNode->name = expect(TokenType::Identifier, "Expected namespace name after 'using namespace'").text;
        while (match(TokenType::ColonColon)) {
            uNode->name += "::" + expect(TokenType::Identifier, "Expected identifier").text;
        }
        expect(TokenType::Semicolon, "Expected ';' after using namespace");
        return uNode;
    }
    if (peek().type == TokenType::Identifier) {
        uNode->name = advance().text;
        if (match(TokenType::Equal)) {
            uNode->evaluatedType = parseType();
        }
    }
    while (!isAtEnd() && peek().type != TokenType::Semicolon) advance();
    expect(TokenType::Semicolon, "Expected ';' after using declaration");
    return uNode;
}

std::shared_ptr<AstNode> Cpp20Parser::parseClassDecl() {
    auto nameTok = expect(TokenType::Identifier, "Expected class/struct name");
    auto classNode = std::make_shared<AstNode>();
    classNode->kind = AstNodeKind::RecordDecl;
    classNode->name = nameTok.text;

    // Optional template specialization arguments on class declaration: struct Box<T*>
    if (match(TokenType::Less)) {
        int depth = 1;
        std::string specArgs = "<";
        while (!isAtEnd() && depth > 0) {
            if (peek().type == TokenType::Less) depth++;
            else if (peek().type == TokenType::Greater) depth--;
            specArgs += advance().text;
        }
        classNode->op = specArgs;
    }

    // Base classes (: public Base)
    if (match(TokenType::Colon)) {
        while (!isAtEnd() && peek().type != TokenType::OpenBrace) {
            classNode->strVal += advance().text + " ";
        }
    }

    if (match(TokenType::OpenBrace)) {
        while (!match(TokenType::CloseBrace) && !isAtEnd()) {
            if (matchKeyword("public") || matchKeyword("private") || matchKeyword("protected")) {
                expect(TokenType::Colon, "Expected ':' after access specifier");
                continue;
            }
            if (peek().type == TokenType::Semicolon) {
                advance();
                continue;
            }
            // Handle constructors/destructors inside class
            if ((peek().type == TokenType::Tilde && peek(1).type == TokenType::Identifier) ||
                (peek().type == TokenType::Identifier && peek().text == classNode->name && peek(1).type == TokenType::OpenParen)) {
                std::string ctorName;
                if (match(TokenType::Tilde)) ctorName = "~";
                ctorName += advance().text;
                expect(TokenType::OpenParen, "Expected '(' in constructor/destructor");
                std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params;
                while (!match(TokenType::CloseParen) && !isAtEnd()) {
                    auto pType = parseType();
                    std::string pName = (peek().type == TokenType::Identifier) ? advance().text : "";
                    params.push_back({pName, pType});
                    match(TokenType::Comma);
                }
                matchKeyword("noexcept");
                // Member initializer list `: a(1), b(2)`
                if (match(TokenType::Colon)) {
                    while (!isAtEnd() && peek().type != TokenType::OpenBrace && peek().type != TokenType::Semicolon) {
                        advance();
                    }
                }
                std::shared_ptr<AstNode> body = nullptr;
                if (peek().type == TokenType::OpenBrace) {
                    body = parseCompoundStmt();
                } else if (match(TokenType::Equal)) {
                    // = default; or = delete;
                    advance();
                    match(TokenType::Semicolon);
                } else {
                    match(TokenType::Semicolon);
                }
                classNode->children.push_back(AstNode::makeFunctionDecl(ctorName, CppType::makeVoid(), params, body));
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
    bool isScoped = matchKeyword("class") || matchKeyword("struct");
    auto nameTok = expect(TokenType::Identifier, "Expected enum name");
    auto enumNode = std::make_shared<AstNode>();
    enumNode->kind = AstNodeKind::RecordDecl;
    enumNode->name = "enum_" + nameTok.text;
    enumNode->boolVal = isScoped;

    std::shared_ptr<CppType> underlying = CppType::makeInt64();
    if (match(TokenType::Colon)) {
        underlying = parseType();
    }
    enumNode->evaluatedType = CppType::makeEnum(nameTok.text, underlying);

    if (match(TokenType::OpenBrace)) {
        int64_t nextVal = 0;
        while (!match(TokenType::CloseBrace) && !isAtEnd()) {
            if (peek().type == TokenType::Identifier) {
                std::string enumeratorName = advance().text;
                if (match(TokenType::Equal)) {
                    auto valExpr = parseAssignmentExpr();
                    if (valExpr && valExpr->kind == AstNodeKind::LiteralExpr) {
                        nextVal = valExpr->intVal;
                    }
                }
                auto eValNode = AstNode::makeVariableDecl(
                    enumeratorName, enumNode->evaluatedType, AstNode::makeLiteralInt(nextVal), true);
                eValNode->intVal = nextVal;
                enumNode->children.push_back(eValNode);
                nextVal++;
                match(TokenType::Comma);
            } else {
                advance();
            }
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

    std::string declName;
    if (matchKeyword("operator")) {
        declName = "operator";
        if (match(TokenType::Spaceship)) declName += "<=>";
        else if (match(TokenType::EqualEqual)) declName += "==";
        else if (match(TokenType::ExclaimEqual)) declName += "!=";
        else if (match(TokenType::Less)) declName += "<";
        else if (match(TokenType::LessEqual)) declName += "<=";
        else if (match(TokenType::Greater)) declName += ">";
        else if (match(TokenType::GreaterEqual)) declName += ">=";
        else if (match(TokenType::Plus)) declName += "+";
        else if (match(TokenType::Minus)) declName += "-";
        else if (match(TokenType::Star)) declName += "*";
        else if (match(TokenType::Slash)) declName += "/";
        else if (match(TokenType::OpenParen) && match(TokenType::CloseParen)) declName += "()";
        else if (match(TokenType::OpenBracket) && match(TokenType::CloseBracket)) declName += "[]";
        else {
            auto convType = parseType();
            if (convType) declName += " " + convType->toString();
        }
    } else {
        auto nameTok = expect(TokenType::Identifier, "Expected identifier after type");
        declName = nameTok.text;
    }

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
            // Optional default parameter value `= expr`
            if (match(TokenType::Equal)) {
                parseAssignmentExpr();
            }
            params.push_back({pName, pType});
            match(TokenType::Comma);
        }

        // Optional cv-qualifiers on member functions
        matchKeyword("const");
        matchKeyword("volatile");
        match(TokenType::Amp);
        match(TokenType::AmpAmp);

        // Optional trailing return type or attributes
        if (match(TokenType::Arrow)) {
            type = parseType();
        }
        if (matchKeyword("noexcept")) {
            if (match(TokenType::OpenParen)) {
                while (!match(TokenType::CloseParen) && !isAtEnd()) advance();
            }
        }
        // Optional override / final contextual identifiers
        while (peek().type == TokenType::Identifier &&
               (peek().text == "override" || peek().text == "final")) {
            advance();
        }

        // Optional trailing requires clause on function
        std::string trailingRequires;
        if (matchKeyword("requires")) {
            while (!isAtEnd() && peek().type != TokenType::OpenBrace && peek().type != TokenType::Semicolon) {
                trailingRequires += advance().text + " ";
            }
        }

        std::shared_ptr<AstNode> body = nullptr;
        if (peek().type == TokenType::OpenBrace) {
            body = parseCompoundStmt();
        } else if (match(TokenType::Equal)) {
            // = 0, = default, = delete
            advance();
            expect(TokenType::Semicolon, "Expected ';' after '= default/delete/0'");
        } else {
            expect(TokenType::Semicolon, "Expected ';' or '{' after function prototype");
        }

        requireDeclaration("function_decl");
        auto fnNode = AstNode::makeFunctionDecl(declName, type, params, body, isConstexpr);
        fnNode->strVal = trailingRequires;
        return fnNode;
    }

    // Array variable declaration: int arr[10]
    if (match(TokenType::OpenBracket)) {
        size_t arrLen = 0;
        if (peek().type == TokenType::IntLiteral) {
            arrLen = static_cast<size_t>(std::stoull(advance().text));
        }
        expect(TokenType::CloseBracket, "Expected ']' in array declaration");
        type = CppType::makeArray(type, arrLen);
    }

    // Variable declaration: name = init; or name{init};
    std::shared_ptr<AstNode> initializer = nullptr;
    if (match(TokenType::Equal)) {
        initializer = parseExpression();
    } else if (match(TokenType::OpenBrace)) {
        if (peek().type != TokenType::CloseBrace) {
            initializer = parseExpression();
            while (match(TokenType::Comma)) {
                parseExpression();
            }
        }
        expect(TokenType::CloseBrace, "Expected '}' in brace initializer");
    }
    expect(TokenType::Semicolon, "Expected ';' after variable declaration");
    requireDeclaration("variable_decl");
    return AstNode::makeVariableDecl(declName, type, initializer, isConstexpr);
}

std::shared_ptr<CppType> Cpp20Parser::parseType() {
    bool isConst = false;
    bool isVolatile = false;
    while (true) {
        if (matchKeyword("const")) isConst = true;
        else if (matchKeyword("volatile")) isVolatile = true;
        else if (matchKeyword("typename")) {} // Dependent typename disambiguator
        else break;
    }

    bool hasUnsigned = false;
    bool hasSigned = false;
    int longCount = 0;
    bool hasShort = false;
    bool hasInt = false;
    bool hasChar = false;
    bool hasDouble = false;

    while (true) {
        if (matchKeyword("unsigned")) hasUnsigned = true;
        else if (matchKeyword("signed")) hasSigned = true;
        else if (matchKeyword("long")) longCount++;
        else if (matchKeyword("short")) hasShort = true;
        else if (matchKeyword("int")) hasInt = true;
        else if (matchKeyword("char")) hasChar = true;
        else if (matchKeyword("double")) hasDouble = true;
        else break;
    }

    TypeKind kind = TypeKind::Int64;
    std::string typeName;
    std::vector<std::shared_ptr<CppType>> templateArgs;
    bool matchedBase = false;

    if (hasUnsigned || hasSigned || longCount > 0 || hasShort || hasInt || hasChar || hasDouble) {
        matchedBase = true;
        if (hasDouble) {
            kind = TypeKind::Float64;
        } else if (hasChar) {
            kind = hasUnsigned ? TypeKind::UInt8 : (hasSigned ? TypeKind::Int8 : TypeKind::Char);
        } else if (hasShort) {
            kind = hasUnsigned ? TypeKind::UInt16 : TypeKind::Int16;
        } else if (longCount >= 1) {
            kind = hasUnsigned ? TypeKind::UInt64 : TypeKind::Int64;
        } else {
            kind = hasUnsigned ? TypeKind::UInt32 : TypeKind::Int32;
        }
    } else if (matchKeyword("void")) {
        kind = TypeKind::Void; matchedBase = true;
    } else if (matchKeyword("bool")) {
        kind = TypeKind::Bool; matchedBase = true;
    } else if (matchKeyword("char8_t")) {
        kind = TypeKind::Char8; matchedBase = true;
    } else if (matchKeyword("char16_t")) {
        kind = TypeKind::Char16; matchedBase = true;
    } else if (matchKeyword("char32_t")) {
        kind = TypeKind::Char32; matchedBase = true;
    } else if (matchKeyword("wchar_t")) {
        kind = TypeKind::WChar; matchedBase = true;
    } else if (matchKeyword("float")) {
        kind = TypeKind::Float32; matchedBase = true;
    } else if (matchKeyword("auto")) {
        kind = TypeKind::Auto; matchedBase = true;
    } else if (matchKeyword("decltype")) {
        expect(TokenType::OpenParen, "Expected '(' after decltype");
        if (matchKeyword("auto")) {
            expect(TokenType::CloseParen, "Expected ')' after decltype(auto)");
            kind = TypeKind::Auto;
        } else {
            int depth = 1;
            while (!isAtEnd() && depth > 0) {
                if (peek().type == TokenType::OpenParen) depth++;
                else if (peek().type == TokenType::CloseParen) depth--;
                advance();
            }
            kind = TypeKind::Auto;
        }
        matchedBase = true;
    } else if (peek().type == TokenType::Identifier || peek().type == TokenType::ColonColon) {
        if (match(TokenType::ColonColon)) {
            typeName = "::";
        }
        if (peek().type != TokenType::Identifier) return nullptr;
        typeName += advance().text;
        while (match(TokenType::ColonColon)) {
            if (peek().type == TokenType::Identifier) {
                typeName += "::" + advance().text;
            } else {
                break;
            }
        }
        matchedBase = true;

        // Strip leading std:: for standard fixed-width types
        std::string bare = typeName;
        if (bare.rfind("std::", 0) == 0) bare = bare.substr(5);
        if (bare == "int8_t") { kind = TypeKind::Int8; typeName.clear(); }
        else if (bare == "int16_t") { kind = TypeKind::Int16; typeName.clear(); }
        else if (bare == "int32_t") { kind = TypeKind::Int32; typeName.clear(); }
        else if (bare == "int64_t" || bare == "ptrdiff_t") { kind = TypeKind::Int64; typeName.clear(); }
        else if (bare == "uint8_t") { kind = TypeKind::UInt8; typeName.clear(); }
        else if (bare == "uint16_t") { kind = TypeKind::UInt16; typeName.clear(); }
        else if (bare == "uint32_t") { kind = TypeKind::UInt32; typeName.clear(); }
        else if (bare == "uint64_t" || bare == "size_t") { kind = TypeKind::UInt64; typeName.clear(); }
        else if (bare == "nullptr_t") { kind = TypeKind::Nullptr; typeName.clear(); }
        else {
            kind = TypeKind::Record;
            // Parse template arguments <Arg1, Arg2>
            if (match(TokenType::Less)) {
                while (!match(TokenType::Greater) && !isAtEnd()) {
                    if (peek().type == TokenType::IntLiteral) {
                        auto ntType = CppType::makeInt64();
                        ntType->arraySize = static_cast<size_t>(std::stoull(advance().text));
                        templateArgs.push_back(ntType);
                    } else {
                        auto argT = parseType();
                        if (argT) templateArgs.push_back(argT);
                        else advance();
                    }
                    match(TokenType::Comma);
                }
                // Handle nested "::type" after template specialization e.g. std::type_identity<T>::type
                if (match(TokenType::ColonColon) && peek().type == TokenType::Identifier) {
                    std::string memberType = advance().text;
                    typeName += "::" + memberType;
                }
            }
        }
    }

    if (!matchedBase) return nullptr;

    // Trailing cv-qualifiers (e.g. `int const`)
    while (true) {
        if (matchKeyword("const")) isConst = true;
        else if (matchKeyword("volatile")) isVolatile = true;
        else break;
    }

    auto type = std::make_shared<CppType>(kind, isConst, isVolatile, typeName);
    type->templateArgs = std::move(templateArgs);
    if (typeName.find("type_identity") != std::string::npos) {
        type->isNonDeduced = true;
    }

    // Pointer or Reference modifiers (*, &, &&) and pack expansion (...)
    while (!isAtEnd()) {
        if (match(TokenType::Star)) {
            bool ptrConst = matchKeyword("const");
            bool ptrVol = matchKeyword("volatile");
            type = CppType::makePointer(type, ptrConst);
            type->isVolatile = ptrVol;
        } else if (match(TokenType::AmpAmp)) {
            type = CppType::makeRValueRef(type);
        } else if (match(TokenType::Amp)) {
            type = CppType::makeLValueRef(type);
        } else if (peek().type == TokenType::Dot && peek(1).type == TokenType::Dot && peek(2).type == TokenType::Dot) {
            advance(); advance(); advance();
            type->isPackExpansion = true;
        } else {
            break;
        }
    }

    return type;
}

std::shared_ptr<AstNode> Cpp20Parser::parseStatement() {
    // Skip attributes like [[fallthrough]]; or [[likely]]
    if (peek().type == TokenType::OpenBracket && peek(1).type == TokenType::OpenBracket) {
        advance(); advance();
        std::string attrName;
        while (!isAtEnd() && !(peek().type == TokenType::CloseBracket && peek(1).type == TokenType::CloseBracket)) {
            attrName += advance().text;
        }
        if (peek().type == TokenType::CloseBracket && peek(1).type == TokenType::CloseBracket) {
            advance(); advance();
        }
        if (match(TokenType::Semicolon)) {
            auto attrNode = std::make_shared<AstNode>();
            attrNode->kind = AstNodeKind::ExprStmt;
            attrNode->name = "[[" + attrName + "]]";
            return attrNode;
        }
    }

    if (peek().type == TokenType::OpenBrace) {
        requireStatement("compound_stmt");
        return parseCompoundStmt();
    }
    if (matchKeyword("if")) {
        requireStatement("if_stmt");
        return parseIfStmt();
    }
    if (matchKeyword("switch")) {
        requireStatement("switch_stmt");
        return parseSwitchStmt();
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
        return AstNode::makeBreakStmt();
    }
    if (matchKeyword("continue")) {
        requireStatement("continue_stmt");
        expect(TokenType::Semicolon, "Expected ';' after continue");
        return AstNode::makeContinueStmt();
    }
    if (matchKeyword("static_assert")) {
        return parseStaticAssertDecl();
    }

    // Try parsing variable declaration statement, else expression statement
    size_t saved = cursor_;
    try {
        auto decl = parseFunctionOrVariableDecl();
        if (decl && decl->kind == AstNodeKind::VariableDecl) return decl;
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
        if (peek().type == TokenType::Semicolon) {
            advance();
            continue;
        }
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

std::shared_ptr<AstNode> Cpp20Parser::parseSwitchStmt() {
    expect(TokenType::OpenParen, "Expected '(' after switch");

    // Check for C++17/C++20 init-statement inside switch(init; cond)
    std::shared_ptr<AstNode> initStmt = nullptr;
    size_t saved = cursor_;
    try {
        auto maybeDecl = parseFunctionOrVariableDecl();
        if (maybeDecl && maybeDecl->kind == AstNodeKind::VariableDecl && peek().type != TokenType::CloseParen) {
            initStmt = maybeDecl;
        } else {
            cursor_ = saved;
        }
    } catch (...) {
        cursor_ = saved;
    }

    auto cond = parseExpression();
    if (initStmt == nullptr && match(TokenType::Semicolon)) {
        // Expression init-statement: switch (expr; cond)
        initStmt = cond;
        cond = parseExpression();
    }
    expect(TokenType::CloseParen, "Expected ')' after switch condition");
    expect(TokenType::OpenBrace, "Expected '{' for switch body");

    std::vector<std::shared_ptr<AstNode>> cases;
    while (peek().type != TokenType::CloseBrace && !isAtEnd()) {
        if (matchKeyword("case")) {
            requireStatement("case_stmt");
            auto caseExpr = parseExpression();
            expect(TokenType::Colon, "Expected ':' after case expression");
            std::vector<std::shared_ptr<AstNode>> caseStmts;
            bool fallthroughAttr = false;
            while (peek().type != TokenType::CloseBrace && !isAtEnd() &&
                   !peek().isKeyword("case") && !peek().isKeyword("default")) {
                if (peek().type == TokenType::Semicolon) {
                    advance();
                    continue;
                }
                auto st = parseStatement();
                if (st) {
                    if (st->kind == AstNodeKind::ExprStmt && st->name == "[[fallthrough]]") {
                        fallthroughAttr = true;
                    } else {
                        caseStmts.push_back(st);
                    }
                }
            }
            cases.push_back(AstNode::makeCaseStmt(caseExpr, std::move(caseStmts), fallthroughAttr));
        } else if (matchKeyword("default")) {
            requireStatement("default_stmt");
            expect(TokenType::Colon, "Expected ':' after default");
            std::vector<std::shared_ptr<AstNode>> defStmts;
            while (peek().type != TokenType::CloseBrace && !isAtEnd() &&
                   !peek().isKeyword("case") && !peek().isKeyword("default")) {
                if (peek().type == TokenType::Semicolon) {
                    advance();
                    continue;
                }
                auto st = parseStatement();
                if (st && !(st->kind == AstNodeKind::ExprStmt && st->name == "[[fallthrough]]")) {
                    defStmts.push_back(st);
                }
            }
            cases.push_back(AstNode::makeDefaultStmt(std::move(defStmts)));
        } else {
            if (grammar_.strict()) {
                throw std::runtime_error("Expected 'case' or 'default' inside switch body, got '" + peek().text + "'");
            }
            advance();
        }
    }

    expect(TokenType::CloseBrace, "Expected '}' at end of switch statement");
    return AstNode::makeSwitchStmt(cond, std::move(cases), initStmt);
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
        // Conditional ternary operator `cond ? trueExpr : falseExpr` at precedence 2
        if (minimumPrecedence <= 2 && peek().type == TokenType::Question) {
            advance(); // '?'
            auto trueExpr = parseExpression();
            expect(TokenType::Colon, "Expected ':' in conditional expression");
            auto falseExpr = parsePrecedenceExpr(2);
            lhs = AstNode::makeTernaryExpr(lhs, trueExpr, falseExpr);
            continue;
        }
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
    if (matchKeyword("sizeof") || matchKeyword("alignof")) {
        std::string kw = tokens_[cursor_ - 1].text;
        expect(TokenType::OpenParen, "Expected '(' after " + kw);
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (peek().type == TokenType::OpenParen) depth++;
            else if (peek().type == TokenType::CloseParen) depth--;
            advance();
        }
        return AstNode::makeLiteralInt(8);
    }
    if (matchKeyword("co_await")) {
        auto operand = parseUnaryExpr();
        return AstNode::makeCoroutineSuspend(operand);
    }
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
            auto callNode = AstNode::makeCallExpr(calleeName, std::move(args));
            if (expr && !expr->strVal.empty()) {
                callNode->strVal = expr->strVal; // Explicit template arguments e.g. "<int>"
            }
            expr = callNode;
        } else if (match(TokenType::OpenBracket)) {
            auto idxExpr = parseExpression();
            expect(TokenType::CloseBracket, "Expected ']' after subscript index");
            auto subNode = std::make_shared<AstNode>();
            subNode->kind = AstNodeKind::SubscriptExpr;
            subNode->children.push_back(expr);
            subNode->children.push_back(idxExpr);
            expr = subNode;
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

    if (matchKeyword("requires")) {
        return parseRequiresExpr();
    }

    if (match(TokenType::IntLiteral)) {
        const std::string& raw = tokens_[cursor_ - 1].text;
        try {
            size_t idx = 0;
            int base = 10;
            std::string clean = raw;
            if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'b' || clean[1] == 'B')) {
                clean = clean.substr(2);
                base = 2;
            } else if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
                base = 16;
            }
            int64_t val = std::stoll(clean, &idx, base);
            auto lit = AstNode::makeLiteralInt(val);
            // Respect integer literal suffixes (u, U, l, L, ll, LL)
            bool isUnsigned = (raw.find('u') != std::string::npos || raw.find('U') != std::string::npos);
            bool isLongLong = (raw.find("ll") != std::string::npos || raw.find("LL") != std::string::npos ||
                               raw.find('l') != std::string::npos || raw.find('L') != std::string::npos);
            if (isUnsigned) {
                lit->evaluatedType = isLongLong ? CppType::makeUInt64() : CppType::makeUInt32();
            } else if (!isLongLong && val >= -2147483648LL && val <= 2147483647LL) {
                lit->evaluatedType = CppType::makeInt64();
            }
            return lit;
        } catch (...) {
            return AstNode::makeLiteralInt(0);
        }
    }

    if (match(TokenType::FloatLiteral)) {
        const std::string& raw = tokens_[cursor_ - 1].text;
        try {
            double val = std::stod(raw);
            auto lit = AstNode::makeLiteralFloat(val);
            if (!raw.empty() && (raw.back() == 'f' || raw.back() == 'F')) {
                lit->evaluatedType = CppType::makeFloat32();
            }
            return lit;
        } catch (...) {
            return AstNode::makeLiteralFloat(0.0);
        }
    }

    if (match(TokenType::CharLiteral)) {
        const std::string& raw = tokens_[cursor_ - 1].text;
        char ch = raw.empty() ? '\0' : raw[0];
        if (raw.size() >= 2 && raw[0] == '\\') {
            if (raw[1] == 'n') ch = '\n';
            else if (raw[1] == 't') ch = '\t';
            else if (raw[1] == '0') ch = '\0';
            else ch = raw[1];
        }
        auto lit = AstNode::makeLiteralInt(static_cast<int64_t>(ch));
        lit->evaluatedType = CppType::makeChar();
        return lit;
    }

    if (match(TokenType::BoolLiteral)) {
        bool val = (tokens_[cursor_ - 1].text == "true");
        return AstNode::makeLiteralBool(val);
    }

    if (match(TokenType::NullptrLiteral)) {
        auto lit = AstNode::makeLiteralInt(0);
        lit->evaluatedType = CppType::makeNullptr();
        lit->strVal = "nullptr";
        return lit;
    }

    if (match(TokenType::StringLiteral)) {
        return AstNode::makeLiteralString(tokens_[cursor_ - 1].text);
    }

    if (matchKeyword("this")) {
        return AstNode::makeIdentifier("this");
    }

    if (match(TokenType::Identifier)) {
        std::string ident = tokens_[cursor_ - 1].text;
        if (grammar_.strict() && (ident.empty() ||
            !(std::isalpha(static_cast<unsigned char>(ident.front())) || ident.front() == '_')))
            throw std::runtime_error("Invalid genomic identifier: " + ident);

        // Qualified identifier: Foo::Bar::baz
        while (match(TokenType::ColonColon)) {
            if (peek().type == TokenType::Identifier) {
                ident += "::" + advance().text;
            } else {
                break;
            }
        }

        // Check for explicit template arguments on function call: fn<int, double>(...)
        std::string explicitTemplateArgs;
        if (peek().type == TokenType::Less) {
            size_t saved = cursor_;
            try {
                advance(); // '<'
                int depth = 1;
                std::string tArgs;
                while (!isAtEnd() && depth > 0) {
                    if (peek().type == TokenType::Less) depth++;
                    else if (peek().type == TokenType::Greater) {
                        depth--;
                        if (depth == 0) {
                            advance();
                            break;
                        }
                    }
                    tArgs += advance().text + " ";
                }
                if (depth == 0 && (peek().type == TokenType::OpenParen || peek().type == TokenType::OpenBrace)) {
                    explicitTemplateArgs = tArgs;
                } else {
                    cursor_ = saved;
                }
            } catch (...) {
                cursor_ = saved;
            }
        }

        // Check for aggregate / brace initialization: Type{a, b}
        if (match(TokenType::OpenBrace)) {
            std::vector<std::shared_ptr<AstNode>> args;
            while (!match(TokenType::CloseBrace) && !isAtEnd()) {
                args.push_back(parseAssignmentExpr());
                match(TokenType::Comma);
            }
            auto callNode = AstNode::makeCallExpr(ident, std::move(args));
            callNode->strVal = explicitTemplateArgs;
            callNode->op = "{}";
            return callNode;
        }

        auto idNode = AstNode::makeIdentifier(ident);
        idNode->strVal = explicitTemplateArgs;
        return idNode;
    }

    // Cast expressions (static_cast<T>(expr), std::move, etc.)
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
        if (targetType && targetType->isRValueReference()) {
            castNode->valueCategory = ValueCategory::XValue;
        } else if (targetType && targetType->isLValueReference()) {
            castNode->valueCategory = ValueCategory::LValue;
        }
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

    // Optional template parameter list on generic C++20 lambda: []<typename T>(T x)
    if (match(TokenType::Less)) {
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (peek().type == TokenType::Less) depth++;
            else if (peek().type == TokenType::Greater) depth--;
            advance();
        }
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
    matchKeyword("consteval");
    matchKeyword("noexcept");

    std::shared_ptr<CppType> returnType = nullptr;
    if (match(TokenType::Arrow)) {
        returnType = parseType();
    }

    auto body = parseCompoundStmt();
    auto lambdaNode = std::make_shared<AstNode>();
    lambdaNode->kind = AstNodeKind::LambdaExpr;
    lambdaNode->name = captures;
    lambdaNode->evaluatedType = returnType ? returnType : CppType::makeAuto();
    lambdaNode->children.push_back(body);
    return lambdaNode;
}

} // namespace genomic
