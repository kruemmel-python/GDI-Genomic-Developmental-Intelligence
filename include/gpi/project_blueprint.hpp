#pragma once

#include "gpi/generative_project.hpp"
#include "gpi/project_verifier.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

struct BlueprintParameter {
    std::string type;
    std::string name;
};
struct BlueprintFunction {
    std::string name;
    std::string returnType;
    std::string expression;
    std::vector<BlueprintParameter> parameters;
};
struct BlueprintTest {
    std::string name;
    std::string expression;
};
struct ProjectBlueprint {
    std::string projectName;
    std::string namespaceName;
    std::vector<BlueprintFunction> functions;
    std::string entryType;
    std::string entryExpression;
    std::vector<BlueprintTest> tests;
    std::vector<std::string> standardHeaders;
    std::string input;
    std::string expectedOutput;
    std::string expectedError;
    int expectedExitCode{0};
    std::chrono::seconds timeout{10};
    bool requireSandbox{false};
};
struct SynthesizedBlueprintProject {
    GenerativeProjectGraph graph;
    ProjectVerificationContract contract;
};

ProjectBlueprint loadProjectBlueprint(const std::filesystem::path& blueprintFile);
SynthesizedBlueprintProject synthesizeProjectBlueprint(
    const ProjectBlueprint& blueprint,
    Cpp20SourceGenome sourceGenome);

} // namespace genomic::gpi
