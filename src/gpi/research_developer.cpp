#include "gpi/research_developer.hpp"
#include "gpi/line_endings.hpp"
#include "gpi/process_runner.hpp"
#include "genome/genome_decoder.hpp"
#include "genome/genome_strand.hpp"
#include "regulation/enhancer.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <charconv>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace genomic::gpi {
namespace {
using namespace std::chrono_literals;

std::string lower(std::string text) {
    for (char& c:text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::vector<std::string> split(const std::string& text,char separator) {
    std::vector<std::string> fields;std::size_t begin=0;
    while (begin<=text.size()) {
        const auto end=text.find(separator,begin);
        fields.push_back(text.substr(begin,end==std::string::npos?end:end-begin));
        if (end==std::string::npos) break;
        begin=end+1;
    }
    return fields;
}
bool containsAny(const std::string& text,const std::vector<std::string_view>& words) {
    return std::any_of(words.begin(),words.end(),[&](std::string_view word) {
        return text.find(word)!=std::string::npos;
    });
}
struct ReusableComponent {
    std::string id;
    std::string inputType;
    std::string outputType;
    std::vector<std::string> operations;
    std::string strategy;
    std::size_t score{0};
    std::filesystem::path artifact;
};
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read research artifact: "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path& path,const std::string& text) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if (!file || !(file<<text)) throw std::runtime_error("Cannot write research artifact: "+path.string());
}
std::string fencedCpp(const std::string& prompt) {
    auto begin=prompt.find("```cpp");
    std::size_t marker=6;
    if (begin==std::string::npos) { begin=prompt.find("```c++"); marker=6; }
    if (begin==std::string::npos) return {};
    begin=prompt.find('\n',begin+marker);
    if (begin==std::string::npos) return {};
    const auto end=prompt.find("```",begin+1);
    if (end==std::string::npos) return {};
    return prompt.substr(begin+1,end-begin-1);
}
std::string sequenceProgram(bool unique,bool insertion) {
    std::ostringstream s;
    s<<"#include <algorithm>\n#include <cstdint>\n#include <iostream>\n#include <vector>\n";
    if (unique && !insertion) s<<"#include <unordered_set>\n";
    s<<"int main(){ std::vector<std::int64_t> v; std::int64_t x=0; while(std::cin>>x) v.push_back(x);"
        " if(!std::cin.eof()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}";
    if (insertion) {
        s<<" for(std::size_t i=1;i<v.size();++i){auto key=v[i];auto j=i;"
            "while(j>0&&v[j-1]>key){v[j]=v[j-1];--j;}v[j]=key;}";
        if (unique) s<<" v.erase(std::unique(v.begin(),v.end()),v.end());";
    } else if (unique) {
        s<<" std::unordered_set<std::int64_t> seen(v.begin(),v.end());"
            "v.assign(seen.begin(),seen.end());std::sort(v.begin(),v.end());";
    } else s<<" std::sort(v.begin(),v.end());";
    s<<" std::cout<<\"VALUES=\";for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}"
        "std::cout<<'\\n';return 0;}\n";
    return s.str();
}
std::string medianProgram(bool heaps) {
    if (!heaps) return
        "#include <algorithm>\n#include <cstdint>\n#include <iomanip>\n#include <iostream>\n#include <vector>\n"
        "int main(){std::vector<std::int64_t> v;std::int64_t x=0;while(std::cin>>x)v.push_back(x);"
        "if(!std::cin.eof()||v.empty()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}std::sort(v.begin(),v.end());"
        "long double m=v.size()%2?v[v.size()/2]:(static_cast<long double>(v[v.size()/2-1])+v[v.size()/2])/2.0L;"
        "std::cout<<\"MEDIAN=\"<<std::fixed<<std::setprecision(3)<<m<<'\\n';return 0;}\n";
    return
        "#include <cstdint>\n#include <functional>\n#include <iomanip>\n#include <iostream>\n#include <queue>\n#include <vector>\n"
        "int main(){std::priority_queue<std::int64_t> low;std::priority_queue<std::int64_t,std::vector<std::int64_t>,std::greater<>> high;"
        "std::int64_t x=0;while(std::cin>>x){if(low.empty()||x<=low.top())low.push(x);else high.push(x);"
        "if(low.size()>high.size()+1){high.push(low.top());low.pop();}else if(high.size()>low.size()){low.push(high.top());high.pop();}}"
        "if(!std::cin.eof()||low.empty()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}"
        "long double m=low.size()==high.size()?(static_cast<long double>(low.top())+high.top())/2.0L:low.top();"
        "std::cout<<\"MEDIAN=\"<<std::fixed<<std::setprecision(3)<<m<<'\\n';return 0;}\n";
}
std::string frequencyProgram(bool unordered) {
    std::ostringstream s;
    s<<"#include <algorithm>\n#include <cstdint>\n#include <iostream>\n";
    if (unordered) s<<"#include <unordered_map>\n#include <vector>\n";
    else s<<"#include <map>\n";
    s<<"int main(){std::int64_t x=0;";
    if (unordered) s<<"std::unordered_map<std::int64_t,std::size_t> counts;while(std::cin>>x)++counts[x];"
        "if(!std::cin.eof()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}std::vector<std::int64_t> keys;"
        "for(const auto& item:counts)keys.push_back(item.first);std::sort(keys.begin(),keys.end());"
        "for(auto key:keys)std::cout<<\"VALUE=\"<<key<<\" COUNT=\"<<counts[key]<<'\\n';";
    else s<<"std::map<std::int64_t,std::size_t> counts;while(std::cin>>x)++counts[x];"
        "if(!std::cin.eof()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}"
        "for(const auto& item:counts)std::cout<<\"VALUE=\"<<item.first<<\" COUNT=\"<<item.second<<'\\n';";
    s<<"return 0;}\n"; return s.str();
}
std::string compositeProgram(const ResearchPlan& plan,bool hashed,const ReusableComponent* reused=nullptr) {
    std::ostringstream s;
    s<<"#include <algorithm>\n#include <cstdint>\n#include <iomanip>\n#include <iostream>\n#include <map>\n"
       "#include <numeric>\n#include <unordered_set>\n#include <vector>\n"
       <<(reused?"#include \"developed_component.hpp\"\n":"")
       <<"int main(){std::vector<std::int64_t> values;std::int64_t x=0;while(std::cin>>x)values.push_back(x);"
       "if(!std::cin.eof()){std::cerr<<\"INVALID_INPUT\\n\";return 2;}";
    std::size_t beginOperation=0;bool ordered=false;
    if (reused) {
        int64_t threshold=0;
        for (std::size_t i=0;i<reused->operations.size();++i)
            if (plan.operations[i].operation=="filter_gt" && plan.operations[i].integerParameter)
                threshold=*plan.operations[i].integerParameter;
        s<<"values=GpiDeveloped_"<<reused->id<<"::apply(std::move(values),"<<threshold<<");";
        beginOperation=reused->operations.size();ordered=reused->outputType=="OrderedSequence";
    }
    for (std::size_t operationIndex=beginOperation;operationIndex<plan.operations.size();++operationIndex) {
        const auto& operation=plan.operations[operationIndex];
        if (operation.operation=="filter_gt") {
            s<<"values.erase(std::remove_if(values.begin(),values.end(),[](std::int64_t value){return value<="
             <<*operation.integerParameter<<";}),values.end());";
        } else if (operation.operation=="deduplicate") {
            if (hashed) s<<"{std::unordered_set<std::int64_t> unique(values.begin(),values.end());values.assign(unique.begin(),unique.end());}";
            else s<<"std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());";
            ordered=!hashed;
        } else if (operation.operation=="sort_ascending") {
            s<<"std::sort(values.begin(),values.end());";ordered=true;
        } else if (operation.operation=="median") {
            if (!ordered) s<<"std::sort(values.begin(),values.end());";
            s<<"if(values.empty()){std::cerr<<\"EMPTY_INPUT\\n\";return 3;}"
               "long double result=values.size()%2?values[values.size()/2]:"
               "(static_cast<long double>(values[values.size()/2-1])+values[values.size()/2])/2.0L;"
               "std::cout<<\"MEDIAN=\"<<std::fixed<<std::setprecision(3)<<result<<'\\n';return 0;}\n";
            return s.str();
        } else if (operation.operation=="sum") {
            s<<"long double result=std::accumulate(values.begin(),values.end(),0.0L);"
               "std::cout<<\"SUM=\"<<std::fixed<<std::setprecision(3)<<result<<'\\n';return 0;}\n";
            return s.str();
        } else if (operation.operation=="frequency") {
            s<<"std::map<std::int64_t,std::size_t> counts;for(auto value:values)++counts[value];"
               "for(const auto& item:counts)std::cout<<\"VALUE=\"<<item.first<<\" COUNT=\"<<item.second<<'\\n';return 0;}\n";
            return s.str();
        }
    }
    if (!ordered) s<<"std::sort(values.begin(),values.end());";
    s<<"std::cout<<\"VALUES=\";for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<values[i];}"
       "std::cout<<'\\n';return 0;}\n";return s.str();
}
std::string generatedSource(ResearchProblem problem,std::size_t variant) {
    if (problem==ResearchProblem::Sort) return sequenceProgram(false,variant==0);
    if (problem==ResearchProblem::Unique) return sequenceProgram(true,variant==0);
    if (problem==ResearchProblem::Median) return medianProgram(variant!=0);
    if (problem==ResearchProblem::Frequency) return frequencyProgram(variant!=0);
    return {};
}
std::size_t score(ResearchProblem problem,std::size_t variant) {
    if (problem==ResearchProblem::Sort) return variant==0?200:80;
    if (problem==ResearchProblem::Unique) return variant==0?160:90;
    if (problem==ResearchProblem::Median) return variant==0?120:70;
    if (problem==ResearchProblem::Frequency) return variant==0?110:75;
    if (problem==ResearchProblem::Composite) return variant==0?140:85;
    return 100;
}
std::pair<std::string,std::string> contractFixture(const ResearchPlan& plan,std::vector<int64_t> values) {
    std::ostringstream input;
    for(std::size_t i=0;i<values.size();++i)input<<(i?" ":"")<<values[i];
    input<<'\n';bool ordered=false;
    std::ostringstream output;output<<std::fixed<<std::setprecision(3);
    for (const auto& operation:plan.operations) {
        if (operation.operation=="filter_gt")
            values.erase(std::remove_if(values.begin(),values.end(),[&](int64_t value){return value<=*operation.integerParameter;}),values.end());
        else if (operation.operation=="deduplicate") {
            std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());ordered=true;
        } else if (operation.operation=="sort_ascending") { std::sort(values.begin(),values.end());ordered=true; }
        else if (operation.operation=="median") {
            if (!ordered) std::sort(values.begin(),values.end());
            if (values.empty()) throw std::invalid_argument("Composite oracle median is empty");
            const long double median=values.size()%2?values[values.size()/2]:
                (static_cast<long double>(values[values.size()/2-1])+values[values.size()/2])/2.0L;
            output<<"MEDIAN="<<median<<'\n';return {input.str(),output.str()};
        } else if (operation.operation=="sum") {
            long double sum=0;for(auto value:values)sum+=value;output<<"SUM="<<sum<<'\n';return {input.str(),output.str()};
        } else if (operation.operation=="frequency") {
            std::map<int64_t,std::size_t> counts;for(auto value:values)++counts[value];
            for(const auto& item:counts)output<<"VALUE="<<item.first<<" COUNT="<<item.second<<'\n';
            return {input.str(),output.str()};
        }
    }
    if (!ordered) std::sort(values.begin(),values.end());
    output<<"VALUES=";for(std::size_t i=0;i<values.size();++i)output<<(i?",":"")<<values[i];output<<'\n';
    return {input.str(),output.str()};
}
std::string repairArbitrarySource(std::string source,const std::string& diagnostics) {
    struct Repair { std::string_view marker; std::string_view header; };
    static constexpr Repair repairs[]={{"sort","#include <algorithm>\n"},{"vector","#include <vector>\n"},
        {"unordered_map","#include <unordered_map>\n"},{"unordered_set","#include <unordered_set>\n"},
        {"priority_queue","#include <queue>\n"},{"setprecision","#include <iomanip>\n"}};
    for (const auto& repair:repairs) {
        if (diagnostics.find(repair.marker)!=std::string::npos && source.find(repair.header)==std::string::npos)
            return std::string(repair.header)+source;
    }
    return source;
}
void validateArbitraryIncludes(const std::string& source) {
    static const std::vector<std::string> allowed={"algorithm","array","cstdint","cstddef","cmath","deque",
        "functional","iomanip","iostream","limits","map","numeric","optional","queue","set","sstream",
        "string","string_view","tuple","unordered_map","unordered_set","utility","vector"};
    std::istringstream lines(source); std::string line;
    while (std::getline(lines,line)) {
        const auto first=line.find_first_not_of(" \t");
        if (first==std::string::npos || line.compare(first,8,"#include")!=0) continue;
        const auto open=line.find('<',first+8); const auto close=line.find('>',open);
        if (open==std::string::npos || close==std::string::npos || line.find('"',first+8)!=std::string::npos)
            throw std::invalid_argument("Arbitrary research source may include only approved C++ standard headers");
        const auto header=line.substr(open+1,close-open-1);
        if (std::find(allowed.begin(),allowed.end(),header)==allowed.end())
            throw std::invalid_argument("Header is not available in the sandbox: "+header);
    }
}
void copyRuntimeLibraries(const std::string& compiler,const std::filesystem::path& workspace) {
#ifdef _WIN32
    const auto bin=std::filesystem::absolute(std::filesystem::path(compiler)).parent_path();
    for (const auto* name:{"libstdc++-6.dll","libgcc_s_seh-1.dll","libwinpthread-1.dll"}) {
        const auto source=bin/name;
        if (std::filesystem::is_regular_file(source))
            std::filesystem::copy_file(source,workspace/name,std::filesystem::copy_options::overwrite_existing);
    }
#else
    (void)compiler; (void)workspace;
#endif
}
std::string problemName(ResearchProblem problem) {
    if (problem==ResearchProblem::Sort) return "sort";
    if (problem==ResearchProblem::Unique) return "unique";
    if (problem==ResearchProblem::Median) return "median";
    if (problem==ResearchProblem::Frequency) return "frequency";
    if (problem==ResearchProblem::Composite) return "composite";
    return "arbitrary";
}
std::string stateName(ResearchDevelopmentState state) {
    if (state==ResearchDevelopmentState::Control) return "CONTROL";
    if (state==ResearchDevelopmentState::Developed) return "DEVELOPED";
    return "EXPLORE";
}
std::string abstractionSignature(const ResearchPlan& plan) {
    if (plan.operations.empty()) return problemName(plan.problem);
    std::ostringstream signature;
    for (std::size_t i=0;i<plan.operations.size();++i)
        signature<<(i?",":"")<<plan.operations[i].operation;
    return signature.str();
}
bool validMemoryToken(const std::string& value,bool commaAllowed=false) {
    return !value.empty() && value.size()<=256 && std::all_of(value.begin(),value.end(),[&](unsigned char c) {
        return std::isalnum(c) || c=='_' || c=='+' || (commaAllowed && c==',');
    });
}
double abstractionSimilarity(const std::string& left,const std::string& right) {
    if (left==right) return 1.0;
    const auto lhs=split(left,','),rhs=split(right,',');
    std::size_t intersection=0;
    for (const auto& item:lhs)
        if (std::find(rhs.begin(),rhs.end(),item)!=rhs.end()) ++intersection;
    std::size_t unionSize=lhs.size();
    for (const auto& item:rhs)
        if (std::find(lhs.begin(),lhs.end(),item)==lhs.end()) ++unionSize;
    return unionSize==0?0.0:static_cast<double>(intersection)/static_cast<double>(unionSize);
}
struct DevelopmentExperience {
    std::string sourceSignature;
    std::string strategy;
    std::size_t score{0};
    double similarity{0.0};
    bool transferred{false};
};
struct StructureCandidate {
    std::size_t begin{0};
    std::size_t length{0};
    double cohesion{0.0};
    std::string rationale;
};
std::optional<StructureCandidate> discoverReusableStructure(const ResearchPlan& plan) {
    std::size_t transformEnd=0;
    while(transformEnd<plan.operations.size()) {
        const auto& operation=plan.operations[transformEnd].operation;
        if(operation=="median" || operation=="sum" || operation=="frequency")break;
        ++transformEnd;
    }
    std::optional<StructureCandidate> best;
    for(std::size_t begin=0;begin<transformEnd;++begin)for(std::size_t end=begin+2;end<=transformEnd;++end) {
        double continuity=0.0;
        for(std::size_t i=begin+1;i<end;++i) {
            const auto& previous=plan.operations[i-1].outputType;
            const auto& input=plan.operations[i].inputType;
            if(previous==input || input=="NumberSequence")continuity+=1.0;
        }
        const auto length=end-begin;
        const double cohesion=static_cast<double>(length)*10.0+continuity*4.0+
            (plan.operations[end-1].outputType=="OrderedSequence"?3.0:0.0)-
            (plan.operations[begin].integerParameter?0.5:0.0);
        if(!best || cohesion>best->cohesion) {
            best=StructureCandidate{begin,length,cohesion,
                "maximal typed dataflow cohesion="+std::to_string(cohesion)+
                "; internal_edges="+std::to_string(length-1)};
        }
    }
    return best;
}
std::size_t reusablePrefixLength(const ResearchPlan& plan) {
    const auto structure=discoverReusableStructure(plan);
    return structure && structure->begin==0?structure->length:0;
}
uint64_t stableComponentHash(const std::string& text) {
    uint64_t hash=1469598103934665603ULL;
    for (unsigned char c:text) { hash^=c;hash*=1099511628211ULL; }
    return hash;
}
std::string componentId(const std::string& input,const std::string& output,
                        const std::string& operations,const std::string& strategy) {
    std::ostringstream id;id<<"component_"<<std::hex<<stableComponentHash(input+'|'+output+'|'+operations+'|'+strategy);
    return id.str();
}
std::string componentHeader(const ReusableComponent& component) {
    std::ostringstream source;
    source<<"#pragma once\n#include <algorithm>\n#include <cstdint>\n#include <unordered_set>\n"
             "#include <utility>\n#include <vector>\nstruct GpiDeveloped_"<<component.id<<" {\n"
             "static std::vector<std::int64_t> apply(std::vector<std::int64_t> values,std::int64_t threshold){";
    bool usesThreshold=false;
    for (const auto& operation:component.operations) {
        if (operation=="filter_gt") {
            usesThreshold=true;
            source<<"values.erase(std::remove_if(values.begin(),values.end(),[&](std::int64_t value){return value<=threshold;}),values.end());";
        } else if (operation=="deduplicate") {
            if (component.strategy=="hash+component_pipeline")
                source<<"{std::unordered_set<std::int64_t> unique(values.begin(),values.end());values.assign(unique.begin(),unique.end());}";
            else source<<"std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());";
        } else if (operation=="sort_ascending") source<<"std::sort(values.begin(),values.end());";
        else throw std::invalid_argument("Unsupported operation in developed component: "+operation);
    }
    if (!usesThreshold) source<<"(void)threshold;";
    source<<"return values;}\n};\n";return source.str();
}
std::filesystem::path componentArtifactPath(const std::filesystem::path& memory,const std::string& id) {
    return memory.parent_path()/(memory.filename().string()+".components")/(id+".hpp");
}
std::filesystem::path componentProvenancePath(const std::filesystem::path& memory,const std::string& id) {
    return memory.parent_path()/(memory.filename().string()+".components")/(id+".provenance");
}
struct ResearchLocus {
    std::string problem;
    std::string strategy;
    uint64_t signature{0};
    std::string condition;
    GenomeRegionId region{0};
};
std::vector<ResearchLocus> researchLoci(const GenomeStrand& genome) {
    if (!genome.isFrozen() || !genome.verifyIntegrity().passed)
        throw std::invalid_argument("Research requires an intact frozen genome");
    std::vector<ResearchLocus> loci;
    for (const auto& region:genome.getAllRegions()) {
        if (region.kind!=RegionKind::ResearchCapability) continue;
        const auto decoded=GenomeDecoder::parseRegionsFromStrand(region.bases);
        if (decoded.size()!=1 || decoded.front().payload!=region.payload)
            throw std::invalid_argument("Research locus payload/base mismatch");
        const std::string payload(region.payload.begin(),region.payload.end());
        const auto fields=split(payload,'|');
        if (!fields.empty() && fields[0]=="gpi-operation-v1") continue;
        if (fields.size()!=5 || fields[0]!="gpi-research-v1" ||
            (fields[4]!="basal" && fields[4]!="developed"))
            throw std::invalid_argument("Malformed genomic research locus");
        uint64_t signature=0;
        const auto [end,error]=std::from_chars(fields[3].data(),fields[3].data()+fields[3].size(),signature);
        if (error!=std::errc{} || end!=fields[3].data()+fields[3].size() || signature==0)
            throw std::invalid_argument("Invalid genomic research signature");
        loci.push_back({fields[1],fields[2],signature,fields[4],region.id});
    }
    if (loci.empty()) throw std::invalid_argument("Genome has no research capability loci");
    return loci;
}
std::vector<GenomicStrategyActivation> regulateOperations(const GenomeStrand& genome,
        const std::vector<OperationSpec>& operations) {
    const auto loci=researchLoci(genome);(void)loci;
    RegulatoryResolver resolver;MolecularPool molecules;
    struct OperationLocus { std::string operation; uint64_t signature; std::string input; std::string output; GenomeRegionId region; };
    std::vector<OperationLocus> operationLoci;
    for (const auto& region:genome.getAllRegions()) {
        if (region.kind!=RegionKind::ResearchCapability) continue;
        const std::string payload(region.payload.begin(),region.payload.end());const auto fields=split(payload,'|');
        if (fields.size()!=5 || fields[0]!="gpi-operation-v1") continue;
        uint64_t signature=0;const auto [end,error]=std::from_chars(fields[2].data(),fields[2].data()+fields[2].size(),signature);
        if(error!=std::errc{} || end!=fields[2].data()+fields[2].size() || signature==0)
            throw std::invalid_argument("Invalid genomic operation signature");
        operationLoci.push_back({fields[1],signature,fields[3],fields[4],region.id});
    }
    const auto compatible=[](const std::string& actual,const std::string& expected) {
        if(actual==expected)return true;
        if(expected!="NumberSequence")return false;
        return actual=="NumberStream" || actual=="FilteredSequence" || actual=="UniqueSequence" || actual=="OrderedSequence";
    };
    std::vector<GenomicStrategyActivation> result;
    for (const auto& operation:operations) {
        const auto locus=std::find_if(operationLoci.begin(),operationLoci.end(),[&](const auto& item){return item.operation==operation.operation;});
        if(locus==operationLoci.end()) throw std::invalid_argument("Genome does not encode operation: "+operation.operation);
        if(!compatible(operation.inputType,locus->input) || operation.outputType!=locus->output)
            throw std::invalid_argument("Typed development graph violates genomic contract for operation: "+operation.operation);
        GenomeEnhancer enhancer;enhancer.id=locus->region+910000;enhancer.targetRegion=locus->region;
        enhancer.name="Operation_"+locus->operation;enhancer.activationThreshold=0.45;enhancer.cooperativity=1.0;
        enhancer.sites.push_back(GenomeBindingSite{locus->signature,0.30,1.0,0.0,0,false,true});resolver.addEnhancer(enhancer);
        molecules.setConcentration(0,locus->signature,0.90,locus->operation,MoleculeClass::Activator);
    }
    const auto resolution=resolver.resolve(molecules);
    for (const auto& locus:operationLoci) {
        if(std::none_of(operations.begin(),operations.end(),[&](const auto& op){return op.operation==locus.operation;}))continue;
        const auto activity=resolution.regionActivityScores.contains(locus.region)?resolution.regionActivityScores.at(locus.region):0.0;
        result.push_back({locus.operation,locus.region,locus.signature,activity,resolution.activeRegions.contains(locus.region),locus.input+"->"+locus.output});
    }
    return result;
}
std::vector<GenomicStrategyActivation> regulateStrategies(const GenomeStrand& genome,
        ResearchProblem problem,ResearchDevelopmentState state,const std::string& learned) {
    const auto loci=researchLoci(genome);RegulatoryResolver resolver;MolecularPool molecules;
    const auto wanted=problemName(problem);
    for (const auto& locus:loci) {
        if (locus.problem!=wanted) continue;
        GenomeEnhancer enhancer;enhancer.id=locus.region+900000;enhancer.targetRegion=locus.region;
        enhancer.name="Research_"+locus.strategy;enhancer.activationThreshold=0.45;enhancer.cooperativity=1.0;
        enhancer.sites.push_back(GenomeBindingSite{locus.signature,0.30,1.0,0.0,0,false,true});
        resolver.addEnhancer(enhancer);
        double concentration=0.10;
        if (state==ResearchDevelopmentState::Explore) concentration=0.90;
        else if (state==ResearchDevelopmentState::Control && locus.condition=="basal") concentration=0.90;
        else if (state==ResearchDevelopmentState::Developed)
            concentration=(!learned.empty() && locus.strategy==learned)?0.95:
                (learned.empty() && locus.condition=="basal"?0.90:0.10);
        molecules.setConcentration(0,locus.signature,concentration,locus.strategy,MoleculeClass::Development);
    }
    const auto resolution=resolver.resolve(molecules);
    std::vector<GenomicStrategyActivation> activations;
    for (const auto& locus:loci) if (locus.problem==wanted) {
        const auto score=resolution.regionActivityScores.contains(locus.region)?resolution.regionActivityScores.at(locus.region):0.0;
        activations.push_back({locus.strategy,locus.region,locus.signature,score,
            resolution.activeRegions.contains(locus.region),locus.condition});
    }
    return activations;
}
DevelopmentExperience learnedExperience(const std::filesystem::path& path,const ResearchPlan& plan,
                                        uint64_t genomeHash) {
    DevelopmentExperience best;
    if (path.empty() || !std::filesystem::exists(path)) return best;
    std::ifstream file(path,std::ios::binary); if (!file) throw std::runtime_error("Cannot read research memory");
    const auto target=abstractionSignature(plan);
    std::string line;std::size_t count=0;
    while (std::getline(file,line)) {
        if (++count>100000 || line.size()>512) throw std::runtime_error("Research memory limit exceeded");
        const auto fields=split(line,'|');
        if (fields.size()==5 && fields[0]=="GPI-R3" && fields[1]==std::to_string(genomeHash) &&
            validMemoryToken(fields[2],true) && validMemoryToken(fields[3])) {
            std::size_t storedScore=0;
            const auto [end,error]=std::from_chars(fields[4].data(),fields[4].data()+fields[4].size(),storedScore);
            if (error!=std::errc{} || end!=fields[4].data()+fields[4].size()) continue;
            if (std::find(plan.strategies.begin(),plan.strategies.end(),fields[3])==plan.strategies.end()) continue;
            const double similarity=abstractionSimilarity(fields[2],target);
            // Related experience must share at least half of the abstract operations.
            // Exact matches take precedence; ties prefer the empirically lower score.
            if (similarity>=0.50 && (similarity>best.similarity ||
                (similarity==best.similarity && storedScore<best.score)))
                best={fields[2],fields[3],storedScore,similarity,fields[2]!=target};
        } else if (best.strategy.empty() && fields.size()==4 && fields[0]=="GPI-R2" &&
                   fields[1]==std::to_string(genomeHash) && fields[2]==problemName(plan.problem) &&
                   std::find(plan.strategies.begin(),plan.strategies.end(),fields[3])!=plan.strategies.end()) {
            // Read-only compatibility with v0.5/v0.6 memories. New evidence is always R3.
            best={problemName(plan.problem),fields[3],static_cast<std::size_t>(-1),1.0,false};
        }
    }
    if (!file.eof()) throw std::runtime_error("Research memory read failure");
    return best;
}
void rememberExperience(const std::filesystem::path& path,const ResearchPlan& plan,const std::string& strategy,
                        std::size_t score,uint64_t genomeHash) {
    if (path.empty() || plan.problem==ResearchProblem::ArbitraryCpp) return;
    const auto signature=abstractionSignature(plan);
    if (!validMemoryToken(signature,true) || !validMemoryToken(strategy))
        throw std::invalid_argument("Research experience contains an invalid token");
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary|std::ios::app);
    if (!file || !(file<<"GPI-R3|"<<genomeHash<<'|'<<signature<<'|'<<strategy<<'|'<<score<<'\n'))
        throw std::runtime_error("Cannot append research memory");
}
std::string operationPrefix(const ResearchPlan& plan,std::size_t length) {
    std::ostringstream operations;
    for (std::size_t i=0;i<length;++i) operations<<(i?",":"")<<plan.operations[i].operation;
    return operations.str();
}
std::string rememberComponent(const std::filesystem::path& path,const ResearchPlan& plan,
                              const std::string& strategy,std::size_t score,uint64_t genomeHash,
                              std::size_t semanticChecks,uint64_t compileMilliseconds,uint64_t executionMilliseconds) {
    const auto structure=discoverReusableStructure(plan);
    if (path.empty() || !structure || structure->begin!=0 || plan.problem!=ResearchProblem::Composite) return {};
    const auto length=structure->length;
    const auto operations=operationPrefix(plan,length);
    const auto& input=plan.operations.front().inputType;
    const auto& output=plan.operations[length-1].outputType;
    const auto id=componentId(input,output,operations,strategy);
    ReusableComponent component{id,input,output,split(operations,','),strategy,score,componentArtifactPath(path,id)};
    const auto expectedHeader=componentHeader(component);
    if (std::filesystem::exists(component.artifact)) {
        if (read(component.artifact)!=expectedHeader)
            throw std::runtime_error("Developed component artifact failed integrity verification: "+id);
    } else {
        std::filesystem::create_directories(component.artifact.parent_path());
        write(component.artifact,expectedHeader);
    }
    const auto provenancePath=componentProvenancePath(path,id);
    std::ostringstream componentProvenance;
    componentProvenance<<"FORMAT=GPI-COMPONENT\nCOMPONENT="<<id<<"\nGENOME_HASH="<<genomeHash
        <<"\nORIGIN_CONTRACT_HASH="<<plan.contract.semanticHash<<"\nINPUT_TYPE="<<input<<"\nOUTPUT_TYPE="<<output
        <<"\nOPERATIONS="<<operations<<"\nSTRATEGY="<<strategy<<"\nSCORE="<<score
        <<"\nSEMANTIC_CHECKS="<<semanticChecks<<"\nCOMPILE_MS="<<compileMilliseconds
        <<"\nEXECUTION_MS="<<executionMilliseconds
        <<"\nSTRUCTURE_RATIONALE="<<structure->rationale
        <<"\nACCESS_RULE=same_genome\nACCESS_RULE=typed_contiguous_graph\nACCESS_RULE=verified_safe_mode_only\n";
    if(std::filesystem::exists(provenancePath)) {
        const auto existingProvenance=read(provenancePath);
        if(existingProvenance.find("COMPONENT="+id+'\n')==std::string::npos ||
           existingProvenance.find("GENOME_HASH="+std::to_string(genomeHash)+'\n')==std::string::npos)
            throw std::runtime_error("Developed component provenance failed integrity verification: "+id);
    } else write(provenancePath,componentProvenance.str());
    bool exists=false;
    if (std::filesystem::exists(path)) {
        std::ifstream existing(path,std::ios::binary);std::string line;std::size_t count=0;
        while(std::getline(existing,line)) {
            if(++count>100000 || line.size()>512) throw std::runtime_error("Research memory limit exceeded");
            const auto fields=split(line,'|');
            if(fields.size()==8 && fields[0]=="GPI-R4" && fields[1]==std::to_string(genomeHash) && fields[2]==id)
                exists=true;
        }
        if(!existing.eof()) throw std::runtime_error("Research memory read failure");
    }
    if (exists) return {};
    std::ofstream file(path,std::ios::binary|std::ios::app);
    if (!file || !(file<<"GPI-R4|"<<genomeHash<<'|'<<id<<'|'<<input<<'|'<<output<<'|'<<operations<<'|'<<strategy<<'|'<<score<<'\n'))
        throw std::runtime_error("Cannot append developed component memory");
    return id;
}
std::optional<ReusableComponent> reusableComponent(const std::filesystem::path& path,const ResearchPlan& plan,
                                                   uint64_t genomeHash) {
    if (path.empty() || !std::filesystem::exists(path) || plan.problem!=ResearchProblem::Composite) return std::nullopt;
    const auto availableLength=reusablePrefixLength(plan);
    if (availableLength==0) return std::nullopt;
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read component memory");
    std::optional<ReusableComponent> best;std::string line;std::size_t count=0;
    while(std::getline(file,line)) {
        if(++count>100000 || line.size()>512)throw std::runtime_error("Research memory limit exceeded");
        const auto fields=split(line,'|');
        if(fields.size()!=8 || fields[0]!="GPI-R4" || fields[1]!=std::to_string(genomeHash))continue;
        if(!validMemoryToken(fields[2]) || !validMemoryToken(fields[3]) || !validMemoryToken(fields[4]) ||
           !validMemoryToken(fields[5],true) || !validMemoryToken(fields[6]))continue;
        std::size_t storedScore=0;const auto [end,error]=std::from_chars(fields[7].data(),fields[7].data()+fields[7].size(),storedScore);
        if(error!=std::errc{} || end!=fields[7].data()+fields[7].size())continue;
        const auto operations=split(fields[5],',');
        if(operations.size()<2 || operations.size()>availableLength ||
           std::find(plan.strategies.begin(),plan.strategies.end(),fields[6])==plan.strategies.end())continue;
        bool matches=fields[3]==plan.operations.front().inputType && fields[4]==plan.operations[operations.size()-1].outputType;
        for(std::size_t i=0;matches && i<operations.size();++i)matches=operations[i]==plan.operations[i].operation;
        if(!matches)continue;
        ReusableComponent candidate{fields[2],fields[3],fields[4],operations,fields[6],storedScore,
                                    componentArtifactPath(path,fields[2])};
        const auto provenancePath=componentProvenancePath(path,candidate.id);
        if(!std::filesystem::is_regular_file(candidate.artifact) || read(candidate.artifact)!=componentHeader(candidate) ||
           !std::filesystem::is_regular_file(provenancePath))
            throw std::runtime_error("Registered developed component failed integrity verification: "+candidate.id);
        const auto provenance=read(provenancePath);
        if(provenance.find("GENOME_HASH="+std::to_string(genomeHash)+'\n')==std::string::npos ||
           provenance.find("ACCESS_RULE=same_genome\n")==std::string::npos ||
           provenance.find("ACCESS_RULE=typed_contiguous_graph\n")==std::string::npos ||
           provenance.find("ACCESS_RULE=verified_safe_mode_only\n")==std::string::npos)
            throw std::runtime_error("Registered component provenance/access rules are invalid: "+candidate.id);
        if(!best || candidate.operations.size()>best->operations.size() ||
           (candidate.operations.size()==best->operations.size() && candidate.score<best->score))best=std::move(candidate);
    }
    if(!file.eof())throw std::runtime_error("Research memory read failure");
    return best;
}
void finalizeGoalContract(ResearchPlan& plan);
std::optional<ResearchPlan> deriveSafeExperiment(const ResearchPlan& target,const GenomeStrand& genome,std::size_t attempt) {
    if(target.problem!=ResearchProblem::Composite || reusablePrefixLength(target)==0 || target.operations.empty())
        return std::nullopt;
    const auto terminalIndex=target.operations.size()-1;
    const auto& current=target.operations[terminalIndex].operation;
    std::vector<std::pair<std::string,std::string>> alternatives;
    for(const auto& region:genome.getAllRegions()) {
        if(region.kind!=RegionKind::ResearchCapability)continue;
        const std::string payload(region.payload.begin(),region.payload.end());const auto fields=split(payload,'|');
        if(fields.size()!=5 || fields[0]!="gpi-operation-v1")continue;
        const bool implementedTerminal=fields[1]=="median" || fields[1]=="sum" || fields[1]=="frequency";
        if(implementedTerminal && fields[1]!=current)alternatives.emplace_back(fields[1],fields[4]);
    }
    const auto currentOutput=target.operations[terminalIndex].outputType;
    std::stable_sort(alternatives.begin(),alternatives.end(),[&](const auto& left,const auto& right) {
        const bool leftSame=left.second==currentOutput,rightSame=right.second==currentOutput;
        if(leftSame!=rightSame)return leftSame;
        return left.first<right.first;
    });
    if(attempt>=alternatives.size())return std::nullopt;
    ResearchPlan experiment=target;const auto& terminal=alternatives[attempt].first;
    const auto& output=alternatives[attempt].second;
    experiment.goal="Autonomously derived experiment: validate reusable prefix "+
        operationPrefix(target,reusablePrefixLength(target))+" with "+terminal+" oracle";
    experiment.operations[terminalIndex].operation=terminal;
    experiment.operations[terminalIndex].outputType=output;
    experiment.graph[terminalIndex+1].operation=terminal;
    experiment.graph[terminalIndex+1].outputType=output;
    if(terminalIndex+2<experiment.graph.size())experiment.graph[terminalIndex+2].inputType=output;
    experiment.contract.originalRequest=experiment.goal;
    experiment.contract.requiredOperations=experiment.operations;
    experiment.contract.normalizedIntent.clear();
    finalizeGoalContract(experiment);
    return experiment;
}
void finalizeGoalContract(ResearchPlan& plan) {
    auto& contract=plan.contract;
    contract.requiredOperations=plan.operations;
    contract.inputType="TextStream";contract.outputType="TextStream";
    std::ostringstream normalized;
    for(std::size_t i=0;i<plan.operations.size();++i) {
        const auto& operation=plan.operations[i];
        if(i)normalized<<" -> ";
        normalized<<operation.operation<<'('<<operation.inputType<<"->"<<operation.outputType;
        if(operation.integerParameter)normalized<<",parameter="<<*operation.integerParameter;
        normalized<<')';
    }
    contract.normalizedIntent=normalized.str();
    contract.semanticHash=stableComponentHash(contract.originalRequest+'|'+contract.normalizedIntent);
    contract.accepted=plan.supported && contract.unresolvedRequirements.empty() && contract.contradictions.empty();
    if(!contract.accepted)plan.supported=false;
}
} // namespace

