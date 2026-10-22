#pragma once

#include <string>
#include <vector>
#include "gpi/prompt_regulatory_resolver.hpp"

namespace genomic::gpi {

struct PromptProgramArtifact {
    bool generated{false};
    uint64_t genomeHash{0};
    std::string source;
    std::vector<std::string> plan;
    std::vector<std::string> diagnostics;
};

class PromptProgramSynthesizer {
public:
    // Emits a standalone C++20 application from an activated genomic plan.
    // No arbitrary prompt text is interpolated into the emitted source.
    static PromptProgramArtifact synthesize(const PromptResolution& resolution);
};

} // namespace genomic::gpi
