#include "expression/splice_graph.hpp"
#include <queue>
#include <stdexcept>

namespace genomic {

void SpliceGraph::addExon(const SpliceExon& exon) {
    exons_[exon.id] = exon;
}

bool SpliceGraph::hasExon(const std::string& id) const noexcept {
    return exons_.find(id) != exons_.end();
}

const SpliceExon& SpliceGraph::getExon(const std::string& id) const {
    auto it = exons_.find(id);
    if (it == exons_.end()) {
        throw std::out_of_range("Splice exon not found: " + id);
    }
    return it->second;
}

std::vector<SpliceExon> SpliceGraph::splice(
    const std::unordered_set<GenomeRegionId>& activeRegions,
    const std::string& startExonId,
    const std::string& endExonId
) const {
    std::vector<SpliceExon> result;

    if (!hasExon(startExonId) || !hasExon(endExonId)) {
        return result;
    }

    const auto& startExon = getExon(startExonId);
    if (activeRegions.find(startExon.regionId) == activeRegions.end()) {
        return result; // Start exon not active
    }

    // BFS to find valid path through active, type-compatible exons
    std::queue<std::vector<std::string>> queue;
    queue.push({startExonId});
    std::unordered_set<std::string> visited;
    visited.insert(startExonId);

    std::vector<std::string> bestPath;

    while (!queue.empty()) {
        auto currentPath = queue.front();
        queue.pop();

        const std::string& currentId = currentPath.back();
        if (currentId == endExonId) {
            bestPath = currentPath;
            break;
        }

        const auto& currentExon = getExon(currentId);
        for (const auto& nextId : currentExon.compatibleNextExons) {
            if (!hasExon(nextId)) continue;
            const auto& nextExon = getExon(nextId);

            // Must be in active genomic regions
            if (activeRegions.find(nextExon.regionId) == activeRegions.end()) {
                continue;
            }

            // Must be type compatible
            if (currentExon.outputType && nextExon.inputType &&
                !TypeSystem::canImplicitlyConvert(*currentExon.outputType, *nextExon.inputType)) {
                continue;
            }

            if (visited.find(nextId) == visited.end()) {
                visited.insert(nextId);
                auto nextPath = currentPath;
                nextPath.push_back(nextId);
                queue.push(nextPath);
            }
        }
    }

    for (const auto& exonId : bestPath) {
        result.push_back(getExon(exonId));
    }
    return result;
}

std::shared_ptr<AstNode> SpliceGraph::composeFunction(
    const std::vector<SpliceExon>& exons,
    const std::string& functionName,
    const GenomeProvenance& provenance
) const {
    if (exons.empty()) return nullptr;

    auto returnType = exons.back().outputType ? exons.back().outputType : CppType::makeInt64();
    auto inputType = exons.front().inputType ? exons.front().inputType : CppType::makeInt64();

    std::vector<std::shared_ptr<AstNode>> bodyStatements;

    // Working state variable
    auto stateVar = AstNode::makeVariableDecl("state", inputType, AstNode::makeIdentifier("input"));
    stateVar->provenance = provenance;
    bodyStatements.push_back(stateVar);

    for (const auto& exon : exons) {
        if (exon.astFragmentBuilder) {
            auto fragment = exon.astFragmentBuilder();
            if (fragment) {
                fragment->provenance = provenance;
                bodyStatements.push_back(fragment);
            }
        }
    }

    // Return final state
    auto retStmt = AstNode::makeReturnStmt(AstNode::makeIdentifier("state"));
    retStmt->provenance = provenance;
    bodyStatements.push_back(retStmt);

    auto compBody = AstNode::makeCompoundStmt(std::move(bodyStatements));
    compBody->provenance = provenance;

    std::vector<std::pair<std::string, std::shared_ptr<CppType>>> params;
    params.push_back({"input", inputType});

    auto fnDecl = AstNode::makeFunctionDecl(functionName, returnType, params, compBody);
    fnDecl->provenance = provenance;
    return fnDecl;
}

} // namespace genomic
