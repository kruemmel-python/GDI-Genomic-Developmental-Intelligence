#include "gpi/generative_project.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace genomic::gpi {
namespace {
void hashText(uint64_t& hash,const std::string& value) noexcept {
    for(const unsigned char byte:value){hash^=byte;hash*=1099511628211ULL;}
    hash^=0xffU;hash*=1099511628211ULL;
}
}

GenerativeProjectGraph::GenerativeProjectGraph(Cpp20SourceGenome sourceGenome)
    : sourceGenome_(std::move(sourceGenome)) {}

void GenerativeProjectGraph::addNode(std::string id,ProjectNodeKind kind,
                                     const std::filesystem::path& relativePath,
                                     const std::string& exactContent) {
    if(id.empty())throw std::invalid_argument("Project node ID must not be empty");
    if(std::any_of(nodes_.begin(),nodes_.end(),[&](const auto& node){
        return node.id==id||node.artifact.relativePath==relativePath;}))
        throw std::invalid_argument("Project node IDs and paths must be unique");
    nodes_.push_back({std::move(id),kind,sourceGenome_.encode(relativePath,exactContent)});
}

void GenerativeProjectGraph::addDependency(std::string consumer,std::string provider,
                                           ProjectDependencyKind kind) {
    if(consumer==provider)throw std::invalid_argument("A project node cannot depend on itself");
    dependencies_.push_back({std::move(consumer),std::move(provider),kind});
}

void GenerativeProjectGraph::addTarget(std::string name,ProjectTargetKind kind,
                                       std::vector<std::string> nodes) {
    if(name.empty()||nodes.empty())throw std::invalid_argument("Project targets require a name and nodes");
    if(std::any_of(targets_.begin(),targets_.end(),[&](const auto& target){return target.name==name;}))
        throw std::invalid_argument("Project target names must be unique");
    targets_.push_back({std::move(name),kind,std::move(nodes)});
}

std::string GenerativeProjectGraph::nodeContent(const std::string& id) const {
    const auto found=std::find_if(nodes_.begin(),nodes_.end(),[&](const auto& node){return node.id==id;});
    if(found==nodes_.end())throw std::invalid_argument("Unknown project node: "+id);
    return sourceGenome_.decode(found->artifact);
}

void GenerativeProjectGraph::replaceNodeContent(const std::string& id,const std::string& exactContent) {
    const auto found=std::find_if(nodes_.begin(),nodes_.end(),[&](const auto& node){return node.id==id;});
    if(found==nodes_.end())throw std::invalid_argument("Unknown project node: "+id);
    found->artifact=sourceGenome_.encode(found->artifact.relativePath,exactContent);
}

void GenerativeProjectGraph::validate() const {
    if(nodes_.empty()||targets_.empty())throw std::invalid_argument("Project graph requires nodes and targets");
    std::unordered_map<std::string,std::size_t> index;
    for(std::size_t i=0;i<nodes_.size();++i) {
        if(!sourceGenome_.verifies(nodes_[i].artifact))throw std::invalid_argument("Project contains an invalid genomic artifact");
        index.emplace(nodes_[i].id,i);
    }
    std::vector<std::vector<std::size_t>> outgoing(nodes_.size());
    std::unordered_set<std::string> uniqueEdges;
    for(const auto& edge:dependencies_) {
        const auto consumer=index.find(edge.consumer),provider=index.find(edge.provider);
        if(consumer==index.end()||provider==index.end())throw std::invalid_argument("Dependency references an unknown project node");
        const auto consumerKind=nodes_[consumer->second].kind;
        const auto providerKind=nodes_[provider->second].kind;
        const bool typed=
            (edge.kind==ProjectDependencyKind::Includes&&
                (providerKind==ProjectNodeKind::Header||providerKind==ProjectNodeKind::ModuleInterface))||
            (edge.kind==ProjectDependencyKind::Imports&&providerKind==ProjectNodeKind::ModuleInterface)||
            (edge.kind==ProjectDependencyKind::Links&&
                (providerKind==ProjectNodeKind::Source||providerKind==ProjectNodeKind::ModuleImplementation)&&
                (consumerKind==ProjectNodeKind::Source||consumerKind==ProjectNodeKind::ModuleImplementation||consumerKind==ProjectNodeKind::Test))||
            (edge.kind==ProjectDependencyKind::Tests&&consumerKind==ProjectNodeKind::Test&&
                providerKind!=ProjectNodeKind::Test&&providerKind!=ProjectNodeKind::BuildManifest)||
            (edge.kind==ProjectDependencyKind::Generates&&providerKind==ProjectNodeKind::BuildManifest)||
            (edge.kind==ProjectDependencyKind::UsesResource&&providerKind==ProjectNodeKind::Resource);
        if(!typed)throw std::invalid_argument("Dependency roles are incompatible with their typed relationship");
        const auto key=edge.consumer+'\0'+edge.provider+'\0'+std::to_string(static_cast<unsigned>(edge.kind));
        if(!uniqueEdges.insert(key).second)throw std::invalid_argument("Duplicate project dependency");
        outgoing[consumer->second].push_back(provider->second);
    }
    std::vector<unsigned char> state(nodes_.size(),0);
    std::function<void(std::size_t)> visit=[&](std::size_t current) {
        if(state[current]==1)throw std::invalid_argument("Project dependency graph contains a cycle");
        if(state[current]==2)return;
        state[current]=1;
        for(const auto next:outgoing[current])visit(next);
        state[current]=2;
    };
    for(std::size_t i=0;i<nodes_.size();++i)visit(i);
    std::unordered_set<std::string> targetNames;
    for(const auto& target:targets_) {
        if(!targetNames.insert(target.name).second)throw std::invalid_argument("Duplicate target name");
        std::unordered_set<std::string> members;
        bool hasExecutableSource=false,hasTest=false;
        for(const auto& node:target.nodes) {
            if(index.find(node)==index.end()||!members.insert(node).second)
                throw std::invalid_argument("Target contains an unknown or duplicate node");
            const auto kind=nodes_[index.at(node)].kind;
            hasExecutableSource=hasExecutableSource||kind==ProjectNodeKind::Source||kind==ProjectNodeKind::ModuleImplementation;
            hasTest=hasTest||kind==ProjectNodeKind::Test;
        }
        if(target.kind==ProjectTargetKind::Executable&&!hasExecutableSource)
            throw std::invalid_argument("Executable target has no compilable source node");
        if(target.kind==ProjectTargetKind::Test&&!hasTest)
            throw std::invalid_argument("Test target has no test node");
    }
}

