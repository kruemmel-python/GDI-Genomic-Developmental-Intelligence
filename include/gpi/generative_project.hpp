#pragma once

#include "cpp20/cpp20_source_genome.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace genomic::gpi {

enum class ProjectNodeKind {
    Header, Source, ModuleInterface, ModuleImplementation, Test, Resource, BuildManifest
};
enum class ProjectDependencyKind { Includes, Imports, Links, Tests, Generates, UsesResource };
enum class ProjectTargetKind { Library, Executable, Test };

struct GenerativeProjectNode {
    std::string id;
    ProjectNodeKind kind{ProjectNodeKind::Source};
    GenomicSourceArtifact artifact;
};
struct GenerativeProjectDependency {
    std::string consumer;
    std::string provider;
    ProjectDependencyKind kind{ProjectDependencyKind::Includes};
};
struct GenerativeProjectTarget {
    std::string name;
    ProjectTargetKind kind{ProjectTargetKind::Executable};
    std::vector<std::string> nodes;
};

class GenerativeProjectGraph {
public:
    explicit GenerativeProjectGraph(Cpp20SourceGenome sourceGenome);

    void addNode(std::string id,ProjectNodeKind kind,const std::filesystem::path& relativePath,
                 const std::string& exactContent);
    void addDependency(std::string consumer,std::string provider,ProjectDependencyKind kind);
    void addTarget(std::string name,ProjectTargetKind kind,std::vector<std::string> nodes);
    std::string nodeContent(const std::string& id) const;
    void replaceNodeContent(const std::string& id,const std::string& exactContent);

    void validate() const;
    void materialize(const std::filesystem::path& emptyRoot) const;
    uint64_t graphHash() const;
    std::vector<std::string> resolvedTargetNodes(const std::string& targetName) const;
    uint64_t genomeHash() const noexcept { return sourceGenome_.genomeHash(); }
    bool mutationEnabled(std::string_view operation) const { return sourceGenome_.mutationEnabled(operation); }
    std::vector<std::string> suggestedHeaders(std::string_view diagnostic) const {
        return sourceGenome_.suggestedHeaders(diagnostic);
    }

    const std::vector<GenerativeProjectNode>& nodes() const noexcept { return nodes_; }
    const std::vector<GenerativeProjectDependency>& dependencies() const noexcept { return dependencies_; }
    const std::vector<GenerativeProjectTarget>& targets() const noexcept { return targets_; }

private:
    Cpp20SourceGenome sourceGenome_;
    std::vector<GenerativeProjectNode> nodes_;
    std::vector<GenerativeProjectDependency> dependencies_;
    std::vector<GenerativeProjectTarget> targets_;
};

} // namespace genomic::gpi
