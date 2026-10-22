#pragma once
#include <algorithm>
#include <string>

namespace genomic::gpi {
// Normalize platform-specific text-mode newlines for behavior-oracle comparisons.
// Preserve all other bytes so mismatched output remains detectable.
inline std::string normalizeLineEndings(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}
} // namespace genomic::gpi
