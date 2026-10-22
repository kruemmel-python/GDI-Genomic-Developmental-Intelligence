#pragma once

#include <memory>
#include "genome/genome_strand.hpp"
#include "genome/genome_reference.hpp"
#include "genome/genomic_rule_engine.hpp"
#include "regulation/regulatory_resolver.hpp"
#include "expression/splice_graph.hpp"
#include "development/phenotype_builder.hpp"

namespace genomic {

struct BootstrappedSystem {
    std::shared_ptr<GenomeStrand> strand;
    std::shared_ptr<RegulatoryResolver> resolver;
    std::shared_ptr<SpliceGraph> spliceGraph;
    std::shared_ptr<PhenotypeBuilder> phenotypeBuilder;
    std::shared_ptr<GenomicRuleEngine> ruleEngine;
};

class GenomeBootstrap {
public:
    // Builds the full synthetic C++20 genome strand and wires the regulatory resolver & splice graph
    static BootstrappedSystem buildCompleteSystem();
};

} // namespace genomic
