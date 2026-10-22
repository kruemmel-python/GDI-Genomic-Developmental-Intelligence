#include "gpi/experience_store.hpp"
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>
namespace genomic::gpi {
uint64_t fingerprint(const std::string& text) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ULL; }
    return hash;
}
namespace {
uint64_t parseUint(std::string_view text) {
    uint64_t value = 0;
    const auto [p,ec] = std::from_chars(text.data(),text.data()+text.size(),value);
    if (ec != std::errc{} || p != text.data()+text.size()) throw std::runtime_error("Invalid experience number");
    return value;
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start=0;
    while (start <= line.size()) {
        const auto pos = line.find('|',start);
        fields.push_back(line.substr(start,pos == std::string::npos ? pos : pos-start));
        if (pos == std::string::npos) break;
        start=pos+1;
    }
    return fields;
}
}
std::vector<Experience> ExperienceStore::load() const {
    std::vector<Experience> entries;
    if (!std::filesystem::exists(path_)) return entries;
    std::ifstream file(path_,std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open experience store");
    std::string line;
    uint64_t previous = 0;
    while (std::getline(file,line)) {
        if (line.size() > 512 || entries.size() >= 100000) throw std::runtime_error("Experience store limit exceeded");
        const auto fields = split(line);
        if (fields.size() != 8 || fields[0] != "GPI1") throw std::runtime_error("Corrupt experience record");
        const auto lastSeparator = line.rfind('|');
        const auto expected = fingerprint(line.substr(0,lastSeparator));
        if (parseUint(fields[7]) != expected || parseUint(fields[6]) != previous)
            throw std::runtime_error("Experience integrity chain mismatch");
        Experience e;
        e.genomeHash = parseUint(fields[1]); e.taskHash = parseUint(fields[2]);
        e.sourceHash = parseUint(fields[3]); e.outcome = fields[4];
        if (e.outcome != "PASS" && e.outcome != "BLOCKED") throw std::runtime_error("Invalid experience outcome");
        e.attempts = static_cast<std::size_t>(parseUint(fields[5]));
        if (e.attempts == 0 || e.attempts > 3) throw std::runtime_error("Invalid experience attempts");
        previous = expected;
        entries.push_back(std::move(e));
    }
    if (!file.eof()) throw std::runtime_error("Experience store read failure");
    return entries;
}
void ExperienceStore::append(const Experience& entry) const {
    const auto existing = load();
    if (existing.size() >= 100000) throw std::runtime_error("Experience store full");
    uint64_t previous=0;
    if (!existing.empty()) {
        std::ifstream file(path_,std::ios::binary);
        std::string line, last;
        while (std::getline(file,line)) last=line;
        const auto fields=split(last);
        previous=parseUint(fields[7]);
    }
    const std::string payload = "GPI1|" + std::to_string(entry.genomeHash) + "|" +
        std::to_string(entry.taskHash) + "|" + std::to_string(entry.sourceHash) + "|" +
        entry.outcome + "|" + std::to_string(entry.attempts) + "|" + std::to_string(previous);
    std::ofstream file(path_,std::ios::binary | std::ios::app);
    if (!file || !(file << payload << '|' << fingerprint(payload) << '\n'))
        throw std::runtime_error("Cannot append experience");
    file.flush();
    if (!file) throw std::runtime_error("Experience flush failed");
}
} // namespace genomic::gpi
