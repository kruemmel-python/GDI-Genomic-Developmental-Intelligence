#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "cpp20/cpp20_lexicon.hpp"

namespace genomic {

struct MacroDefinition {
    std::string name;
    bool isFunctionLike{false};
    std::vector<std::string> parameters;
    bool hasVarArgs{false};
    std::vector<Token> replacementTokens;
};

class Cpp20Preprocessor {
public:
    Cpp20Preprocessor();

    void defineMacro(const std::string& name, const std::string& value = "1");
    void defineFunctionMacro(const std::string& name, const std::vector<std::string>& params,
                            const std::string& body, bool hasVarArgs = false);
    void undefineMacro(const std::string& name);
    bool isDefined(const std::string& name) const;

    void addVirtualHeader(const std::string& filename, const std::string& content);

    // Full preprocess pass
    std::string process(const std::string& source);

private:
    std::unordered_map<std::string, MacroDefinition> macros_;
    std::unordered_map<std::string, std::string> virtualFiles_;

    std::vector<Token> expandTokens(const std::vector<Token>& tokens);
};

} // namespace genomic