ResearchPlan decomposeResearchTask(const std::string& prompt,bool unsafeMode) {
    ResearchPlan plan; plan.goal=prompt;plan.contract.originalRequest=prompt;
    if (prompt.empty() || prompt.size()>65536) {
        plan.diagnostics.push_back("Research prompt must contain 1..65536 bytes"); return plan;
    }
    plan.embeddedSource=fencedCpp(prompt);
    if (!plan.embeddedSource.empty()) {
        plan.problem=ResearchProblem::ArbitraryCpp; plan.requiresUnsafeMode=true;
        if (!unsafeMode) { plan.diagnostics.push_back("Embedded C++ requires --research-unsafe"); return plan; }
        plan.supported=true;
        plan.components={{"candidate","Execute prompt-provided C++ candidate","unrestricted candidate-owned structures","compiler-directed mutation"},
                         {"oracle","Compare observable behavior","fixture files","exact normalized comparison"}};
        plan.graph={{"candidate","arbitrary_candidate","SourceText","Executable",{}},
                    {"verify","behavior_oracle","Executable","VerifiedArtifact",{"candidate"}}};
        plan.strategies={"prompt-source","compiler-diagnostic-repair"}; return plan;
    }
    const auto text=lower(prompt);
    const bool frequency=containsAny(text,{"frequency","histogram","haeufig","häufig","vorkommen"});
    const bool median=containsAny(text,{"median","zentralwert"});
    const bool unique=containsAny(text,{"unique","deduplic","duplik","eindeutig"});
    const bool sorting=containsAny(text,{"sort","ordne","reihenfolge"});
    const bool sum=containsAny(text,{"summe","summiere","sum"});
    const bool descending=containsAny(text,{"descending","absteigend","fallende reihenfolge"});
    const bool lowerThreshold=containsAny(text,{"less than","kleiner als","unter ","<"});
    const bool unsupportedAggregate=containsAny(text,{"average","mean","mittelwert","minimum","maximum"});
    const bool unsupportedSideEffect=containsAny(text,{"speicher","save ","datei","csv","json","netzwerk","network","http","sende","send ","datenbank","database"});
    if(descending)plan.contract.unresolvedRequirements.push_back("descending ordering is not genomically encoded");
    if(lowerThreshold)plan.contract.unresolvedRequirements.push_back("lower-bound filter direction is not genomically encoded");
    if(unsupportedAggregate)plan.contract.unresolvedRequirements.push_back("requested aggregation is not genomically encoded");
    if(unsupportedSideEffect)plan.contract.unresolvedRequirements.push_back("requested side effect or external data format is not genomically encoded");
    const std::regex exactCardinality(R"((?:genau|exactly)\s+[0-9]+)",std::regex::icase);
    if(std::regex_search(prompt,exactCardinality))
        plan.contract.unresolvedRequirements.push_back("exact input cardinality is not represented by this research contract");
    std::optional<int64_t> threshold;
    const std::regex thresholdPattern(R"((?:>|greater than|groesser als|größer als|ueber|über)\s*(-?[0-9]+))",
                                      std::regex::icase);
    std::smatch thresholdMatch;
    if (std::regex_search(prompt,thresholdMatch,thresholdPattern)) {
        int64_t value=0;const auto token=thresholdMatch[1].str();
        const auto [end,error]=std::from_chars(token.data(),token.data()+token.size(),value);
        if(error==std::errc{} && end==token.data()+token.size())threshold=value;
        for(auto cursor=std::sregex_iterator(prompt.begin(),prompt.end(),thresholdPattern);
            cursor!=std::sregex_iterator();++cursor) {
            int64_t other=0;const auto otherToken=(*cursor)[1].str();
            const auto [otherEnd,otherError]=std::from_chars(otherToken.data(),otherToken.data()+otherToken.size(),other);
            if(otherError==std::errc{} && otherEnd==otherToken.data()+otherToken.size() && threshold && other!=*threshold)
                plan.contract.contradictions.push_back("multiple incompatible filter thresholds");
        }
    }
    const std::size_t aggregateCount=static_cast<std::size_t>(frequency)+static_cast<std::size_t>(median)+static_cast<std::size_t>(sum);
    if (aggregateCount>1) {
        plan.contract.contradictions.push_back("multiple terminal aggregations");
        plan.diagnostics.push_back("Composite task has conflicting terminal aggregations");
        finalizeGoalContract(plan);return plan;
    }
    if(!plan.contract.unresolvedRequirements.empty()) {
        plan.diagnostics.insert(plan.diagnostics.end(),plan.contract.unresolvedRequirements.begin(),plan.contract.unresolvedRequirements.end());
        finalizeGoalContract(plan);return plan;
    }
    const std::size_t operationCount=static_cast<std::size_t>(frequency)+static_cast<std::size_t>(median)+
        static_cast<std::size_t>(unique)+static_cast<std::size_t>(sorting)+static_cast<std::size_t>(sum)+
        static_cast<std::size_t>(threshold.has_value());
    if (operationCount>=2) plan.problem=ResearchProblem::Composite;
    else if (frequency) plan.problem=ResearchProblem::Frequency;
    else if (median) plan.problem=ResearchProblem::Median;
    else if (unique) plan.problem=ResearchProblem::Unique;
    else if (sorting) plan.problem=ResearchProblem::Sort;
    else { plan.diagnostics.push_back("No research strategy can yet ground this goal; provide a C++ candidate in --research-unsafe mode"); return plan; }
    plan.supported=true;
    plan.components.push_back({"input","Parse an open numeric stream","std::vector<int64_t>","validated extraction"});
    plan.graph.push_back({"input","parse_stream","TextStream","NumberStream",{}});
    if (plan.problem==ResearchProblem::Composite) {
        plan.strategies={"vector+component_pipeline","hash+component_pipeline"};
        std::string dependency="input",currentType="NumberStream";std::size_t node=0;
        auto add=[&](std::string operation,std::string output,std::optional<int64_t> parameter={}) {
            const auto id="operation_"+std::to_string(++node);
            plan.operations.push_back({operation,currentType,output,parameter});
            plan.graph.push_back({id,operation,currentType,output,{dependency}});
            plan.components.push_back({id,"Compose operation "+operation,output,"genomically gated component"});
            dependency=id;currentType=output;
        };
        if (threshold) add("filter_gt","FilteredSequence",threshold);
        if (unique) add("deduplicate","UniqueSequence");
        if (sorting) add("sort_ascending","OrderedSequence");
        if (median) add("median","Scalar");
        else if (sum) add("sum","Scalar");
        else if (frequency) add("frequency","FrequencyTable");
    } else if (plan.problem==ResearchProblem::Sort) {
        plan.operations.push_back({"sort_ascending","NumberStream","OrderedSequence",{}});
        plan.components.push_back({"ordering","Produce ascending order","vector","insertion sort then std::sort"});
        plan.strategies={"vector+insertion_sort","vector+introsort"};
        plan.graph.push_back({"transform","order","NumberStream","OrderedSequence",{"input"}});
    } else if (plan.problem==ResearchProblem::Unique) {
        plan.operations.push_back({"deduplicate","NumberStream","UniqueSequence",{}});
        plan.components.push_back({"identity","Remove repeated values","vector / unordered_set","sort-unique then hash-deduplicate"});
        plan.strategies={"vector+sort_unique","unordered_set+sorted_projection"};
        plan.graph.push_back({"transform","deduplicate","NumberStream","UniqueSequence",{"input"}});
    } else if (plan.problem==ResearchProblem::Median) {
        plan.operations.push_back({"median","NumberStream","Scalar",{}});
        plan.components.push_back({"selection","Find the central value","vector / two priority queues","full sort then streaming two-heaps"});
        plan.strategies={"vector+full_sort","two_heaps+streaming_balance"};
        plan.graph.push_back({"aggregate","median","NumberStream","Scalar",{"input"}});
    } else {
        plan.operations.push_back({"frequency","NumberStream","FrequencyTable",{}});
        plan.components.push_back({"aggregation","Count occurrences","map / unordered_map","ordered counting then hash counting"});
        plan.strategies={"map+ordered_count","unordered_map+sorted_keys"};
        plan.graph.push_back({"aggregate","frequency","NumberStream","FrequencyTable",{"input"}});
    }
    plan.components.push_back({"output","Emit deterministic machine-checkable results","ordered text stream","canonical serialization"});
    plan.graph.push_back({"output","canonicalize","ResultValue","TextStream",{plan.graph.back().id}});
    finalizeGoalContract(plan);
    return plan;
}

