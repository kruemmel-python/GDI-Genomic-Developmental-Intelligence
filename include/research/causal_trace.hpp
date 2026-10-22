#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "genome/genome_reference.hpp"
#include "runtime/expressed_program.hpp"
#include "expression/expression_trace.hpp"

namespace genomic {

struct CausalTraceStep {
    std::string stepName;
    std::string description;
    uint64_t targetId{0};
};

class CausalTrace {
public:
    static std::vector<CausalTraceStep> traceProgramBackwards(
        const ExpressedProgram& program,
        const ExpressionTrace& expressionTrace
    );

    static std::string formatCausalChain(const std::vector<CausalTraceStep>& steps);
};

} // namespace genomic
