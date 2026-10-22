#include "gpi/project_experience.hpp"

#include "gpi/experience_store.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace genomic::gpi {
namespace {
std::string hex(const std::string& value) {
    static constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(value.size()*2);
    for(const unsigned char byte:value){result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
std::string unhex(std::string_view value) {
    if(value.size()%2)throw std::runtime_error("Corrupt project experience string");
    auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);throw std::runtime_error("Corrupt project experience hex");};
    std::string result;result.reserve(value.size()/2);
    for(std::size_t i=0;i<value.size();i+=2)
        result.push_back(static_cast<char>((digit(value[i])<<4)|digit(value[i+1])));
    return result;
}
uint64_t number(std::string_view value) {
    uint64_t result=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())
        throw std::runtime_error("Corrupt project experience number");
    return result;
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;std::size_t begin=0;
    while(begin<=line.size()){const auto end=line.find('|',begin);fields.push_back(line.substr(begin,end==std::string::npos?end:end-begin));if(end==std::string::npos)break;begin=end+1;}return fields;
}
std::string operatorName(const VerifiedProjectTransformation& transformation) {
    if(transformation.kind==ProjectTransformationKind::InsertHeader)return "insert-standard-header";
    if(transformation.kind==ProjectTransformationKind::AddLinkDependency)return "infer-link-provider";
    return transformation.auxiliary;
}
void validateTransformationShape(const VerifiedProjectTransformation& entry) {
    if(entry.genomeHash==0||entry.originGraphHash==0||entry.verifiedGraphHash==0||entry.evidenceHash==0||entry.node.empty())
        throw std::invalid_argument("Incomplete project transformation provenance");
    if(entry.auxiliary.empty())throw std::invalid_argument("Project transformation operation/provider is empty");
    if(entry.kind==ProjectTransformationKind::ReplaceExactToken&&
       (entry.before.empty()||entry.before==entry.after))
        throw std::invalid_argument("Project token transformation is empty or ineffective");
}
}

std::vector<VerifiedProjectTransformation> ProjectExperienceStore::load() const {
    std::vector<VerifiedProjectTransformation> result;if(!std::filesystem::exists(path_))return result;
    std::ifstream stream(path_,std::ios::binary);if(!stream)throw std::runtime_error("Cannot open project experience store");
    std::string line;uint64_t previous=0;
    while(std::getline(stream,line)) {
        if(line.size()>65536||result.size()>=100000)throw std::runtime_error("Project experience store limit exceeded");
        const auto fields=split(line);if(fields.size()!=12||fields[0]!="GPIT1")throw std::runtime_error("Corrupt project experience record");
        const auto separator=line.rfind('|');const auto recordHash=fingerprint(line.substr(0,separator));
        if(number(fields[11])!=recordHash||number(fields[10])!=previous)throw std::runtime_error("Project experience integrity chain mismatch");
        const auto rawKind=number(fields[4]);if(rawKind<1||rawKind>3)throw std::runtime_error("Unknown project transformation kind");
        VerifiedProjectTransformation entry;entry.genomeHash=number(fields[1]);entry.originGraphHash=number(fields[2]);
        entry.verifiedGraphHash=number(fields[3]);entry.kind=static_cast<ProjectTransformationKind>(rawKind);
        entry.node=unhex(fields[5]);entry.auxiliary=unhex(fields[6]);entry.before=unhex(fields[7]);entry.after=unhex(fields[8]);
        entry.evidenceHash=number(fields[9]);
        try {validateTransformationShape(entry);}
        catch(const std::invalid_argument& error) {throw std::runtime_error(error.what());}
        result.push_back(std::move(entry));previous=recordHash;
    }
    if(!stream.eof())throw std::runtime_error("Project experience read failure");
    return result;
}

void ProjectExperienceStore::append(const VerifiedProjectTransformation& entry) const {
    const auto existing=load();uint64_t previous=0;
    if(!existing.empty()) {
        std::ifstream stream(path_,std::ios::binary);std::string line,last;while(std::getline(stream,line))last=line;
        previous=number(split(last).back());
    }
    validateTransformationShape(entry);
    const std::string payload="GPIT1|"+std::to_string(entry.genomeHash)+'|'+std::to_string(entry.originGraphHash)+'|'+
        std::to_string(entry.verifiedGraphHash)+'|'+std::to_string(static_cast<unsigned>(entry.kind))+'|'+hex(entry.node)+'|'+
        hex(entry.auxiliary)+'|'+hex(entry.before)+'|'+hex(entry.after)+'|'+std::to_string(entry.evidenceHash)+'|'+std::to_string(previous);
    if(!path_.parent_path().empty())std::filesystem::create_directories(path_.parent_path());
    std::ofstream stream(path_,std::ios::binary|std::ios::app);
    if(!stream||!(stream<<payload<<'|'<<fingerprint(payload)<<'\n'))throw std::runtime_error("Cannot append project transformation");
}

GenerativeProjectGraph applyVerifiedTransformation(const GenerativeProjectGraph& seed,
    const VerifiedProjectTransformation& transformation) {
    if(transformation.genomeHash!=seed.genomeHash())throw std::invalid_argument("Project experience belongs to another genome");
    if(!seed.mutationEnabled(operatorName(transformation)))throw std::invalid_argument("Project experience operation is not genomically authorized");
    auto result=seed;
    if(transformation.kind==ProjectTransformationKind::InsertHeader) {
        auto content=result.nodeContent(transformation.node);const auto directive="#include <"+transformation.auxiliary+">";
        if(content.find(directive)==std::string::npos)result.replaceNodeContent(transformation.node,directive+'\n'+content);
    } else if(transformation.kind==ProjectTransformationKind::AddLinkDependency) {
        result.addDependency(transformation.node,transformation.auxiliary,ProjectDependencyKind::Links);
    } else {
        auto content=result.nodeContent(transformation.node);const auto first=content.find(transformation.before);
        if(first==std::string::npos||content.find(transformation.before,first+transformation.before.size())!=std::string::npos)
            throw std::invalid_argument("Transfer token context is absent or ambiguous");
        content.replace(first,transformation.before.size(),transformation.after);result.replaceNodeContent(transformation.node,content);
    }
    result.validate();return result;
}

uint64_t projectEvidenceHash(const std::filesystem::path& evidenceFile) {
    std::ifstream stream(evidenceFile,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read project evidence");
    const std::string content{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    if(content.empty())throw std::runtime_error("Project evidence is empty");
    return fingerprint(content);
}
} // namespace genomic::gpi
