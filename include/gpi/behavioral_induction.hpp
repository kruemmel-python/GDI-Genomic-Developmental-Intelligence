#pragma once

#include "gpi/goal_architect.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

struct BehavioralExample {
    std::vector<std::int64_t> arguments;
    std::int64_t expected{};
    std::vector<std::string> rawArguments;
    std::string rawExpected;
};

struct BehavioralParameterSpec {
    std::string type{"long long"};
    std::string name;
};

struct BehavioralMethodGoal {
    std::string name;
    std::string returnType{"long long"};
    std::vector<BehavioralParameterSpec> parameters;
    std::vector<BehavioralExample> samples;
    std::vector<BehavioralExample> checks;
    std::size_t maxDepth{3};
};

struct BehavioralTypeGoal {
    std::string name;
    std::string moduleName;
    std::vector<GoalField> fields;
    std::vector<BehavioralMethodGoal> methods;
};

struct BehavioralFunctionGoal {
    std::string name;
    std::string returnType{"long long"};
    std::string moduleName;
    std::string templateParam;
    std::string requiresConcept;
    std::vector<BehavioralParameterSpec> parameters;
    std::vector<BehavioralExample> samples;
    std::vector<BehavioralExample> checks;
    std::size_t maxDepth{3};
};

struct BehavioralGoal {
    std::string projectName;
    std::string namespaceName{"generated"};
    // Legacy V1 single-function properties (preserved for 100% backward compatibility)
    std::string functionName;
    std::vector<std::string> parameters;
    std::vector<BehavioralExample> samples;
    std::vector<BehavioralExample> checks;
    std::size_t maxDepth{3};

    // Extended V2 multi-component, custom types, enums, concepts & modules
    std::vector<GoalModule> modules;
    std::vector<GoalEnum> enums;
    std::vector<GoalConcept> concepts;
    std::vector<BehavioralTypeGoal> types;
    std::vector<BehavioralFunctionGoal> functions;
    std::string entryFunction;

    // Pre-verified components (from ComponentExperienceStore or earlier developmental stages)
    // that can be composed and invoked by newly induced functions without re-inducing them.
    std::vector<GoalType> importedTypes;
    std::vector<GoalFunction> importedFunctions;
};

struct InducedArchitectureGoal {
    TypedArchitectureGoal goal;
    std::string report;
    std::size_t totalSearchNodes{0};
    std::size_t reusedComponents{0};
    std::vector<std::string> reusedComponentNames;
};

BehavioralGoal loadBehavioralGoal(const std::filesystem::path& file);
BehavioralGoal parseBehavioralGoalText(const std::string& text);
InducedArchitectureGoal induceArchitectureGoal(const BehavioralGoal& input,
                                               const Cpp20SourceGenome& genome);
std::string serializeTypedArchitectureGoal(const TypedArchitectureGoal& goal);

} // namespace genomic::gpi
