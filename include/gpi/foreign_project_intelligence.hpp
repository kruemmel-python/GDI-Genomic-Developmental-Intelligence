#pragma once

#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_parser.hpp"
#include "cpp20/cpp20_semantics.hpp"
#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/developmental_intelligence.hpp"
#include "gpi/generative_project.hpp"
#include "gpi/project_experience.hpp"
#include "gpi/project_verifier.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

// =========================================================================
// 1. Foreign Software Project Discovery & Deep Language/Architecture Model
// =========================================================================

struct DiscoveredSymbol {
    std::string kind;          // "type", "enum", "concept", "function", "template_function", "method"
    std::string name;          // e.g. "SensorReading", "total_value", "clamp_value"
    std::string qualifiedName; // e.g. "telemetry::SensorReading", "telemetry::SensorReading::scaled"
    std::string returnType;    // For functions/methods
    std::vector<std::pair<std::string, std::string>> parametersOrFields; // (type, name)
    std::string definingNodeId;
    std::string definingPath;
    std::string originKrPath;  // Non-empty if mapped from a primary .kr source file!
    bool isGeneratedFromKr{false};
    bool hasDeclarationInHeader{false};
    bool hasDefinitionInSource{false};
    bool isTested{false};
    std::vector<std::string> calledFunctions; // Intra-project dataflow / call graph edges
};

struct DiscoveredModuleInterface {
    std::string moduleName;
    std::string headerNodeId;
    std::string sourceNodeId;
    std::string originKrPath;  // e.g. "workspace/dumper.kr" when generated inside KR_BUILD/
    bool isGeneratedKrBuildArtifact{false};
    std::vector<std::string> includedHeaders;
    std::vector<std::string> exportedTypes;
    std::vector<std::string> exportedFunctions;
    std::vector<std::string> unimplementedDeclarations; // Declared in header, missing in source!
    std::vector<std::string> untestedFunctions;         // Defined in module, never called in tests!
};

struct CrossLanguageAbiContract {
    std::string sourceLanguage;
    std::string sourcePath;
    std::string functionName;
    std::string returnType;
    std::vector<std::pair<std::string,std::string>> parameters;
    std::string cppExpression;
    bool semanticallyDerivable{false};
    std::string diagnostic;
};

enum class ArchitecturalHypothesisKind : std::uint8_t {
    MissingStandardHeader = 1,
    MissingModuleLinkDependency = 2,
    TypeMismatchOrConversionDefect = 3,
    UnimplementedInterfaceContract = 4,
    UntestedPublicCapability = 5,
    ComposableDomainCapability = 6,
    DeprecatedCpp20ApiUsage = 7,
    GeneratedKrBuildSeparation = 8
};

struct ArchitecturalHypothesis {
    std::string id;
    ArchitecturalHypothesisKind kind{ArchitecturalHypothesisKind::ComposableDomainCapability};
    std::string targetModule;
    std::string targetSymbol;
    std::string hypothesisStatement;
    std::string investigationMethod;
    std::string derivedDevelopmentGoal;
    double confidence{0.0};
    bool investigated{false};
    bool confirmed{false};
    bool resolvedAndVerified{false};
};

// =========================================================================
// 2. Dual Memory Architecture:
//    - ProjectMemoryStore (GDIPM1): Concrete knowledge about a specific foreign project
//    - DevelopmentalPrincipleStore (GDIDP1): Transferable engineering & synthesis principles
// =========================================================================

struct ForeignProjectKnowledgeRecord {
    std::uint64_t genomeHash{0};
    std::uint64_t projectFingerprint{0};
    std::string projectName;
    std::string category;   // "module", "type", "function", "contract", "defect", "capability"
    std::string symbolName;
    std::string details;
    std::uint64_t verifiedGraphHash{0};
};

