#include "cpp20/cpp20_preprocessor.hpp"
#include <sstream>
#include <stdexcept>

namespace genomic {

Cpp20Preprocessor::Cpp20Preprocessor() {
    // Standard predefined C++20 macros
    defineMacro("__cplusplus", "202002L");
    defineMacro("__GENOMIC_CPP20__", "1");
}

void Cpp20Preprocessor::defineMacro(const std::string& name, const std::string& value) {
    MacroDefinition def;
    def.name = name;
    def.isFunctionLike = false;
    def.replacementTokens = Cpp20Lexicon::tokenize(value);
    // Remove EndOfFile token from replacement
    if (!def.replacementTokens.empty() && def.replacementTokens.back().type == TokenType::EndOfFile) {
        def.replacementTokens.pop_back();
    }
    macros_[name] = std::move(def);
}

void Cpp20Preprocessor::defineFunctionMacro(const std::string& name, const std::vector<std::string>& params,
                                            const std::string& body, bool hasVarArgs) {
    MacroDefinition def;
    def.name = name;
    def.isFunctionLike = true;
    def.parameters = params;
    def.hasVarArgs = hasVarArgs;
    def.replacementTokens = Cpp20Lexicon::tokenize(body);
    if (!def.replacementTokens.empty() && def.replacementTokens.back().type == TokenType::EndOfFile) {
        def.replacementTokens.pop_back();
    }
    macros_[name] = std::move(def);
}

void Cpp20Preprocessor::undefineMacro(const std::string& name) {
    macros_.erase(name);
}

bool Cpp20Preprocessor::isDefined(const std::string& name) const {
    return macros_.find(name) != macros_.end();
}

void Cpp20Preprocessor::addVirtualHeader(const std::string& filename, const std::string& content) {
    virtualFiles_[filename] = content;
}

std::string Cpp20Preprocessor::process(const std::string& source) {
    std::istringstream stream(source);
    std::string line;
    std::ostringstream output;

    std::vector<bool> conditionStack; // true = emit lines, false = suppress
    auto currentEmit = [&]() {
        for (bool c : conditionStack) {
            if (!c) return false;
        }
        return true;
    };

    while (std::getline(stream, line)) {
        // Find first non-whitespace
        size_t firstNonWs = line.find_first_not_of(" \t");
        if (firstNonWs == std::string::npos) {
            if (currentEmit()) output << "\n";
            continue;
        }

        if (line[firstNonWs] == '#') {
            std::string directiveLine = line.substr(firstNonWs + 1);
            std::istringstream dirStream(directiveLine);
            std::string dir;
            dirStream >> dir;

            if (dir == "ifdef") {
                std::string macroName;
                dirStream >> macroName;
                conditionStack.push_back(isDefined(macroName));
            } else if (dir == "ifndef") {
                std::string macroName;
                dirStream >> macroName;
                conditionStack.push_back(!isDefined(macroName));
            } else if (dir == "if") {
                std::string condExpr;
                std::getline(dirStream, condExpr);
                size_t p = condExpr.find("defined(");
                bool val = true;
                if (p != std::string::npos) {
                    size_t pEnd = condExpr.find(')', p);
                    std::string m = condExpr.substr(p + 8, pEnd - (p + 8));
                    val = isDefined(m);
                }
                conditionStack.push_back(val);
            } else if (dir == "else") {
                if (!conditionStack.empty()) {
                    bool cur = conditionStack.back();
                    conditionStack.back() = !cur;
                }
            } else if (dir == "endif") {
                if (!conditionStack.empty()) {
                    conditionStack.pop_back();
                }
            } else if (currentEmit()) {
                if (dir == "define") {
                    std::string mName;
                    dirStream >> mName;
                    size_t parenPos = mName.find('(');
                    if (parenPos != std::string::npos) {
                        // Function-like macro
                        std::string baseName = mName.substr(0, parenPos);
                        std::string paramList = mName.substr(parenPos + 1);
                        if (!paramList.empty() && paramList.back() == ')') paramList.pop_back();
                        std::vector<std::string> params;
                        std::istringstream pStream(paramList);
                        std::string p;
                        while (std::getline(pStream, p, ',')) {
                            while (!p.empty() && p.front() == ' ') p.erase(0, 1);
                            while (!p.empty() && p.back() == ' ') p.pop_back();
                            params.push_back(p);
                        }
                        std::string body;
                        std::getline(dirStream, body);
                        defineFunctionMacro(baseName, params, body);
                    } else {
                        std::string mVal;
                        std::getline(dirStream, mVal);
                        defineMacro(mName, mVal);
                    }
                } else if (dir == "undef") {
                    std::string mName;
                    dirStream >> mName;
                    undefineMacro(mName);
                } else if (dir == "include") {
                    std::string headerName;
                    dirStream >> headerName;
                    if (!headerName.empty() && (headerName.front() == '<' || headerName.front() == '"')) {
                        headerName = headerName.substr(1, headerName.size() - 2);
                    }
                    if (virtualFiles_.find(headerName) != virtualFiles_.end()) {
                        output << process(virtualFiles_[headerName]) << "\n";
                    }
                } else if (dir == "error") {
                    std::string msg;
                    std::getline(dirStream, msg);
                    throw std::runtime_error("#error: " + msg);
                }
            }
            continue;
        }

        if (currentEmit()) {
            // Macro substitution on line
            auto tokens = Cpp20Lexicon::tokenize(line);
            auto expanded = expandTokens(tokens);
            std::ostringstream lineOut;
            for (size_t t = 0; t < expanded.size(); ++t) {
                if (expanded[t].type == TokenType::EndOfFile) continue;
                lineOut << expanded[t].text;
                if (t + 1 < expanded.size() && expanded[t+1].type != TokenType::EndOfFile) {
                    lineOut << " ";
                }
            }
            output << lineOut.str() << "\n";
        }
    }

    return output.str();
}

std::vector<Token> Cpp20Preprocessor::expandTokens(const std::vector<Token>& tokens) {
    std::vector<Token> result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Identifier && isDefined(tokens[i].text)) {
            const auto& def = macros_[tokens[i].text];
            if (!def.isFunctionLike) {
                // Object-like replacement
                for (const auto& repTok : def.replacementTokens) {
                    result.push_back(repTok);
                }
            } else if (i + 1 < tokens.size() && tokens[i+1].type == TokenType::OpenParen) {
                // Function-like macro invocation
                size_t p = i + 2;
                std::vector<std::vector<Token>> args;
                std::vector<Token> curArg;
                int parenDepth = 1;
                while (p < tokens.size()) {
                    if (tokens[p].type == TokenType::OpenParen) parenDepth++;
                    else if (tokens[p].type == TokenType::CloseParen) {
                        parenDepth--;
                        if (parenDepth == 0) break;
                    } else if (tokens[p].type == TokenType::Comma && parenDepth == 1) {
                        args.push_back(curArg);
                        curArg.clear();
                        p++;
                        continue;
                    }
                    curArg.push_back(tokens[p]);
                    p++;
                }
                if (!curArg.empty()) args.push_back(curArg);
                i = p; // advance past closing paren

                // Map param names to arguments
                std::unordered_map<std::string, std::vector<Token>> paramMap;
                for (size_t pi = 0; pi < def.parameters.size(); ++pi) {
                    if (pi < args.size()) {
                        paramMap[def.parameters[pi]] = args[pi];
                    }
                }

                // Expand replacement tokens
                for (size_t r = 0; r < def.replacementTokens.size(); ++r) {
                    const auto& rep = def.replacementTokens[r];
                    // Stringification '#'
                    if (rep.type == TokenType::Hash && r + 1 < def.replacementTokens.size()) {
                        const auto& nextTok = def.replacementTokens[r + 1];
                        if (paramMap.find(nextTok.text) != paramMap.end()) {
                            std::string stringified = "\"";
                            for (const auto& aTok : paramMap[nextTok.text]) {
                                stringified += aTok.text;
                            }
                            stringified += "\"";
                            result.push_back(Token{TokenType::StringLiteral, stringified, rep.line, rep.column});
                            r++;
                            continue;
                        }
                    }
                    // Token Concatenation '##'
                    if (rep.type == TokenType::HashHash && !result.empty() && r + 1 < def.replacementTokens.size()) {
                        const auto& nextTok = def.replacementTokens[r + 1];
                        std::string combined = result.back().text;
                        if (paramMap.find(nextTok.text) != paramMap.end() && !paramMap[nextTok.text].empty()) {
                            combined += paramMap[nextTok.text][0].text;
                        } else {
                            combined += nextTok.text;
                        }
                        result.back().text = combined;
                        r++;
                        continue;
                    }

                    if (paramMap.find(rep.text) != paramMap.end()) {
                        for (const auto& aTok : paramMap[rep.text]) {
                            result.push_back(aTok);
                        }
                    } else {
                        result.push_back(rep);
                    }
                }
            } else {
                result.push_back(tokens[i]);
            }
        } else {
            result.push_back(tokens[i]);
        }
    }
    return result;
}

} // namespace genomic
