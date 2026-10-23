#include "gpi/foreign_project_intelligence.hpp"
#include "gpi/filesystem_safety.hpp"

#include "cpp20/cpp20_preprocessor.hpp"
#include "gpi/experience_store.hpp"
#include "gpi/project_manifest.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace genomic::gpi {
namespace {

std::string trimAbi(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return {};
    return value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}

std::string canonicalAbiType(std::string type) {
    type.erase(std::remove_if(type.begin(),type.end(),[](unsigned char c){return std::isspace(c)!=0;}),type.end());
    if(type=="i32"||type=="int32"||type=="int32_t"||type=="::int32_t"||type=="std::int32_t")return "int32_t";
    if(type=="i64"||type=="int64"||type=="int64_t"||type=="::int64_t"||type=="std::int64_t"||type=="longlong")return "int64_t";
    if(type=="bool")return "bool";
    return type;
}

std::optional<std::string> boundedAbiExpression(std::string body) {
    body=trimAbi(body);
    if(body.rfind("return ",0)==0){body=trimAbi(body.substr(7));if(!body.empty()&&body.back()==';')body.pop_back();}
    const std::regex rustIf(R"(^if\s+(.+?)\s*\{\s*(.+?)\s*\}\s*else\s*\{\s*(.+?)\s*\}$)");
    std::smatch match;if(std::regex_match(body,match,rustIf))body="("+match[1].str()+" ? "+match[2].str()+" : "+match[3].str()+")";
    if(body.empty()||body.size()>512)return std::nullopt;
    for(const unsigned char c:body)if(!(std::isalnum(c)||std::isspace(c)||std::string_view("_()+-*/%<>=!&|?:.").find(static_cast<char>(c))!=std::string_view::npos))return std::nullopt;
    return body;
}

std::string hexEncode(const std::string& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}

std::string hexDecode(std::string_view value) {
    if (value.size() % 2 != 0) throw std::runtime_error("Corrupt hex string in memory store");
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        throw std::runtime_error("Corrupt hex digit in memory store");
    };
    std::string result;
    result.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        result.push_back(static_cast<char>((digit(value[i]) << 4) | digit(value[i + 1])));
    }
    return result;
}

std::uint64_t parseU64(std::string_view value) {
    std::uint64_t result = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || end != value.data() + value.size()) {
        throw std::runtime_error("Corrupt integer in memory store");
    }
    return result;
}

std::vector<std::string> splitPipes(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto pos = line.find('|', start);
        parts.push_back(line.substr(start, pos == std::string::npos ? pos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return parts;
}

std::string jsonEscapeStr(const std::string& value) {
    std::string result = "\"";
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') result += '\\';
        if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c >= 0x20) result += static_cast<char>(c);
    }
    return result + '"';
}

std::string readFileString(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read file: " + path.string());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::vector<CrossLanguageAbiContract> discoverAbiContracts(const std::filesystem::path& root) {
    std::vector<CrossLanguageAbiContract> result;
    const std::regex goFn(R"(func\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*([A-Za-z_]\w*)\s*\{([^{}]*)\})");
    const std::regex rustFn(R"((?:pub\s+)?(?:extern\s+"C"\s+)?fn\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*->\s*([A-Za-z_]\w*)\s*\{([^\r\n]*)\})");
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
        if(!entry.is_regular_file())continue;
        const auto ext=entry.path().extension().string();
        if(ext!=".kr"&&ext!=".go"&&ext!=".rs")continue;
        const auto source=readFileString(entry.path());
        for(const auto& parser:{std::pair<const std::regex*,std::string>{&goFn,"go"},{&rustFn,"rust"}}) {
            for(std::sregex_iterator it(source.begin(),source.end(),*parser.first),end;it!=end;++it) {
                CrossLanguageAbiContract contract;contract.sourceLanguage=ext==".kr"?"kr/"+parser.second:parser.second;
                contract.sourcePath=std::filesystem::relative(entry.path(),root).generic_string();
                contract.functionName=(*it)[1].str();contract.returnType=canonicalAbiType((*it)[3].str());
                std::istringstream params((*it)[2].str());std::string part;
                while(std::getline(params,part,',')) {
                    part=trimAbi(part);if(part.empty())continue;
                    const auto colon=part.find(':');const auto space=part.find_last_of(" \t");
                    if(colon!=std::string::npos)contract.parameters.push_back({canonicalAbiType(part.substr(colon+1)),trimAbi(part.substr(0,colon))});
                    else if(space!=std::string::npos)contract.parameters.push_back({canonicalAbiType(part.substr(space+1)),trimAbi(part.substr(0,space))});
                    else contract.diagnostic="unsupported parameter declaration";
                }
                const auto expression=boundedAbiExpression((*it)[4].str());
                if(expression&&contract.diagnostic.empty()){contract.cppExpression=*expression;contract.semanticallyDerivable=true;}
                else if(contract.diagnostic.empty())contract.diagnostic="body is outside bounded scalar ABI grammar";
                const auto duplicate=std::find_if(result.begin(),result.end(),[&](const auto& item){return item.functionName==contract.functionName&&item.parameters==contract.parameters&&item.returnType==contract.returnType;});
                if(duplicate==result.end())result.push_back(std::move(contract));
                else if(!duplicate->semanticallyDerivable&&contract.semanticallyDerivable)*duplicate=std::move(contract);
            }
        }
    }
    return result;
}

void writeFileString(const std::filesystem::path& path, const std::string& content) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    if (!out) throw std::runtime_error("Cannot write file: " + path.string());
}

std::string sanitizeNodeId(std::string s) {
    for (char& c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            c = '_';
        }
    }
    return s;
}

std::vector<std::string> extractIncludedHeaders(const std::string& source) {
    std::vector<std::string> headers;
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find("#include");
        if (pos == std::string::npos) continue;
        auto q1 = line.find_first_of("\"<", pos + 8);
        if (q1 == std::string::npos) continue;
        char closeCh = (line[q1] == '<') ? '>' : '"';
        auto q2 = line.find(closeCh, q1 + 1);
        if (q2 != std::string::npos && q2 > q1 + 1) {
            headers.push_back(line.substr(q1 + 1, q2 - q1 - 1));
        }
    }
    return headers;
}

// Extract KR origin path from generated C++20 comments such as:
//   "// Automatisch generiertes C++20-Modul aus workspace/dumper.kr"
//   "// --- Segment aus gui/colors.kr:1 ---"
std::string extractKrOriginPathFromComments(const std::string& source) {
    std::istringstream in(source);
    std::string line;
    for (int i = 0; i < 40 && std::getline(in, line); ++i) {
        const auto pos = line.find(" aus ");
        if (pos != std::string::npos && line.find(".kr") != std::string::npos) {
            std::string tail = line.substr(pos + 5);
            while (!tail.empty() && (tail.back() == '\r' || tail.back() == '\n' || tail.back() == ' ' || tail.back() == '-')) {
                tail.pop_back();
            }
            const auto colon = tail.find(':');
            if (colon != std::string::npos) {
                tail = tail.substr(0, colon);
            }
            return tail;
        }
    }
    return {};
}

