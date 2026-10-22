#pragma once

#include "genome/genome_base.hpp"
#include "genome/genome_reference.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace genomic {

class GenomeStrand;

struct GenomicSourceArtifact {
    std::filesystem::path relativePath;
    std::vector<GenomeBase> encodedSource;
    std::size_t sourceBytes{0};
    uint64_t sourceHash{0};
    uint64_t immutableGenomeHash{0};
    GenomeRegionId generativeRuleRegion{0};
};

// Lossless source-space representation. It deliberately does not reduce a
// translation unit to the project's partial AST: every source octet remains
// reachable and an external conforming compiler remains the semantic oracle.
class Cpp20SourceGenome {
public:
    static Cpp20SourceGenome fromGenome(const GenomeStrand& genome);

    GenomicSourceArtifact encode(const std::filesystem::path& relativePath,
                                 const std::string& source) const;
    std::string decode(const GenomicSourceArtifact& artifact) const;
    bool verifies(const GenomicSourceArtifact& artifact) const noexcept;

    uint64_t genomeHash() const noexcept { return genomeHash_; }
    GenomeRegionId ruleRegion() const noexcept { return ruleRegion_; }
    const std::string& format() const noexcept { return format_; }
    bool mutationEnabled(std::string_view operation) const;
    bool structureEnabled(std::string_view structure) const;
    bool compositionEnabled(std::string_view operation) const;
    std::vector<std::string> suggestedHeaders(std::string_view compilerDiagnostic) const;

private:
    uint64_t genomeHash_{0};
    GenomeRegionId ruleRegion_{0};
    std::string format_;
    std::unordered_set<std::string> mutationOperators_;
    std::unordered_set<std::string> projectStructures_;
    std::unordered_set<std::string> compositionOperators_;
    std::unordered_map<std::string,std::string> symbolHeaders_;
};

} // namespace genomic