ResearchResult runResearchDevelopment(const ResearchPlan& plan,const ResearchConfig& config) {
    ResearchResult result;
    if (!plan.supported) { result.events.push_back("BLOCKED: research plan is unsupported"); return result; }
    if (plan.requiresUnsafeMode && !config.unsafeMode) { result.events.push_back("BLOCKED: unsafe mode is required"); return result; }
    if (plan.requiresUnsafeMode && config.sandboxRunner.empty()) {
        result.events.push_back("BLOCKED: arbitrary code requires a configured sandbox runner"); return result;
    }
    if (config.workspace.empty() || config.compiler.empty() || config.maxCycles<1 || config.maxCycles>8)
        throw std::invalid_argument("Research workspace/compiler and 1..8 cycles are required");
    if (!config.genome) throw std::invalid_argument("Research requires a frozen genomic context");
    result.genomeHash=config.genome->getGenomeHash();
    if (std::filesystem::exists(config.workspace) && !std::filesystem::is_empty(config.workspace))
        throw std::invalid_argument("Research workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    std::string input,expected;
    if (plan.problem==ResearchProblem::ArbitraryCpp) {
        if (config.fixtureInput.empty() || config.fixtureExpected.empty())
            throw std::invalid_argument("Arbitrary C++ requires explicit input and expected-output fixtures");
        input=read(config.fixtureInput); expected=read(config.fixtureExpected);
    } else {
        if(!plan.contract.accepted)throw std::invalid_argument("Safe research requires an accepted explicit goal contract");
        const auto fixture=contractFixture(plan,{5,1,3,3,2});input=fixture.first;expected=fixture.second;
    }
    const auto inputPath=config.workspace/"fixture.in";
    const auto expectedPath=config.workspace/"fixture.expected";
    write(inputPath,input); write(expectedPath,expected);
    std::filesystem::path goalInputPath,goalExpectedPath;
    std::string goalExpected;
    if(plan.problem!=ResearchProblem::ArbitraryCpp) {
        const auto goalFixture=contractFixture(plan,{9,-2,4,4,1,7});
        goalInputPath=config.workspace/"goal_contract.in";
        goalExpectedPath=config.workspace/"goal_contract.expected";
        goalExpected=goalFixture.second;write(goalInputPath,goalFixture.first);write(goalExpectedPath,goalExpected);
    }
    std::string arbitrarySource=plan.embeddedSource;
    if (plan.problem==ResearchProblem::ArbitraryCpp) validateArbitraryIncludes(arbitrarySource);
    std::vector<std::size_t> strategyOrder;
    const auto experience=config.developmentState==ResearchDevelopmentState::Developed?
        learnedExperience(config.memory,plan,result.genomeHash):DevelopmentExperience{};
    const auto& learned=experience.strategy;
    result.experienceSignature=abstractionSignature(plan);
    result.experienceSourceSignature=experience.sourceSignature;
    result.experienceSimilarity=experience.similarity;
    result.experienceTransferred=experience.transferred;
    const auto reusable=config.developmentState==ResearchDevelopmentState::Developed?
        reusableComponent(config.memory,plan,result.genomeHash):std::optional<ReusableComponent>{};
    if (!learned.empty()) {
        if (experience.transferred)
            result.events.push_back("Transferred developmental experience from "+experience.sourceSignature+
                " to "+result.experienceSignature+"; similarity="+std::to_string(experience.similarity)+
                "; activated strategy="+learned);
        else result.events.push_back("Reused verified strategy from research memory: "+learned);
    }
    if (plan.problem!=ResearchProblem::ArbitraryCpp) {
        result.regulation=regulateStrategies(*config.genome,plan.problem,config.developmentState,learned);
        if (plan.problem==ResearchProblem::Composite) {
            auto operationRegulation=regulateOperations(*config.genome,plan.operations);
            if(std::any_of(operationRegulation.begin(),operationRegulation.end(),[](const auto& item){return !item.expressed;})) {
                result.events.push_back("BLOCKED: a required genomic operation locus was not expressed");return result;
            }
            result.regulation.insert(result.regulation.end(),operationRegulation.begin(),operationRegulation.end());
        }
        for (std::size_t i=0;i<plan.strategies.size();++i) {
            const auto active=std::find_if(result.regulation.begin(),result.regulation.end(),[&](const auto& item) {
                return item.strategy==plan.strategies[i] && item.expressed;
            });
            if (active!=result.regulation.end()) strategyOrder.push_back(i);
        }
        if (strategyOrder.empty()) {
            result.events.push_back("BLOCKED: genomic regulation expressed no strategy for this developmental state");
            return result;
        }
    }
    const auto candidateCount=plan.problem==ResearchProblem::ArbitraryCpp?config.maxCycles:
        std::min(config.maxCycles,strategyOrder.size());
    std::size_t bestScore=static_cast<std::size_t>(-1);
    for (std::size_t i=0;i<candidateCount;++i) {
        const auto variant=plan.problem==ResearchProblem::ArbitraryCpp?i:strategyOrder[i];
        ResearchCycle cycle; cycle.number=i+1;
        cycle.strategy=plan.problem==ResearchProblem::ArbitraryCpp?(i==0?"prompt-source":"compiler-diagnostic-repair"):
            plan.strategies[variant];
        cycle.score=score(plan.problem,variant);
        if (plan.problem!=ResearchProblem::ArbitraryCpp) {
            const auto activation=std::find_if(result.regulation.begin(),result.regulation.end(),[&](const auto& item) {
                return item.strategy==cycle.strategy;
            });
            cycle.genomeRegion=activation->regionId;cycle.moleculeSignature=activation->moleculeSignature;
            cycle.regulatoryActivity=activation->activity;
        }
        cycle.source=config.workspace/("candidate_"+std::to_string(i+1)+".cpp");
#ifdef _WIN32
        cycle.executable=config.workspace/("candidate_"+std::to_string(i+1)+".exe");
#else
        cycle.executable=config.workspace/("candidate_"+std::to_string(i+1)+".bin");
#endif
        const auto* cycleComponent=reusable && cycle.strategy==reusable->strategy?&*reusable:nullptr;
        if (cycleComponent && result.reusedComponent.empty()) {
            result.reusedComponent=cycleComponent->id;
            result.componentArtifact=config.workspace/"developed_component.hpp";
            std::filesystem::copy_file(cycleComponent->artifact,result.componentArtifact,
                                       std::filesystem::copy_options::overwrite_existing);
            result.events.push_back("Reused verified typed component: "+cycleComponent->id+
                " ("+cycleComponent->inputType+"->"+cycleComponent->outputType+")");
        }
        const auto source=plan.problem==ResearchProblem::ArbitraryCpp?arbitrarySource:
            (plan.problem==ResearchProblem::Composite?compositeProgram(plan,variant!=0,cycleComponent):generatedSource(plan.problem,variant));
        write(cycle.source,source);
        const auto compilerOut=config.workspace/("cycle_"+std::to_string(i+1)+".compiler.out");
        const auto compilerErr=config.workspace/("cycle_"+std::to_string(i+1)+".compiler.err");
        std::vector<std::string> buildCommand{config.compiler,"-std=c++20","-Wall","-Wextra","-Wpedantic","-Werror","-O2",
            cycle.source.string(),"-o",cycle.executable.string()};
#ifdef _WIN32
        if (plan.requiresUnsafeMode) {
            buildCommand.insert(buildCommand.end()-2,"-static-libgcc");
            buildCommand.insert(buildCommand.end()-2,"-static-libstdc++");
        }
#endif
        const auto compileStart=std::chrono::steady_clock::now();
        const auto build=runProcess(buildCommand,config.workspace,{},compilerOut,compilerErr,120s);
        cycle.compileMilliseconds=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-compileStart).count());
        cycle.compiled=build.launched&&!build.timedOut&&build.exitCode==0;
        if(cycle.compiled)++result.totalCompiledCandidates;
        if (!cycle.compiled) {
            cycle.diagnostic=read(compilerErr);
            result.events.push_back("Cycle "+std::to_string(cycle.number)+" compile failed; candidate will be mutated if a diagnostic rule applies");
            if (plan.problem==ResearchProblem::ArbitraryCpp) {
                const auto repaired=repairArbitrarySource(arbitrarySource,cycle.diagnostic);
                if (repaired==arbitrarySource) { result.history.push_back(std::move(cycle)); break; }
                arbitrarySource=repaired;
            }
            result.totalCompileMilliseconds+=cycle.compileMilliseconds;
            result.history.push_back(std::move(cycle)); continue;
        }
        if (plan.requiresUnsafeMode) copyRuntimeLibraries(config.compiler,config.workspace);
        const auto runtimeOut=config.workspace/("cycle_"+std::to_string(i+1)+".runtime.out");
        const auto runtimeErr=config.workspace/("cycle_"+std::to_string(i+1)+".runtime.err");
        std::vector<std::string> runCommand{std::filesystem::absolute(cycle.executable).string()};
        if (plan.requiresUnsafeMode) {
            runCommand.insert(runCommand.begin(),{"--workspace",config.workspace.string(),"--timeout-seconds","10","--"});
            runCommand.insert(runCommand.begin(),config.sandboxRunner.string());
        }
        // The AppContainer runner enforces the candidate's 10 second limit itself.
        // Leave the supervising process enough time for profile setup, ACL work and
        // process teardown; otherwise a busy machine can kill a valid sandbox run
        // just as its output is being flushed.
        const auto supervisorTimeout=plan.requiresUnsafeMode ? 20s : 10s;
        const auto executionStart=std::chrono::steady_clock::now();
        const auto run=runProcess(runCommand,config.workspace,inputPath,runtimeOut,runtimeErr,supervisorTimeout);
        const bool fixtureVerified=run.launched&&!run.timedOut&&run.exitCode==0&&
            normalizeLineEndings(read(runtimeOut))==normalizeLineEndings(expected);
        cycle.semanticChecks=1;
        bool contractVerified=true;
        if(plan.problem!=ResearchProblem::ArbitraryCpp) {
            const auto goalOut=config.workspace/("cycle_"+std::to_string(i+1)+".goal.out");
            const auto goalErr=config.workspace/("cycle_"+std::to_string(i+1)+".goal.err");
            const auto goalRun=runProcess(runCommand,config.workspace,goalInputPath,goalOut,goalErr,supervisorTimeout);
            contractVerified=goalRun.launched&&!goalRun.timedOut&&goalRun.exitCode==0&&
                normalizeLineEndings(read(goalOut))==normalizeLineEndings(goalExpected);
            cycle.semanticChecks=2;
        }
        cycle.executionMilliseconds=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-executionStart).count());
        cycle.behaviorVerified=fixtureVerified&&contractVerified;
        if(cycle.compiled&&!cycle.behaviorVerified)++result.totalDecisionErrors;
        result.totalCompileMilliseconds+=cycle.compileMilliseconds;
        result.totalExecutionMilliseconds+=cycle.executionMilliseconds;
        result.totalSemanticChecks+=cycle.semanticChecks;
        if (cycle.behaviorVerified && cycle.score<bestScore) {
            bestScore=cycle.score; result.selectedStrategy=cycle.strategy; result.selectedSource=cycle.source;
            result.events.push_back("Cycle "+std::to_string(cycle.number)+" verified; new best score="+std::to_string(cycle.score));
        } else if (cycle.behaviorVerified) result.events.push_back("Cycle "+std::to_string(cycle.number)+" verified but did not improve score");
        else result.events.push_back("Cycle "+std::to_string(cycle.number)+" behavior mismatch");
        result.history.push_back(std::move(cycle));
        if (plan.problem==ResearchProblem::ArbitraryCpp && result.history.back().behaviorVerified) break;
    }
    result.cycles=result.history.size();
    if (!result.selectedSource.empty()) {
        const auto solution=config.workspace/"solution.cpp";
        std::filesystem::copy_file(result.selectedSource,solution,std::filesystem::copy_options::overwrite_existing);
        result.selectedSource=solution; result.success=true; result.behaviorVerified=true;
        if (config.developmentState!=ResearchDevelopmentState::Control) {
            rememberExperience(config.memory,plan,result.selectedStrategy,bestScore,result.genomeHash);
            result.developedComponent=rememberComponent(config.memory,plan,result.selectedStrategy,bestScore,result.genomeHash,
                result.totalSemanticChecks,result.totalCompileMilliseconds,result.totalExecutionMilliseconds);
            if(!result.developedComponent.empty()) {
                result.componentArtifact=componentArtifactPath(config.memory,result.developedComponent);
                result.events.push_back("Developed reusable typed component: "+result.developedComponent);
            }
        }
        write(config.workspace/"research_report.txt","RESULT=PASS\nCYCLES="+std::to_string(result.cycles)+
            "\nGENOME_HASH="+std::to_string(result.genomeHash)+"\nSELECTED_STRATEGY="+result.selectedStrategy+
            "\nGENOME_REGION="+std::to_string(result.history.empty()?0:result.history.back().genomeRegion)+
             "\nDEVELOPMENT_STATE="+stateName(config.developmentState)+
             "\nDEVELOPED_COMPONENT="+result.developedComponent+
             "\nREUSED_COMPONENT="+result.reusedComponent+
             "\nSCORE="+std::to_string(bestScore)+"\n");
    } else write(config.workspace/"research_report.txt","RESULT=BLOCKED\nCYCLES="+std::to_string(result.cycles)+"\n");
    std::ostringstream provenance;
    provenance<<"{\n  \"format\": \"GPI-research-v1.0\",\n"
              <<"  \"genome_hash\": "<<result.genomeHash<<",\n"
              <<"  \"development_state\": \""<<stateName(config.developmentState)<<"\",\n"
              <<"  \"experience_strategy\": \""<<learned<<"\",\n"
              <<"  \"experience_signature\": \""<<result.experienceSignature<<"\",\n"
              <<"  \"experience_source_signature\": \""<<result.experienceSourceSignature<<"\",\n"
              <<"  \"experience_similarity\": "<<result.experienceSimilarity<<",\n"
              <<"  \"experience_transferred\": "<<(result.experienceTransferred?"true":"false")<<",\n"
              <<"  \"developed_component\": \""<<result.developedComponent<<"\",\n"
              <<"  \"reused_component\": \""<<result.reusedComponent<<"\",\n"
              <<"  \"component_artifact\": \""<<result.componentArtifact.generic_string()<<"\",\n"
              <<"  \"regulation\": [\n";
    for (std::size_t i=0;i<result.regulation.size();++i) {
        const auto& activation=result.regulation[i];
        provenance<<"    {\"strategy\": \""<<activation.strategy<<"\", \"region\": "<<activation.regionId
                  <<", \"signature\": "<<activation.moleculeSignature<<", \"activity\": "<<activation.activity
                  <<", \"expressed\": "<<(activation.expressed?"true":"false")<<"}"
                  <<(i+1==result.regulation.size()?"\n":",\n");
    }
    provenance<<"  ],\n  \"cycles\": [\n";
    for (std::size_t i=0;i<result.history.size();++i) {
        const auto& cycle=result.history[i];
        provenance<<"    {\"number\": "<<cycle.number<<", \"strategy\": \""<<cycle.strategy
                  <<"\", \"region\": "<<cycle.genomeRegion<<", \"signature\": "<<cycle.moleculeSignature
                  <<", \"activity\": "<<cycle.regulatoryActivity<<", \"semantic_checks\": "<<cycle.semanticChecks
                  <<", \"compile_ms\": "<<cycle.compileMilliseconds<<", \"execution_ms\": "<<cycle.executionMilliseconds<<", \"compiled\": "
                  <<(cycle.compiled?"true":"false")<<", \"behavior_verified\": "
                  <<(cycle.behaviorVerified?"true":"false")<<", \"score\": "<<cycle.score<<"}"
                  <<(i+1==result.history.size()?"\n":",\n");
    }
    provenance<<"  ],\n  \"goal_contract_hash\": "<<plan.contract.semanticHash
              <<",\n  \"goal_contract_verified\": "<<(result.behaviorVerified?"true":"false")
              <<",\n  \"total_compiled_candidates\": "<<result.totalCompiledCandidates
              <<",\n  \"total_decision_errors\": "<<result.totalDecisionErrors
              <<",\n  \"total_semantic_checks\": "<<result.totalSemanticChecks
              <<",\n  \"total_compile_ms\": "<<result.totalCompileMilliseconds
              <<",\n  \"total_execution_ms\": "<<result.totalExecutionMilliseconds
              <<",\n  \"selected_strategy\": \""<<result.selectedStrategy
              <<"\",\n  \"success\": "<<(result.success?"true":"false")<<"\n}\n";
    write(config.workspace/"research_provenance.json",provenance.str());
    return result;
}

