#pragma once
#include "genome/genome_strand.hpp"
#include "gpi/task_ir.hpp"
#include <cstdint>
#include <set>
#include <string>
namespace genomic::gpi {
struct TaskGenomePolicy {
    uint64_t regionId{0};
    uint64_t signature{0};
    std::set<std::string> operations;
    std::set<std::string> reductions;
    explicit TaskGenomePolicy(const GenomeStrand& genome);
    // A genuine molecule -> enhancer -> active region check is required.
    bool activatedByTaskMolecule() const;
    std::string validate(const TaskIR& task) const;
};
} // namespace genomic::gpi
