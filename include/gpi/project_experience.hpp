#pragma once

#include "gpi/generative_project.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class ProjectTransformationKind { InsertHeader=1, AddLinkDependency=2, ReplaceExactToken=3 };

struct VerifiedProjectTransformation {
    uint64_t genomeHash{0};
    uint64_t originGraphHash{0};
    uint64_t verifiedGraphHash{0};
    ProjectTransformationKind kind{ProjectTransformationKind::InsertHeader};
    std::string node;
    std::string auxiliary;
    std::string before;
    std::string after;
    uint64_t evidenceHash{0};
};

class ProjectExperienceStore {
public:
    explicit ProjectExperienceStore(std::filesystem::path path):path_(std::move(path)){}
    std::vector<VerifiedProjectTransformation> load() const;
    void append(const VerifiedProjectTransformation& transformation) const;
private:
    std::filesystem::path path_;
};

GenerativeProjectGraph applyVerifiedTransformation(
    const GenerativeProjectGraph& seed,
    const VerifiedProjectTransformation& transformation);

uint64_t projectEvidenceHash(const std::filesystem::path& evidenceFile);

} // namespace genomic::gpi