class ProjectMemoryStore {
public:
    explicit ProjectMemoryStore(std::filesystem::path path) : path_(std::move(path)) {}
    std::vector<ForeignProjectKnowledgeRecord> load() const;
    std::vector<ForeignProjectKnowledgeRecord> loadForProject(std::uint64_t projectFingerprint) const;
    void appendGroup(const std::vector<ForeignProjectKnowledgeRecord>& records) const;
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

// Transferable developmental principle learned from analyzing & evolving foreign projects.
// Unlike ProjectMemoryStore (which records facts about Project A), DevelopmentalPrincipleStore
// captures generalizable C++20 synthesis & repair patterns that transfer to Project B!
struct TransferableDevelopmentalPrinciple {
    std::uint64_t genomeHash{0};
    std::uint64_t originProjectFingerprint{0};
    std::uint64_t verifiedEvidenceHash{0};
    std::string principleId;      // e.g. "principle:interface_completion:accumulate_field", "principle:header_guard:iostream"
    std::string patternCategory;  // "interface_synthesis", "header_repair", "link_topology", "type_bridge", "component_template"
    std::string triggerSignature; // Abstract structural signature (e.g. "unimplemented_fn(struct_pair)->scalar_sum")
    std::string actionTemplate;   // Parameterized synthesis/transformation rule
    std::string rationale;
};

class DevelopmentalPrincipleStore {
public:
    explicit DevelopmentalPrincipleStore(std::filesystem::path path) : path_(std::move(path)) {}
    std::vector<TransferableDevelopmentalPrinciple> load() const;
    void appendGroup(const std::vector<TransferableDevelopmentalPrinciple>& principles) const;
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

// =========================================================================
// 3. Coupled Iterative Foreign Project Analysis & Development Loop
//    (Analyze -> Understand -> Hypothesize -> Derive Goal -> Implement -> Verify -> Consolidate)
// =========================================================================

struct ForeignProjectIterationReport {
    std::size_t iterationNumber{0};
    std::uint64_t graphHashBefore{0};
    std::uint64_t graphHashAfter{0};
    std::vector<std::string> activeHypotheses;
    std::string targetedProbeOrGoal;
    std::string actionTaken;
    bool compilationPassed{false};
    bool behavioralTestsPassed{false};
    std::vector<std::string> newlyDiscoveredFacts;
    std::vector<std::string> newlyLearnedPrinciples;
};

struct ForeignProjectAnalysisReport {
    std::string projectName;
    std::filesystem::path originalRoot;
    std::uint64_t originalDirectoryHash{0}; // Cryptographic/FNV proof that original project files were not touched
    std::uint64_t immutableGenomeHash{0};
    std::size_t discoveredFiles{0};
    std::size_t discoveredKrSourceFiles{0};
    std::size_t discoveredGeneratedKrBuildFiles{0};
    std::vector<std::string> krSourceFiles;
    std::vector<std::string> generatedKrBuildFiles;
    std::vector<DiscoveredModuleInterface> modules;
    std::vector<DiscoveredSymbol> symbols;
    std::vector<ArchitecturalHypothesis> hypotheses;
    std::vector<std::string> semanticDiagnostics;
    std::vector<std::string> dataflowEdges;
    std::vector<CrossLanguageAbiContract> abiContracts;
    std::string summaryText;
};

struct ForeignProjectDevelopmentConfig {
    std::filesystem::path foreignProjectRoot; // Read-only unless allowWritebackToOriginal is explicitly true
    std::filesystem::path isolatedWorkspace;  // All builds, probes, syntheses, and tests run here
    std::filesystem::path compiler{"g++"};
    std::filesystem::path sandboxRunner;
    std::filesystem::path projectMemoryPath;        // GDIPM1 (Project-specific memory)
    std::filesystem::path developmentalMemoryPath;  // GDIDP1 (Transferable developmental principles)
    std::filesystem::path componentMemoryPath;      // GDIC1  (Reusable synthesized components)
    std::filesystem::path repairExperiencePath;     // GPIG2  (Verified AST/source repair transformations)
    std::size_t maxIterations{6};
    bool requireSandbox{true};
    bool allowWritebackToOriginal{false}; // Safety gate: default false (original foreign project remains untouched!)
};

struct ForeignProjectDevelopmentResult {
    bool analysisSucceeded{false};
    bool developmentVerified{false};
    bool originalProjectUntouched{true};
    bool writebackPerformed{false};
    bool transferredPrincipleApplied{false};
    std::uint64_t immutableGenomeHash{0};
    std::uint64_t originalDirectoryHashBefore{0};
    std::uint64_t originalDirectoryHashAfter{0};
    std::string originalSha256Before;
    std::string originalSha256After;
    std::uint64_t finalVerifiedGraphHash{0};
    ForeignProjectAnalysisReport initialAnalysis;
    ForeignProjectAnalysisReport finalAnalysis;
    std::vector<ForeignProjectIterationReport> iterations;
    std::vector<ForeignProjectKnowledgeRecord> projectMemoryRecords;
    std::vector<TransferableDevelopmentalPrinciple> learnedPrinciples;
    std::vector<std::string> appliedTransferredPrinciples;
    std::filesystem::path isolatedProjectOutputDir;
    std::filesystem::path reportPath;
};

// Computes a deterministic hash over all files and relative paths in a directory tree
// to enforce and prove the read-only safety boundary on foreign projects.
std::uint64_t computeDirectoryTreeHash(const std::filesystem::path& rootDir);

// Performs a read-only genomic C++20 language, AST, semantic, module, dataflow, and contract
// analysis of an arbitrary foreign C++20 project directory (with or without project.gpi)
// and immediately formulates architectural and developmental hypotheses.
ForeignProjectAnalysisReport analyzeForeignCpp20Project(
    const std::filesystem::path& foreignProjectRoot,
    const std::filesystem::path& projectMemoryPath = {},
    const std::filesystem::path& developmentalMemoryPath = {});

// Executes the coupled iterative cycle on a foreign C++20 project inside an isolated workspace:
// Analyze -> Understand -> Formulate Hypotheses -> Derive Development Goal ->
// Synthesize/Repair/Extend -> Compile & Sandbox Verify -> Consolidate Project & Developmental Memory.
ForeignProjectDevelopmentResult runForeignProjectDevelopmentalLoop(
    const ForeignProjectDevelopmentConfig& config);

} // namespace genomic::gpi
