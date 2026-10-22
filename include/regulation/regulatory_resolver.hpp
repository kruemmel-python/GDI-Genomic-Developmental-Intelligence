#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include "regulation/molecule.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/binding_site.hpp"
#include "regulation/enhancer.hpp"
#include "regulation/repressor.hpp"
#include "genome/genome_reference.hpp"

namespace genomic {

struct SiteBindingEvaluation {
    GenomeBindingSite site;
    bool bound{false};
    double effectiveAffinity{0.0};
    double concentration{0.0};
    std::string reason;
};

struct EnhancerResolution {
    GenomeRegionId enhancerId{0};
    GenomeRegionId targetRegion{0};
    std::string name;
    std::vector<SiteBindingEvaluation> siteEvaluations;
    double rawActivation{0.0};
    double cooperativityMultiplier{1.0};
    double totalActivation{0.0};
    bool activated{false};
};

struct RepressorResolution {
    GenomeRegionId repressorId{0};
    GenomeRegionId targetRegion{0};
    std::string name;
    std::vector<SiteBindingEvaluation> siteEvaluations;
    double rawRepression{0.0};
    double totalRepression{0.0};
    bool repressed{false};
};

struct RegionResolutionTrace {
    GenomeRegionId regionId{0};
    double netActivation{0.0};
    double netRepression{0.0};
    double effectiveActivity{0.0};
    bool isExpressed{false};
    std::vector<GenomeRegionId> contributingEnhancers;
    std::vector<GenomeRegionId> contributingRepressors;
    std::vector<uint64_t> activeMoleculeSignatures;
};

struct RegulatoryResolutionResult {
    uint32_t compartment{0};
    std::unordered_set<GenomeRegionId> activeRegions;
    std::unordered_map<GenomeRegionId, double> regionActivityScores;
    std::vector<EnhancerResolution> enhancerDetails;
    std::vector<RepressorResolution> repressorDetails;
    std::unordered_map<GenomeRegionId, RegionResolutionTrace> traces;
};

class RegulatoryResolver {
public:
    RegulatoryResolver() = default;

    void addEnhancer(const GenomeEnhancer& enhancer);
    void addRepressor(const GenomeRepressor& repressor);
    void clear();

    RegulatoryResolutionResult resolve(const MolecularPool& pool, uint32_t compartment = 0,
                                       bool defaultOrientationForward = true) const;

    // Helper for direct evaluation of a single binding site
    static SiteBindingEvaluation evaluateSite(const GenomeBindingSite& site,
                                             double concentration,
                                             bool currentOrientationForward = true);

private:
    std::vector<GenomeEnhancer> enhancers_;
    std::vector<GenomeRepressor> repressors_;
};

} // namespace genomic
