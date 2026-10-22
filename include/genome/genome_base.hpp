#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>
#include <array>

namespace genomic {

enum class GenomeBase : uint8_t {
    A = 0,
    C = 1,
    G = 2,
    T = 3
};

inline char baseToChar(GenomeBase b) noexcept {
    switch (b) {
        case GenomeBase::A: return 'A';
        case GenomeBase::C: return 'C';
        case GenomeBase::G: return 'G';
        case GenomeBase::T: return 'T';
    }
    return '?';
}

inline GenomeBase charToBase(char c) {
    switch (c) {
        case 'A': case 'a': return GenomeBase::A;
        case 'C': case 'c': return GenomeBase::C;
        case 'G': case 'g': return GenomeBase::G;
        case 'T': case 't': return GenomeBase::T;
        default:
            throw std::invalid_argument(std::string("Invalid genome base character: ") + c);
    }
}

// 64-Codon Genome Instruction Set Architecture (GISA)
enum class GisaOpcode : uint8_t {
    LITERAL         = 0,
    REFERENCE       = 1,
    BEGIN           = 2,
    END             = 3,
    TYPE            = 4,
    VALUE           = 5,
    RULE            = 6,
    BIND            = 7,
    UNBIND          = 8,
    CALL            = 9,
    RETURN          = 10,
    BRANCH          = 11,
    REPEAT          = 12,
    COMPOSE         = 13,
    SPLICE          = 14,
    ANNOTATE        = 15,
    CHECK           = 16,
    HASH            = 17,
    ENHANCER        = 18,
    REPRESSOR       = 19,
    PROMOTER        = 20,
    EXON            = 21,
    INTRON          = 22,
    ONTOLOGY        = 23,
    TRAIT           = 24,
    CONSTRAINT      = 25,
    DEVELOPMENT     = 26,
    STAGE           = 27,
    COMPARTMENT     = 28,
    LINEAGE         = 29,
    EMIT            = 30,
    DECAY           = 31,
    SYNTHESIZE      = 32,
    VALIDATE        = 33,
    EXECUTE         = 34,
    ATOMIC          = 35,
    BARRIER         = 36,
    SUSPEND         = 37,
    RESUME          = 38,
    EXCEPTION       = 39,
    CLEANUP         = 40,
    GUARD           = 41,
    PROVENANCE      = 42,
    DEBUG_INFO      = 43,
    CONST_EXPR      = 44,
    CONCEPT_DEF     = 45,
    OVERLOAD        = 46,
    TEMPLATE_DEF    = 47,
    MODULE_DEF      = 48,
    IMPORT_DEF      = 49,
    CLASS_DEF       = 50,
    VIRTUAL_CALL    = 51,
    STATIC_CALL     = 52,
    MEM_ALLOC       = 53,
    MEM_FREE        = 54,
    JUMP_IF         = 55,
    JUMP            = 56,
    PUSH_FRAME      = 57,
    POP_FRAME       = 58,
    LOAD_REG        = 59,
    STORE_REG       = 60,
    READ_SENSOR     = 61,
    WRITE_ACTUATOR  = 62,
    HALT            = 63
};

struct Codon {
    GenomeBase b1;
    GenomeBase b2;
    GenomeBase b3;

    constexpr uint8_t toIndex() const noexcept {
        return static_cast<uint8_t>(b1) * 16 +
               static_cast<uint8_t>(b2) * 4 +
               static_cast<uint8_t>(b3);
    }

    static constexpr Codon fromIndex(uint8_t idx) {
        return Codon{
            static_cast<GenomeBase>((idx / 16) % 4),
            static_cast<GenomeBase>((idx / 4) % 4),
            static_cast<GenomeBase>(idx % 4)
        };
    }

    constexpr GisaOpcode toOpcode() const noexcept {
        return static_cast<GisaOpcode>(toIndex());
    }

    static constexpr Codon fromOpcode(GisaOpcode op) noexcept {
        return fromIndex(static_cast<uint8_t>(op));
    }

