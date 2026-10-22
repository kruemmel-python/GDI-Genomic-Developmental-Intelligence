#pragma once

#include <cstdint>

namespace genomic {

struct GenomeBindingSite {
    uint64_t acceptedSignature{0};
    double minimumAffinity{0.2};
    double activationWeight{1.0};
    double repressionWeight{1.0};
    int32_t position{0};
    bool orientationSensitive{false};
    bool orientationForward{true};

    bool operator==(const GenomeBindingSite& other) const noexcept {
        return acceptedSignature == other.acceptedSignature &&
               minimumAffinity == other.minimumAffinity &&
               activationWeight == other.activationWeight &&
               repressionWeight == other.repressionWeight &&
               position == other.position &&
               orientationSensitive == other.orientationSensitive &&
               orientationForward == other.orientationForward;
    }
};

} // namespace genomic
