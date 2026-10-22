#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_set>
#include "genome/genome_strand.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"
#include "expression/splice_graph.hpp"
#include "expression/program_requirement.hpp"
#include "expression/expression_trace.hpp"
#include "runtime/expressed_program.hpp"
#include "runtime/genome_vm.hpp"

namespace genomic {

class GenomicRuleEngine;

class ProgramSynthesizer {
public:
    ProgramSynthesizer(const GenomeStrand& genome,
                       const RegulatoryResolver& resolver,
                       const SpliceGraph& spliceGraph,
                       const GenomicRuleEngine* ruleEngine = nullptr);

    // Run full 11-step synthesis pipeline
    std::pair<std::shared_ptr<ExpressedProgram>, ExpressionTrace> synthesize(
        const MolecularPool& pool,
        const ProgramRequirement& requirement,
        const std::string& startExonId,
        const std::string& endExonId,
        uint32_t compartment = 0,
        uint64_t expressionEventId = 1
    );

private:
    const GenomeStrand& genome_;
    const RegulatoryResolver& resolver_;
    const SpliceGraph& spliceGraph_;
    const GenomicRuleEngine* ruleEngine_{nullptr};
    uint64_t nextProgramId_{1000};
};

} // namespace genomic