ResearchResult runAutonomousDevelopment(const ResearchPlan& plan,const ResearchConfig& config,
                                        std::size_t maxAutonomousCycles) {
    if(maxAutonomousCycles<2 || maxAutonomousCycles>8)
        throw std::invalid_argument("Autonomous development requires 2..8 bounded cycles");
    if(!plan.supported || plan.requiresUnsafeMode || plan.problem!=ResearchProblem::Composite)
        throw std::invalid_argument("Autonomous development is restricted to safe typed composite plans");
    if(reusablePrefixLength(plan)==0)
        throw std::invalid_argument("Autonomous development requires at least two reusable transform operations");
    if(!config.genome || config.memory.empty())
        throw std::invalid_argument("Autonomous development requires an immutable genome and persistent memory");
    if(config.workspace.empty() || (std::filesystem::exists(config.workspace) && !std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Autonomous campaign workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    const auto genomeHash=config.genome->getGenomeHash();
    ResearchResult campaign;campaign.autonomousMode=true;campaign.genomeHash=genomeHash;
    const auto prefix=operationPrefix(plan,reusablePrefixLength(plan));
    auto component=reusableComponent(config.memory,plan,genomeHash);
    std::size_t phases=0,experiments=0;bool baselineSolvable=false,researchJustified=false;
    if(component)campaign.capabilityDiagnosis="VERIFIED_REUSABLE_CAPABILITY";
    if(!component) {
        ResearchConfig probeConfig=config;probeConfig.workspace=config.workspace/"diagnosis_probe";
        probeConfig.developmentState=ResearchDevelopmentState::Control;probeConfig.maxCycles=1;probeConfig.unsafeMode=false;
        const auto probe=runResearchDevelopment(plan,probeConfig);++phases;
        campaign.baselineVerified=probe.behaviorVerified;
        campaign.baselineScore=probe.history.empty()?static_cast<std::size_t>(-1):probe.history.front().score;
        campaign.totalSemanticChecks+=probe.totalSemanticChecks;
        campaign.totalCompileMilliseconds+=probe.totalCompileMilliseconds;
        campaign.totalExecutionMilliseconds+=probe.totalExecutionMilliseconds;
        campaign.totalCompiledCandidates+=probe.totalCompiledCandidates;
        campaign.totalDecisionErrors+=probe.totalDecisionErrors;
        baselineSolvable=probe.behaviorVerified;
        std::uintmax_t sourceBytes=0;
        if(probe.behaviorVerified && !probe.selectedSource.empty() && std::filesystem::is_regular_file(probe.selectedSource))
            sourceBytes=std::filesystem::file_size(probe.selectedSource);
        if(baselineSolvable) {
            researchJustified=reusablePrefixLength(plan)>=2 && probe.totalSemanticChecks>=2 && sourceBytes>=256;
            campaign.capabilityDiagnosis=researchJustified?"SOLVABLE_OPTIMIZATION_EVIDENCE":"SOLVABLE_NO_RESEARCH_REQUIRED";
            campaign.events.push_back("Capability probe verified the original goal; component absence is not a capability gap");
            if(researchJustified)campaign.events.push_back("Research authorized by measured evidence: source_bytes="+
                std::to_string(sourceBytes)+"; compile_ms="+std::to_string(probe.totalCompileMilliseconds)+
                "; semantic_checks="+std::to_string(probe.totalSemanticChecks));
        } else {
            campaign.capabilityDiagnosis="MISSING_EXECUTABLE_CAPABILITY";
            campaign.capabilityGaps.push_back("Original goal failed independent baseline verification for prefix: "+prefix);
            researchJustified=true;
        }
        for(std::size_t attempt=0;researchJustified && phases+1<maxAutonomousCycles && !component;++attempt) {
            const auto experiment=deriveSafeExperiment(plan,*config.genome,attempt);
            if(!experiment)break;
            campaign.derivedGoals.push_back(experiment->goal);
            ResearchConfig experimentConfig=config;
            experimentConfig.workspace=config.workspace/("experiment_"+std::to_string(attempt+1));
            experimentConfig.developmentState=ResearchDevelopmentState::Explore;
            experimentConfig.maxCycles=2;
            experimentConfig.unsafeMode=false;
            const auto evidence=runResearchDevelopment(*experiment,experimentConfig);
            ++experiments;++phases;
            campaign.totalSemanticChecks+=evidence.totalSemanticChecks;
            campaign.totalCompileMilliseconds+=evidence.totalCompileMilliseconds;
            campaign.totalExecutionMilliseconds+=evidence.totalExecutionMilliseconds;
            campaign.totalCompiledCandidates+=evidence.totalCompiledCandidates;
            campaign.totalDecisionErrors+=evidence.totalDecisionErrors;
            campaign.events.push_back("Autonomous experiment "+std::to_string(experiments)+
                (evidence.behaviorVerified?" verified":" failed verification")+": "+experiment->goal);
            if(evidence.behaviorVerified)component=reusableComponent(config.memory,plan,genomeHash);
        }
    }
    if(!component && !baselineSolvable) {
        campaign.autonomousCycles=phases;
        campaign.events.push_back("BLOCKED: autonomous cycle budget produced no reusable verified component");
        write(config.workspace/"autonomous_research_report.txt","FORMAT=GPI-autonomous-v1.0\nRESULT=BLOCKED\nCYCLES="+
              std::to_string(campaign.autonomousCycles)+"\nGAP="+(campaign.capabilityGaps.empty()?"":campaign.capabilityGaps.front())+"\n");
        return campaign;
    }
    ResearchConfig targetConfig=config;
    targetConfig.workspace=config.workspace/"target";
    targetConfig.developmentState=component?ResearchDevelopmentState::Developed:ResearchDevelopmentState::Control;
    targetConfig.unsafeMode=false;
    auto target=runResearchDevelopment(plan,targetConfig);
    target.autonomousMode=true;target.autonomousCycles=phases+1;
    target.capabilityDiagnosis=campaign.capabilityDiagnosis;
    target.baselineVerified=campaign.baselineVerified;target.baselineScore=campaign.baselineScore;
    target.totalSemanticChecks+=campaign.totalSemanticChecks;
    target.totalCompileMilliseconds+=campaign.totalCompileMilliseconds;
    target.totalExecutionMilliseconds+=campaign.totalExecutionMilliseconds;
    target.totalCompiledCandidates+=campaign.totalCompiledCandidates;
    target.totalDecisionErrors+=campaign.totalDecisionErrors;
    target.capabilityGaps=std::move(campaign.capabilityGaps);
    target.derivedGoals=std::move(campaign.derivedGoals);
    target.events.insert(target.events.end(),campaign.events.begin(),campaign.events.end());
    target.events.push_back("Autonomous target cycle "+std::string(target.behaviorVerified?"verified":"failed verification"));
    std::ostringstream report;
    report<<"FORMAT=GPI-autonomous-v1.0\nRESULT="<<(target.success?"PASS":"BLOCKED")
          <<"\nCYCLES="<<target.autonomousCycles<<"\nGENOME_HASH="<<target.genomeHash
          <<"\nCAPABILITY_DIAGNOSIS="<<target.capabilityDiagnosis
          <<"\nBASELINE_VERIFIED="<<(target.baselineVerified?"YES":"NO")
          <<"\nBASELINE_SCORE="<<target.baselineScore
          <<"\nTOTAL_COMPILED_CANDIDATES="<<target.totalCompiledCandidates
          <<"\nTOTAL_DECISION_ERRORS="<<target.totalDecisionErrors
          <<"\nTOTAL_SEMANTIC_CHECKS="<<target.totalSemanticChecks
          <<"\nTOTAL_COMPILE_MS="<<target.totalCompileMilliseconds
          <<"\nTOTAL_EXECUTION_MS="<<target.totalExecutionMilliseconds
          <<"\nTARGET_COMPONENT="<<target.reusedComponent<<'\n';
    for(const auto& gap:target.capabilityGaps)report<<"CAPABILITY_GAP="<<gap<<'\n';
    for(const auto& goal:target.derivedGoals)report<<"DERIVED_GOAL="<<goal<<'\n';
    write(config.workspace/"autonomous_research_report.txt",report.str());
    std::ostringstream provenance;
    provenance<<"{\n  \"format\": \"GPI-autonomous-v1.0\",\n  \"genome_hash\": "<<target.genomeHash
              <<",\n  \"bounded_cycles\": "<<maxAutonomousCycles<<",\n  \"executed_cycles\": "<<target.autonomousCycles
              <<",\n  \"capability_diagnosis\": \""<<target.capabilityDiagnosis
              <<"\",\n  \"baseline_verified\": "<<(target.baselineVerified?"true":"false")
              <<",\n  \"total_compiled_candidates\": "<<target.totalCompiledCandidates
              <<",\n  \"total_decision_errors\": "<<target.totalDecisionErrors
              <<",\n  \"total_semantic_checks\": "<<target.totalSemanticChecks
              <<",\n  \"total_compile_ms\": "<<target.totalCompileMilliseconds
              <<",\n  \"total_execution_ms\": "<<target.totalExecutionMilliseconds
              <<",\n  \"capability_gaps\": [";
    for(std::size_t i=0;i<target.capabilityGaps.size();++i)
        provenance<<(i?", ":"")<<'"'<<target.capabilityGaps[i]<<'"';
    provenance<<"],\n  \"derived_goals\": [";
    for(std::size_t i=0;i<target.derivedGoals.size();++i)
        provenance<<(i?", ":"")<<'"'<<target.derivedGoals[i]<<'"';
    provenance<<"],\n  \"reused_component\": \""<<target.reusedComponent
              <<"\",\n  \"behavior_verified\": "<<(target.behaviorVerified?"true":"false")
              <<",\n  \"success\": "<<(target.success?"true":"false")<<"\n}\n";
    write(config.workspace/"autonomous_provenance.json",provenance.str());
    return target;
}

} // namespace genomic::gpi
