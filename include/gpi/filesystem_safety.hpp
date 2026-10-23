#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

struct FileManifestEntry {
    std::string relativePath;
    std::uintmax_t size{0};
    std::string sha256;
};

struct DirectoryManifest {
    std::vector<FileManifestEntry> files;
    std::uintmax_t totalBytes{0};
    std::string sha256;
};

struct ImportSafetyLimits {
    std::size_t maximumFiles{100000};
    std::uintmax_t maximumTotalBytes{4ULL * 1024ULL * 1024ULL * 1024ULL};
    std::uintmax_t maximumFileBytes{512ULL * 1024ULL * 1024ULL};
};

// Rejects traversal through symbolic links/reparse points and enforces bounded,
// deterministic hashing of regular files. Project content is never executed.
DirectoryManifest createSha256DirectoryManifest(
    const std::filesystem::path& root,
    const ImportSafetyLimits& limits = {});

// Enforces that the isolated workspace is outside the protected original tree,
// is not an ancestor of it, and does not alias it through canonicalization.
void validateIsolatedWorkspace(
    const std::filesystem::path& protectedRoot,
    const std::filesystem::path& isolatedWorkspace);

} // namespace genomic::gpi
