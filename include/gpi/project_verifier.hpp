#pragma once

#include "gpi/generative_project.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

struct ProjectBehaviorContract {
    std::string name;
    std::string target;
    std::string input;
    std::string expectedOutput;
    std::string expectedError;
    int expectedExitCode{0};
};

struct ProjectVerificationContract {
    std::vector<std::string> buildTargets;
    std::vector<std::filesystem::path> includeRoots;
    std::vector<ProjectBehaviorContract> behaviors;
    std::chrono::seconds compileTimeout{120};
    std::chrono::seconds executionTimeout{10};
    bool requireSandbox{false};
};

struct ProjectVerificationCheck {
    std::string name;
    std::string kind;
    bool passed{false};
    bool sandboxed{false};
    bool timedOut{false};
    int exitCode{-1};
    uint64_t milliseconds{0};
    std::filesystem::path standardOutput;
    std::filesystem::path standardError;
    std::string diagnostic;
};

struct ProjectVerificationEvidence {
    bool success{false};
    uint64_t genomeHash{0};
    uint64_t graphHash{0};
    std::vector<ProjectVerificationCheck> checks;
    std::filesystem::path report;
};

ProjectVerificationEvidence verifyGenerativeProject(
    const GenerativeProjectGraph& graph,
    const ProjectVerificationContract& contract,
    const std::filesystem::path& emptyWorkspace,
    const std::filesystem::path& compiler,
    const std::filesystem::path& sandboxRunner={});

} // namespace genomic::gpi
