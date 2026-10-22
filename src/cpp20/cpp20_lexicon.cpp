#include "cpp20/cpp20_lexicon.hpp"
#include "cpp20/genomic_expression_grammar.hpp"
#include <cctype>

namespace genomic {

const std::unordered_set<std::string>& Cpp20Lexicon::getAllKeywords() {
    static const std::unordered_set<std::string> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "import", "inline", "int", "long", "module", "mutable", "namespace",
        "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
        "private", "protected", "public", "register", "reinterpret_cast", "requires",
        "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
        "struct", "switch", "template", "this", "thread_local", "throw", "true",
        "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    };
    return keywords;
}

bool Cpp20Lexicon::isKeyword(const std::string& str) {
    const auto& kw = getAllKeywords();
    return kw.find(str) != kw.end();
}

std::vector<Token> Cpp20Lexicon::tokenize(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;
    uint32_t line = 1;
    uint32_t col = 1;

    auto advance = [&](size_t count = 1) {
        for (size_t c = 0; c < count; ++c) {
            if (i < source.size()) {
                if (source[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
                i++;
            }
        }
    };

    while (i < source.size()) {
        char c = source[i];

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        // Single-line comment
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            advance(2);
            while (i < source.size() && source[i] != '\n') {
                advance();
            }
            continue;
        }

        // Multi-line comment
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            advance(2);
            while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) {
                advance();
            }
            if (i + 1 < source.size()) advance(2);
            continue;
        }

        // Preprocessor directives starting with #
        if (c == '#' && (col == 1 || (tokens.empty() || tokens.back().line < line))) {
            uint32_t tLine = line;
            uint32_t tCol = col;
            advance();
            while (i < source.size() && (source[i] == ' ' || source[i] == '\t')) advance();
            std::string dirName;
            while (i < source.size() && std::isalpha(static_cast<unsigned char>(source[i]))) {
                dirName += source[i];
                advance();
            }
            TokenType pt = TokenType::Hash;
            if (dirName == "define") pt = TokenType::PpDefine;
            else if (dirName == "undef") pt = TokenType::PpUndef;
            else if (dirName == "include") pt = TokenType::PpInclude;
            else if (dirName == "if") pt = TokenType::PpIf;
            else if (dirName == "ifdef") pt = TokenType::PpIfdef;
            else if (dirName == "ifndef") pt = TokenType::PpIfndef;
            else if (dirName == "elif") pt = TokenType::PpElif;
            else if (dirName == "else") pt = TokenType::PpElse;
            else if (dirName == "endif") pt = TokenType::PpEndif;
            else if (dirName == "pragma") pt = TokenType::PpPragma;
            else if (dirName == "error") pt = TokenType::PpError;

            tokens.push_back(Token{pt, "#" + dirName, tLine, tCol});
            continue;
        }

        // String Literal (including raw string literals R"(...)")
        if (c == 'R' && i + 2 < source.size() && source[i + 1] == '"' && source[i + 2] == '(') {
            uint32_t tLine = line;
            uint32_t tCol = col;
            advance(3);
            std::string content;
            while (i + 1 < source.size() && !(source[i] == ')' && source[i + 1] == '"')) {
                content += source[i];
                advance();
            }
            if (i + 1 < source.size()) advance(2);
            tokens.push_back(Token{TokenType::StringLiteral, content, tLine, tCol});
            continue;
        }

        if (c == '"') {
            uint32_t tLine = line;
            uint32_t tCol = col;
            advance();
            std::string content;
            while (i < source.size() && source[i] != '"') {
                if (source[i] == '\\' && i + 1 < source.size()) {
                    content += source[i];
                    advance();
                }
                content += source[i];
                advance();
            }
            if (i < source.size() && source[i] == '"') advance();
            tokens.push_back(Token{TokenType::StringLiteral, content, tLine, tCol});
            continue;
        }

        // Char Literal
        if (c == '\'') {
            uint32_t tLine = line;
            uint32_t tCol = col;
            advance();
            std::string content;
            while (i < source.size() && source[i] != '\'') {
                if (source[i] == '\\' && i + 1 < source.size()) {
                    content += source[i];
                    advance();
                }
                content += source[i];
                advance();
            }
            if (i < source.size() && source[i] == '\'') advance();
            tokens.push_back(Token{TokenType::CharLiteral, content, tLine, tCol});
            continue;
        }

        // Identifiers and Keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            uint32_t tLine = line;
            uint32_t tCol = col;
            std::string ident;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) {
                ident += source[i];
                advance();
            }
            if (ident == "true" || ident == "false") {
                tokens.push_back(Token{TokenType::BoolLiteral, ident, tLine, tCol});
            } else if (ident == "nullptr") {
                tokens.push_back(Token{TokenType::NullptrLiteral, ident, tLine, tCol});
            } else if (isKeyword(ident)) {
                tokens.push_back(Token{TokenType::Keyword, ident, tLine, tCol});
            } else {
                tokens.push_back(Token{TokenType::Identifier, ident, tLine, tCol});
            }
            continue;
        }

        // Numeric Literals (Hex, Binary, Octal, Floating, Integer with suffixes)
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
            uint32_t tLine = line;
            uint32_t tCol = col;
            std::string num;
            bool isFloat = false;

            if (c == '0' && i + 1 < source.size() && (source[i + 1] == 'x' || source[i + 1] == 'X')) {
                num += source[i]; num += source[i + 1];
                advance(2);
                while (i < source.size() && (std::isxdigit(static_cast<unsigned char>(source[i])) || source[i] == '\'')) {
                    if (source[i] != '\'') num += source[i];
                    advance();
                }
            } else if (c == '0' && i + 1 < source.size() && (source[i + 1] == 'b' || source[i + 1] == 'B')) {
                num += source[i]; num += source[i + 1];
                advance(2);
                while (i < source.size() && (source[i] == '0' || source[i] == '1' || source[i] == '\'')) {
                    if (source[i] != '\'') num += source[i];
                    advance();
                }
            } else {
                while (i < source.size() && (std::isdigit(static_cast<unsigned char>(source[i])) || source[i] == '.' || source[i] == '\'')) {
                    if (source[i] == '.') isFloat = true;
                    if (source[i] != '\'') num += source[i];
                    advance();
                }
                if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
                    isFloat = true;
                    num += source[i];
                    advance();
                    if (i < source.size() && (source[i] == '+' || source[i] == '-')) {
                        num += source[i];
                        advance();
                    }
                    while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) {
                        num += source[i];
                        advance();
                    }
                }
            }
            // Suffixes (f, l, u, ll, etc.)
            while (i < source.size() && (std::isalpha(static_cast<unsigned char>(source[i])) || source[i] == '_')) {
                num += source[i];
                advance();
            }

            tokens.push_back(Token{isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral, num, tLine, tCol});
            continue;
        }

        // Multi-character and Single-character Operators
        uint32_t tLine = line;
        uint32_t tCol = col;
        auto match2 = [&](char c1, char c2) {
            return (i + 1 < source.size() && source[i] == c1 && source[i + 1] == c2);
        };
        auto match3 = [&](char c1, char c2, char c3) {
            return (i + 2 < source.size() && source[i] == c1 && source[i + 1] == c2 && source[i + 2] == c3);
        };

        if (match3('<', '=', '>')) { tokens.push_back(Token{TokenType::Spaceship, "<=>", tLine, tCol}); advance(3); continue; }
        if (match3('<', '<', '=')) { tokens.push_back(Token{TokenType::LessLessEqual, "<<=", tLine, tCol}); advance(3); continue; }
        if (match3('>', '>', '=')) { tokens.push_back(Token{TokenType::GreaterGreaterEqual, ">>=", tLine, tCol}); advance(3); continue; }

        if (match2('+', '+')) { tokens.push_back(Token{TokenType::PlusPlus, "++", tLine, tCol}); advance(2); continue; }
        if (match2('+', '=')) { tokens.push_back(Token{TokenType::PlusEqual, "+=", tLine, tCol}); advance(2); continue; }
        if (match2('-', '-')) { tokens.push_back(Token{TokenType::MinusMinus, "--", tLine, tCol}); advance(2); continue; }
        if (match2('-', '=')) { tokens.push_back(Token{TokenType::MinusEqual, "-=", tLine, tCol}); advance(2); continue; }
        if (match2('-', '>')) { tokens.push_back(Token{TokenType::Arrow, "->", tLine, tCol}); advance(2); continue; }
        if (match2('*', '=')) { tokens.push_back(Token{TokenType::StarEqual, "*=", tLine, tCol}); advance(2); continue; }
        if (match2('/', '=')) { tokens.push_back(Token{TokenType::SlashEqual, "/=", tLine, tCol}); advance(2); continue; }
        if (match2('%', '=')) { tokens.push_back(Token{TokenType::PercentEqual, "%=", tLine, tCol}); advance(2); continue; }
        if (match2('=', '=')) { tokens.push_back(Token{TokenType::EqualEqual, "==", tLine, tCol}); advance(2); continue; }
        if (match2('!', '=')) { tokens.push_back(Token{TokenType::ExclaimEqual, "!=", tLine, tCol}); advance(2); continue; }
        if (match2('<', '<')) { tokens.push_back(Token{TokenType::LessLess, "<<", tLine, tCol}); advance(2); continue; }
        if (match2('<', '=')) { tokens.push_back(Token{TokenType::LessEqual, "<=", tLine, tCol}); advance(2); continue; }
        if (match2('>', '>')) { tokens.push_back(Token{TokenType::GreaterGreater, ">>", tLine, tCol}); advance(2); continue; }
        if (match2('>', '=')) { tokens.push_back(Token{TokenType::GreaterEqual, ">=", tLine, tCol}); advance(2); continue; }
        if (match2('&', '&')) { tokens.push_back(Token{TokenType::AmpAmp, "&&", tLine, tCol}); advance(2); continue; }
        if (match2('&', '=')) { tokens.push_back(Token{TokenType::AmpEqual, "&=", tLine, tCol}); advance(2); continue; }
        if (match2('|', '|')) { tokens.push_back(Token{TokenType::PipePipe, "||", tLine, tCol}); advance(2); continue; }
        if (match2('|', '=')) { tokens.push_back(Token{TokenType::PipeEqual, "|=", tLine, tCol}); advance(2); continue; }
        if (match2('^', '=')) { tokens.push_back(Token{TokenType::CaretEqual, "^=", tLine, tCol}); advance(2); continue; }
        if (match2(':', ':')) { tokens.push_back(Token{TokenType::ColonColon, "::", tLine, tCol}); advance(2); continue; }
        if (match2('#', '#')) { tokens.push_back(Token{TokenType::HashHash, "##", tLine, tCol}); advance(2); continue; }

        switch (c) {
            case '+': tokens.push_back(Token{TokenType::Plus, "+", tLine, tCol}); break;
            case '-': tokens.push_back(Token{TokenType::Minus, "-", tLine, tCol}); break;
            case '*': tokens.push_back(Token{TokenType::Star, "*", tLine, tCol}); break;
            case '/': tokens.push_back(Token{TokenType::Slash, "/", tLine, tCol}); break;
            case '%': tokens.push_back(Token{TokenType::Percent, "%", tLine, tCol}); break;
            case '=': tokens.push_back(Token{TokenType::Equal, "=", tLine, tCol}); break;
            case '!': tokens.push_back(Token{TokenType::Exclaim, "!", tLine, tCol}); break;
            case '<': tokens.push_back(Token{TokenType::Less, "<", tLine, tCol}); break;
            case '>': tokens.push_back(Token{TokenType::Greater, ">", tLine, tCol}); break;
            case '&': tokens.push_back(Token{TokenType::Amp, "&", tLine, tCol}); break;
            case '|': tokens.push_back(Token{TokenType::Pipe, "|", tLine, tCol}); break;
            case '^': tokens.push_back(Token{TokenType::Caret, "^", tLine, tCol}); break;
            case '~': tokens.push_back(Token{TokenType::Tilde, "~", tLine, tCol}); break;
            case '?': tokens.push_back(Token{TokenType::Question, "?", tLine, tCol}); break;
            case ':': tokens.push_back(Token{TokenType::Colon, ":", tLine, tCol}); break;
            case ';': tokens.push_back(Token{TokenType::Semicolon, ";", tLine, tCol}); break;
            case ',': tokens.push_back(Token{TokenType::Comma, ",", tLine, tCol}); break;
            case '.': tokens.push_back(Token{TokenType::Dot, ".", tLine, tCol}); break;
            case '(': tokens.push_back(Token{TokenType::OpenParen, "(", tLine, tCol}); break;
            case ')': tokens.push_back(Token{TokenType::CloseParen, ")", tLine, tCol}); break;
            case '{': tokens.push_back(Token{TokenType::OpenBrace, "{", tLine, tCol}); break;
            case '}': tokens.push_back(Token{TokenType::CloseBrace, "}", tLine, tCol}); break;
            case '[': tokens.push_back(Token{TokenType::OpenBracket, "[", tLine, tCol}); break;
            case ']': tokens.push_back(Token{TokenType::CloseBracket, "]", tLine, tCol}); break;
            case '#': tokens.push_back(Token{TokenType::Hash, "#", tLine, tCol}); break;
            default:
                tokens.push_back(Token{TokenType::Identifier, std::string(1, c), tLine, tCol});
                break;
        }
        advance();
    }

    tokens.push_back(Token{TokenType::EndOfFile, "", line, col});
    return tokens;
}

std::vector<Token> Cpp20Lexicon::tokenize(const std::string& source,
                                           const GenomicExpressionGrammar& grammar) {
    auto tokens = tokenize(source);
    for (auto& token : tokens) {
        if (token.type == TokenType::Keyword || token.type == TokenType::Identifier) {
            token.type = grammar.keyword(token.text) ? TokenType::Keyword : TokenType::Identifier;
        }
    }
    return tokens;
}

} // namespace genomic
