#include "expression/program_synthesizer.hpp"
#include "expression/semantic_validator.hpp"
#include "cpp20/cpp20_compiler.hpp"
#include "runtime/substrate_kernel.hpp"
#include <iostream>

namespace genomic {

ProgramSynthesizer::ProgramSynthesizer(const GenomeStrand& genome,
                                       const RegulatoryResolver& resolver,
                                       const SpliceGraph& spliceGraph,
                                       const GenomicRuleEngine* ruleEngine)
    : genome_(genome), resolver_(resolver), spliceGraph_(spliceGraph), ruleEngine_(ruleEngine) {}

std::pair<std::shared_ptr<ExpressedProgram>, ExpressionTrace> ProgramSynthesizer::synthesize(
    const MolecularPool& pool,
    const ProgramRequirement& requirement,
    const std::string& startExonId,
    const std::string& endExonId,
    uint32_t compartment,
    uint64_t expressionEventId
) {
    ExpressionTrace trace;
    trace.expressionEventId = expressionEventId;
    trace.genomeHash = genome_.getGenomeHash();
    trace.triggerName = requirement.capabilityName;

    // Step 1: Determine active genomic regions via Regulatory Resolver
    auto regResult = resolver_.resolve(pool, compartment);
    trace.activeRegions.assign(regResult.activeRegions.begin(), regResult.activeRegions.end());

    // Record molecules and contributing enhancers
    for (const auto& enh : regResult.enhancerDetails) {
        if (enh.activated) {
            trace.activeEnhancers.push_back(enh.enhancerId);
            for (const auto& site : enh.siteEvaluations) {
                if (site.bound) {
                    trace.boundMolecules.push_back({site.site.acceptedSignature, site.concentration});
                }
            }
        }
    }

    // Step 2 & 3 & 4: Splice compatible active exons
    auto splicedExons = spliceGraph_.splice(regResult.activeRegions, startExonId, endExonId);
    if (splicedExons.empty()) {
        trace.semanticValidationPassed = false;
        return {nullptr, trace};
    }

    for (const auto& ex : splicedExons) {
        trace.splicedExonNames.push_back(ex.name);
    }

    // Build Provenance
    GenomeProvenance prov;
    prov.sourceRegion = splicedExons.front().regionId;
    prov.enhancer = trace.activeEnhancers.empty() ? 0 : trace.activeEnhancers.front();
    for (const auto& [sig, conc] : trace.boundMolecules) {
        prov.triggeringMolecules.push_back(sig);
    }
    prov.expressionEvent = expressionEventId;

    // Step 5 & 6: Compose candidates into AST
    std::string functionName = "synthesized_" + requirement.capabilityName;
    auto ast = spliceGraph_.composeFunction(splicedExons, functionName, prov);
    if (!ast) {
        trace.semanticValidationPassed = false;
        return {nullptr, trace};
    }

    // Step 7: C++ Semantic Validation (Gates 1-6)
    auto valResult = SemanticValidator::validate(*ast, requirement.energyBudget, requirement.memoryBudget, ruleEngine_);
    trace.semanticValidationPassed = valResult.allGatesPassed;
    if (!valResult.allGatesPassed) {
        return {nullptr, trace};
    }

    // Generate C++20 source representation (Blueprint Section 37)
    trace.generatedCppSource = Cpp20Printer::print(*ast);

    // Compute AST hash
    trace.astHash = SubstrateKernel::fnv1a64(
        reinterpret_cast<const uint8_t*>(trace.generatedCppSource.data()),
        trace.generatedCppSource.size()
    );

    // Step 8: Compile to Typed Genome IR using genomic rules
    GenomeCompiler compiler(ruleEngine_);
    auto irModule = compiler.compile(*ast, functionName + "_Module");
    trace.irHash = irModule->irHash;

    // Step 9 & 10: Assemble ExpressedProgram
    auto program = std::make_shared<ExpressedProgram>();
    program->programId = nextProgramId_++;
    program->genomeHash = genome_.getGenomeHash();
    program->contextHash = SubstrateKernel::fnv1a64(
        reinterpret_cast<const uint8_t*>(&compartment), sizeof(compartment)
    );
    program->astHash = trace.astHash;
    program->irHash = trace.irHash;
    program->name = functionName;
    program->ast = ast;
    program->irModule = irModule;
    program->provenance = prov;

    trace.executionActive = true;
    return {program, trace};
}

} // namespace genomic
