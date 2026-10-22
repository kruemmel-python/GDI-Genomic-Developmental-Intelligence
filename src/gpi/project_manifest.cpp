#include "gpi/project_manifest.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace genomic::gpi {
namespace {
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;std::size_t begin=0;
    while(begin<=line.size()) {
        const auto end=line.find('\t',begin);
        result.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return result;
}
std::vector<std::string> list(const std::string& value) {
    std::vector<std::string> result;std::size_t begin=0;
    while(begin<=value.size()) {
        const auto end=value.find(',',begin);auto item=value.substr(begin,end==std::string::npos?end:end-begin);
        if(item.empty())throw std::invalid_argument("Manifest list contains an empty item");
        result.push_back(std::move(item));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return result;
}
std::filesystem::path safePath(const std::filesystem::path& root,const std::string& relative) {
    const std::filesystem::path input(relative);
    if(input.empty()||input.is_absolute())throw std::invalid_argument("Manifest path must be relative");
    const auto normalized=(root/input).lexically_normal();const auto rel=normalized.lexically_relative(root);
    if(rel.empty()||*rel.begin()=="..")throw std::invalid_argument("Manifest path escapes project root");
    return normalized;
}
std::string content(const std::filesystem::path& root,const std::string& relative) {
    if(relative=="-")return {};
    const auto path=safePath(root,relative);
    if(!std::filesystem::is_regular_file(path))throw std::invalid_argument("Manifest input file does not exist: "+relative);
    const auto canonicalRoot=std::filesystem::canonical(root);
    const auto canonicalPath=std::filesystem::canonical(path);
    const auto resolved=canonicalPath.lexically_relative(canonicalRoot);
    if(resolved.empty()||*resolved.begin()=="..")throw std::invalid_argument("Manifest input resolves outside project root");
    std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read manifest input file");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
ProjectNodeKind nodeKind(const std::string& value) {
    if(value=="header")return ProjectNodeKind::Header;
    if(value=="source")return ProjectNodeKind::Source;
    if(value=="module-interface")return ProjectNodeKind::ModuleInterface;
    if(value=="module-implementation")return ProjectNodeKind::ModuleImplementation;
    if(value=="test")return ProjectNodeKind::Test;
    if(value=="resource")return ProjectNodeKind::Resource;
    if(value=="build-manifest")return ProjectNodeKind::BuildManifest;
    throw std::invalid_argument("Unknown manifest node kind: "+value);
}
ProjectDependencyKind dependencyKind(const std::string& value) {
    if(value=="includes")return ProjectDependencyKind::Includes;
    if(value=="imports")return ProjectDependencyKind::Imports;
    if(value=="links")return ProjectDependencyKind::Links;
    if(value=="tests")return ProjectDependencyKind::Tests;
    if(value=="generates")return ProjectDependencyKind::Generates;
    if(value=="uses-resource")return ProjectDependencyKind::UsesResource;
    throw std::invalid_argument("Unknown manifest dependency kind: "+value);
}
ProjectTargetKind targetKind(const std::string& value) {
    if(value=="library")return ProjectTargetKind::Library;
    if(value=="executable")return ProjectTargetKind::Executable;
    if(value=="test")return ProjectTargetKind::Test;
    throw std::invalid_argument("Unknown manifest target kind: "+value);
}
int exitCode(const std::string& value) {
    int result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Invalid manifest exit code");
    return result;
}
}

LoadedProjectSpecification loadProjectManifest(const std::filesystem::path& manifest,
    const std::filesystem::path& projectRoot,Cpp20SourceGenome sourceGenome) {
    if(!std::filesystem::is_regular_file(manifest)||!std::filesystem::is_directory(projectRoot))
        throw std::invalid_argument("Manifest and project root must exist");
    const auto root=std::filesystem::absolute(projectRoot).lexically_normal();
    std::ifstream stream(manifest,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read project manifest");
    LoadedProjectSpecification result{GenerativeProjectGraph(std::move(sourceGenome)),{}};
    std::string line;std::size_t lineNumber=0;bool version=false;
    while(std::getline(stream,line)) {
        ++lineNumber;if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>65536)throw std::invalid_argument("Manifest line is too long");
        if(line.empty()||line[0]=='#')continue;
        if(!version) {if(line!="GPI_PROJECT_CONTRACT_V1")throw std::invalid_argument("Unsupported project manifest version");version=true;continue;}
        const auto item=fields(line);const auto malformed=[&]{throw std::invalid_argument("Malformed project manifest line "+std::to_string(lineNumber));};
        if(item[0]=="node") {
            if(item.size()!=4)malformed();
            static_cast<void>(safePath(root,item[3]));
            result.graph.addNode(item[1],nodeKind(item[2]),std::filesystem::path(item[3]),content(root,item[3]));
        } else if(item[0]=="dependency") {
            if(item.size()!=4)malformed();
            result.graph.addDependency(item[1],item[2],dependencyKind(item[3]));
        } else if(item[0]=="target") {
            if(item.size()!=4)malformed();
            result.graph.addTarget(item[1],targetKind(item[2]),list(item[3]));
        } else if(item[0]=="build") {
            if(item.size()!=2)malformed();
            result.contract.buildTargets.push_back(item[1]);
        } else if(item[0]=="include") {
            if(item.size()!=2)malformed();
            static_cast<void>(safePath(root,item[1]));result.contract.includeRoots.emplace_back(item[1]);
        } else if(item[0]=="behavior") {
            if(item.size()!=7)malformed();
            result.contract.behaviors.push_back({item[1],item[2],content(root,item[3]),content(root,item[4]),content(root,item[5]),exitCode(item[6])});
        } else if(item[0]=="timeout-seconds") {
            if(item.size()!=2)malformed();
            const int seconds=exitCode(item[1]);if(seconds<1||seconds>60)malformed();
            result.contract.executionTimeout=std::chrono::seconds(seconds);
        } else if(item[0]=="require-sandbox") {
            if(item.size()!=2||(item[1]!="true"&&item[1]!="false"))malformed();
            result.contract.requireSandbox=item[1]=="true";
        } else malformed();
    }
    if(!stream.eof())throw std::runtime_error("Project manifest read failure");
    if(!version)throw std::invalid_argument("Project manifest is empty");
    if(result.contract.buildTargets.empty())for(const auto& target:result.graph.targets())result.contract.buildTargets.push_back(target.name);
    result.graph.validate();return result;
}
} // namespace genomic::gpi
