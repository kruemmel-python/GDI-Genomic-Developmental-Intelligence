#pragma once

#include <string>
#include <vector>
#include "genome/genome_strand.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"
#include "runtime/expressed_program.hpp"
#include "expression/expression_trace.hpp"
#include "development/phenotype_builder.hpp"

namespace genomic {

class GenomeInspector {
public:
    static std::string inspectGenome(const GenomeStrand& strand);
    static std::string inspectMolecularLandscape(const MolecularPool& pool, uint32_t compartment = 0);
    static std::string inspectPhenotype(const PhenotypeBuilder& builder);

    // Section 38 Research Panel
    static std::string generateResearchPanel(
        const ExpressedProgram& program,
        const ExpressionTrace& trace,
        const MolecularPool& pool,
        uint32_t compartment = 0
    );
};

} // namespace genomic
