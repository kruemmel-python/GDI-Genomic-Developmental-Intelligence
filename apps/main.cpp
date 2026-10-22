#include <iostream>
#include <iomanip>
#include "genome/genome_bootstrap.hpp"
#include "research/genome_inspector.hpp"
#include "research/causal_trace.hpp"
#include "expression/program_synthesizer.hpp"
#include "runtime/substrate_kernel.hpp"
#include "runtime/genome_vm.hpp"

using namespace genomic;

int main() {
    std::cout << "======================================================================\n";
    std::cout << "               GENOMIC C++20 SYSTEM - INITIALIZING                    \n";
    std::cout << "    A complete C++20 programming space inside an immutable genome     \n";
    std::cout << "======================================================================\n\n";

    // Step 0: Substrate Kernel & Deterministic Clock
    SubstrateKernel kernel(1024 * 1024, 1000000, 421337);
    std::cout << "[Kernel] Substrate kernel initialized. Memory: "
              << kernel.memorySize() / 1024 << " KB, Energy budget: "
              << kernel.remainingEnergy() << " gas units.\n";

    // Step 1: Bootstrap the complete synthetic genome
    std::cout << "[Bootstrap] Building linear synthetic genome strand...\n";
    auto sys = GenomeBootstrap::buildCompleteSystem();
    std::cout << "[Bootstrap] Immutable genome finalized with " << sys.strand->getAllRegions().size()
              << " functional regions and " << sys.strand->length() << " physical bases.\n";
    std::cout << "[Bootstrap] Global Genome Hash: 0x" << std::hex << sys.strand->getGenomeHash() << std::dec << "\n\n";

    // Print Genome inspection
    std::cout << GenomeInspector::inspectGenome(*sys.strand) << "\n";

    // Step 2: Initialize Molecular Pool
    MolecularPool pool;
    ProgramSynthesizer synthesizer(*sys.strand, *sys.resolver, *sys.spliceGraph, sys.ruleEngine.get());
    GenomeVm vm(&kernel, &pool);

    // -----------------------------------------------------------------
    // SCENARIO 1: Threat Detected (Fast Evasion Route)
    // -----------------------------------------------------------------
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << ">>> SCENARIO 1: Threat Evasion (ThreatDetected molecule = 0.85)\n";
    std::cout << "----------------------------------------------------------------------\n";
    pool.clear();
    pool.setConcentration(0, MoleculeSignatures::NutrientRich, 0.4, "NutrientRich");
    pool.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.85, "ThreatDetected");

    ProgramRequirement threatReq;
    threatReq.desiredCapability = 101;
    threatReq.capabilityName = "ThreatEvasionNavigation";
    threatReq.energyBudget = 5000.0;
    threatReq.memoryBudget = 64.0 * 1024.0;

    auto [threatProg, threatTrace] = synthesizer.synthesize(
        pool, threatReq, "NAV_A", "NAV_C", 0, 1001
    );

    if (threatProg) {
        std::cout << GenomeInspector::generateResearchPanel(*threatProg, threatTrace, pool, 0) << "\n";

        // Execute synthesized program in Genome VM
        std::cout << "[VM] Executing synthesized Threat Evasion program in Genome VM...\n";
        auto vmRes = vm.execute(*threatProg->irModule, threatProg->name, {VmValue::makeInt(15)});
        std::cout << "[VM] Status: " << (vmRes.status == VmExecutionStatus::Success ? "SUCCESS" : "ERROR")
                  << " | Result Value: " << vmRes.returnValue.iVal
                  << " | Cycles: " << vmRes.cyclesExecuted << "\n\n";

        // Reverse Causal Trace
        auto steps = CausalTrace::traceProgramBackwards(*threatProg, threatTrace);
        std::cout << CausalTrace::formatCausalChain(steps) << "\n";
    }

    // -----------------------------------------------------------------
    // SCENARIO 2: Energy Crisis (Energy-Optimal Safe Route with Feedback)
    // -----------------------------------------------------------------
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << ">>> SCENARIO 2: Energy Crisis (EnergyLow molecule = 0.90)\n";
    std::cout << "----------------------------------------------------------------------\n";
    pool.clear();
    pool.setConcentration(0, MoleculeSignatures::NutrientRich, 0.2, "NutrientRich");
    pool.setConcentration(0, MoleculeSignatures::EnergyLow, 0.90, "EnergyLow");

    ProgramRequirement energyReq;
    energyReq.desiredCapability = 102;
    energyReq.capabilityName = "EnergyOptimalSafeNavigation";
    energyReq.energyBudget = 5000.0;
    energyReq.memoryBudget = 64.0 * 1024.0;

    auto [energyProg, energyTrace] = synthesizer.synthesize(
        pool, energyReq, "NAV_A", "NAV_E", 0, 1002
    );

    if (energyProg) {
        std::cout << GenomeInspector::generateResearchPanel(*energyProg, energyTrace, pool, 0) << "\n";

        // Execute in VM with Molecular Feedback Loop
        std::cout << "[VM] Executing synthesized Energy-Optimal program in Genome VM...\n";
        auto vmRes = vm.execute(*energyProg->irModule, energyProg->name, {VmValue::makeInt(15)});
        std::cout << "[VM] Status: " << (vmRes.status == VmExecutionStatus::Success ? "SUCCESS" : "ERROR")
                  << " | Result Value: " << vmRes.returnValue.iVal
                  << " | Cycles: " << vmRes.cyclesExecuted << "\n";
        std::cout << "[VM] Emitted Molecules count: " << vmRes.emittedMolecules.size() << "\n";
        for (const auto& [sig, conc] : vmRes.emittedMolecules) {
            std::cout << "     -> Emitted 0x" << std::hex << sig << std::dec << " with concentration " << conc << "\n";
        }

        // Show that molecular pool now contains the emitted feedback molecule!
        std::cout << "\n[Feedback Loop] Updated molecular landscape in pool:\n";
        std::cout << GenomeInspector::inspectMolecularLandscape(pool, 0) << "\n";

        // Reverse Causal Trace
        auto steps = CausalTrace::traceProgramBackwards(*energyProg, energyTrace);
        std::cout << CausalTrace::formatCausalChain(steps) << "\n";
    }

    // -----------------------------------------------------------------
    // SCENARIO 3: Developmental Progression & Ontogeny
    // -----------------------------------------------------------------
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << ">>> SCENARIO 3: Developmental Ontogeny & Phenotype Growth\n";
    std::cout << "----------------------------------------------------------------------\n";
    LineageContext anteriorLineage{TissueLineage::AnteriorNervous, 0, "AnteriorCortical"};

    std::cout << "[Ontogeny] Current Organism Stage: Neonatal\n";
    sys.phenotypeBuilder->setStage(NeuralDevelopmentStage::Neonatal);

    std::cout << " - Can develop Neonatal Neuron (Entity 1): "
              << (sys.phenotypeBuilder->canDevelop(1, anteriorLineage, pool) ? "YES" : "NO") << "\n";
    sys.phenotypeBuilder->developEntity(1, anteriorLineage, pool, kernel.tick());

    std::cout << " - Can develop Mature Program (Entity 4) in Neonatal stage: "
              << (sys.phenotypeBuilder->canDevelop(4, anteriorLineage, pool) ? "YES" : "NO") << "\n";

    std::cout << "[Ontogeny] Advancing Organism to Stage: Mature...\n";
    sys.phenotypeBuilder->setStage(NeuralDevelopmentStage::Mature);
    std::cout << " - Can develop Mature Program (Entity 4) now: "
              << (sys.phenotypeBuilder->canDevelop(4, anteriorLineage, pool) ? "YES" : "NO") << "\n";
    sys.phenotypeBuilder->developEntity(4, anteriorLineage, pool, kernel.tick());

    std::cout << "\n" << GenomeInspector::inspectPhenotype(*sys.phenotypeBuilder) << "\n";

    std::cout << "======================================================================\n";
    std::cout << "            GENOMIC C++20 SYSTEM RUN COMPLETED SUCCESSFULLY           \n";
    std::cout << "======================================================================\n";

    return 0;
}
