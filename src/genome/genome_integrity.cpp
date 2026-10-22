#include "genome/genome_integrity.hpp"
#include "runtime/substrate_kernel.hpp"
#include <unordered_set>
#include <sstream>

namespace genomic {

uint32_t GenomeIntegrity::computeCrc32(const std::vector<GenomeBase>& bases) {
    std::vector<uint8_t> byteData;
    byteData.reserve(bases.size());
    for (auto b : bases) {
        byteData.push_back(static_cast<uint8_t>(b));
    }
    return SubstrateKernel::crc32(byteData.data(), byteData.size());
}

uint32_t GenomeIntegrity::computeCrc32(const std::vector<uint8_t>& data) {
    return SubstrateKernel::crc32(data.data(), data.size());
}

uint64_t GenomeIntegrity::computeHash64(const std::vector<GenomeBase>& bases) {
    std::vector<uint8_t> byteData;
    byteData.reserve(bases.size());
    for (auto b : bases) {
        byteData.push_back(static_cast<uint8_t>(b));
    }
    return SubstrateKernel::fnv1a64(byteData.data(), byteData.size());
}

uint64_t GenomeIntegrity::computeHash64(const std::vector<uint8_t>& data) {
    return SubstrateKernel::fnv1a64(data.data(), data.size());
}

uint64_t GenomeIntegrity::computeRegionHash(const GenomeRegion& region) {
    // Hash composition of id, kind, offset, length, bases, payload, and references
    std::vector<uint8_t> bytes;
    auto appendU64 = [&](uint64_t val) {
        for (int i = 0; i < 8; ++i) {
            bytes.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    };
    appendU64(region.id);
    appendU64(static_cast<uint64_t>(region.kind));
    appendU64(region.baseOffset);
    appendU64(region.baseLength);
    for (auto b : region.bases) {
        bytes.push_back(static_cast<uint8_t>(b));
    }
    for (auto p : region.payload) {
        bytes.push_back(p);
    }
    for (auto ref : region.referencedRegions) {
        appendU64(ref);
    }
    return SubstrateKernel::fnv1a64(bytes.data(), bytes.size());
}

IntegrityReport GenomeIntegrity::validateStrand(
    const std::vector<GenomeBase>& fullStrand,
    const std::vector<GenomeRegion>& regions,
    uint64_t expectedStrandHash
) {
    IntegrityReport report;
    report.regionCount = static_cast<uint32_t>(regions.size());
    report.globalStrandHash = computeHash64(fullStrand);

    if (expectedStrandHash != 0 && report.globalStrandHash != expectedStrandHash) {
        report.passed = false;
        std::ostringstream ss;
        ss << "Global strand hash mismatch! Expected: 0x" << std::hex << expectedStrandHash
           << ", Computed: 0x" << report.globalStrandHash;
        report.errors.push_back(ss.str());
    }

    std::unordered_set<GenomeRegionId> regionIds;

    for (const auto& r : regions) {
        if (regionIds.count(r.id)) {
            report.passed = false;
            report.errors.push_back("Duplicate region ID detected: " + std::to_string(r.id));
        }
        regionIds.insert(r.id);

        if (r.baseOffset + r.baseLength > fullStrand.size()) {
            report.passed = false;
            report.errors.push_back("Region " + std::to_string(r.id) + " (" + r.name + ") exceeds strand length");
        } else {
            // Verify region bases match full strand slice
            bool sliceMatches = true;
            for (size_t i = 0; i < r.baseLength; ++i) {
                if (fullStrand[r.baseOffset + i] != r.bases[i]) {
                    sliceMatches = false;
                    break;
                }
            }
            if (!sliceMatches) {
                report.passed = false;
                report.errors.push_back("Region " + std::to_string(r.id) + " bases do not match physical strand slice");
            }
        }

        // Verify CRC32
        uint32_t computedCrc = computeCrc32(r.bases);
        if (r.crc32 != 0 && r.crc32 != computedCrc) {
            report.passed = false;
            report.errors.push_back("Region " + std::to_string(r.id) + " CRC32 mismatch");
        }

        // Verify region hash
        uint64_t computedHash = computeRegionHash(r);
        if (r.hash64 != 0 && r.hash64 != computedHash) {
            report.passed = false;
            report.errors.push_back("Region " + std::to_string(r.id) + " Hash64 mismatch");
        }
    }

    validateReferenceGraph(regions, report.errors);
    if (!report.errors.empty()) {
        report.passed = false;
    }

    return report;
}

bool GenomeIntegrity::validateReferenceGraph(
    const std::vector<GenomeRegion>& regions,
    std::vector<std::string>& outErrors
) {
    std::unordered_set<GenomeRegionId> knownIds;
    for (const auto& r : regions) {
        knownIds.insert(r.id);
    }

    bool valid = true;
    for (const auto& r : regions) {
        for (auto targetId : r.referencedRegions) {
            if (knownIds.find(targetId) == knownIds.end()) {
                valid = false;
                outErrors.push_back("Region " + std::to_string(r.id) + " (" + r.name +
                                    ") has dangling reference to non-existent region " + std::to_string(targetId));
            }
        }
    }
    return valid;
}

} // namespace genomic
