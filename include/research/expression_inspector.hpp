#pragma once

#include <string>
#include "expression/expression_trace.hpp"
#include "research/causal_trace.hpp"

namespace genomic {

class ExpressionInspector {
public:
    static std::string formatTraceSummary(const ExpressionTrace& trace);
};

} // namespace genomic
