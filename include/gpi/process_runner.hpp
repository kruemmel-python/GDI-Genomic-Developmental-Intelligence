#pragma once
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
namespace genomic::gpi {
struct ProcessResult {
    bool launched{false};
    bool timedOut{false};
    int exitCode{-1};
    std::string error;
};
// Direct argv execution; no shell; bounded wall-clock execution.
ProcessResult runProcess(const std::vector<std::string>& args,
                         const std::filesystem::path& cwd,
                         const std::filesystem::path& stdinFile,
                         const std::filesystem::path& stdoutFile,
                         const std::filesystem::path& stderrFile,
                         std::chrono::milliseconds timeout);
}