// Comprehensive token-level C++20 declaration & definition scanner that supplements Cpp20Parser::parseWithGenome
// so real-world C++20 files (with raw string literals, #ifdef blocks, extern "C", std::filesystem, Win32/SDL types)
// have all their structs, classes, enums, and functions semantically discovered and linked to their KR origin!
void extractResilientCpp20Symbols(
    const std::string& rawSource,
    const std::string& nodeId,
    const std::string& relPath,
    const std::string& originKrPath,
    bool isGeneratedFromKr,
    bool isHeader,
    bool isTestFile,
    std::vector<DiscoveredSymbol>& symbols,
    std::vector<std::string>& testCalledFunctions) {

    std::vector<Token> tokens;
    try {
        tokens = Cpp20Lexicon::tokenize(rawSource);
    } catch (...) {
        return;
    }

    auto addOrMergeSymbol = [&](DiscoveredSymbol sym) {
        for (auto& existing : symbols) {
            if (existing.kind == sym.kind && existing.name == sym.name) {
                if (isHeader) existing.hasDeclarationInHeader = true;
                if (sym.hasDefinitionInSource) {
                    existing.hasDefinitionInSource = true;
                    existing.definingNodeId = nodeId;
                    existing.definingPath = relPath;
                    if (!originKrPath.empty()) existing.originKrPath = originKrPath;
                    if (isGeneratedFromKr) existing.isGeneratedFromKr = true;
                }
                for (const auto& c : sym.calledFunctions) {
                    if (std::find(existing.calledFunctions.begin(), existing.calledFunctions.end(), c) == existing.calledFunctions.end()) {
                        existing.calledFunctions.push_back(c);
                    }
                }
                return;
            }
        }
        symbols.push_back(std::move(sym));
    };

    static const std::unordered_set<std::string> controlKeywords = {
        "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "decltype",
        "static_assert", "reinterpret_cast", "static_cast", "const_cast", "dynamic_cast", "namespace"
    };

    int braceDepth = 0;
    std::vector<std::pair<int, std::string>> namespaceStack; // (braceDepth, nsName)

    auto currentNsString = [&]() {
        std::string ns;
        for (const auto& [d, name] : namespaceStack) {
            if (name.empty()) continue;
            if (!ns.empty()) ns += "::";
            ns += name;
        }
        return ns;
    };

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& tok = tokens[i];
        if (tok.text == "{") {
            ++braceDepth;
            continue;
        }
        if (tok.text == "}") {
            while (!namespaceStack.empty() && namespaceStack.back().first >= braceDepth) {
                namespaceStack.pop_back();
            }
            if (braceDepth > 0) --braceDepth;
            continue;
        }

        // Track top-level `namespace Foo {` (ignore `namespace fs = std::filesystem;`)
        if (tok.text == "namespace" && i + 2 < tokens.size() && tokens[i + 1].type == TokenType::Identifier) {
            if (tokens[i + 2].text == "{") {
                namespaceStack.emplace_back(braceDepth + 1, tokens[i + 1].text);
                ++braceDepth;
                i += 2;
                continue;
            }
        }

        // Only scan top-level or namespace-level declarations (braceDepth == namespaceStack.size(), or +1 for extern "C")
        if (braceDepth > static_cast<int>(namespaceStack.size()) + 1) continue;

        // Detect `struct Name {` or `enum [class] Name {`
        if ((tok.text == "struct" || tok.text == "class" || tok.text == "enum") && i + 2 < tokens.size()) {
            std::size_t nameIdx = i + 1;
            bool isEnum = (tok.text == "enum");
            if (isEnum && (tokens[nameIdx].text == "class" || tokens[nameIdx].text == "struct") && nameIdx + 1 < tokens.size()) {
                ++nameIdx;
            }
            if (tokens[nameIdx].type == TokenType::Identifier && nameIdx + 1 < tokens.size() &&
                (tokens[nameIdx + 1].text == "{" || tokens[nameIdx + 1].text == ":")) {
                DiscoveredSymbol sym;
                sym.kind = isEnum ? "enum" : "type";
                sym.name = tokens[nameIdx].text;
                const std::string ns = currentNsString();
                sym.qualifiedName = ns.empty() ? sym.name : (ns + "::" + sym.name);
                sym.definingNodeId = nodeId;
                sym.definingPath = relPath;
                sym.originKrPath = originKrPath;
                sym.isGeneratedFromKr = isGeneratedFromKr;
                sym.hasDeclarationInHeader = isHeader;
                sym.hasDefinitionInSource = true;
                addOrMergeSymbol(std::move(sym));
                continue;
            }
        }

        // Detect top-level/namespace-level function declaration or definition:
        // `<ReturnType> <FunctionName>(<Params>) [const] [noexcept] {` or `;`
        if (tok.type == TokenType::Identifier && controlKeywords.find(tok.text) == controlKeywords.end() &&
            i >= 1 && i + 1 < tokens.size() && tokens[i + 1].text == "(") {
            // Check preceding token is part of a return type (Identifier,Keyword, `*`, `&`, `>`) and not `.` or `->` or `::`
            const std::string& prevText = tokens[i - 1].text;
            if (prevText == "." || prevText == "->" || prevText == "::" || prevText == "=" || prevText == "(" || prevText == ",") {
                continue;
            }
            bool prevValidTypeToken = (tokens[i - 1].type == TokenType::Identifier ||
                                       tokens[i - 1].type == TokenType::Keyword ||
                                       prevText == "*" || prevText == "&" || prevText == ">");
            if (!prevValidTypeToken) continue;

            // Find matching `)`
            std::size_t cur = i + 2;
            int parenDepth = 1;
            std::vector<std::pair<std::string, std::string>> params;
            std::string curParamTokens;
            auto parseParamTypeAndName = [](const std::string& raw) -> std::pair<std::string, std::string> {
                std::string s = raw;
                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
                if (s.empty()) return {"", ""};
                auto lastSpace = s.rfind(' ');
                if (lastSpace == std::string::npos) return {s, ""};
                return {s.substr(0, lastSpace), s.substr(lastSpace + 1)};
            };
            while (cur < tokens.size() && parenDepth > 0) {
                if (tokens[cur].text == "(") ++parenDepth;
                else if (tokens[cur].text == ")") {
                    --parenDepth;
                    if (parenDepth == 0) break;
                }
                if (parenDepth == 1 && tokens[cur].text == ",") {
                    auto p = parseParamTypeAndName(curParamTokens);
                    params.emplace_back(std::move(p.first), std::move(p.second));
                    curParamTokens.clear();
                } else {
                    if (!curParamTokens.empty()) curParamTokens += " ";
                    curParamTokens += tokens[cur].text;
                }
                ++cur;
            }
            if (parenDepth != 0 || cur >= tokens.size()) continue;
            if (!curParamTokens.empty() && curParamTokens != "void") {
                auto p = parseParamTypeAndName(curParamTokens);
                params.emplace_back(std::move(p.first), std::move(p.second));
            }

            // Skip trailing `const`, `noexcept`
            std::size_t afterParen = cur + 1;
            while (afterParen < tokens.size() &&
                   (tokens[afterParen].text == "const" || tokens[afterParen].text == "noexcept")) {
                ++afterParen;
            }
            if (afterParen >= tokens.size()) continue;

            const bool isDef = (tokens[afterParen].text == "{");
            const bool isDecl = (tokens[afterParen].text == ";");
            if (!isDef && !isDecl) continue;

            // Reconstruct return type from preceding tokens on the same statement
            std::string retType = prevText;
            if (i >= 2 && (tokens[i - 2].type == TokenType::Identifier || tokens[i - 2].type == TokenType::Keyword || tokens[i - 2].text == "::")) {
                if (tokens[i - 2].text != "static" && tokens[i - 2].text != "inline" && tokens[i - 2].text != "extern" &&
                    tokens[i - 2].text != "}" && tokens[i - 2].text != ";") {
                    retType = tokens[i - 2].text + " " + retType;
                }
            }

            DiscoveredSymbol fnSym;
            fnSym.kind = "function";
            fnSym.name = tok.text;
            const std::string ns = currentNsString();
            fnSym.qualifiedName = ns.empty() ? fnSym.name : (ns + "::" + fnSym.name);
            fnSym.returnType = retType;
            fnSym.parametersOrFields = std::move(params);
            fnSym.definingNodeId = nodeId;
            fnSym.definingPath = relPath;
            fnSym.originKrPath = originKrPath;
            fnSym.isGeneratedFromKr = isGeneratedFromKr;
            fnSym.hasDeclarationInHeader = isHeader || isDecl;
            fnSym.hasDefinitionInSource = isDef;

            // If it has a body `{ ... }`, scan body tokens for called functions!
            if (isDef) {
                std::size_t bodyIdx = afterParen + 1;
                int bodyBrace = 1;
                while (bodyIdx < tokens.size() && bodyBrace > 0) {
                    if (tokens[bodyIdx].text == "{") ++bodyBrace;
                    else if (tokens[bodyIdx].text == "}") --bodyBrace;
                    else if (tokens[bodyIdx].type == TokenType::Identifier &&
                             controlKeywords.find(tokens[bodyIdx].text) == controlKeywords.end() &&
                             bodyIdx + 1 < tokens.size() && tokens[bodyIdx + 1].text == "(") {
                        const std::string& callee = tokens[bodyIdx].text;
                        if (std::find(fnSym.calledFunctions.begin(), fnSym.calledFunctions.end(), callee) == fnSym.calledFunctions.end()) {
                            fnSym.calledFunctions.push_back(callee);
                        }
                        if (isTestFile || fnSym.name == "main") {
                            if (std::find(testCalledFunctions.begin(), testCalledFunctions.end(), callee) == testCalledFunctions.end()) {
                                testCalledFunctions.push_back(callee);
                            }
                        }
                    }
                    ++bodyIdx;
                }
            }

            addOrMergeSymbol(std::move(fnSym));
        }
    }
}
std::string prepareSourceForGenomicAst(const std::string& rawSource) {
    std::istringstream in(rawSource);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = line;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
            trimmed.erase(trimmed.begin());
        }
        if (trimmed.rfind("#", 0) == 0) {
            out << "\n";
            continue;
        }
        // Replace `std::cout << ... ;` lines with a benign expression statement so Cpp20Parser can focus on declarations/functions
        if (trimmed.find("std::cout") != std::string::npos || trimmed.find("std::cerr") != std::string::npos) {
            out << "0;\n";
            continue;
        }
        out << line << "\n";
    }
    return out.str();
}

void collectCallsFromAst(const AstNode& node, std::vector<std::string>& outCalls) {
    if (node.kind == AstNodeKind::CallExpr && !node.name.empty()) {
        if (std::find(outCalls.begin(), outCalls.end(), node.name) == outCalls.end()) {
            outCalls.push_back(node.name);
        }
    }
    for (const auto& child : node.children) {
        if (child) collectCallsFromAst(*child, outCalls);
    }
}

void collectSymbolsFromAst(
    const AstNode& node,
    const std::string& currentNamespace,
    const std::string& nodeId,
    const std::string& relPath,
    bool isHeader,
    bool isTestFile,
    std::vector<DiscoveredSymbol>& symbols,
    std::vector<std::string>& testCalledFunctions) {

    if (node.kind == AstNodeKind::TranslationUnit) {
        for (const auto& child : node.children) {
            if (child) collectSymbolsFromAst(*child, currentNamespace, nodeId, relPath, isHeader, isTestFile, symbols, testCalledFunctions);
        }
        return;
    }

    if (node.kind == AstNodeKind::NamespaceDecl) {
        std::string nextNs = currentNamespace.empty() ? node.name : (currentNamespace + "::" + node.name);
        for (const auto& child : node.children) {
            if (child) collectSymbolsFromAst(*child, nextNs, nodeId, relPath, isHeader, isTestFile, symbols, testCalledFunctions);
        }
        return;
    }

    if (node.kind == AstNodeKind::TemplateDecl && !node.children.empty() && node.children[0]) {
        collectSymbolsFromAst(*node.children[0], currentNamespace, nodeId, relPath, isHeader, isTestFile, symbols, testCalledFunctions);
        if (!symbols.empty() && symbols.back().name == node.children[0]->name) {
            if (symbols.back().kind == "function") symbols.back().kind = "template_function";
        }
        return;
    }

    if (node.kind == AstNodeKind::ConceptDecl) {
        DiscoveredSymbol sym;
        sym.kind = "concept";
        sym.name = node.name;
        sym.qualifiedName = currentNamespace.empty() ? node.name : (currentNamespace + "::" + node.name);
        sym.returnType = node.strVal;
        sym.definingNodeId = nodeId;
        sym.definingPath = relPath;
        sym.hasDeclarationInHeader = isHeader;
        sym.hasDefinitionInSource = true;
        symbols.push_back(std::move(sym));
        return;
    }

    if (node.kind == AstNodeKind::RecordDecl) {
        bool isEnum = node.name.rfind("enum_", 0) == 0;
        std::string cleanName = isEnum ? node.name.substr(5) : node.name;
        DiscoveredSymbol sym;
        sym.kind = isEnum ? "enum" : "type";
        sym.name = cleanName;
        sym.qualifiedName = currentNamespace.empty() ? cleanName : (currentNamespace + "::" + cleanName);
        sym.definingNodeId = nodeId;
        sym.definingPath = relPath;
        sym.hasDeclarationInHeader = isHeader;
        sym.hasDefinitionInSource = !node.children.empty();
        for (const auto& member : node.children) {
            if (!member) continue;
            if (member->kind == AstNodeKind::VariableDecl) {
                sym.parametersOrFields.emplace_back(
                    member->evaluatedType ? member->evaluatedType->toString() : "long long",
                    member->name);
            } else if (member->kind == AstNodeKind::FunctionDecl) {
                DiscoveredSymbol mSym;
                mSym.kind = "method";
                mSym.name = cleanName + "::" + member->name;
                mSym.qualifiedName = sym.qualifiedName + "::" + member->name;
                mSym.returnType = member->evaluatedType ? member->evaluatedType->toString() : "void";
                mSym.definingNodeId = nodeId;
                mSym.definingPath = relPath;
                mSym.hasDeclarationInHeader = isHeader;
                bool hasBody = !member->children.empty() && member->children.back() &&
                               member->children.back()->kind == AstNodeKind::CompoundStmt;
                mSym.hasDefinitionInSource = hasBody;
                symbols.push_back(std::move(mSym));
            }
        }
        symbols.push_back(std::move(sym));
        return;
    }

    if (node.kind == AstNodeKind::FunctionDecl) {
        bool hasBody = !node.children.empty() && node.children.back() &&
                       node.children.back()->kind == AstNodeKind::CompoundStmt;
        std::size_t paramCount = hasBody ? (node.children.size() - 1) : node.children.size();
        std::vector<std::pair<std::string, std::string>> params;
        for (std::size_t i = 0; i < paramCount; ++i) {
            if (node.children[i]) {
                params.emplace_back(
                    node.children[i]->evaluatedType ? node.children[i]->evaluatedType->toString() : "long long",
                    node.children[i]->name);
            }
        }
        std::vector<std::string> calls;
        if (hasBody) {
            collectCallsFromAst(*node.children.back(), calls);
            if (isTestFile || node.name == "main") {
                for (const auto& c : calls) {
                    if (std::find(testCalledFunctions.begin(), testCalledFunctions.end(), c) == testCalledFunctions.end()) {
                        testCalledFunctions.push_back(c);
                    }
                }
            }
        }

        // Merge with existing symbol if already declared in header or defined in source
        for (auto& existing : symbols) {
            if ((existing.kind == "function" || existing.kind == "template_function") &&
                existing.name == node.name && existing.parametersOrFields.size() == params.size()) {
                if (isHeader) existing.hasDeclarationInHeader = true;
                if (hasBody) {
                    existing.hasDefinitionInSource = true;
                    existing.definingNodeId = nodeId;
                    existing.definingPath = relPath;
                    for (const auto& c : calls) {
                        if (std::find(existing.calledFunctions.begin(), existing.calledFunctions.end(), c) == existing.calledFunctions.end()) {
                            existing.calledFunctions.push_back(c);
                        }
                    }
                }
                return;
            }
        }

        DiscoveredSymbol sym;
        sym.kind = "function";
        sym.name = node.name;
        sym.qualifiedName = currentNamespace.empty() ? node.name : (currentNamespace + "::" + node.name);
        sym.returnType = node.evaluatedType ? node.evaluatedType->toString() : "void";
        sym.parametersOrFields = std::move(params);
        sym.definingNodeId = nodeId;
        sym.definingPath = relPath;
        sym.hasDeclarationInHeader = isHeader;
        sym.hasDefinitionInSource = hasBody;
        sym.calledFunctions = std::move(calls);
        symbols.push_back(std::move(sym));
    }
}

