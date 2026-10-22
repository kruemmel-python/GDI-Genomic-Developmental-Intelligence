#pragma once
#include "gpi/task_ir.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace genomic::gpi {
struct ProjectAgentConfig {
    std::filesystem::path projectDir;
    std::string compiler{"g++"};
    bool repairProbe{false};
    bool learnedMathHeaderRepair{false};
};
struct ProjectAgentResult {
    bool success{false};
    bool behaviorVerified{false};
    std::size_t attempts{0};
    std::vector<std::string> events;
};
ProjectAgentResult runProjectAgent(const TaskIR& task, uint64_t genomeHash,
                                   const ProjectAgentConfig& config);
}
