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
struct BlueprintField {
    std::string type;
    std::string name;
};
struct BlueprintMethod {
    std::string name;
    std::string returnType;
    std::string expression;
    std::vector<BlueprintParameter> parameters;
    bool isConst{true};
    bool isConstexpr{true};
};
struct BlueprintType {
    std::string name;
    std::string moduleName;
    std::vector<BlueprintField> fields;
    std::vector<BlueprintMethod> methods;
    bool generateEquality{true};
};
struct BlueprintEnumValue {
    std::string name;
    long long value{0};
};
struct BlueprintEnum {
    std::string name;
    std::string underlyingType{"long long"};
    std::string moduleName;
    std::vector<BlueprintEnumValue> values;
};
struct BlueprintConcept {
    std::string name;
    std::string param{"T"};
    std::string expression;
    std::string moduleName;
};
struct BlueprintFunction {
    std::string name;
    std::string returnType;
    std::string expression;
    std::vector<BlueprintParameter> parameters;
    std::string moduleName;
    std::string templateParam;
    std::string requiresConcept;
    bool isConstexpr{false};
};
struct BlueprintModule {
    std::string name;
    std::vector<std::string> dependencies;
};
struct BlueprintTest {
    std::string name;
    std::string expression;
};
struct ProjectBlueprint {
    std::string projectName;
    std::string namespaceName;
    std::vector<BlueprintModule> modules;
    std::vector<BlueprintEnum> enums;
    std::vector<BlueprintConcept> concepts;
    std::vector<BlueprintType> types;
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
ProjectBlueprint parseProjectBlueprintText(const std::string& text,
                                           const std::filesystem::path& fixtureRoot = {});
std::string serializeProjectBlueprint(const ProjectBlueprint& blueprint);
SynthesizedBlueprintProject synthesizeProjectBlueprint(
    const ProjectBlueprint& blueprint,
    Cpp20SourceGenome sourceGenome);

} // namespace genomic::gpi