// Discover or load a foreign project into a GenerativeProjectGraph + ProjectVerificationContract
LoadedProjectSpecification discoverOrLoadForeignProject(
    const std::filesystem::path& foreignRoot,
    const Cpp20SourceGenome& sourceGenome) {

    const auto manifestPath = foreignRoot / "project.gpi";
    if (std::filesystem::is_regular_file(manifestPath)) {
        return loadProjectManifest(manifestPath, foreignRoot, sourceGenome);
    }

    // Automatic discovery of arbitrary C++20 directory layout (include/, src/, tests/, CMakeLists.txt, fixtures/)
    GenerativeProjectGraph graph(sourceGenome);
    ProjectVerificationContract contract;
    contract.executionTimeout = std::chrono::seconds(20);
    contract.requireSandbox = true;

    if (std::filesystem::is_directory(foreignRoot / "include")) {
        contract.includeRoots.push_back("include");
    }

    std::vector<std::filesystem::path> allFiles;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(foreignRoot)) {
        if (entry.is_regular_file()) {
            allFiles.push_back(entry.path());
        }
    }
    std::sort(allFiles.begin(), allFiles.end());

    // Register all directories containing headers as include roots (including root, include/, KR_BUILD/, etc.)
    for (const auto& file : allFiles) {
        const auto ext = file.extension().string();
        if (ext == ".hpp" || ext == ".h") {
            const auto relDir = std::filesystem::relative(file.parent_path(), foreignRoot).generic_string();
            const std::string normDir = (relDir.empty() || relDir == ".") ? "." : relDir;
            if (std::find(contract.includeRoots.begin(), contract.includeRoots.end(), normDir) == contract.includeRoots.end()) {
                contract.includeRoots.push_back(normDir);
            }
        }
    }

    bool hasModularKrBuild = false;
    for (const auto& file : allFiles) {
        const auto rel = std::filesystem::relative(file, foreignRoot).generic_string();
        if (rel.find("KR_BUILD/modules/") != std::string::npos && file.extension() == ".cpp") {
            hasModularKrBuild = true;
            break;
        }
    }

    std::map<std::string, std::string> headerPathToNodeId;
    std::vector<std::string> sourceNodes;
    std::string mainNodeId;
    std::string testNodeId;

    for (const auto& file : allFiles) {
        const auto rel = std::filesystem::relative(file, foreignRoot).generic_string();
        const auto ext = file.extension().string();
        const auto stem = file.stem().string();
        const std::string content = readFileString(file);

        if (ext == ".hpp" || ext == ".h") {
            std::string id = "hdr_" + sanitizeNodeId(stem);
            graph.addNode(id, ProjectNodeKind::Header, rel, content);
            headerPathToNodeId[rel] = id;
            headerPathToNodeId[file.filename().generic_string()] = id;
            if (rel.rfind("include/", 0) == 0) {
                headerPathToNodeId[rel.substr(8)] = id;
            }
        } else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
            bool isTest = (rel.rfind("tests/", 0) == 0 || stem.find("test") != std::string::npos);
            bool hasMain = (content.find("int main(") != std::string::npos || content.find("int main (") != std::string::npos);
            bool isAmalgamatedDuplicate = (hasModularKrBuild && rel == "KR_BUILD/kr_cpp.cpp");
            bool requiresExternalGui = (content.find("#include <SDL3/SDL.h>") != std::string::npos || content.find("#include <SDL.h>") != std::string::npos);
            std::string id = (isTest ? "test_" : (hasMain ? "app_" : "src_")) + sanitizeNodeId(stem);
            graph.addNode(id, isTest ? ProjectNodeKind::Test : ProjectNodeKind::Source, rel, content);
            if (isTest) {
                testNodeId = id;
            } else if (hasMain) {
                if (!requiresExternalGui) {
                    mainNodeId = id;
                }
            } else if (!isAmalgamatedDuplicate && !requiresExternalGui) {
                sourceNodes.push_back(id);
            }
        } else if (file.filename() == "CMakeLists.txt") {
            graph.addNode("build_manifest", ProjectNodeKind::BuildManifest, rel, content);
        }
    }

    // Wire include dependencies and link dependencies
    for (const auto& node : graph.nodes()) {
        if (node.kind == ProjectNodeKind::Source || node.kind == ProjectNodeKind::Test) {
            for (const auto& inc : extractIncludedHeaders(graph.nodeContent(node.id))) {
                auto it = headerPathToNodeId.find(inc);
                if (it != headerPathToNodeId.end()) {
                    try { graph.addDependency(node.id, it->second, ProjectDependencyKind::Includes); } catch (...) {}
                }
            }
        }
    }

    if (!sourceNodes.empty()) {
        graph.addTarget("core", ProjectTargetKind::Library, sourceNodes);
        contract.buildTargets.push_back("core");
    }
    if (!mainNodeId.empty()) {
        for (const auto& srcId : sourceNodes) {
            try { graph.addDependency(mainNodeId, srcId, ProjectDependencyKind::Links); } catch (...) {}
        }
        graph.addTarget("app", ProjectTargetKind::Executable, {mainNodeId});
        contract.buildTargets.push_back("app");
        std::string expectedOut;
        if (std::filesystem::is_regular_file(foreignRoot / "fixtures/expected.txt")) {
            expectedOut = readFileString(foreignRoot / "fixtures/expected.txt");
        }
        if (!expectedOut.empty()) {
            contract.behaviors.push_back({"program-output", "app", "", expectedOut, "", 0});
        }
    }
    if (!testNodeId.empty()) {
        for (const auto& srcId : sourceNodes) {
            try { graph.addDependency(testNodeId, srcId, ProjectDependencyKind::Tests); } catch (...) {}
        }
        graph.addTarget("behavior", ProjectTargetKind::Test, {testNodeId});
        contract.buildTargets.push_back("behavior");
        contract.behaviors.push_back({"unit-tests", "behavior", "", "", "", 0});
    }

    return {std::move(graph), std::move(contract)};
}

