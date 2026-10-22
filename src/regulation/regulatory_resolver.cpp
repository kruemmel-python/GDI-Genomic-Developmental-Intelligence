#include "regulation/regulatory_resolver.hpp"
#include <cmath>
#include <algorithm>

namespace genomic {

void RegulatoryResolver::addEnhancer(const GenomeEnhancer& enhancer) {
    enhancers_.push_back(enhancer);
}

void RegulatoryResolver::addRepressor(const GenomeRepressor& repressor) {
    repressors_.push_back(repressor);
}

void RegulatoryResolver::clear() {
    enhancers_.clear();
    repressors_.clear();
}

SiteBindingEvaluation RegulatoryResolver::evaluateSite(const GenomeBindingSite& site,
                                                      double concentration,
                                                      bool currentOrientationForward) {
    SiteBindingEvaluation eval;
    eval.site = site;
    eval.concentration = concentration;

    if (site.orientationSensitive && (site.orientationForward != currentOrientationForward)) {
        eval.bound = false;
        eval.effectiveAffinity = 0.0;
        eval.reason = "Orientation mismatch: site requires " +
                      std::string(site.orientationForward ? "Forward" : "Reverse");
        return eval;
    }

    if (concentration < site.minimumAffinity) {
        eval.bound = false;
        eval.effectiveAffinity = 0.0;
        eval.reason = "Concentration " + std::to_string(concentration) +
                      " below minimum affinity " + std::to_string(site.minimumAffinity);
        return eval;
    }

    eval.bound = true;
    // Michaelis-Menten-like occupancy: theta = [L] / (Kd + [L])
    double kd = std::max(0.01, site.minimumAffinity);
    eval.effectiveAffinity = concentration / (kd + concentration);
    eval.reason = "Successfully bound with occupancy " + std::to_string(eval.effectiveAffinity);
    return eval;
}

RegulatoryResolutionResult RegulatoryResolver::resolve(const MolecularPool& pool,
                                                       uint32_t compartment,
                                                       bool defaultOrientationForward) const {
    RegulatoryResolutionResult result;
    result.compartment = compartment;

    std::unordered_map<GenomeRegionId, double> regionActivationSum;
    std::unordered_map<GenomeRegionId, double> regionRepressionSum;
    std::unordered_map<GenomeRegionId, std::vector<GenomeRegionId>> regionEnhancersMap;
    std::unordered_map<GenomeRegionId, std::vector<GenomeRegionId>> regionRepressorsMap;
    std::unordered_map<GenomeRegionId, std::vector<uint64_t>> regionMoleculesMap;

    // 1. Evaluate Enhancers
    for (const auto& enh : enhancers_) {
        EnhancerResolution res;
        res.enhancerId = enh.id;
        res.targetRegion = enh.targetRegion;
        res.name = enh.name;

        int boundCount = 0;
        double weightedAffinitySum = 0.0;
        std::vector<int32_t> boundPositions;

        for (const auto& site : enh.sites) {
            double conc = pool.getConcentration(compartment, site.acceptedSignature);
            auto siteEval = evaluateSite(site, conc, defaultOrientationForward);
            res.siteEvaluations.push_back(siteEval);

            if (siteEval.bound) {
                boundCount++;
                weightedAffinitySum += siteEval.effectiveAffinity * site.activationWeight;
                boundPositions.push_back(site.position);
                regionMoleculesMap[enh.targetRegion].push_back(site.acceptedSignature);
            }
        }

        res.rawActivation = (boundCount == 0) ? 0.0 : (weightedAffinitySum / static_cast<double>(boundCount));

        // Cooperativity calculation: if multiple sites are bound within maxBindingDistance
        res.cooperativityMultiplier = 1.0;
        if (boundCount > 1) {
            bool withinDistance = true;
            for (size_t i = 1; i < boundPositions.size(); ++i) {
                if (std::abs(boundPositions[i] - boundPositions[i-1]) > enh.maxBindingDistance) {
                    withinDistance = false;
                    break;
                }
            }
            if (withinDistance) {
                // Hill cooperativity bonus
                double fraction = static_cast<double>(boundCount) / static_cast<double>(enh.sites.size());
                res.cooperativityMultiplier = 1.0 + (enh.cooperativity - 1.0) * fraction;
            }
        }

        res.totalActivation = res.rawActivation * res.cooperativityMultiplier;
        res.activated = (res.totalActivation >= enh.activationThreshold);

        if (res.activated) {
            regionActivationSum[enh.targetRegion] += res.totalActivation;
            regionEnhancersMap[enh.targetRegion].push_back(enh.id);
        }

        result.enhancerDetails.push_back(std::move(res));
    }

    // 2. Evaluate Repressors
    for (const auto& rep : repressors_) {
        RepressorResolution res;
        res.repressorId = rep.id;
        res.targetRegion = rep.targetRegion;
        res.name = rep.name;

        int boundCount = 0;
        double weightedAffinitySum = 0.0;

        for (const auto& site : rep.sites) {
            double conc = pool.getConcentration(compartment, site.acceptedSignature);
            auto siteEval = evaluateSite(site, conc, defaultOrientationForward);
            res.siteEvaluations.push_back(siteEval);

            if (siteEval.bound) {
                boundCount++;
                weightedAffinitySum += siteEval.effectiveAffinity * site.repressionWeight;
                regionMoleculesMap[rep.targetRegion].push_back(site.acceptedSignature);
            }
        }

        res.rawRepression = (boundCount == 0) ? 0.0 : (weightedAffinitySum / static_cast<double>(boundCount));
        res.totalRepression = res.rawRepression * rep.potency;
        res.repressed = (res.totalRepression >= rep.repressionThreshold);

        if (res.repressed) {
            regionRepressionSum[rep.targetRegion] += res.totalRepression;
            regionRepressorsMap[rep.targetRegion].push_back(rep.id);
        }

        result.repressorDetails.push_back(std::move(res));
    }

    // 3. Resolve Target Regions
    std::unordered_set<GenomeRegionId> allTargetRegions;
    for (const auto& [regId, val] : regionActivationSum) allTargetRegions.insert(regId);
    for (const auto& [regId, val] : regionRepressionSum) allTargetRegions.insert(regId);

    for (auto regId : allTargetRegions) {
        double act = regionActivationSum[regId];
        double rep = regionRepressionSum[regId];
        double effective = std::max(0.0, act - rep);

        RegionResolutionTrace trace;
        trace.regionId = regId;
        trace.netActivation = act;
        trace.netRepression = rep;
        trace.effectiveActivity = effective;
        trace.isExpressed = (effective >= 0.45); // Expression threshold
        trace.contributingEnhancers = regionEnhancersMap[regId];
        trace.contributingRepressors = regionRepressorsMap[regId];

        // Deduplicate molecules
        auto& mols = regionMoleculesMap[regId];
        std::sort(mols.begin(), mols.end());
        mols.erase(std::unique(mols.begin(), mols.end()), mols.end());
        trace.activeMoleculeSignatures = mols;

        if (trace.isExpressed) {
            result.activeRegions.insert(regId);
        }
        result.regionActivityScores[regId] = effective;
        result.traces[regId] = std::move(trace);
    }

    return result;
}

} // namespace genomic
