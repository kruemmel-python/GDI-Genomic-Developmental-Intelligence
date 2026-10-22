#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "genome/genome_strand.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"

namespace genomic::gpi {

// A finite, explicit capability vocabulary. This is not a general-language LLM.
struct PromptCapability {
    GenomeRegionId regionId{0};
    std::string id;
    std::string operation;
    uint64_t signature{0};
    std::vector<std::string> triggers;
};

struct ActivatedCapability {
    std::string id;
    std::string operation;
    GenomeRegionId regionId{0};
    uint64_t moleculeSignature{0};
    std::string matchedTerm;
    double activity{0.0};
};

enum class ThresholdMode { None, Greater, GreaterEqual, Less, LessEqual };

struct PromptResolution {
    bool supported{false};
    uint64_t genomeHash{0};
    std::string originalPrompt;
    std::optional<std::size_t> expectedSamples;
    std::optional<int64_t> threshold;
    ThresholdMode thresholdMode{ThresholdMode::None};
    std::vector<ActivatedCapability> capabilities;
    MolecularPool molecules;
    RegulatoryResolutionResult regulation;
    std::vector<std::string> diagnostics;
};

class PromptRegulatoryResolver {
public:
    explicit PromptRegulatoryResolver(const GenomeStrand& genome);
    PromptResolution resolve(const std::string& prompt) const;
    const std::vector<PromptCapability>& capabilities() const noexcept { return capabilities_; }

private:
    uint64_t genomeHash_{0};
    std::vector<PromptCapability> capabilities_;
};

} // namespace genomic::gpi
