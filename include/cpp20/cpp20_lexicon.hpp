#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace genomic {

class GenomicExpressionGrammar;

enum class TokenType : uint16_t {
    EndOfFile,
    Identifier,
    Keyword,
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,
    BoolLiteral,
    NullptrLiteral,

    // Operators & Punctuators
    Plus, PlusPlus, PlusEqual,
    Minus, MinusMinus, MinusEqual, Arrow, ArrowStar,
    Star, StarEqual,
    Slash, SlashEqual,
    Percent, PercentEqual,
    Equal, EqualEqual,
    Exclaim, ExclaimEqual,
    Less, LessEqual, LessLess, LessLessEqual, Spaceship,
    Greater, GreaterEqual, GreaterGreater, GreaterGreaterEqual,
    Amp, AmpAmp, AmpEqual,
    Pipe, PipePipe, PipeEqual,
    Caret, CaretEqual,
    Tilde,
    Question, Colon, ColonColon,
    Semicolon, Comma, Dot, DotStar,
    OpenParen, CloseParen,
    OpenBrace, CloseBrace,
    OpenBracket, CloseBracket,
    Hash, HashHash,

    // Preprocessor directive tokens
    PpDefine, PpUndef, PpInclude, PpIf, PpIfdef, PpIfndef, PpElif, PpElse, PpEndif, PpPragma, PpError
};

struct Token {
    TokenType type{TokenType::EndOfFile};
    std::string text;
    uint32_t line{1};
    uint32_t column{1};

    bool isKeyword(const std::string& kw) const noexcept {
        return type == TokenType::Keyword && text == kw;
    }
};

class Cpp20Lexicon {
public:
    static const std::unordered_set<std::string>& getAllKeywords();
    static bool isKeyword(const std::string& str);

    // Full C++20 Lexer
    static std::vector<Token> tokenize(const std::string& source);
    static std::vector<Token> tokenize(const std::string& source, const GenomicExpressionGrammar& grammar);
};

} // namespace genomic
