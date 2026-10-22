#pragma once
#include "gpi/task_ir.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace genomic::gpi {
struct GeneratedProject {
    std::filesystem::path root;
    std::filesystem::path mainSource;
    std::filesystem::path pipelineSource;
    std::filesystem::path testSource;
    std::filesystem::path includeDirectory;
    std::vector<std::filesystem::path> files;
};
GeneratedProject emitProject(const TaskIR& task, uint64_t genomeHash,
                             const std::filesystem::path& directory,
                             bool includeMathHeader=true);
std::string expectedOutputForInputs(const TaskIR& task, const std::vector<double>& inputs);
}
