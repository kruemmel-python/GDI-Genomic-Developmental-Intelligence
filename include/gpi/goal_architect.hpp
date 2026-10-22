#pragma once

#include "gpi/project_blueprint.hpp"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace genomic::gpi {

struct GoalParameter { std::string type; std::string name; };
struct GoalOperation {
    std::string id;
    std::string operation;
    std::vector<std::string> operands;
};
struct GoalFunction {
    std::string name;
    std::string returnType;
    std::vector<GoalParameter> parameters;
    std::vector<GoalOperation> operations;
    std::string returnValue;
};
struct GoalCase {
    std::string name;
    std::string function;
    std::string expected;
    std::vector<std::string> arguments;
};
struct TypedArchitectureGoal {
    std::string projectName;
    std::string namespaceName;
    std::vector<GoalFunction> functions;
    std::string entryFunction;
    std::string entryExpected;
    std::vector<std::string> entryArguments;
    std::vector<GoalCase> cases;
    std::chrono::seconds timeout{10};
    bool requireSandbox{true};
};
struct ArchitectedProject {
    ProjectBlueprint blueprint;
    SynthesizedBlueprintProject project;
    std::string architectureReport;
    std::uint64_t goalHash{0};
};

TypedArchitectureGoal loadTypedArchitectureGoal(const std::filesystem::path& goalFile);
ArchitectedProject architectTypedGoal(const TypedArchitectureGoal& goal,Cpp20SourceGenome sourceGenome);

} // namespace genomic::gpi