// Synthesize missing C++20 function implementation in a foreign module based on its header declaration,
// struct fields in the module, call sites in tests/application, or transferable developmental principles!
std::optional<std::pair<std::string, TransferableDevelopmentalPrinciple>> synthesizeMissingModuleFunction(
    const DiscoveredSymbol& missingFn,
    const std::vector<DiscoveredSymbol>& allSymbols,
    const std::string& testAndAppSource,
    const std::vector<TransferableDevelopmentalPrinciple>& priorPrinciples,
    const std::vector<CrossLanguageAbiContract>& abiContracts,
    std::uint64_t genomeHash,
    std::uint64_t projectFingerprint,
    bool& outUsedTransferredPrinciple,
    std::string& outAppliedPrincipleId) {

    outUsedTransferredPrinciple = false;
    outAppliedPrincipleId.clear();

    // Extract namespace if any
    std::string nsName;
    if (auto pos = missingFn.qualifiedName.rfind("::"); pos != std::string::npos) {
        nsName = missingFn.qualifiedName.substr(0, pos);
    }

    // Build parameter list string (normalize AST's "int64_t" back to C++20 "long long" so no <cstdint> is required)
    auto normCppType = [](const std::string& t) -> std::string {
        return t == "int64_t" ? "long long" : t;
    };
    const std::string retType = normCppType(missingFn.returnType);
    std::ostringstream sigStream;
    sigStream << retType << " " << missingFn.name << "(";
    for (std::size_t i = 0; i < missingFn.parametersOrFields.size(); ++i) {
        if (i > 0) sigStream << ", ";
        sigStream << normCppType(missingFn.parametersOrFields[i].first) << " "
                  << (missingFn.parametersOrFields[i].second.empty() ? ("p" + std::to_string(i)) : missingFn.parametersOrFields[i].second);
    }
    sigStream << ")";

    // Prefer semantics from an accessible, type-compatible foreign-language
    // primary source over induction from a finite set of C++ test examples.
    for(const auto& contract:abiContracts) {
        if(contract.functionName!=missingFn.name||!contract.semanticallyDerivable||
           canonicalAbiType(missingFn.returnType)!=contract.returnType||
           missingFn.parametersOrFields.size()!=contract.parameters.size())continue;
        bool typesMatch=true;
        for(std::size_t i=0;i<contract.parameters.size();++i)
            if(canonicalAbiType(missingFn.parametersOrFields[i].first)!=contract.parameters[i].first)typesMatch=false;
        if(!typesMatch)continue;
        std::ostringstream code;
        code<<"extern \"C\" "<<sigStream.str()<<" {\n    return "<<contract.cppExpression<<";\n}\n";
        TransferableDevelopmentalPrinciple prin{genomeHash,projectFingerprint,0,
            "principle:cross_language_scalar_abi:"+contract.functionName,"cross_language_abi_bridge",
            "scalar_abi:"+std::to_string(contract.parameters.size())+"->"+contract.returnType,
            contract.cppExpression,"Synthesize a type-matched scalar C ABI from bounded "+contract.sourceLanguage+" source semantics"};
        return std::make_pair(code.str(),prin);
    }

    // Check if a transferable developmental principle from a previous foreign project matches this structural signature!
    std::string triggerSig = "unimplemented_fn:" + std::to_string(missingFn.parametersOrFields.size()) + "_params->" + retType;
    for (const auto& p : priorPrinciples) {
        if (p.genomeHash == genomeHash && p.patternCategory == "interface_synthesis" &&
            p.triggerSignature == triggerSig && !p.actionTemplate.empty()) {
            // Apply transferable principle!
            // E.g. actionTemplate == "weighted_pair_sum" or "struct_field_manhattan" or "clamp_range"
            if (p.actionTemplate == "struct_field_weighted_sum" && missingFn.parametersOrFields.size() == 1) {
                const std::string pName = missingFn.parametersOrFields[0].second.empty() ? "p0" : missingFn.parametersOrFields[0].second;
                const std::string pType = missingFn.parametersOrFields[0].first;
                // Find struct fields
                for (const auto& s : allSymbols) {
                    if (s.kind == "type" && (s.name == pType || s.qualifiedName == pType) && s.parametersOrFields.size() == 2) {
                        outUsedTransferredPrinciple = true;
                        outAppliedPrincipleId = p.principleId;
                        std::string f1 = s.parametersOrFields[0].second;
                        std::string f2 = s.parametersOrFields[1].second;
                        std::ostringstream code;
                        if (!nsName.empty()) code << "namespace " << nsName << " {\n";
                        code << sigStream.str() << " {\n    return " << pName << "." << f1 << " + " << pName << "." << f2 << ";\n}\n";
                        if (!nsName.empty()) code << "} // namespace " << nsName << "\n";
                        return std::make_pair(code.str(), p);
                    }
                }
            } else if (p.actionTemplate == "binary_linear_combination_2x_plus_y" && missingFn.parametersOrFields.size() == 2) {
                const std::string p0 = missingFn.parametersOrFields[0].second.empty() ? "p0" : missingFn.parametersOrFields[0].second;
                const std::string p1 = missingFn.parametersOrFields[1].second.empty() ? "p1" : missingFn.parametersOrFields[1].second;
                outUsedTransferredPrinciple = true;
                outAppliedPrincipleId = p.principleId;
                std::ostringstream code;
                if (!nsName.empty()) code << "namespace " << nsName << " {\n";
                code << sigStream.str() << " {\n    return 2 * " << p0 << " + " << p1 << ";\n}\n";
                if (!nsName.empty()) code << "} // namespace " << nsName << "\n";
                return std::make_pair(code.str(), p);
            }
        }
    }

    // Otherwise, induce the missing function's behavior from call-site assertions in tests/app (`fn(args) == expected`)!
    // Look for patterns like `missingFn.name(a, b) == expected` or `missingFn.name(Type{a, b}) == expected`
    std::vector<BehavioralExample> extractedSamples;
    std::size_t searchPos = 0;
    while ((searchPos = testAndAppSource.find(missingFn.name + "(", searchPos)) != std::string::npos) {
        if (searchPos > 0 && (std::isalnum(static_cast<unsigned char>(testAndAppSource[searchPos - 1])) || testAndAppSource[searchPos - 1] == '_')) {
            searchPos += missingFn.name.size();
            continue;
        }
        std::size_t openParen = searchPos + missingFn.name.size();
        int depth = 1;
        std::size_t cur = openParen + 1;
        while (cur < testAndAppSource.size() && depth > 0) {
            if (testAndAppSource[cur] == '(') depth++;
            else if (testAndAppSource[cur] == ')') depth--;
            cur++;
        }
        if (depth == 0) {
            std::string argsText = testAndAppSource.substr(openParen + 1, cur - openParen - 2);
            // A direct equality or the conventional test guard
            // `if (fn(...) != expected) return ...` both encode the same
            // behavioral sample for synthesis.
            std::size_t afterCall = cur;
            while (afterCall < testAndAppSource.size() && std::isspace(static_cast<unsigned char>(testAndAppSource[afterCall]))) afterCall++;
            if (afterCall + 2 <= testAndAppSource.size() &&
                (testAndAppSource.substr(afterCall, 2) == "==" || testAndAppSource.substr(afterCall, 2) == "!=")) {
                std::size_t rhsStart = afterCall + 2;
                while (rhsStart < testAndAppSource.size() && std::isspace(static_cast<unsigned char>(testAndAppSource[rhsStart]))) rhsStart++;
                std::size_t rhsEnd = rhsStart;
                if (rhsEnd < testAndAppSource.size() && (testAndAppSource[rhsEnd] == '-' || testAndAppSource[rhsEnd] == '+')) rhsEnd++;
                while (rhsEnd < testAndAppSource.size() && std::isdigit(static_cast<unsigned char>(testAndAppSource[rhsEnd]))) rhsEnd++;
                if (rhsEnd > rhsStart) {
                    try {
                        std::int64_t expVal = std::stoll(testAndAppSource.substr(rhsStart, rhsEnd - rhsStart));
                        // Parse comma-separated numbers or struct literal `{a, b}`
                        std::vector<std::int64_t> nums;
                        for (std::size_t i = 0; i < argsText.size();) {
                            if (argsText[i] == '-' || std::isdigit(static_cast<unsigned char>(argsText[i]))) {
                                std::size_t j = i + 1;
                                while (j < argsText.size() && std::isdigit(static_cast<unsigned char>(argsText[j]))) j++;
                                nums.push_back(std::stoll(argsText.substr(i, j - i)));
                                i = j;
                            } else {
                                ++i;
                            }
                        }
                        if (!nums.empty()) {
                            BehavioralExample ex;
                            ex.arguments = nums;
                            ex.expected = expVal;
                            extractedSamples.push_back(std::move(ex));
                        }
                    } catch (...) {}
                }
            }
        }
        searchPos = cur;
    }

    // Case 1: Single struct parameter with 2 fields
    if (missingFn.parametersOrFields.size() == 1) {
        const std::string pType = missingFn.parametersOrFields[0].first;
        const std::string pName = missingFn.parametersOrFields[0].second.empty() ? "item" : missingFn.parametersOrFields[0].second;
        for (const auto& s : allSymbols) {
            if (s.kind == "type" && (s.name == pType || s.qualifiedName == pType) && s.parametersOrFields.size() == 2) {
                std::string f1 = s.parametersOrFields[0].second;
                std::string f2 = s.parametersOrFields[1].second;
                // Check candidate expressions over (f1, f2): f1 + f2, f1 * f2, 2*f1 + f2, f1*f1 + f2*f2
                struct Cand { std::string expr; std::string tplName; std::function<std::int64_t(std::int64_t, std::int64_t)> fn; };
                std::vector<Cand> cands = {
                    {pName + "." + f1 + " + " + pName + "." + f2, "struct_field_weighted_sum", [](auto a, auto b) { return a + b; }},
                    {pName + "." + f1 + " * " + pName + "." + f2, "struct_field_product", [](auto a, auto b) { return a * b; }},
                    {"2 * " + pName + "." + f1 + " + " + pName + "." + f2, "struct_field_2x_plus_y", [](auto a, auto b) { return 2 * a + b; }},
                    {pName + "." + f1 + " * " + pName + "." + f1 + " + " + pName + "." + f2 + " * " + pName + "." + f2, "struct_field_norm_sq", [](auto a, auto b) { return a * a + b * b; }}
                };
                for (const auto& c : cands) {
                    bool fitsAll = true;
                    for (const auto& ex : extractedSamples) {
                        if (ex.arguments.size() == 2 && c.fn(ex.arguments[0], ex.arguments[1]) != ex.expected) {
                            fitsAll = false;
                            break;
                        }
                    }
                    if (fitsAll) {
                        std::ostringstream code;
                        if (!nsName.empty()) code << "namespace " << nsName << " {\n";
                        code << sigStream.str() << " {\n    return " << c.expr << ";\n}\n";
                        if (!nsName.empty()) code << "} // namespace " << nsName << "\n";
                        TransferableDevelopmentalPrinciple prin{
                            genomeHash, projectFingerprint, 0,
                            "principle:interface_synthesis:" + c.tplName,
                            "interface_synthesis",
                            triggerSig,
                            c.tplName,
                            "Synthesize unimplemented 2-field struct metric function via verified algebraic field combination (" + c.tplName + ")"
                        };
                        return std::make_pair(code.str(), prin);
                    }
                }
            }
        }
    }

    // Case 2: Two scalar parameters (a, b)
    if (missingFn.parametersOrFields.size() == 2) {
        const std::string p0 = missingFn.parametersOrFields[0].second.empty() ? "a" : missingFn.parametersOrFields[0].second;
        const std::string p1 = missingFn.parametersOrFields[1].second.empty() ? "b" : missingFn.parametersOrFields[1].second;
        struct Cand2 { std::string expr; std::string tplName; std::function<std::int64_t(std::int64_t, std::int64_t)> fn; };
        std::vector<Cand2> cands = {
            {"2 * " + p0 + " + " + p1, "binary_linear_combination_2x_plus_y", [](auto a, auto b) { return 2 * a + b; }},
            {p0 + " + " + p1, "binary_sum", [](auto a, auto b) { return a + b; }},
            {p0 + " * " + p1, "binary_product", [](auto a, auto b) { return a * b; }},
            {p0 + " * " + p0 + " + " + p1 + " * " + p1, "binary_sum_squares", [](auto a, auto b) { return a * a + b * b; }},
            {"(" + p0 + " > " + p1 + " ? " + p0 + " : " + p1 + ")", "binary_max", [](auto a, auto b) { return a > b ? a : b; }}
        };
        for (const auto& c : cands) {
            bool fitsAll = !extractedSamples.empty();
            for (const auto& ex : extractedSamples) {
                if (ex.arguments.size() == 2 && c.fn(ex.arguments[0], ex.arguments[1]) != ex.expected) {
                    fitsAll = false;
                    break;
                }
            }
            if (fitsAll) {
                std::ostringstream code;
                if (!nsName.empty()) code << "namespace " << nsName << " {\n";
                code << sigStream.str() << " {\n    return " << c.expr << ";\n}\n";
                if (!nsName.empty()) code << "} // namespace " << nsName << "\n";
                TransferableDevelopmentalPrinciple prin{
                    genomeHash, projectFingerprint, 0,
                    "principle:interface_synthesis:" + c.tplName,
                    "interface_synthesis",
                    triggerSig,
                    c.tplName,
                    "Synthesize unimplemented binary scalar function via call-site behavioral contract induction (" + c.tplName + ")"
                };
                return std::make_pair(code.str(), prin);
            }
        }
    }

    return std::nullopt;
}

} // namespace

// =========================================================================
// ProjectMemoryStore (GDIPM1) Implementation
// =========================================================================

std::vector<ForeignProjectKnowledgeRecord> ProjectMemoryStore::load() const {
    std::vector<ForeignProjectKnowledgeRecord> result;
    if (path_.empty() || !std::filesystem::exists(path_)) return result;
    std::ifstream in(path_, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open ProjectMemoryStore: " + path_.string());
    std::string line;
    std::uint64_t previous = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto fields = splitPipes(line);
        if (fields.size() < 11 || fields[0] != "GDIPM1") {
            throw std::runtime_error("Corrupt ProjectMemoryStore record header");
        }
        const auto sep = line.rfind('|');
        const auto recordHash = fingerprint(line.substr(0, sep));
        if (parseU64(fields.back()) != recordHash) {
            throw std::runtime_error("ProjectMemoryStore hash mismatch (tampering detected)");
        }
        const auto genomeHash = parseU64(fields[1]);
        const auto projFp = parseU64(fields[2]);
        const auto projName = hexDecode(fields[3]);
        const auto graphHash = parseU64(fields[4]);
        const auto count = parseU64(fields[5]);
        if (count < 1 || count > 512 || fields.size() != 8 + count * 3) {
            throw std::runtime_error("Corrupt ProjectMemoryStore group count");
        }
        const auto prevIdx = 6 + count * 3;
        if (parseU64(fields[prevIdx]) != previous) {
            throw std::runtime_error("ProjectMemoryStore hash chain broken");
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto off = 6 + i * 3;
            ForeignProjectKnowledgeRecord rec;
            rec.genomeHash = genomeHash;
            rec.projectFingerprint = projFp;
            rec.projectName = projName;
            rec.verifiedGraphHash = graphHash;
            rec.category = hexDecode(fields[off]);
            rec.symbolName = hexDecode(fields[off + 1]);
            rec.details = hexDecode(fields[off + 2]);
            result.push_back(std::move(rec));
        }
        previous = recordHash;
    }
    return result;
}

std::vector<ForeignProjectKnowledgeRecord> ProjectMemoryStore::loadForProject(std::uint64_t projectFingerprint) const {
    std::vector<ForeignProjectKnowledgeRecord> filtered;
    for (auto& rec : load()) {
        if (rec.projectFingerprint == projectFingerprint) {
            filtered.push_back(std::move(rec));
        }
    }
    return filtered;
}

