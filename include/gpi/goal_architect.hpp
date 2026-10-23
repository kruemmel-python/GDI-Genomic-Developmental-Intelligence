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
struct GoalField {
    std::string type;
    std::string name;
};
struct GoalMethod {
    std::string name;
    std::string returnType;
    std::vector<GoalParameter> parameters;
    std::vector<GoalOperation> operations;
    std::string returnValue;
};
struct GoalType {
    std::string name;
    std::string moduleName;
    std::vector<GoalField> fields;
    std::vector<GoalMethod> methods;
};
struct GoalEnumValue {
    std::string name;
    long long value{0};
};
struct GoalEnum {
    std::string name;
    std::string underlyingType{"long long"};
    std::string moduleName;
    std::vector<GoalEnumValue> values;
};
struct GoalConcept {
    std::string name;
    std::string param{"T"};
    std::string expression;
    std::string moduleName;
};
struct GoalFunction {
    std::string name;
    std::string returnType;
    std::vector<GoalParameter> parameters;
    std::vector<GoalOperation> operations;
    std::string returnValue;
    std::string moduleName;
    std::string templateParam;
    std::string requiresConcept;
    bool isConstexpr{false};
};
struct GoalModule {
    std::string name;
    std::vector<std::string> dependencies;
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
    std::vector<GoalModule> modules;
    std::vector<GoalEnum> enums;
    std::vector<GoalConcept> concepts;
    std::vector<GoalType> types;
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
TypedArchitectureGoal parseTypedArchitectureGoalText(const std::string& text);
ArchitectedProject architectTypedGoal(const TypedArchitectureGoal& goal,Cpp20SourceGenome sourceGenome);

} // namespace genomic::gpi
