#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "genome/genome_base.hpp"
#include "genome/genome_region.hpp"
#include "genome/genome_strand.hpp"

namespace genomic {

struct DecodedInstruction {
    GisaOpcode opcode;
    std::vector<uint8_t> arguments;
    size_t strandPosition;
};

class GenomeDecoder {
public:
    // Encode string / raw bytes into base sequence using GISA codons
    static std::vector<GenomeBase> encodeBytesToBases(const uint8_t* data, size_t length);
    static std::vector<uint8_t> decodeBasesToBytes(const std::vector<GenomeBase>& bases);

    // Encode string directly
    static std::vector<GenomeBase> encodeString(const std::string& str);
    static std::string decodeString(const std::vector<GenomeBase>& bases);

    // Decode full instruction stream from codons
    static std::vector<DecodedInstruction> decodeInstructions(const std::vector<Codon>& codons);

    // Encode structured region into bases
    static std::vector<GenomeBase> encodeRegion(RegionKind kind,
                                                const std::string& name,
                                                const std::vector<uint8_t>& payload);

    // Parse bases into structured regions
    static std::vector<GenomeRegion> parseRegionsFromStrand(const std::vector<GenomeBase>& strand);
};

} // namespace genomic
