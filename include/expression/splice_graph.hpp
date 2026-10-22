#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include "genome/genome_reference.hpp"
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_types.hpp"

namespace genomic {

struct SpliceExon {
    std::string id;
    std::string name;
    GenomeRegionId regionId{0};
    std::shared_ptr<CppType> inputType;
    std::shared_ptr<CppType> outputType;
    std::vector<std::string> compatibleNextExons;
    std::function<std::shared_ptr<AstNode>()> astFragmentBuilder;
};

class SpliceGraph {
public:
    SpliceGraph() = default;

    void addExon(const SpliceExon& exon);

    // Splice active exons into an execution pipeline according to regulatory regions
    std::vector<SpliceExon> splice(
        const std::unordered_set<GenomeRegionId>& activeRegions,
        const std::string& startExonId,
        const std::string& endExonId
    ) const;

    // Compose spliced exons into a cohesive C++20 function AST
    std::shared_ptr<AstNode> composeFunction(
        const std::vector<SpliceExon>& exons,
        const std::string& functionName,
        const GenomeProvenance& provenance
    ) const;

    bool hasExon(const std::string& id) const noexcept;
    const SpliceExon& getExon(const std::string& id) const;

private:
    std::unordered_map<std::string, SpliceExon> exons_;
};

} // namespace genomic
