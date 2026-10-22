#include "research/causal_trace.hpp"
#include <sstream>

namespace genomic {

std::vector<CausalTraceStep> CausalTrace::traceProgramBackwards(
    const ExpressedProgram& program,
    const ExpressionTrace& expressionTrace
) {
    std::vector<CausalTraceStep> steps;

    // 1. Executed Program / IR Instruction
    CausalTraceStep s1;
    s1.stepName = "IR Execution / Output";
    s1.description = "Program P-" + std::to_string(program.programId) + " (" + program.name + ")";
    s1.targetId = program.programId;
    steps.push_back(s1);

    // 2. Intermediate Representation
    CausalTraceStep s2;
    s2.stepName = "Genome IR Module";
    s2.description = "Typed SSA IR (hash: 0x" + std::to_string(program.irHash) + ")";
    s2.targetId = program.irHash;
    steps.push_back(s2);

    // 3. C++ AST
    CausalTraceStep s3;
    s3.stepName = "C++20 AST Node";
    s3.description = "Validated C++20 Function (AST hash: 0x" + std::to_string(program.astHash) + ")";
    s3.targetId = program.astHash;
    steps.push_back(s3);

    // 4. Genomic Fragments / Spliced Exons
    CausalTraceStep s4;
    s4.stepName = "Spliced Genomic Exons";
    std::string exonList;
    for (size_t i = 0; i < expressionTrace.splicedExonNames.size(); ++i) {
        if (i > 0) exonList += " + ";
        exonList += expressionTrace.splicedExonNames[i];
    }
    s4.description = "Exons: " + exonList;
    s4.targetId = program.provenance.sourceRegion;
    steps.push_back(s4);

    // 5. Active Enhancers
    CausalTraceStep s5;
    s5.stepName = "Active Enhancers";
    std::string enhList;
    for (size_t i = 0; i < expressionTrace.activeEnhancers.size(); ++i) {
        if (i > 0) enhList += ", ";
        enhList += "E-" + std::to_string(expressionTrace.activeEnhancers[i]);
    }
    s5.description = "Enhancers: " + (enhList.empty() ? "None" : enhList);
    s5.targetId = program.provenance.enhancer;
    steps.push_back(s5);

    // 6. Bound Molecules
    CausalTraceStep s6;
    s6.stepName = "Regulatory Molecules";
    std::string molList;
    for (size_t i = 0; i < expressionTrace.boundMolecules.size(); ++i) {
        if (i > 0) molList += ", ";
        molList += "0x" + std::to_string(expressionTrace.boundMolecules[i].first) +
                   " (" + std::to_string(expressionTrace.boundMolecules[i].second) + ")";
    }
    s6.description = "Bound: " + (molList.empty() ? "None" : molList);
    s6.targetId = expressionTrace.boundMolecules.empty() ? 0 : expressionTrace.boundMolecules.front().first;
    steps.push_back(s6);

    // 7. Physiological / Environmental Trigger
    CausalTraceStep s7;
    s7.stepName = "Environmental / Physiological Trigger";
    s7.description = expressionTrace.triggerName.empty() ? "Basal Trigger" : expressionTrace.triggerName;
    s7.targetId = 0;
    steps.push_back(s7);

    return steps;
}

std::string CausalTrace::formatCausalChain(const std::vector<CausalTraceStep>& steps) {
    std::ostringstream ss;
    ss << "=== REVERSE CAUSAL TRACE ===\n";
    for (size_t i = 0; i < steps.size(); ++i) {
        if (i > 0) {
            ss << "    ^\n    | (caused by)\n";
        }
        ss << "[" << (i + 1) << "] " << steps[i].stepName << "\n";
        ss << "    Details: " << steps[i].description << "\n";
    }
    return ss.str();
}

} // namespace genomic