void ProjectMemoryStore::appendGroup(const std::vector<ForeignProjectKnowledgeRecord>& records) const {
    if (records.empty() || path_.empty()) return;
    const auto existing = load();
    std::uint64_t previous = 0;
    if (!existing.empty()) {
        std::ifstream in(path_, std::ios::binary);
        std::string line, last;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last = line;
        }
        previous = parseU64(splitPipes(last).back());
    }
    const auto& first = records.front();
    std::string payload = "GDIPM1|" + std::to_string(first.genomeHash) + "|" +
                          std::to_string(first.projectFingerprint) + "|" +
                          hexEncode(first.projectName) + "|" +
                          std::to_string(first.verifiedGraphHash) + "|" +
                          std::to_string(records.size());
    for (const auto& rec : records) {
        payload += "|" + hexEncode(rec.category) + "|" + hexEncode(rec.symbolName) + "|" + hexEncode(rec.details);
    }
    payload += "|" + std::to_string(previous);
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
    std::ofstream out(path_, std::ios::binary | std::ios::app);
    if (!out || !(out << payload << "|" << fingerprint(payload) << "\n")) {
        throw std::runtime_error("Cannot append to ProjectMemoryStore");
    }
}

// =========================================================================
// DevelopmentalPrincipleStore (GDIDP1) Implementation
// =========================================================================

std::vector<TransferableDevelopmentalPrinciple> DevelopmentalPrincipleStore::load() const {
    std::vector<TransferableDevelopmentalPrinciple> result;
    if (path_.empty() || !std::filesystem::exists(path_)) return result;
    std::ifstream in(path_, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open DevelopmentalPrincipleStore: " + path_.string());
    std::string line;
    std::uint64_t previous = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto fields = splitPipes(line);
        if (fields.size() < 12 || fields[0] != "GDIDP1") {
            throw std::runtime_error("Corrupt DevelopmentalPrincipleStore record header");
        }
        const auto sep = line.rfind('|');
        const auto recordHash = fingerprint(line.substr(0, sep));
        if (parseU64(fields.back()) != recordHash) {
            throw std::runtime_error("DevelopmentalPrincipleStore hash mismatch (tampering detected)");
        }
        const auto genomeHash = parseU64(fields[1]);
        const auto origFp = parseU64(fields[2]);
        const auto evHash = parseU64(fields[3]);
        const auto count = parseU64(fields[4]);
        if (count < 1 || count > 256 || fields.size() != 7 + count * 5) {
            throw std::runtime_error("Corrupt DevelopmentalPrincipleStore group size");
        }
        const auto prevIdx = 5 + count * 5;
        if (parseU64(fields[prevIdx]) != previous) {
            throw std::runtime_error("DevelopmentalPrincipleStore hash chain broken");
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto off = 5 + i * 5;
            TransferableDevelopmentalPrinciple p;
            p.genomeHash = genomeHash;
            p.originProjectFingerprint = origFp;
            p.verifiedEvidenceHash = evHash;
            p.principleId = hexDecode(fields[off]);
            p.patternCategory = hexDecode(fields[off + 1]);
            p.triggerSignature = hexDecode(fields[off + 2]);
            p.actionTemplate = hexDecode(fields[off + 3]);
            p.rationale = hexDecode(fields[off + 4]);
            result.push_back(std::move(p));
        }
        previous = recordHash;
    }
    return result;
}

void DevelopmentalPrincipleStore::appendGroup(const std::vector<TransferableDevelopmentalPrinciple>& principles) const {
    if (principles.empty() || path_.empty()) return;
    const auto existing = load();
    std::uint64_t previous = 0;
    if (!existing.empty()) {
        std::ifstream in(path_, std::ios::binary);
        std::string line, last;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last = line;
        }
        previous = parseU64(splitPipes(last).back());
    }
    const auto& first = principles.front();
    std::string payload = "GDIDP1|" + std::to_string(first.genomeHash) + "|" +
                          std::to_string(first.originProjectFingerprint) + "|" +
                          std::to_string(first.verifiedEvidenceHash) + "|" +
                          std::to_string(principles.size());
    for (const auto& p : principles) {
        payload += "|" + hexEncode(p.principleId) + "|" + hexEncode(p.patternCategory) + "|" +
                   hexEncode(p.triggerSignature) + "|" + hexEncode(p.actionTemplate) + "|" +
                   hexEncode(p.rationale);
    }
    payload += "|" + std::to_string(previous);
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
    std::ofstream out(path_, std::ios::binary | std::ios::app);
    if (!out || !(out << payload << "|" << fingerprint(payload) << "\n")) {
        throw std::runtime_error("Cannot append to DevelopmentalPrincipleStore");
    }
}

// =========================================================================
// Read-Only Directory Tree Hash & Foreign C++20 Project Analysis
// =========================================================================