void GenerativeProjectGraph::materialize(const std::filesystem::path& emptyRoot) const {
    validate();
    if(emptyRoot.empty()||(std::filesystem::exists(emptyRoot)&&!std::filesystem::is_empty(emptyRoot)))
        throw std::invalid_argument("Project output directory must be new or empty");
    const auto root=std::filesystem::absolute(emptyRoot).lexically_normal();
    std::filesystem::create_directories(root);
    for(const auto& node:nodes_) {
        const auto output=(root/node.artifact.relativePath).lexically_normal();
        auto relative=output.lexically_relative(root);
        if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Project artifact escapes output directory");
        std::filesystem::create_directories(output.parent_path());
        const auto content=sourceGenome_.decode(node.artifact);
        std::ofstream stream(output,std::ios::binary|std::ios::trunc);
        stream.write(content.data(),static_cast<std::streamsize>(content.size()));
        if(!stream)throw std::runtime_error("Cannot materialize project artifact: "+output.string());
    }
}

uint64_t GenerativeProjectGraph::graphHash() const {
    validate();uint64_t hash=1469598103934665603ULL;
    hashText(hash,std::to_string(sourceGenome_.genomeHash()));
    auto nodes=nodes_;std::sort(nodes.begin(),nodes.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    for(const auto& node:nodes){hashText(hash,node.id);hashText(hash,std::to_string(static_cast<unsigned>(node.kind)));hashText(hash,node.artifact.relativePath.generic_string());hashText(hash,std::to_string(node.artifact.sourceHash));}
    auto edges=dependencies_;std::sort(edges.begin(),edges.end(),[](const auto& a,const auto& b){
        return std::tie(a.consumer,a.provider,a.kind)<std::tie(b.consumer,b.provider,b.kind);});
    for(const auto& edge:edges){hashText(hash,edge.consumer);hashText(hash,edge.provider);hashText(hash,std::to_string(static_cast<unsigned>(edge.kind)));}
    auto targets=targets_;std::sort(targets.begin(),targets.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    for(auto& target:targets){hashText(hash,target.name);hashText(hash,std::to_string(static_cast<unsigned>(target.kind)));std::sort(target.nodes.begin(),target.nodes.end());for(const auto& node:target.nodes)hashText(hash,node);}
    return hash;
}

std::vector<std::string> GenerativeProjectGraph::resolvedTargetNodes(const std::string& targetName) const {
    validate();
    const auto target=std::find_if(targets_.begin(),targets_.end(),[&](const auto& item){return item.name==targetName;});
    if(target==targets_.end())throw std::invalid_argument("Unknown project target: "+targetName);
    std::unordered_map<std::string,std::vector<std::string>> providers;
    for(const auto& edge:dependencies_)providers[edge.consumer].push_back(edge.provider);
    for(auto& [consumer,items]:providers){(void)consumer;std::sort(items.begin(),items.end());}
    std::vector<std::string> result;std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> visit=[&](const std::string& node) {
        if(!visited.insert(node).second)return;
        result.push_back(node);
        const auto found=providers.find(node);
        if(found!=providers.end())for(const auto& provider:found->second)visit(provider);
    };
    auto roots=target->nodes;std::sort(roots.begin(),roots.end());for(const auto& root:roots)visit(root);
    return result;
}
} // namespace genomic::gpi