    bool operator==(const Codon& other) const noexcept {
        return b1 == other.b1 && b2 == other.b2 && b3 == other.b3;
    }
};

inline std::string codonToString(const Codon& c) {
    std::string s;
    s += baseToChar(c.b1);
    s += baseToChar(c.b2);
    s += baseToChar(c.b3);
    return s;
}

inline Codon stringToCodon(const std::string& str) {
    if (str.size() < 3) {
        throw std::invalid_argument("Codon string must be at least 3 characters");
    }
    return Codon{charToBase(str[0]), charToBase(str[1]), charToBase(str[2])};
}

inline std::string opcodeToString(GisaOpcode op) {
    switch (op) {
        case GisaOpcode::LITERAL: return "LITERAL";
        case GisaOpcode::REFERENCE: return "REFERENCE";
        case GisaOpcode::BEGIN: return "BEGIN";
        case GisaOpcode::END: return "END";
        case GisaOpcode::TYPE: return "TYPE";
        case GisaOpcode::VALUE: return "VALUE";
        case GisaOpcode::RULE: return "RULE";
        case GisaOpcode::BIND: return "BIND";
        case GisaOpcode::UNBIND: return "UNBIND";
        case GisaOpcode::CALL: return "CALL";
        case GisaOpcode::RETURN: return "RETURN";
        case GisaOpcode::BRANCH: return "BRANCH";
        case GisaOpcode::REPEAT: return "REPEAT";
        case GisaOpcode::COMPOSE: return "COMPOSE";
        case GisaOpcode::SPLICE: return "SPLICE";
        case GisaOpcode::ANNOTATE: return "ANNOTATE";
        case GisaOpcode::CHECK: return "CHECK";
        case GisaOpcode::HASH: return "HASH";
        case GisaOpcode::ENHANCER: return "ENHANCER";
        case GisaOpcode::REPRESSOR: return "REPRESSOR";
        case GisaOpcode::PROMOTER: return "PROMOTER";
        case GisaOpcode::EXON: return "EXON";
        case GisaOpcode::INTRON: return "INTRON";
        case GisaOpcode::ONTOLOGY: return "ONTOLOGY";
        case GisaOpcode::TRAIT: return "TRAIT";
        case GisaOpcode::CONSTRAINT: return "CONSTRAINT";
        case GisaOpcode::DEVELOPMENT: return "DEVELOPMENT";
        case GisaOpcode::STAGE: return "STAGE";
        case GisaOpcode::COMPARTMENT: return "COMPARTMENT";
        case GisaOpcode::LINEAGE: return "LINEAGE";
        case GisaOpcode::EMIT: return "EMIT";
        case GisaOpcode::DECAY: return "DECAY";
        case GisaOpcode::SYNTHESIZE: return "SYNTHESIZE";
        case GisaOpcode::VALIDATE: return "VALIDATE";
        case GisaOpcode::EXECUTE: return "EXECUTE";
        case GisaOpcode::ATOMIC: return "ATOMIC";
        case GisaOpcode::BARRIER: return "BARRIER";
        case GisaOpcode::SUSPEND: return "SUSPEND";
        case GisaOpcode::RESUME: return "RESUME";
        case GisaOpcode::EXCEPTION: return "EXCEPTION";
        case GisaOpcode::CLEANUP: return "CLEANUP";
        case GisaOpcode::GUARD: return "GUARD";
        case GisaOpcode::PROVENANCE: return "PROVENANCE";
        case GisaOpcode::DEBUG_INFO: return "DEBUG_INFO";
        case GisaOpcode::CONST_EXPR: return "CONST_EXPR";
        case GisaOpcode::CONCEPT_DEF: return "CONCEPT_DEF";
        case GisaOpcode::OVERLOAD: return "OVERLOAD";
        case GisaOpcode::TEMPLATE_DEF: return "TEMPLATE_DEF";
        case GisaOpcode::MODULE_DEF: return "MODULE_DEF";
        case GisaOpcode::IMPORT_DEF: return "IMPORT_DEF";
        case GisaOpcode::CLASS_DEF: return "CLASS_DEF";
        case GisaOpcode::VIRTUAL_CALL: return "VIRTUAL_CALL";
        case GisaOpcode::STATIC_CALL: return "STATIC_CALL";
        case GisaOpcode::MEM_ALLOC: return "MEM_ALLOC";
        case GisaOpcode::MEM_FREE: return "MEM_FREE";
        case GisaOpcode::JUMP_IF: return "JUMP_IF";
        case GisaOpcode::JUMP: return "JUMP";
        case GisaOpcode::PUSH_FRAME: return "PUSH_FRAME";
        case GisaOpcode::POP_FRAME: return "POP_FRAME";
        case GisaOpcode::LOAD_REG: return "LOAD_REG";
        case GisaOpcode::STORE_REG: return "STORE_REG";
        case GisaOpcode::READ_SENSOR: return "READ_SENSOR";
        case GisaOpcode::WRITE_ACTUATOR: return "WRITE_ACTUATOR";
        case GisaOpcode::HALT: return "HALT";
    }
    return "UNKNOWN";
}

// Byte packing: 4 bases per byte
inline std::vector<uint8_t> packBases(const std::vector<GenomeBase>& bases) {
    std::vector<uint8_t> packed;
    packed.reserve((bases.size() + 3) / 4);
    uint8_t current = 0;
    for (size_t i = 0; i < bases.size(); ++i) {
        int shift = (3 - (i % 4)) * 2;
        current |= (static_cast<uint8_t>(bases[i]) & 0x3) << shift;
        if ((i % 4) == 3 || i + 1 == bases.size()) {
            packed.push_back(current);
            current = 0;
        }
    }
    return packed;
}

inline std::vector<GenomeBase> unpackBases(const std::vector<uint8_t>& packed, size_t totalBases) {
    std::vector<GenomeBase> bases;
    bases.reserve(totalBases);
    for (size_t i = 0; i < totalBases; ++i) {
        size_t byteIdx = i / 4;
        int shift = (3 - (i % 4)) * 2;
        uint8_t val = (packed[byteIdx] >> shift) & 0x3;
        bases.push_back(static_cast<GenomeBase>(val));
    }
    return bases;
}

} // namespace genomic
