#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace genomic::gpi {

// A language-level grammar, not a catalogue of domain algorithms.  The caller
// supplies names and types recovered from a target translation unit.
struct GenerativeProgramContext {
    std::vector<std::string> scalarValues;
    std::vector<std::string> integerVectors;
    std::vector<std::string> ownedIntegerVectors;
    std::vector<std::string> uniquePointers;
    std::string pointeeType{"int"};
    std::string selfFunction;
};

class GenerativeProgramGrammar {
public:
    static std::vector<std::string> enumerate(const GenerativeProgramContext& context,
                                              std::size_t limit);
};

} // namespace genomic::gpi
