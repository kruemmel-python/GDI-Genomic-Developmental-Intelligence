#include "genome/genome_strand.hpp"
#include <stdexcept>
#include <sstream>

namespace genomic {

GenomeStrand::GenomeStrand()
    : genomeHash_(0), frozen_(false), nextRegionId_(1) {}

void GenomeStrand::appendBases(const std::vector<GenomeBase>& bases) {
    if (frozen_) {
        throw std::runtime_error("Invariant violation: Attempted to mutate an immutable GenomeStrand after freeze!");
    }
    physicalStrand_.insert(physicalStrand_.end(), bases.begin(), bases.end());
}

void GenomeStrand::appendBasesFromString(const std::string& sequence) {
    if (frozen_) {
        throw std::runtime_error("Invariant violation: Attempted to mutate an immutable GenomeStrand after freeze!");
    }
    physicalStrand_.reserve(physicalStrand_.size() + sequence.size());
    for (char c : sequence) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        physicalStrand_.push_back(charToBase(c));
    }
}

GenomeRegionId GenomeStrand::addRegion(RegionKind kind,
                                      const std::string& name,
                                      const std::vector<GenomeBase>& regionBases,
                                      const std::vector<uint8_t>& payload,
                                      const std::vector<GenomeRegionId>& references,
                                      const std::string& description) {
    if (frozen_) {
        throw std::runtime_error("Invariant violation: Attempted to add region to an immutable GenomeStrand after freeze!");
    }

    GenomeRegionId id = static_cast<uint64_t>(kind) * 10000 + nextRegionId_++;
    size_t offset = physicalStrand_.size();
    size_t length = regionBases.size();

    // Append to physical strand
    physicalStrand_.insert(physicalStrand_.end(), regionBases.begin(), regionBases.end());

    GenomeRegion reg;
    reg.id = id;
    reg.kind = kind;
    reg.name = name;
    reg.baseOffset = offset;
    reg.baseLength = length;
    reg.bases = regionBases;
    reg.payload = payload;
    reg.referencedRegions = references;
    reg.description = description;
    reg.crc32 = GenomeIntegrity::computeCrc32(reg.bases);
    reg.hash64 = GenomeIntegrity::computeRegionHash(reg);

    size_t index = regions_.size();
    regions_.push_back(std::move(reg));
    regionIndexMap_[id] = index;

    return id;
}

void GenomeStrand::freeze() {
    if (frozen_) return;

    // Recalculate CRC and hashes for all regions
    for (auto& reg : regions_) {
        reg.crc32 = GenomeIntegrity::computeCrc32(reg.bases);
        reg.hash64 = GenomeIntegrity::computeRegionHash(reg);
    }

    genomeHash_ = GenomeIntegrity::computeHash64(physicalStrand_);
    frozen_ = true;

    // Verify integrity immediately upon freeze
    auto report = verifyIntegrity();
    if (!report.passed) {
        std::ostringstream ss;
        ss << "Genome integrity verification failed on freeze with " << report.errors.size() << " errors:\n";
        for (const auto& err : report.errors) {
            ss << " - " << err << "\n";
        }
        throw std::runtime_error(ss.str());
    }
}

bool GenomeStrand::isFrozen() const noexcept {
    return frozen_;
}

size_t GenomeStrand::length() const noexcept {
    return physicalStrand_.size();
}

const std::vector<GenomeBase>& GenomeStrand::getBases() const noexcept {
    return physicalStrand_;
}

std::string GenomeStrand::getSequenceString() const {
    std::string s;
    s.reserve(physicalStrand_.size());
    for (auto b : physicalStrand_) {
        s.push_back(baseToChar(b));
    }
    return s;
}

std::vector<Codon> GenomeStrand::getCodons() const {
    std::vector<Codon> codons;
    size_t count = physicalStrand_.size() / 3;
    codons.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        codons.push_back(Codon{
            physicalStrand_[i * 3],
            physicalStrand_[i * 3 + 1],
            physicalStrand_[i * 3 + 2]
        });
    }
    return codons;
}

bool GenomeStrand::hasRegion(GenomeRegionId id) const noexcept {
    return regionIndexMap_.find(id) != regionIndexMap_.end();
}

const GenomeRegion& GenomeStrand::getRegion(GenomeRegionId id) const {
    auto it = regionIndexMap_.find(id);
    if (it == regionIndexMap_.end()) {
        throw std::out_of_range("Region ID not found: " + std::to_string(id));
    }
    return regions_[it->second];
}

const std::vector<GenomeRegion>& GenomeStrand::getAllRegions() const noexcept {
    return regions_;
}

uint64_t GenomeStrand::getGenomeHash() const noexcept {
    return genomeHash_;
}

IntegrityReport GenomeStrand::verifyIntegrity() const {
    return GenomeIntegrity::validateStrand(physicalStrand_, regions_, genomeHash_);
}

std::string GenomeStrand::toFasta(const std::string& header) const {
    std::ostringstream ss;
    ss << ">" << header << " length=" << physicalStrand_.size()
       << " regions=" << regions_.size()
       << " hash=0x" << std::hex << genomeHash_ << "\n";
    for (size_t i = 0; i < physicalStrand_.size(); ++i) {
        ss << baseToChar(physicalStrand_[i]);
        if ((i + 1) % 80 == 0) ss << "\n";
    }
    if (physicalStrand_.size() % 80 != 0) ss << "\n";
    return ss.str();
}

void GenomeStrand::assertImmutable() const {
    if (!frozen_) {
        throw std::runtime_error("GenomeStrand must be frozen before runtime execution");
    }
}

} // namespace genomic
