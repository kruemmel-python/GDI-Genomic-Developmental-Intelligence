#include "research/genome_inspector.hpp"
#include "research/expression_inspector.hpp"
#include <sstream>
#include <iomanip>

namespace genomic {

std::string GenomeInspector::inspectGenome(const GenomeStrand& strand) {
    std::ostringstream ss;
    ss << "=== GENOME STRAND INSPECTION ===\n"
       << "Status: " << (strand.isFrozen() ? "IMMUTABLE (FROZEN)" : "MUTABLE (UNFROZEN)") << "\n"
       << "Total Bases: " << strand.length() << "\n"
       << "Global Hash: 0x" << std::hex << strand.getGenomeHash() << std::dec << "\n"
       << "Regions (" << strand.getAllRegions().size() << "):\n";

    for (const auto& reg : strand.getAllRegions()) {
        ss << "  [" << std::setw(6) << reg.id << "] "
           << std::setw(22) << regionKindToString(reg.kind) << " | "
           << std::setw(24) << reg.name
           << " | offset: " << std::setw(6) << reg.baseOffset
           << " | len: " << std::setw(5) << reg.baseLength
           << " | CRC: 0x" << std::hex << reg.crc32 << std::dec
           << "\n";
    }
    return ss.str();
}

std::string GenomeInspector::inspectMolecularLandscape(const MolecularPool& pool, uint32_t compartment) {
    std::ostringstream ss;
    ss << "=== MOLECULAR LANDSCAPE (Compartment " << compartment << ") ===\n";
    auto molecules = pool.getMolecules(compartment);
    if (molecules.empty()) {
        ss << "  (Pool is quiescent / no active molecules)\n";
    } else {
        for (const auto& mol : molecules) {
            ss << "  - Signature: 0x" << std::hex << mol.signature << std::dec
               << " | Name: " << std::setw(18) << mol.name
               << " | Concentration: " << std::fixed << std::setprecision(3) << mol.concentration
               << " | Half-Life: " << mol.halfLife << " ticks\n";
        }
    }
    return ss.str();
}

std::string GenomeInspector::inspectPhenotype(const PhenotypeBuilder& builder) {
    std::ostringstream ss;
    ss << "=== PHENOTYPE STATUS ===\n"
       << "Developmental Stage: " << stageToString(builder.getStage()) << "\n"
       << "Resource Level: " << builder.getResourceLevel() << "\n"
       << "Neurons: " << builder.countByCategory(EntityCategory::Neuron) << "\n"
       << "Synapses: " << builder.countByCategory(EntityCategory::Synapse) << "\n"
       << "Assemblies: " << builder.countByCategory(EntityCategory::Assembly) << "\n"
       << "Active Programs: " << builder.countByCategory(EntityCategory::Program) << "\n";
    return ss.str();
}

std::string GenomeInspector::generateResearchPanel(
    const ExpressedProgram& program,
    const ExpressionTrace& trace,
    const MolecularPool& pool,
    uint32_t compartment
) {
    std::ostringstream ss;
    ss << "======================================================================\n"
       << "                         RESEARCH PANEL                               \n"
       << "======================================================================\n"
       << "PROGRAM P-" << program.programId << " (" << program.name << ")\n\n"
       << "Genome:\n"
       << "    Global Hash: 0x" << std::hex << trace.genomeHash << std::dec << "\n"
       << "    Source Region: " << program.provenance.sourceRegion << "\n"
       << "    Compartment: " << compartment << " (Pool size: " << pool.getAllMolecules().size() << ")\n\n"
       << "Trigger:\n"
       << "    " << trace.triggerName << "\n\n"
       << "Molecules (Bound):\n";

    for (const auto& [sig, conc] : trace.boundMolecules) {
        ss << "    0x" << std::hex << sig << std::dec << " -> concentration: "
           << std::fixed << std::setprecision(2) << conc << "\n";
    }
    if (trace.boundMolecules.empty()) {
        ss << "    (None)\n";
    }

    ss << "\nEnhancers (Active):\n";
    for (auto enhId : trace.activeEnhancers) {
        ss << "    E-" << enhId << " active\n";
    }
    if (trace.activeEnhancers.empty()) {
        ss << "    (None)\n";
    }

    ss << "\nSplicing:\n";
    for (const auto& exName : trace.splicedExonNames) {
        ss << "    " << exName << "\n";
    }

    ss << "\nC++20 Generated Phenotype Source:\n"
       << "----------------------------------------------------------------------\n"
       << trace.generatedCppSource << "\n"
       << "----------------------------------------------------------------------\n"
       << "Validation: " << (trace.semanticValidationPassed ? "PASS (Valid C++20)" : "FAIL") << "\n"
       << "AST Hash:   0x" << std::hex << trace.astHash << "\n"
       << "IR Hash:    0x" << trace.irHash << std::dec << "\n"
       << "Execution:  " << (trace.executionActive ? "ACTIVE" : "INACTIVE") << "\n"
       << "======================================================================\n";

    return ss.str();
}

std::string ExpressionInspector::formatTraceSummary(const ExpressionTrace& trace) {
    std::ostringstream ss;
    ss << "Expression Event #" << trace.expressionEventId << " [" << trace.triggerName << "]\n"
       << "  Active Regions: " << trace.activeRegions.size() << "\n"
       << "  Spliced Exons: " << trace.splicedExonNames.size() << "\n"
       << "  AST Hash: 0x" << std::hex << trace.astHash << std::dec << "\n";
    return ss.str();
}

} // namespace genomic
