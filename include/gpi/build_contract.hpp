#pragma once

#include "gpi/process_runner.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class ContractEvidence { Discovered, Inferred, Verified, Unavailable, Conflicting };
enum class BuildSystemKind { CMake, MSBuild, Ninja, Make, DirectCompiler, KrUnified, Mixed, Unknown };
enum class BuildBaselineStatus {
    BuildContractUnverified, BuildContractVerified, BaselineReproduced,
    BaselineFailedProjectDefect, BaselineBlockedEnvironment,
    BaselineBlockedToolchain, BaselineInconclusive
};
enum class BuildDiagnosticClass {
    SourceCode, MissingConfiguredInclude, MissingSdk, WrongCompilerFamily,
    IncompatibleCompilerOption, MissingLinkLibrary, WrongTargetDependency,
    MissingGeneratedSource, AbiIncompatibility, ResourceCompiler, CMakeConfiguration,
    TestFailure, RuntimeFailure, ReconstructionFailure, Unknown
};

struct ContractValue {
    std::string value;
    ContractEvidence evidence{ContractEvidence::Unavailable};
    std::string origin;
};
struct BuildTargetContract {
    std::string name;
    std::string kind;
    std::vector<std::string> sources;
    std::vector<std::string> dependencies;
    std::vector<std::string> linkLibraries;
    ContractEvidence evidence{ContractEvidence::Discovered};
};
struct BuildGraphEdge { std::string consumer, provider, relation, origin; };

struct BuildContract {
    std::string format{"GDIBC1"};
    std::filesystem::path projectRoot;
    BuildSystemKind buildSystem{BuildSystemKind::Unknown};
    ContractValue buildSystemVersion, generator, generatorPlatform, compilerFamily,
        compilerVersion, targetArchitecture, cCompiler, cppCompiler, rcCompiler,
        windowsSdk, toolchainFile, languageStandard, workingDirectory;
    std::vector<std::string> configurations, includeDirectories, definitions,
        compileFeatures, compileOptions, linkDirectories, linkOptions, linkLibraries,
        staticLibraries, dynamicLibraries, executables, resources, generatedFiles,
        preBuildCommands, postBuildCommands, customCommands, testTargets,
        runtimeDependencies;
    std::map<std::string,std::string> environment;
    std::vector<BuildTargetContract> targets;
    std::vector<BuildGraphEdge> graph;
    std::vector<std::string> discoveryEvidence, conflicts;
    bool nativeBuildRequired{true};
    bool verified{false};
};

struct BuildDiagnostic {
    BuildDiagnosticClass classification{BuildDiagnosticClass::Unknown};
    std::string message, evidence, target, contractOrigin;
    bool causedByReconstruction{false};
};
struct NativeBuildStep {
    std::string name;
    std::vector<std::string> command;
    ProcessResult process;
    std::filesystem::path stdoutLog, stderrLog;
};
struct NativeBuildResult {
    BuildBaselineStatus baselineStatus{BuildBaselineStatus::BuildContractUnverified};
    bool configured{false}, built{false}, testsPassed{false}, originalUntouched{false};
    std::filesystem::path isolatedProject, buildDirectory, reportPath;
    std::vector<NativeBuildStep> steps;
    std::vector<BuildDiagnostic> diagnostics;
    std::vector<std::filesystem::path> artifacts;
    BuildContract executedContract;
    std::string originalManifestBefore, originalManifestAfter;
};
struct NativeBuildConfig {
    std::filesystem::path originalProject, isolatedWorkspace;
    std::string configuration{"Release"};
    std::size_t timeoutMilliseconds{600000};
    std::size_t testTimeoutMilliseconds{2400000};
    bool runTests{true};
    std::filesystem::path cmakeExecutable{"cmake"};
    std::filesystem::path ctestExecutable{"ctest"};
    std::filesystem::path msbuildExecutable;
    std::filesystem::path ninjaExecutable{"ninja"};
    std::filesystem::path makeExecutable{"make"};
    std::filesystem::path cppCompilerOverride;
    std::string generatorOverride;
    std::string generatorPlatformOverride;
    std::string windowsSdkOverride;
};

BuildContract discoverBuildContract(const std::filesystem::path& projectRoot);
void saveBuildContract(const BuildContract& contract,const std::filesystem::path& path);
BuildContract loadBuildContract(const std::filesystem::path& path);
std::string renderBuildContractJson(const BuildContract& contract);
NativeBuildResult executeNativeBuild(const BuildContract& contract,const NativeBuildConfig& config);
std::vector<BuildDiagnostic> classifyBuildDiagnostics(const BuildContract& contract,
                                                       const std::string& output,
                                                       bool reconstructionFailed=false);
const char* toString(ContractEvidence value) noexcept;
const char* toString(BuildSystemKind value) noexcept;
const char* toString(BuildBaselineStatus value) noexcept;

} // namespace genomic::gpi
