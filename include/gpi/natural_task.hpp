#pragma once
#include <string>
namespace genomic::gpi {
struct NaturalTaskResult {
    std::string structured;
    std::string error;
};
// Deterministic, deliberately narrow German/English task language.
// Rejects unrecognized or ambiguous requests instead of inventing intent.
NaturalTaskResult translateNaturalTask(const std::string& request);
}