std::uint64_t computeDirectoryTreeHash(const std::filesystem::path& rootDir) {
    if (rootDir.empty() || !std::filesystem::exists(rootDir)) {
        throw std::invalid_argument("Foreign project root directory does not exist: " + rootDir.string());
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    std::ostringstream combined;
    for (const auto& f : files) {
        const auto rel = std::filesystem::relative(f, rootDir).generic_string();
        combined << rel << ":" << fingerprint(readFileString(f)) << ";";
    }
    return fingerprint(combined.str());
}

ForeignProjectAnalysisReport analyzeForeignCpp20Project(
    const std::filesystem::path& foreignProjectRoot,
    const std::filesystem::path& projectMemoryPath,
    const std::filesystem::path& developmentalMemoryPath) {

    if (foreignProjectRoot.empty() || !std::filesystem::is_directory(foreignProjectRoot)) {
        throw std::invalid_argument("Foreign project root must be an existing directory: " + foreignProjectRoot.string());
    }

    auto sys = GenomeBootstrap::buildCompleteSystem();
    const auto sourceGenome = Cpp20SourceGenome::fromGenome(*sys.strand);

    ForeignProjectAnalysisReport report;
    report.projectName = foreignProjectRoot.filename().string();
    report.originalRoot = std::filesystem::absolute(foreignProjectRoot);
    report.originalDirectoryHash = computeDirectoryTreeHash(foreignProjectRoot);
    report.abiContracts = discoverAbiContracts(foreignProjectRoot);
    report.immutableGenomeHash = sys.strand->getGenomeHash();

    const auto loaded = discoverOrLoadForeignProject(foreignProjectRoot, sourceGenome);
    report.discoveredFiles = loaded.graph.nodes().size();

    // Scan foreignProjectRoot for primary .kr sources vs generated KR_BUILD artifacts
    for (const auto& entry : std::filesystem::recursive_directory_iterator(foreignProjectRoot)) {
        if (!entry.is_regular_file()) continue;
        const auto rel = std::filesystem::relative(entry.path(), foreignProjectRoot).generic_string();
        if (entry.path().extension() == ".kr") {
            report.krSourceFiles.push_back(rel);
        } else if (rel.find("KR_BUILD/") != std::string::npos || rel.rfind("KR_BUILD", 0) == 0) {
            report.generatedKrBuildFiles.push_back(rel);
        }
    }
    std::sort(report.krSourceFiles.begin(), report.krSourceFiles.end());
    std::sort(report.generatedKrBuildFiles.begin(), report.generatedKrBuildFiles.end());
    report.discoveredKrSourceFiles = report.krSourceFiles.size();
    report.discoveredGeneratedKrBuildFiles = report.generatedKrBuildFiles.size();

    if (!report.generatedKrBuildFiles.empty()) {
        ArchitecturalHypothesis krHyp;
        krHyp.id = "hyp_kr_build_separation";
        krHyp.kind = ArchitecturalHypothesisKind::GeneratedKrBuildSeparation;
        krHyp.targetModule = "KR_BUILD";
        krHyp.targetSymbol = "KR_BUILD/*";
        krHyp.hypothesisStatement = "Projekt enthaelt " + std::to_string(report.discoveredGeneratedKrBuildFiles) +
            " generierte C++20-Dateien in KR_BUILD/ (Primaerquellen: " + std::to_string(report.discoveredKrSourceFiles) +
            " .kr-Dateien). KR_BUILD wird vom KR-Compiler neu erzeugt und darf nicht direkt ueberschrieben werden.";
        krHyp.investigationMethod = "Trenne primaere .kr-Quellen und generierte KR_BUILD-C++20-Artefakte über Quellkommentar-Herkunftsanalyse.";
        krHyp.derivedDevelopmentGoal = "Verifiziere C++20-Reparaturen ausschließlich in der isolierten Arbeitskopie und ordne Befunde den ursprünglichen .kr-Modulen zu.";
        krHyp.confidence = 0.99;
        report.hypotheses.push_back(std::move(krHyp));
    }

    // Group nodes into logical modules (header + source pairs) and parse ASTs with Cpp20Parser + SemanticAnalyzer
    SemanticAnalyzer semAnalyzer(sys.ruleEngine.get());
    std::vector<std::string> testCalledFunctions;
    std::string combinedTestAndAppSource;

    std::map<std::string, DiscoveredModuleInterface> moduleMap;
    // Pass 1: Headers first so declarations & types are registered before sources and tests
    for (const auto passKind : {ProjectNodeKind::Header, ProjectNodeKind::Source, ProjectNodeKind::Test}) {
        for (const auto& node : loaded.graph.nodes()) {
            if (node.kind != passKind) continue;
            const std::string rawSrc = loaded.graph.nodeContent(node.id);
            const std::string relPath = node.artifact.relativePath.generic_string();
            const std::string stem = node.artifact.relativePath.stem().string();
            const bool isHeader = (node.kind == ProjectNodeKind::Header);
            const bool isTest = (node.kind == ProjectNodeKind::Test);
            const std::string originKr = extractKrOriginPathFromComments(rawSrc);
            const bool isGenKr = (!originKr.empty() || relPath.find("KR_BUILD/") != std::string::npos ||
                                  rawSrc.find("Automatisch generiert") != std::string::npos);

            if (isTest || rawSrc.find("int main") != std::string::npos) {
                combinedTestAndAppSource += rawSrc + "\n";
            }

            if (isHeader) {
                auto& mod = moduleMap[stem];
                mod.moduleName = stem;
                mod.headerNodeId = node.id;
                if (!originKr.empty()) mod.originKrPath = originKr;
                if (isGenKr) mod.isGeneratedKrBuildArtifact = true;
                mod.includedHeaders = extractIncludedHeaders(rawSrc);
            } else if (node.kind == ProjectNodeKind::Source && rawSrc.find("int main") == std::string::npos) {
                auto& mod = moduleMap[stem];
                if (mod.moduleName.empty()) mod.moduleName = stem;
                mod.sourceNodeId = node.id;
                if (!originKr.empty()) mod.originKrPath = originKr;
                if (isGenKr) mod.isGeneratedKrBuildArtifact = true;
                auto incs = extractIncludedHeaders(rawSrc);
                mod.includedHeaders.insert(mod.includedHeaders.end(), incs.begin(), incs.end());
            }

            // 1. Parse with Cpp20Parser::parseWithGenome
            try {
                const std::string cleaned = prepareSourceForGenomicAst(rawSrc);
                auto tu = Cpp20Parser::parseWithGenome(cleaned, *sys.ruleEngine);
                if (tu) {
                    collectSymbolsFromAst(*tu, "", node.id, relPath, isHeader, isTest, report.symbols, testCalledFunctions);
                    if (!semAnalyzer.analyze(*tu)) {
                        for (const auto& d : semAnalyzer.getDiagnostics()) {
                            report.semanticDiagnostics.push_back(relPath + ": " + d.message);
                        }
                    }
                }
            } catch (const std::exception& e) {
                report.semanticDiagnostics.push_back(relPath + ": parser diagnostic: " + e.what());
            }

            // 2. Supplement with resilient token-level C++20 symbol & call-graph extraction so real-world
            //    translation units (raw string literals, #ifdefs, extern "C", Win32/SDL3) have 100% symbol coverage!
            extractResilientCpp20Symbols(rawSrc, node.id, relPath, originKr, isGenKr, isHeader, isTest, report.symbols, testCalledFunctions);

            // Detect static defects directly in source:
            // (a) Missing <iostream> when std::cout is used
            if ((rawSrc.find("std::cout") != std::string::npos || rawSrc.find("std::cerr") != std::string::npos) &&
                rawSrc.find("<iostream>") == std::string::npos) {
                ArchitecturalHypothesis hyp;
                hyp.id = "hyp_missing_header_iostream_" + node.id;
                hyp.kind = ArchitecturalHypothesisKind::MissingStandardHeader;
                hyp.targetModule = stem;
                hyp.targetSymbol = "std::cout";
                hyp.hypothesisStatement = "Knoten '" + node.id + "' (" + relPath + ") nutzt std::cout ohne #include <iostream>.";
                hyp.investigationMethod = "Kompilierungsprobe & genomische Header-Injektionsregel pruefen.";
                hyp.derivedDevelopmentGoal = "Fuege genomisch autorisiertes #include <iostream> in '" + node.id + "' ein und verifiziere den Build.";
                hyp.confidence = 0.96;
                report.hypotheses.push_back(std::move(hyp));
            }
            // (b) Deprecated C++20 API usage: std::filesystem::u8path / fs::u8path (-Werror=deprecated-declarations)
            if (rawSrc.find("u8path(") != std::string::npos) {
                ArchitecturalHypothesis hyp;
                hyp.id = "hyp_deprecated_u8path_" + node.id;
                hyp.kind = ArchitecturalHypothesisKind::DeprecatedCpp20ApiUsage;
                hyp.targetModule = stem;
                hyp.targetSymbol = "std::filesystem::u8path";
                hyp.hypothesisStatement = "Knoten '" + node.id + "' (" + relPath +
                    (originKr.empty() ? "" : (" [aus " + originKr + "]")) +
                    ") verwendet die in C++20 als veraltet markierte API 'filesystem::u8path' (-Werror=deprecated-declarations).";
                hyp.investigationMethod = "Pruefe UTF-8-Pfadkonstruktion unter C++20 (char8_t / std::u8string) fuer Windows- und POSIX-Kompatibilitaet.";
                hyp.derivedDevelopmentGoal = "Ersetze 'u8path(expr)' in der isolierten Projektkopie durch C++20-konformes 'path(std::u8string(reinterpret_cast<const char8_t*>((expr).data()), (expr).size()))' und verifiziere Kompilierung & Verhalten.";
                hyp.confidence = 0.98;
                report.hypotheses.push_back(std::move(hyp));
            }
        }
    }

    // Mark tested symbols and record dataflow edges
    for (auto& sym : report.symbols) {
        if (std::find(testCalledFunctions.begin(), testCalledFunctions.end(), sym.name) != testCalledFunctions.end()) {
            sym.isTested = true;
        }
        for (const auto& callee : sym.calledFunctions) {
            report.dataflowEdges.push_back(sym.name + " -> " + callee);
        }
    }

    // Populate module interfaces and formulate architectural hypotheses simultaneously!
    for (auto& [mName, mod] : moduleMap) {
        for (const auto& sym : report.symbols) {
            const auto symStem = std::filesystem::path(sym.definingPath).stem().string();
            if (symStem == mName || sym.definingNodeId == mod.headerNodeId || sym.definingNodeId == mod.sourceNodeId) {
                if (sym.kind == "type" || sym.kind == "enum") {
                    mod.exportedTypes.push_back(sym.name);
                } else if (sym.kind == "function" || sym.kind == "template_function") {
                    mod.exportedFunctions.push_back(sym.name);
                    if (sym.hasDeclarationInHeader && !sym.hasDefinitionInSource && sym.kind == "function") {
                        mod.unimplementedDeclarations.push_back(sym.name);
                        ArchitecturalHypothesis hyp;
                        hyp.id = "hyp_unimplemented_fn_" + sym.name;
                        hyp.kind = ArchitecturalHypothesisKind::UnimplementedInterfaceContract;
                        hyp.targetModule = mName;
                        hyp.targetSymbol = sym.name;
                        hyp.hypothesisStatement = "Modulschnittstelle '" + mName + "' deklariert '" + sym.name +
                            "', besitzt aber keine Definition in der Implementierungsdatei.";
                        hyp.investigationMethod = "Untersuche Aufrufstellen/Tests und Modultypen zur verhaltensbasierten Funktionssynthese.";
                        hyp.derivedDevelopmentGoal = "Synthetisiere und integriere eine C++20-Implementierung fuer '" +
                            sym.qualifiedName + "' und verifiziere alle Modul- und Verhaltenstests.";
                        hyp.confidence = 0.95;
                        report.hypotheses.push_back(std::move(hyp));
                    } else if (sym.hasDefinitionInSource && !sym.isTested && sym.name != "main") {
                        mod.untestedFunctions.push_back(sym.name);
                        ArchitecturalHypothesis hyp;
                        hyp.id = "hyp_untested_fn_" + sym.name;
                        hyp.kind = ArchitecturalHypothesisKind::UntestedPublicCapability;
                        hyp.targetModule = mName;
                        hyp.targetSymbol = sym.name;
                        hyp.hypothesisStatement = "Funktion '" + sym.name + "' im Modul '" + mName + "' ist implementiert, wird aber in keinem Test geprueft.";
                        hyp.investigationMethod = "Leite Eingabe-/Ausgabevertrag aus Signatur und Datenfluss ab.";
                        hyp.derivedDevelopmentGoal = "Erweitere die Verifikationssuite um unabhängige Verhaltenstests fuer '" + sym.name + "'.";
                        hyp.confidence = 0.85;
                        report.hypotheses.push_back(std::move(hyp));
                    }
                }
            }
        }
        report.modules.push_back(std::move(mod));
    }

    // Check for missing link dependencies between executable/test nodes and library source nodes
    for (const auto& node : loaded.graph.nodes()) {
        if (node.kind == ProjectNodeKind::Source || node.kind == ProjectNodeKind::Test) {
            bool isAppOrTest = (node.kind == ProjectNodeKind::Test || loaded.graph.nodeContent(node.id).find("int main") != std::string::npos);
            if (!isAppOrTest) continue;
            for (const auto& mod : report.modules) {
                if (mod.sourceNodeId.empty()) continue;
                // Check if app/test includes mod's header but doesn't link mod.sourceNodeId
                bool includesMod = false;
                bool linksMod = false;
                for (const auto& dep : loaded.graph.dependencies()) {
                    if (dep.consumer == node.id && dep.provider == mod.headerNodeId && dep.kind == ProjectDependencyKind::Includes) {
                        includesMod = true;
                    }
                    if (dep.consumer == node.id && dep.provider == mod.sourceNodeId &&
                        (dep.kind == ProjectDependencyKind::Links || dep.kind == ProjectDependencyKind::Tests)) {
                        linksMod = true;
                    }
                }
                if (includesMod && !linksMod) {
                    ArchitecturalHypothesis hyp;
                    hyp.id = "hyp_missing_link_" + node.id + "_" + mod.sourceNodeId;
                    hyp.kind = ArchitecturalHypothesisKind::MissingModuleLinkDependency;
                    hyp.targetModule = mod.moduleName;
                    hyp.targetSymbol = mod.sourceNodeId;
                    hyp.hypothesisStatement = "Knoten '" + node.id + "' bindet Header von Modul '" + mod.moduleName +
                        "' ein, besitzt aber keine Link-Abhaengigkeit auf '" + mod.sourceNodeId + "'.";
                    hyp.investigationMethod = "Pruefe Linker-Aufloesung im Projektgraphen.";
                    hyp.derivedDevelopmentGoal = "Ergänze Link-Kante '" + node.id + " -> " + mod.sourceNodeId + "' im Projektgraphen.";
                    hyp.confidence = 0.94;
                    report.hypotheses.push_back(std::move(hyp));
                }
            }
        }
    }

    // Check prior ProjectMemoryStore & DevelopmentalPrincipleStore if provided
    std::size_t priorProjectFacts = 0;
    if (!projectMemoryPath.empty() && std::filesystem::exists(projectMemoryPath)) {
        try {
            priorProjectFacts = ProjectMemoryStore(projectMemoryPath).loadForProject(report.originalDirectoryHash).size();
        } catch (...) {}
    }
    std::size_t priorPrinciples = 0;
    if (!developmentalMemoryPath.empty() && std::filesystem::exists(developmentalMemoryPath)) {
        try {
            priorPrinciples = DevelopmentalPrincipleStore(developmentalMemoryPath).load().size();
        } catch (...) {}
    }

    // Build human-readable summary
    std::ostringstream sum;
    sum << "FREMDSYSTEM-ANALYSE & ENTWICKLUNGSPLANUNG (GDI Read-Only Isolation)\n"
        << "Projekt: " << report.projectName << "\n"
        << "Originalpfad: " << report.originalRoot.string() << "\n"
        << "Original-Verzeichnis-Hash (Unveraenderlichkeits-Anker): " << report.originalDirectoryHash << "\n"
        << "Erkannte Projektdateien: " << report.discoveredFiles
        << "    Erschlossene Module: " << report.modules.size()
        << "    Analysierte C++20-Symbole: " << report.symbols.size() << "\n"
        << "Gespeicherte Projektfakten (GDIPM1): " << priorProjectFacts
        << "    Uebertragbare Entwicklungsprinzipien (GDIDP1): " << priorPrinciples << "\n";

    for (const auto& m : report.modules) {
        sum << "  [Modul " << m.moduleName << "] Typen: " << m.exportedTypes.size()
            << ", Funktionen: " << m.exportedFunctions.size()
            << ", Offene Schnittstellen: " << m.unimplementedDeclarations.size() << "\n";
    }
    if (report.discoveredGeneratedKrBuildFiles > 0 || report.discoveredKrSourceFiles > 0) {
        sum << "  [KR-Separation] Primaere .kr-Quellen: " << report.discoveredKrSourceFiles
            << " | Generierte KR_BUILD-Dateien: " << report.discoveredGeneratedKrBuildFiles
            << " (Originale geschuetzt vor Writeback; Herkunftsanalyse aktiv)\n";
    }
    if (!report.hypotheses.empty()) {
        sum << "\nWaehrend der Analyse gebildete Architektur- & Entwicklungshypothesen (" << report.hypotheses.size() << "):\n";
        for (const auto& h : report.hypotheses) {
            sum << "  * [" << h.id << "] " << h.hypothesisStatement << "\n"
                << "    -> Abgeleitetes Entwicklungsziel: " << h.derivedDevelopmentGoal << "\n";
        }
    } else {
        sum << "\nAlle erkannten Modulschnittstellen sind implementiert; Projekt bereit fuer isolierte Build- und Sandbox-Verifikation.\n";
    }
    report.summaryText = sum.str();
    return report;
}

// =========================================================================
// Coupled Iterative Foreign Project Analysis & Development Loop
// =========================================================================

ForeignProjectDevelopmentResult runForeignProjectDevelopmentalLoop(
    const ForeignProjectDevelopmentConfig& config) {

    if (config.foreignProjectRoot.empty() || !std::filesystem::is_directory(config.foreignProjectRoot)) {
        throw std::invalid_argument("Foreign project root directory does not exist");
    }
    validateIsolatedWorkspace(config.foreignProjectRoot, config.isolatedWorkspace);
    const auto originalManifestBefore = createSha256DirectoryManifest(config.foreignProjectRoot);
    if (config.isolatedWorkspace.empty() ||
        (std::filesystem::exists(config.isolatedWorkspace) && !std::filesystem::is_empty(config.isolatedWorkspace))) {
        throw std::invalid_argument("Isolated workspace for foreign project development must be new or empty");
    }
    if (config.compiler.empty() || !std::filesystem::is_regular_file(config.compiler)) {
        throw std::invalid_argument("Foreign project development requires an existing C++20 compiler binary");
    }
    if (config.requireSandbox && (config.sandboxRunner.empty() || !std::filesystem::is_regular_file(config.sandboxRunner))) {
        throw std::invalid_argument("Foreign project development requires an existing sandbox runner (fail closed)");
    }

    std::filesystem::create_directories(config.isolatedWorkspace);

    auto sys = GenomeBootstrap::buildCompleteSystem();
    const auto initialGenomeHash = sys.strand->getGenomeHash();
    const auto sourceGenome = Cpp20SourceGenome::fromGenome(*sys.strand);

    ForeignProjectDevelopmentResult result;
    result.originalSha256Before = originalManifestBefore.sha256;
    result.immutableGenomeHash = initialGenomeHash;
    result.originalDirectoryHashBefore = computeDirectoryTreeHash(config.foreignProjectRoot);

    // 1. Initial simultaneous analysis + hypothesis generation on the read-only foreign project
    result.initialAnalysis = analyzeForeignCpp20Project(
        config.foreignProjectRoot, config.projectMemoryPath, config.developmentalMemoryPath);
    result.analysisSucceeded = true;

    // Load prior transferable principles from DevelopmentalPrincipleStore (GDIDP1)
    DevelopmentalPrincipleStore principleStore(config.developmentalMemoryPath);
    std::vector<TransferableDevelopmentalPrinciple> priorPrinciples;
    if (!config.developmentalMemoryPath.empty() && std::filesystem::exists(config.developmentalMemoryPath) &&
        std::filesystem::file_size(config.developmentalMemoryPath) > 0) {
        priorPrinciples = principleStore.load();
    }

    // Load working copy of the project graph into memory (isolated from original directory!)
    auto workingSpec = discoverOrLoadForeignProject(config.foreignProjectRoot, sourceGenome);
    workingSpec.contract.requireSandbox = config.requireSandbox;

    // Gather all test/app source text for call-site contract inspection
    std::string testAndAppSource;
    for (const auto& node : workingSpec.graph.nodes()) {
        const std::string nodeSrc = workingSpec.graph.nodeContent(node.id);
        if (node.kind == ProjectNodeKind::Test || nodeSrc.find("int main") != std::string::npos) {
            testAndAppSource += nodeSrc + "\n";
        }
    }

    // Iterative loop: Analyze -> Understand -> Hypothesize -> Derive Goal -> Implement -> Verify -> Learn
    std::size_t iteration = 0;
    bool verified = false;

    while (iteration < config.maxIterations && !verified) {
        ++iteration;
        ForeignProjectIterationReport iterReport;
        iterReport.iterationNumber = iteration;
        iterReport.graphHashBefore = workingSpec.graph.graphHash();

        for (const auto& h : result.initialAnalysis.hypotheses) {
            if (!h.resolvedAndVerified) {
                iterReport.activeHypotheses.push_back(h.id);
            }
        }

        // Step A: If our simultaneous AST/interface analysis found unimplemented module functions,
        // synthesize and integrate them into the isolated project graph!
        bool synthesizedMissingFunction = false;
        for (auto& hyp : result.initialAnalysis.hypotheses) {
            if (hyp.resolvedAndVerified) continue;
            if (hyp.kind == ArchitecturalHypothesisKind::UnimplementedInterfaceContract) {
                hyp.investigated = true;
                for (const auto& sym : result.initialAnalysis.symbols) {
                    if (sym.name == hyp.targetSymbol && sym.hasDeclarationInHeader && !sym.hasDefinitionInSource) {
                        bool usedTransfer = false;
                        std::string appliedPrincipleId;
                        auto synth = synthesizeMissingModuleFunction(
                            sym, result.initialAnalysis.symbols, testAndAppSource, priorPrinciples,
                            result.initialAnalysis.abiContracts,
                            initialGenomeHash, result.originalDirectoryHashBefore,
                            usedTransfer, appliedPrincipleId);
                        if (synth.has_value()) {
                            // Find the module's source node (or create one if header-only)
                            std::string targetSourceNodeId;
                            for (const auto& m : result.initialAnalysis.modules) {
                                if (m.moduleName == hyp.targetModule && !m.sourceNodeId.empty()) {
                                    targetSourceNodeId = m.sourceNodeId;
                                    break;
                                }
                            }
                            if (targetSourceNodeId.empty()) {
                                targetSourceNodeId = "src_" + hyp.targetModule;
                                std::string newSource = "// Automatisch synthetisierte C++20-Implementierung fuer Modul " + hyp.targetModule + "\n";
                                std::string headerInc;
                                for (const auto& m : result.initialAnalysis.modules) {
                                    if (m.moduleName == hyp.targetModule && !m.headerNodeId.empty()) {
                                        for (const auto& n : workingSpec.graph.nodes()) {
                                            if (n.id == m.headerNodeId) {
                                                headerInc = n.artifact.relativePath.filename().generic_string();
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (!headerInc.empty()) {
                                    newSource += "#include \"" + headerInc + "\"\n\n";
                                }
                                newSource += synth->first + "\n";
                                const std::filesystem::path srcRelPath = hyp.targetModule + ".cpp";
                                workingSpec.graph.addNode(targetSourceNodeId, ProjectNodeKind::Source, srcRelPath, newSource);
                                for (auto& m : result.initialAnalysis.modules) {
                                    if (m.moduleName == hyp.targetModule) {
                                        m.sourceNodeId = targetSourceNodeId;
                                        break;
                                    }
                                }
                            } else {
                                const std::string oldSource = workingSpec.graph.nodeContent(targetSourceNodeId);
                                const std::string updatedSource = oldSource + "\n" + synth->first;
                                workingSpec.graph.replaceNodeContent(targetSourceNodeId, updatedSource);
                            }
                            hyp.confirmed = true;
                            hyp.resolvedAndVerified = true;
                            synthesizedMissingFunction = true;
                            iterReport.targetedProbeOrGoal = hyp.derivedDevelopmentGoal;
                            iterReport.actionTaken = "Synthesized C++20 implementation for '" + sym.qualifiedName +
                                "' in node '" + targetSourceNodeId + "'" +
                                (usedTransfer ? (" [applied transferred principle " + appliedPrincipleId + "]") : " [induced from project contracts]");
                            iterReport.newlyDiscoveredFacts.push_back("Implemented missing function " + sym.qualifiedName);
                            if (usedTransfer) {
                                result.transferredPrincipleApplied = true;
                                result.appliedTransferredPrinciples.push_back(appliedPrincipleId);
                            } else {
                                result.learnedPrinciples.push_back(synth->second);
                                iterReport.newlyLearnedPrinciples.push_back(synth->second.principleId);
                            }
                        }
                    }
                }
                if (synthesizedMissingFunction) break;
            }
        }

        // Step A2: Check if prior transferable API modernization principles apply (e.g. modernize-deprecated-cpp20-api)
        const auto stableNodes = workingSpec.graph.nodes();
        for (const auto& p : priorPrinciples) {
            if (p.patternCategory == "api_modernization" && p.actionTemplate.find("modernize-deprecated-cpp20-api") != std::string::npos) {
                for (const auto& node : stableNodes) {
                    if (node.kind == ProjectNodeKind::Source || node.kind == ProjectNodeKind::Header) {
                        const std::string content = workingSpec.graph.nodeContent(node.id);
                        if (content.find("u8path(") != std::string::npos) {
                            VerifiedProjectTransformation tr;
                            tr.genomeHash = initialGenomeHash;
                            tr.originGraphHash = workingSpec.graph.graphHash();
                            tr.verifiedGraphHash = workingSpec.graph.graphHash();
                            tr.evidenceHash = p.verifiedEvidenceHash != 0 ? p.verifiedEvidenceHash : fingerprint(p.principleId);
                            tr.kind = ProjectTransformationKind::ReplaceExactToken;
                            tr.node = node.id;
                            tr.before = "u8path";
                            tr.after = "path(std::u8string(reinterpret_cast<const char8_t*>...))";
                            tr.auxiliary = "modernize-deprecated-cpp20-api";
                            workingSpec.graph = applyVerifiedTransformation(workingSpec.graph, tr);
                            result.transferredPrincipleApplied = true;
                            if (std::find(result.appliedTransferredPrinciples.begin(), result.appliedTransferredPrinciples.end(), p.principleId) == result.appliedTransferredPrinciples.end()) {
                                result.appliedTransferredPrinciples.push_back(p.principleId);
                            }
                            iterReport.actionTaken = "Applied transferred API modernization principle '" + p.principleId + "' to node '" + node.id + "'";
                            iterReport.targetedProbeOrGoal = "Modernize deprecated C++20 API usage in node '" + node.id + "'";
                            iterReport.newlyDiscoveredFacts.push_back("Modernized u8path to std::u8string in " + node.artifact.relativePath.generic_string());
                            for (auto& hyp : result.initialAnalysis.hypotheses) {
                                if (hyp.kind == ArchitecturalHypothesisKind::DeprecatedCpp20ApiUsage && hyp.targetModule == node.artifact.relativePath.stem().string()) {
                                    hyp.investigated = true;
                                    hyp.confirmed = true;
                                    hyp.resolvedAndVerified = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Step B: Verify current working graph (or run cumulative autonomous repair if compiler/linker/runtime fails)
        const auto iterWorkspace = config.isolatedWorkspace / ("iteration_" + std::to_string(iteration));
        const auto verifyEv = verifyGenerativeProject(
            workingSpec.graph, workingSpec.contract, iterWorkspace / "verify", config.compiler, config.sandboxRunner);

        if (verifyEv.success) {
            iterReport.compilationPassed = true;
            iterReport.behavioralTestsPassed = true;
            iterReport.graphHashAfter = workingSpec.graph.graphHash();
            if (iterReport.actionTaken.empty()) {
                iterReport.targetedProbeOrGoal = "Verify complete foreign project build and behavioral contracts in sandbox";
                iterReport.actionTaken = "Verified unmodified project graph in isolated sandbox";
            }
            verified = true;
            result.finalVerifiedGraphHash = workingSpec.graph.graphHash();
            result.isolatedProjectOutputDir = iterWorkspace / "verify" / "project";
            result.iterations.push_back(std::move(iterReport));
            break;
        }

        // Step C: If verification failed, check if we have remaining unimplemented functions first;
        // otherwise invoke experience-transferred or cumulative autonomous project repair!
        bool hasMoreUnimplemented = false;
        for (const auto& h : result.initialAnalysis.hypotheses) {
            if (!h.resolvedAndVerified && h.kind == ArchitecturalHypothesisKind::UnimplementedInterfaceContract) {
                hasMoreUnimplemented = true;
                break;
            }
        }
        if (synthesizedMissingFunction && hasMoreUnimplemented) {
            iterReport.graphHashAfter = workingSpec.graph.graphHash();
            result.iterations.push_back(std::move(iterReport));
            continue;
        }

        // Try repair experience transfer first if repairExperiencePath is provided
        AutonomousProjectConfig repairCfg{
            iterWorkspace / "autonomous_repair",
            config.compiler,
            config.sandboxRunner,
            4,
            config.repairExperiencePath
        };
        AutonomousProjectResult repairRes;
        if (!config.repairExperiencePath.empty() && std::filesystem::exists(config.repairExperiencePath) &&
            std::filesystem::file_size(config.repairExperiencePath) > 0) {
            AutonomousProjectConfig transferCfg = repairCfg;
            transferCfg.workspace = iterWorkspace / "experience_transfer";
            transferCfg.maxCycles = 1;
            transferCfg.experienceStore.clear();
            repairRes = runExperienceTransferredProjectDevelopment(
                workingSpec.graph, workingSpec.contract, transferCfg, ProjectExperienceStore(config.repairExperiencePath));
            if (repairRes.success && repairRes.experienceTransferred) {
                result.transferredPrincipleApplied = true;
                result.appliedTransferredPrinciples.push_back("repair_experience_group:" + std::to_string(repairRes.experienceOriginGraphHash));
            }
        }
        if (!repairRes.success) {
            repairRes = runCumulativeAutonomousProjectRepair(workingSpec.graph, workingSpec.contract, repairCfg);
        }

        if (repairRes.success && !repairRes.cycles.empty()) {
            // Re-apply learned transformations to workingSpec.graph so we hold the repaired graph
            for (const auto& tr : repairRes.transformations) {
                workingSpec.graph = applyVerifiedTransformation(workingSpec.graph, tr);
                // Also record a transferable developmental principle for each verified repair!
                std::string pCat = (tr.kind == ProjectTransformationKind::InsertHeader) ? "header_repair" :
                                   (tr.kind == ProjectTransformationKind::AddLinkDependency ? "link_topology" :
                                   (tr.auxiliary == "modernize-deprecated-cpp20-api" ? "api_modernization" : "type_bridge"));
                std::string pId = "principle:" + pCat + ":" + tr.auxiliary;
                TransferableDevelopmentalPrinciple prin{
                    initialGenomeHash,
                    result.originalDirectoryHashBefore,
                    tr.evidenceHash,
                    pId,
                    pCat,
                    "diagnostic_repair:" + pCat,
                    tr.auxiliary + ":" + tr.after,
                    "Transferable C++20 project repair principle (" + pCat + ") verified on foreign project"
                };
                result.learnedPrinciples.push_back(std::move(prin));
                iterReport.newlyLearnedPrinciples.push_back(pId);
                for (auto& hyp : result.initialAnalysis.hypotheses) {
                    if (hyp.kind == ArchitecturalHypothesisKind::DeprecatedCpp20ApiUsage &&
                        (tr.auxiliary == "modernize-deprecated-cpp20-api" || tr.before.find("u8path") != std::string::npos)) {
                        hyp.investigated = true;
                        hyp.confirmed = true;
                        hyp.resolvedAndVerified = true;
                    }
                }
            }
            iterReport.targetedProbeOrGoal = "Resolve compiler/linker/behavioral failure via autonomous hypothesis repair";
            iterReport.actionTaken = "Selected hypothesis '" + repairRes.selectedHypothesis + "' across " +
                std::to_string(repairRes.cycles.size()) + " repair cycle(s)";
            iterReport.compilationPassed = true;
            iterReport.behavioralTestsPassed = true;
            iterReport.graphHashAfter = workingSpec.graph.graphHash();
            verified = true;
            result.finalVerifiedGraphHash = workingSpec.graph.graphHash();
            result.isolatedProjectOutputDir = config.isolatedWorkspace / "verified_foreign_project";
            workingSpec.graph.materialize(result.isolatedProjectOutputDir);
            result.iterations.push_back(std::move(iterReport));
            break;
        } else {
            iterReport.targetedProbeOrGoal = "Attempted autonomous repair of foreign project failure";
            iterReport.actionTaken = "Repair cycles exhausted without passing all contracts";
            iterReport.graphHashAfter = workingSpec.graph.graphHash();
            result.iterations.push_back(std::move(iterReport));
            break;
        }
    }

    result.developmentVerified = verified;

    // Materialize final evolved project into isolated workspace if not already materialized
    if (result.isolatedProjectOutputDir.empty()) {
        result.isolatedProjectOutputDir = config.isolatedWorkspace / "isolated_project_snapshot";
        workingSpec.graph.materialize(result.isolatedProjectOutputDir);
    }

    // Re-analyze the evolved project in the isolated output directory to capture the updated architecture state
    try {
        result.finalAnalysis = analyzeForeignCpp20Project(
            result.isolatedProjectOutputDir, config.projectMemoryPath, config.developmentalMemoryPath);
    } catch (...) {
        result.finalAnalysis = result.initialAnalysis;
    }

    // Consolidate ProjectMemoryStore (GDIPM1): concrete facts discovered about THIS foreign project
    for (const auto& mod : result.initialAnalysis.modules) {
        result.projectMemoryRecords.push_back({
            initialGenomeHash,
            result.originalDirectoryHashBefore,
            result.initialAnalysis.projectName,
            "module",
            mod.moduleName,
            "types=" + std::to_string(mod.exportedTypes.size()) + ",functions=" + std::to_string(mod.exportedFunctions.size()) +
            ",unimplemented=" + std::to_string(mod.unimplementedDeclarations.size()),
            result.finalVerifiedGraphHash
        });
    }
    for (const auto& sym : result.initialAnalysis.symbols) {
        result.projectMemoryRecords.push_back({
            initialGenomeHash,
            result.originalDirectoryHashBefore,
            result.initialAnalysis.projectName,
            sym.kind,
            sym.qualifiedName,
            "return=" + sym.returnType + ",defined=" + (sym.hasDefinitionInSource ? "true" : "false") +
            ",tested=" + (sym.isTested ? "true" : "false"),
            result.finalVerifiedGraphHash
        });
    }
    for (const auto& hyp : result.initialAnalysis.hypotheses) {
        result.projectMemoryRecords.push_back({
            initialGenomeHash,
            result.originalDirectoryHashBefore,
            result.initialAnalysis.projectName,
            "hypothesis",
            hyp.id,
            hyp.hypothesisStatement + " | resolved=" + (hyp.resolvedAndVerified ? "true" : "false"),
            result.finalVerifiedGraphHash
        });
    }

    if (!config.projectMemoryPath.empty() && !result.projectMemoryRecords.empty()) {
        ProjectMemoryStore(config.projectMemoryPath).appendGroup(result.projectMemoryRecords);
    }

    // Consolidate DevelopmentalPrincipleStore (GDIDP1): generalizable principles that transfer to other projects!
    if (result.developmentVerified && !config.developmentalMemoryPath.empty() && !result.learnedPrinciples.empty()) {
        const auto evHash = fingerprint(std::to_string(result.finalVerifiedGraphHash) + "|" + result.initialAnalysis.projectName);
        for (auto& p : result.learnedPrinciples) {
            if (p.verifiedEvidenceHash == 0) p.verifiedEvidenceHash = evHash;
        }
        principleStore.appendGroup(result.learnedPrinciples);
    }

    // Safety Boundary Verification: verify that original foreign project directory was NOT modified!
    result.originalDirectoryHashAfter = computeDirectoryTreeHash(config.foreignProjectRoot);
    result.originalProjectUntouched = (result.originalDirectoryHashBefore == result.originalDirectoryHashAfter);
    const auto originalManifestAfter = createSha256DirectoryManifest(config.foreignProjectRoot);
    result.originalSha256After = originalManifestAfter.sha256;
    result.originalProjectUntouched = result.originalProjectUntouched &&
        originalManifestBefore.sha256 == originalManifestAfter.sha256;

    if (!config.allowWritebackToOriginal && !result.originalProjectUntouched) {
        throw std::runtime_error("Safety boundary violation: original foreign project directory was modified without authorization");
    }

    // Only if explicitly authorized (`allowWritebackToOriginal == true`) and verified, write back changes.
    // Notice: Generated KR_BUILD files are never written back to the original project directory,
    // because the KR compiler regenerates them from .kr sources!
    if (config.allowWritebackToOriginal && result.developmentVerified) {
        for (const auto& node : workingSpec.graph.nodes()) {
            const auto relStr = node.artifact.relativePath.generic_string();
            if (relStr.find("KR_BUILD/") != std::string::npos || relStr.rfind("KR_BUILD", 0) == 0) {
                continue; // Protected: never overwrite generated KR_BUILD in original project!
            }
            const auto targetPath = config.foreignProjectRoot / node.artifact.relativePath;
            if (!targetPath.parent_path().empty()) std::filesystem::create_directories(targetPath.parent_path());
            writeFileString(targetPath, workingSpec.graph.nodeContent(node.id));
        }
        result.writebackPerformed = true;
    }

    // Enforce genome immutability invariant
    if (sys.strand->getGenomeHash() != initialGenomeHash || !sys.strand->isFrozen()) {
        throw std::runtime_error("Genome immutability invariant violated during foreign project development");
    }

    // Write comprehensive JSON report
    result.reportPath = config.isolatedWorkspace / "foreign_project_intelligence.json";
    std::ostringstream json;
    json << "{\n"
         << "  \"format\": \"gdi-foreign-project-intelligence-v1\",\n"
         << "  \"project_name\": " << jsonEscapeStr(result.initialAnalysis.projectName) << ",\n"
         << "  \"immutable_genome_hash\": " << result.immutableGenomeHash << ",\n"
         << "  \"original_directory_hash_before\": " << result.originalDirectoryHashBefore << ",\n"
         << "  \"original_directory_hash_after\": " << result.originalDirectoryHashAfter << ",\n"
         << "  \"original_sha256_before\": " << jsonEscapeStr(result.originalSha256Before) << ",\n"
         << "  \"original_sha256_after\": " << jsonEscapeStr(result.originalSha256After) << ",\n"
         << "  \"original_project_untouched\": " << (result.originalProjectUntouched ? "true" : "false") << ",\n"
         << "  \"writeback_authorized\": " << (config.allowWritebackToOriginal ? "true" : "false") << ",\n"
         << "  \"writeback_performed\": " << (result.writebackPerformed ? "true" : "false") << ",\n"
         << "  \"analysis_succeeded\": " << (result.analysisSucceeded ? "true" : "false") << ",\n"
         << "  \"development_verified\": " << (result.developmentVerified ? "true" : "false") << ",\n"
         << "  \"transferred_principle_applied\": " << (result.transferredPrincipleApplied ? "true" : "false") << ",\n"
         << "  \"final_verified_graph_hash\": " << result.finalVerifiedGraphHash << ",\n"
         << "  \"discovered_modules\": " << result.initialAnalysis.modules.size() << ",\n"
         << "  \"discovered_symbols\": " << result.initialAnalysis.symbols.size() << ",\n"
         << "  \"discovered_kr_sources\": " << result.initialAnalysis.discoveredKrSourceFiles << ",\n"
         << "  \"discovered_kr_build_files\": " << result.initialAnalysis.discoveredGeneratedKrBuildFiles << ",\n"
         << "  \"initial_hypotheses\": " << result.initialAnalysis.hypotheses.size() << ",\n"
         << "  \"iterations\": " << result.iterations.size() << ",\n"
         << "  \"project_memory_records\": " << result.projectMemoryRecords.size() << ",\n"
         << "  \"learned_developmental_principles\": " << result.learnedPrinciples.size() << ",\n"
         << "  \"applied_transferred_principles\": [";
    for (std::size_t i = 0; i < result.appliedTransferredPrinciples.size(); ++i) {
        if (i > 0) json << ", ";
        json << jsonEscapeStr(result.appliedTransferredPrinciples[i]);
    }
    json << "]\n}\n";
    writeFileString(result.reportPath, json.str());

    return result;
}

} // namespace genomic::gpi
