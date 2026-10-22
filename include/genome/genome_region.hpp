#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "genome/genome_base.hpp"
#include "genome/genome_reference.hpp"

namespace genomic {

enum class RegionKind : uint16_t {
    BootDictionary         = 1,
    SymbolTable            = 2,
    CppGrammar             = 100,
    CppLexicalRule         = 101,
    CppGrammarRule         = 102,
    CppGenerativeRule      = 103,
    TypeSystem             = 200,
    CppTypeRule            = 201,
    CppConversionRule      = 202,
    SemanticRules          = 300,
    CppSemanticRule        = 301,
    CppLookupRule          = 302,
    CppOverloadRule        = 303,
    CppTemplateRule        = 304,
    CppConstraintRule      = 305,
    CppLifetimeRule        = 306,
    CppMemoryModelRule     = 307,
    CompilerLogic          = 400,
    CppLoweringRule        = 401,
    RuntimeLibrary         = 500,
    CppRuntimeRule         = 501,
    RegulatoryNetwork      = 1000,
    DevelopmentalPrograms  = 2000,
    DomainModules          = 3000,
    PromptCapability       = 3100,
    TaskGrammar            = 3101,
    ResearchCapability     = 3102
};

inline std::string regionKindToString(RegionKind kind) {
    switch (kind) {
        case RegionKind::BootDictionary: return "BootDictionary";
        case RegionKind::SymbolTable: return "SymbolTable";
        case RegionKind::CppGrammar: return "CppGrammar";
        case RegionKind::CppLexicalRule: return "CppLexicalRule";
        case RegionKind::CppGrammarRule: return "CppGrammarRule";
        case RegionKind::CppGenerativeRule: return "CppGenerativeRule";
        case RegionKind::TypeSystem: return "TypeSystem";
        case RegionKind::CppTypeRule: return "CppTypeRule";
        case RegionKind::CppConversionRule: return "CppConversionRule";
        case RegionKind::SemanticRules: return "SemanticRules";
        case RegionKind::CppSemanticRule: return "CppSemanticRule";
        case RegionKind::CppLookupRule: return "CppLookupRule";
        case RegionKind::CppOverloadRule: return "CppOverloadRule";
        case RegionKind::CppTemplateRule: return "CppTemplateRule";
        case RegionKind::CppConstraintRule: return "CppConstraintRule";
        case RegionKind::CppLifetimeRule: return "CppLifetimeRule";
        case RegionKind::CppMemoryModelRule: return "CppMemoryModelRule";
        case RegionKind::CompilerLogic: return "CompilerLogic";
        case RegionKind::CppLoweringRule: return "CppLoweringRule";
        case RegionKind::RuntimeLibrary: return "RuntimeLibrary";
        case RegionKind::CppRuntimeRule: return "CppRuntimeRule";
        case RegionKind::RegulatoryNetwork: return "RegulatoryNetwork";
        case RegionKind::DevelopmentalPrograms: return "DevelopmentalPrograms";
        case RegionKind::DomainModules: return "DomainModules";
        case RegionKind::PromptCapability: return "PromptCapability";
        case RegionKind::TaskGrammar: return "TaskGrammar";
        case RegionKind::ResearchCapability: return "ResearchCapability";
    }
    return "UnknownRegion";
}

struct GenomeRegion {
    GenomeRegionId id{0};
    RegionKind kind{RegionKind::DomainModules};
    std::string name;
    size_t baseOffset{0};
    size_t baseLength{0};
    std::vector<GenomeBase> bases;
    std::vector<uint8_t> payload;
    uint32_t crc32{0};
    uint64_t hash64{0};
    std::vector<GenomeRegionId> referencedRegions;
    std::string description;

    bool operator==(const GenomeRegion& other) const noexcept {
        return id == other.id &&
               kind == other.kind &&
               name == other.name &&
               baseOffset == other.baseOffset &&
               baseLength == other.baseLength &&
               crc32 == other.crc32 &&
               hash64 == other.hash64 &&
               referencedRegions == other.referencedRegions;
    }
};

} // namespace genomic
