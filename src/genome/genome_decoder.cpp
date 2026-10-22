#include "genome/genome_decoder.hpp"
#include <sstream>

namespace genomic {

std::vector<GenomeBase> GenomeDecoder::encodeBytesToBases(const uint8_t* data, size_t length) {
    std::vector<GenomeBase> bases;
    bases.reserve(length * 4);
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        for (int shift = 6; shift >= 0; shift -= 2) {
            uint8_t val = (byte >> shift) & 0x03;
            bases.push_back(static_cast<GenomeBase>(val));
        }
    }
    return bases;
}

std::vector<uint8_t> GenomeDecoder::decodeBasesToBytes(const std::vector<GenomeBase>& bases) {
    std::vector<uint8_t> bytes;
    size_t fullBytes = bases.size() / 4;
    bytes.reserve(fullBytes);
    for (size_t i = 0; i < fullBytes; ++i) {
        uint8_t b = 0;
        for (int j = 0; j < 4; ++j) {
            b = (b << 2) | (static_cast<uint8_t>(bases[i * 4 + j]) & 0x03);
        }
        bytes.push_back(b);
    }
    return bytes;
}

std::vector<GenomeBase> GenomeDecoder::encodeString(const std::string& str) {
    return encodeBytesToBases(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::string GenomeDecoder::decodeString(const std::vector<GenomeBase>& bases) {
    auto bytes = decodeBasesToBytes(bases);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::vector<DecodedInstruction> GenomeDecoder::decodeInstructions(const std::vector<Codon>& codons) {
    std::vector<DecodedInstruction> instructions;
    instructions.reserve(codons.size());
    for (size_t i = 0; i < codons.size(); ++i) {
        DecodedInstruction inst;
        inst.opcode = codons[i].toOpcode();
        inst.strandPosition = i * 3;
        instructions.push_back(std::move(inst));
    }
    return instructions;
}

std::vector<GenomeBase> GenomeDecoder::encodeRegion(RegionKind kind,
                                                    const std::string& name,
                                                    const std::vector<uint8_t>& payload) {
    std::vector<GenomeBase> bases;

    // Header: BEGIN codon
    Codon beginCodon = Codon::fromOpcode(GisaOpcode::BEGIN);
    bases.push_back(beginCodon.b1);
    bases.push_back(beginCodon.b2);
    bases.push_back(beginCodon.b3);

    // Kind as 2 bytes
    uint16_t kindVal = static_cast<uint16_t>(kind);
    uint8_t kindBytes[2] = {static_cast<uint8_t>((kindVal >> 8) & 0xFF), static_cast<uint8_t>(kindVal & 0xFF)};
    auto kindBases = encodeBytesToBases(kindBytes, 2);
    bases.insert(bases.end(), kindBases.begin(), kindBases.end());

    // Name length (2 bytes) + Name bytes
    uint16_t nameLen = static_cast<uint16_t>(name.size());
    uint8_t nameLenBytes[2] = {static_cast<uint8_t>((nameLen >> 8) & 0xFF), static_cast<uint8_t>(nameLen & 0xFF)};
    auto nameLenBases = encodeBytesToBases(nameLenBytes, 2);
    bases.insert(bases.end(), nameLenBases.begin(), nameLenBases.end());

    auto nameBases = encodeString(name);
    bases.insert(bases.end(), nameBases.begin(), nameBases.end());

    // Payload length (4 bytes) + Payload bytes
    uint32_t payloadLen = static_cast<uint32_t>(payload.size());
    uint8_t payloadLenBytes[4] = {
        static_cast<uint8_t>((payloadLen >> 24) & 0xFF),
        static_cast<uint8_t>((payloadLen >> 16) & 0xFF),
        static_cast<uint8_t>((payloadLen >> 8) & 0xFF),
        static_cast<uint8_t>(payloadLen & 0xFF)
    };
    auto payloadLenBases = encodeBytesToBases(payloadLenBytes, 4);
    bases.insert(bases.end(), payloadLenBases.begin(), payloadLenBases.end());

    auto payloadBases = encodeBytesToBases(payload.data(), payload.size());
    bases.insert(bases.end(), payloadBases.begin(), payloadBases.end());

    // Trailer: END codon
    Codon endCodon = Codon::fromOpcode(GisaOpcode::END);
    bases.push_back(endCodon.b1);
    bases.push_back(endCodon.b2);
    bases.push_back(endCodon.b3);

    return bases;
}

std::vector<GenomeRegion> GenomeDecoder::parseRegionsFromStrand(const std::vector<GenomeBase>& strand) {
    std::vector<GenomeRegion> parsed;
    size_t i = 0;
    GenomeRegionId nextId = 1;

    Codon beginCodon = Codon::fromOpcode(GisaOpcode::BEGIN);
    Codon endCodon = Codon::fromOpcode(GisaOpcode::END);

    while (i + 3 <= strand.size()) {
        Codon c{strand[i], strand[i+1], strand[i+2]};
        if (c == beginCodon) {
            size_t regionStart = i;
            i += 3; // Skip BEGIN

            // Read kind (2 bytes = 8 bases)
            if (i + 8 > strand.size()) break;
            std::vector<GenomeBase> kindBases(strand.begin() + i, strand.begin() + i + 8);
            auto kindBytes = decodeBasesToBytes(kindBases);
            uint16_t kindVal = (static_cast<uint16_t>(kindBytes[0]) << 8) | kindBytes[1];
            i += 8;

            // Read name length (2 bytes = 8 bases)
            if (i + 8 > strand.size()) break;
            std::vector<GenomeBase> nameLenBases(strand.begin() + i, strand.begin() + i + 8);
            auto nameLenBytes = decodeBasesToBytes(nameLenBases);
            uint16_t nameLen = (static_cast<uint16_t>(nameLenBytes[0]) << 8) | nameLenBytes[1];
            i += 8;

            // Read name bases
            size_t nameBasesCount = nameLen * 4;
            if (i + nameBasesCount > strand.size()) break;
            std::vector<GenomeBase> nameBases(strand.begin() + i, strand.begin() + i + nameBasesCount);
            std::string name = decodeString(nameBases);
            i += nameBasesCount;

            // Read payload length (4 bytes = 16 bases)
            if (i + 16 > strand.size()) break;
            std::vector<GenomeBase> payloadLenBases(strand.begin() + i, strand.begin() + i + 16);
            auto pLenBytes = decodeBasesToBytes(payloadLenBases);
            uint32_t payloadLen = (static_cast<uint32_t>(pLenBytes[0]) << 24) |
                                  (static_cast<uint32_t>(pLenBytes[1]) << 16) |
                                  (static_cast<uint32_t>(pLenBytes[2]) << 8)  |
                                   static_cast<uint32_t>(pLenBytes[3]);
            i += 16;

            // Read payload bases
            size_t payloadBasesCount = payloadLen * 4;
            if (i + payloadBasesCount > strand.size()) break;
            std::vector<GenomeBase> payloadBases(strand.begin() + i, strand.begin() + i + payloadBasesCount);
            auto payloadBytes = decodeBasesToBytes(payloadBases);
            i += payloadBasesCount;

            // Check END codon
            if (i + 3 <= strand.size()) {
                Codon checkEnd{strand[i], strand[i+1], strand[i+2]};
                if (checkEnd == endCodon) {
                    i += 3;
                    size_t regionLen = i - regionStart;
                    GenomeRegion reg;
                    reg.id = static_cast<uint64_t>(kindVal) * 10000 + nextId++;
                    reg.kind = static_cast<RegionKind>(kindVal);
                    reg.name = name;
                    reg.baseOffset = regionStart;
                    reg.baseLength = regionLen;
                    reg.bases = std::vector<GenomeBase>(strand.begin() + regionStart, strand.begin() + i);
                    reg.payload = payloadBytes;
                    reg.crc32 = GenomeIntegrity::computeCrc32(reg.bases);
                    reg.hash64 = GenomeIntegrity::computeRegionHash(reg);
                    parsed.push_back(std::move(reg));
                    continue;
                }
            }
        }
        i += 1;
    }

    return parsed;
}

} // namespace genomic
