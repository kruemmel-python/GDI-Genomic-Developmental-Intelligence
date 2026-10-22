#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace genomic::gpi {
struct Experience {
    uint64_t genomeHash{0};
    uint64_t taskHash{0};
    uint64_t sourceHash{0};
    std::string outcome;
    std::size_t attempts{0};
};
uint64_t fingerprint(const std::string& text);
class ExperienceStore {
public:
    explicit ExperienceStore(std::filesystem::path path) : path_(std::move(path)) {}
    std::vector<Experience> load() const;
    void append(const Experience& entry) const;
private:
    std::filesystem::path path_;
};
} // namespace genomic::gpi
