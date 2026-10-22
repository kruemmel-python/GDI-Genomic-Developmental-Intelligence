#pragma once
#include "gpi/task_ir.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace genomic::gpi {
struct AgentConfig {
    std::filesystem::path output;
    std::string compiler;
    bool injectMissingMathHeader{false}; // diagnostic-repair regression probe
    bool learnedMathHeaderRepair{false}; // derived only from verified prior experience
};
struct AgentResult {
    bool success{false};
    std::size_t attempts{0};
    bool behaviorVerified{false};
    std::filesystem::path executable;
    std::vector<std::string> events;
    std::string source;
};
AgentResult runAgent(const TaskIR& task, uint64_t genomeHash, const AgentConfig& config);
} // namespace genomic::gpi
