#include "gpi/autonomous_project_developer.hpp"

#include "cpp20/cpp20_lexicon.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace genomic::gpi {
namespace {
std::string json(const std::string& value) {
    std::string result="\"";
    for(const unsigned char c:value) {
        if(c=='\"'||c=='\\')result+='\\';
        if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";
        else if(c>=0x20)result+=static_cast<char>(c);
    }
    return result+'\"';
}
std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
std::string trim(std::string value) {
    const auto begin=value.find_first_not_of(" \t\r\n");
    if(begin==std::string::npos)return {};
    return value.substr(begin,value.find_last_not_of(" \t\r\n")-begin+1);
}
bool integer(const std::string& text,int64_t& value) {
    const auto cleaned=trim(text);if(cleaned.empty())return false;
    const auto [end,error]=std::from_chars(cleaned.data(),cleaned.data()+cleaned.size(),value);
    return error==std::errc{}&&end==cleaned.data()+cleaned.size();
}
bool difference(int64_t left,int64_t right,int64_t& result) {
    if((right<0&&left>std::numeric_limits<int64_t>::max()+right)||
       (right>0&&left<std::numeric_limits<int64_t>::min()+right))return false;
    result=left-right;return true;
}
bool sum(int64_t left,int64_t right,int64_t& result) {
    if((right>0&&left>std::numeric_limits<int64_t>::max()-right)||
       (right<0&&left<std::numeric_limits<int64_t>::min()-right))return false;
    result=left+right;return true;
}
std::size_t tokenOffset(const std::string& source,uint32_t line,uint32_t column) {
    std::size_t offset=0;uint32_t currentLine=1;
    while(currentLine<line&&offset<source.size())if(source[offset++]=='\n')++currentLine;
    return offset+static_cast<std::size_t>(column-1);
}
void writeCampaign(AutonomousProjectResult& result,const AutonomousProjectConfig& config) {
    if(result.success&&!result.experienceTransferred&&!config.experienceStore.empty()) {
        ProjectExperienceStore store(config.experienceStore);
        for(const auto& learned:result.transformations)store.append(learned);
    }
    result.report=config.workspace/"campaign.json";std::ofstream report(result.report,std::ios::binary|std::ios::trunc);
    report<<"{\n  \"format\": \"gpi-autonomous-project-campaign-v1\",\n  \"immutable_genome_hash\": "
          <<result.immutableGenomeHash<<",\n  \"success\": "<<(result.success?"true":"false")
          <<",\n  \"selected_hypothesis\": "<<json(result.selectedHypothesis)
          <<",\n  \"learned_transformations\": "<<result.transformations.size()
          <<",\n  \"experience_transferred\": "<<(result.experienceTransferred?"true":"false")
          <<",\n  \"experience_origin_graph_hash\": "<<result.experienceOriginGraphHash
          <<",\n  \"cycles\": [\n";
    for(std::size_t i=0;i<result.cycles.size();++i) {
        const auto& cycle=result.cycles[i];
        report<<"    {\"number\": "<<cycle.number<<", \"hypothesis\": "<<json(cycle.hypothesis)
              <<", \"rationale\": "<<json(cycle.rationale)<<", \"graph_hash\": "<<cycle.evidence.graphHash
              <<", \"verified\": "<<(cycle.evidence.success?"true":"false")<<"}"
              <<(i+1==result.cycles.size()?"\n":",\n");
    }
    report<<"  ]\n}\n";
    if(!report)throw std::runtime_error("Cannot write autonomous project campaign evidence");
}
VerifiedProjectTransformation transformation(const GenerativeProjectGraph& seed,
    const GenerativeProjectGraph& verified,ProjectTransformationKind kind,std::string node,
    std::string auxiliary,std::string before,std::string after,const ProjectVerificationEvidence& evidence) {
    return {seed.genomeHash(),seed.graphHash(),verified.graphHash(),kind,std::move(node),std::move(auxiliary),
        std::move(before),std::move(after),projectEvidenceHash(evidence.report)};
}
}

AutonomousProjectResult runEvidenceGuidedProjectDevelopment(
    const std::vector<ProjectHypothesis>& hypotheses,
    const ProjectVerificationContract& invariantGoalContract,
    const AutonomousProjectConfig& config) {
    if(hypotheses.empty())throw std::invalid_argument("Autonomous development requires project hypotheses");
    if(config.maxCycles<1||config.maxCycles>16)throw std::invalid_argument("Autonomous project cycles must be in 1..16");
    if(config.workspace.empty()||(std::filesystem::exists(config.workspace)&&!std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Autonomous development workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    AutonomousProjectResult result;result.immutableGenomeHash=hypotheses.front().graph.genomeHash();
    std::unordered_set<std::string> ids;
    const auto cycles=std::min(config.maxCycles,hypotheses.size());
    for(std::size_t i=0;i<cycles;++i) {
        const auto& hypothesis=hypotheses[i];
        if(hypothesis.id.empty()||!ids.insert(hypothesis.id).second)
            throw std::invalid_argument("Hypothesis IDs must be non-empty and unique");
        if(hypothesis.graph.genomeHash()!=result.immutableGenomeHash)
            throw std::invalid_argument("All hypotheses must use the same immutable genome");
        ProjectDevelopmentCycle cycle;cycle.number=i+1;cycle.hypothesis=hypothesis.id;cycle.rationale=hypothesis.rationale;
        cycle.evidence=verifyGenerativeProject(hypothesis.graph,invariantGoalContract,
            config.workspace/("cycle_"+std::to_string(i+1)),config.compiler,config.sandboxRunner);
        result.cycles.push_back(std::move(cycle));
        if(result.cycles.back().evidence.success) {
            result.success=true;result.selectedHypothesis=hypothesis.id;break;
        }
    }
    writeCampaign(result,config);
    return result;
}

AutonomousProjectResult runAutonomousProjectRepair(const GenerativeProjectGraph& seed,
    const ProjectVerificationContract& contract,const AutonomousProjectConfig& config) {
    if(config.maxCycles<2||config.maxCycles>16)throw std::invalid_argument("Autonomous repair requires 2..16 cycles");
    if(config.workspace.empty()||(std::filesystem::exists(config.workspace)&&!std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Autonomous repair workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    AutonomousProjectResult result;result.immutableGenomeHash=seed.genomeHash();
    ProjectDevelopmentCycle initial;initial.number=1;initial.hypothesis="seed";
    initial.rationale="Measure the unmodified project against the invariant goal contract";
    initial.evidence=verifyGenerativeProject(seed,contract,config.workspace/"cycle_1",config.compiler,config.sandboxRunner);
    result.cycles.push_back(initial);
    if(initial.evidence.success){result.success=true;result.selectedHypothesis="seed";writeCampaign(result,config);return result;}

    const ProjectVerificationCheck* failure=nullptr;
    for(auto it=initial.evidence.checks.rbegin();it!=initial.evidence.checks.rend();++it)
        if(!it->passed){failure=&*it;break;}
    if(!failure){writeCampaign(result,config);return result;}
    if(failure->kind=="compile") {
        const auto compilerDiagnostic=read(failure->standardError);
        const bool missingDeclaration=compilerDiagnostic.find("defined in header")!=std::string::npos||
            compilerDiagnostic.find("not a member of")!=std::string::npos||
            compilerDiagnostic.find("was not declared in this scope")!=std::string::npos||
            compilerDiagnostic.find("undeclared identifier")!=std::string::npos||
            compilerDiagnostic.find("unknown type name")!=std::string::npos||
            compilerDiagnostic.find("incomplete type")!=std::string::npos;
        const auto headers=seed.mutationEnabled("insert-standard-header")&&missingDeclaration?
            seed.suggestedHeaders(compilerDiagnostic):std::vector<std::string>{};
        const auto failedTarget=std::find_if(seed.targets().begin(),seed.targets().end(),
            [&](const auto& item){return item.name==failure->name;});
        if(failedTarget!=seed.targets().end()&&!headers.empty()) {
            std::size_t hypothesis=0;
            auto repairNodes=seed.resolvedTargetNodes(failedTarget->name);
            auto normalizedDiagnostic=compilerDiagnostic;
            std::replace(normalizedDiagnostic.begin(),normalizedDiagnostic.end(),'\\','/');
            const auto diagnosticMentions=[&](const std::string& id) {
                const auto found=std::find_if(seed.nodes().begin(),seed.nodes().end(),
                    [&](const auto& item){return item.id==id;});
                return found!=seed.nodes().end()&&
                    normalizedDiagnostic.find(found->artifact.relativePath.generic_string())!=std::string::npos;
            };
            std::stable_sort(repairNodes.begin(),repairNodes.end(),[&](const auto& left,const auto& right) {
                return diagnosticMentions(left)>diagnosticMentions(right);
            });
            for(const auto& nodeId:repairNodes) {
                const auto node=std::find_if(seed.nodes().begin(),seed.nodes().end(),[&](const auto& item){return item.id==nodeId;});
                if(node==seed.nodes().end()||(node->kind!=ProjectNodeKind::Source&&node->kind!=ProjectNodeKind::ModuleImplementation&&node->kind!=ProjectNodeKind::Test))continue;
                auto content=seed.nodeContent(nodeId);std::string inserted;
                for(const auto& header:headers) {
                    const auto directive="#include <"+header+">";
                    if(content.find(directive)==std::string::npos)inserted+=directive+'\n';
                }
                if(inserted.empty())continue;
                auto candidate=seed;candidate.replaceNodeContent(nodeId,inserted+content);
                ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
                cycle.hypothesis="genomic-header-"+std::to_string(++hypothesis);
                cycle.rationale="Compiler diagnostics activated genomic standard-header rules for node "+nodeId;
                cycle.evidence=verifyGenerativeProject(candidate,contract,
                    config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
                result.cycles.push_back(std::move(cycle));
                if(result.cycles.back().evidence.success) {
                    result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                    for(const auto& header:headers)
                        if(content.find("#include <"+header+">")==std::string::npos)
                            result.transformations.push_back(transformation(seed,candidate,ProjectTransformationKind::InsertHeader,
                                nodeId,header,"","",result.cycles.back().evidence));
                    writeCampaign(result,config);return result;
                }
                if(result.cycles.size()>=config.maxCycles)break;
            }
        }
        if(result.cycles.size()<config.maxCycles&&seed.mutationEnabled("infer-link-provider")&&
           (compilerDiagnostic.find("undefined reference")!=std::string::npos||
            compilerDiagnostic.find("unresolved external")!=std::string::npos)&&failedTarget!=seed.targets().end()) {
            std::string consumer;
            for(const auto& id:failedTarget->nodes) {
                const auto node=std::find_if(seed.nodes().begin(),seed.nodes().end(),[&](const auto& item){return item.id==id;});
                if(node!=seed.nodes().end()&&(node->kind==ProjectNodeKind::Source||node->kind==ProjectNodeKind::ModuleImplementation||node->kind==ProjectNodeKind::Test)) {consumer=id;break;}
            }
            const auto closure=seed.resolvedTargetNodes(failedTarget->name);
            std::vector<std::pair<std::size_t,std::string>> providers;
            for(const auto& node:seed.nodes()) {
                if(node.id==consumer||std::find(closure.begin(),closure.end(),node.id)!=closure.end()||
                   (node.kind!=ProjectNodeKind::Source&&node.kind!=ProjectNodeKind::ModuleImplementation))continue;
                std::unordered_set<std::string> identifiers;
                try {
                    for(const auto& token:Cpp20Lexicon::tokenize(seed.nodeContent(node.id)))
                        if(token.type==TokenType::Identifier&&token.text.size()>=3)identifiers.insert(token.text);
                } catch(...) { continue; }
                std::size_t score=0;for(const auto& identifier:identifiers)
                    if(compilerDiagnostic.find(identifier)!=std::string::npos)++score;
                if(score)providers.emplace_back(score,node.id);
            }
            std::sort(providers.begin(),providers.end(),[](const auto& a,const auto& b){
                return a.first!=b.first?a.first>b.first:a.second<b.second;});
            std::size_t linkHypothesis=0;
            for(const auto& [score,provider]:providers) {
                auto candidate=seed;candidate.addDependency(consumer,provider,ProjectDependencyKind::Links);
                ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
                cycle.hypothesis="genomic-link-"+std::to_string(++linkHypothesis);
                cycle.rationale="Linker diagnostics matched "+std::to_string(score)+" provider symbols in node "+provider;
                cycle.evidence=verifyGenerativeProject(candidate,contract,
                    config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
                result.cycles.push_back(std::move(cycle));
                if(result.cycles.back().evidence.success) {
                    result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                    result.transformations.push_back(transformation(seed,candidate,ProjectTransformationKind::AddLinkDependency,
                        consumer,provider,"","",result.cycles.back().evidence));
                    writeCampaign(result,config);return result;
                }
                if(result.cycles.size()>=config.maxCycles)break;
            }
        }
        if(result.cycles.size()<config.maxCycles&&seed.mutationEnabled("coerce-numeric-string-literal")&&
           failedTarget!=seed.targets().end()) {
            std::size_t typeHypothesis=0;
            for(const auto& nodeId:seed.resolvedTargetNodes(failedTarget->name)) {
                const auto node=std::find_if(seed.nodes().begin(),seed.nodes().end(),[&](const auto& item){return item.id==nodeId;});
                if(node==seed.nodes().end()||(node->kind!=ProjectNodeKind::Source&&node->kind!=ProjectNodeKind::ModuleImplementation&&node->kind!=ProjectNodeKind::Test))continue;
                const auto source=seed.nodeContent(nodeId);
                for(const auto& token:Cpp20Lexicon::tokenize(source)) {
                    if(token.type!=TokenType::StringLiteral)continue;
                    int64_t numeric=0;if(!integer(token.text,numeric))continue;
                    const auto offset=tokenOffset(source,token.line,token.column);
                    if(offset+token.text.size()+1>=source.size()||source[offset]!='"'||
                       source.compare(offset+1,token.text.size(),token.text)!=0||source[offset+token.text.size()+1]!='"')continue;
                    auto candidate=seed;auto changed=source;
                    changed.replace(offset,token.text.size()+2,std::to_string(numeric));candidate.replaceNodeContent(nodeId,changed);
                    ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
                    cycle.hypothesis="genomic-type-"+std::to_string(++typeHypothesis);
                    cycle.rationale="Compiler rejection activated numeric string-literal coercion in node "+nodeId;
                    cycle.evidence=verifyGenerativeProject(candidate,contract,
                        config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
                    result.cycles.push_back(std::move(cycle));
                    if(result.cycles.back().evidence.success) {
                        result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                        result.transformations.push_back(transformation(seed,candidate,ProjectTransformationKind::ReplaceExactToken,
                            nodeId,"coerce-numeric-string-literal","\""+token.text+"\"",std::to_string(numeric),result.cycles.back().evidence));
                        writeCampaign(result,config);return result;
                    }
                    if(result.cycles.size()>=config.maxCycles)break;
                }
                if(result.cycles.size()>=config.maxCycles)break;
            }
        }
        writeCampaign(result,config);return result;
    }
    if(failure->kind!="behavior"){writeCampaign(result,config);return result;}
    const auto behavior=std::find_if(contract.behaviors.begin(),contract.behaviors.end(),
        [&](const auto& item){return item.name==failure->name;});
    if(behavior==contract.behaviors.end()){writeCampaign(result,config);return result;}
    const auto target=std::find_if(seed.targets().begin(),seed.targets().end(),
        [&](const auto& item){return item.name==behavior->target;});
    if(target==seed.targets().end()){writeCampaign(result,config);return result;}
    int64_t observed=0,expected=0;
    if(!integer(read(failure->standardOutput),observed)||!integer(behavior->expectedOutput,expected)) {
        if(seed.mutationEnabled("local-integer-neighborhood")) {
            std::size_t hypothesis=0;
            for(const auto& nodeId:seed.resolvedTargetNodes(target->name)) {
                const auto node=std::find_if(seed.nodes().begin(),seed.nodes().end(),[&](const auto& item){return item.id==nodeId;});
                if(node==seed.nodes().end()||(node->kind!=ProjectNodeKind::Test&&node->kind!=ProjectNodeKind::Source&&node->kind!=ProjectNodeKind::ModuleImplementation))continue;
                const auto source=seed.nodeContent(nodeId);
                for(const auto& token:Cpp20Lexicon::tokenize(source)) {
                    if(token.type!=TokenType::IntLiteral)continue;
                    int64_t literal=0;if(!integer(token.text,literal))continue;
                    for(const int64_t direction:{-1LL,1LL}) {
                        int64_t replacement=0;if(!sum(literal,direction,replacement))continue;
                        const auto offset=tokenOffset(source,token.line,token.column);
                        if(offset>source.size()||source.compare(offset,token.text.size(),token.text)!=0)continue;
                        auto candidate=seed;auto changed=source;
                        changed.replace(offset,token.text.size(),std::to_string(replacement));candidate.replaceNodeContent(nodeId,changed);
                        ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
                        cycle.hypothesis="integer-neighbor-"+std::to_string(++hypothesis);
                        cycle.rationale="Failed behavior activated local integer-token search in node "+nodeId;
                        cycle.evidence=verifyGenerativeProject(candidate,contract,
                            config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
                        result.cycles.push_back(std::move(cycle));
                        if(result.cycles.back().evidence.success) {
                            result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                            result.transformations.push_back(transformation(seed,candidate,ProjectTransformationKind::ReplaceExactToken,
                                nodeId,"local-integer-neighborhood",token.text,std::to_string(replacement),result.cycles.back().evidence));
                            writeCampaign(result,config);return result;
                        }
                        if(result.cycles.size()>=config.maxCycles){writeCampaign(result,config);return result;}
                    }
                }
            }
        }
        writeCampaign(result,config);return result;
    }
    int64_t delta=0;if(!difference(expected,observed,delta)||delta==0){writeCampaign(result,config);return result;}
    if(!seed.mutationEnabled("token-integer-delta")){writeCampaign(result,config);return result;}
    std::size_t mutation=0;
    for(const auto& nodeId:seed.resolvedTargetNodes(target->name)) {
        const auto node=std::find_if(seed.nodes().begin(),seed.nodes().end(),[&](const auto& item){return item.id==nodeId;});
        if(node==seed.nodes().end()||(node->kind!=ProjectNodeKind::Source&&node->kind!=ProjectNodeKind::ModuleImplementation))continue;
        const auto source=seed.nodeContent(nodeId);
        for(const auto& token:Cpp20Lexicon::tokenize(source)) {
            if(token.type!=TokenType::IntLiteral)continue;
            int64_t literal=0;if(!integer(token.text,literal))continue;
            int64_t trialLiteral=0;if(!sum(literal,delta,trialLiteral))continue;
            auto candidate=seed;auto changed=source;const auto offset=tokenOffset(source,token.line,token.column);
            if(offset>changed.size()||changed.compare(offset,token.text.size(),token.text)!=0)continue;
            changed.replace(offset,token.text.size(),std::to_string(trialLiteral));candidate.replaceNodeContent(nodeId,changed);
            ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
            cycle.hypothesis="numeric-delta-"+std::to_string(++mutation);
            cycle.rationale="Behavior evidence delta "+std::to_string(delta)+" applied to integer token in node "+nodeId;
            cycle.evidence=verifyGenerativeProject(candidate,contract,
                config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
            result.cycles.push_back(std::move(cycle));
            if(result.cycles.back().evidence.success) {
                result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                result.transformations.push_back(transformation(seed,candidate,ProjectTransformationKind::ReplaceExactToken,
                    nodeId,"token-integer-delta",token.text,std::to_string(trialLiteral),result.cycles.back().evidence));
                writeCampaign(result,config);return result;
            }
            if(result.cycles.size()>=config.maxCycles){writeCampaign(result,config);return result;}
            const ProjectVerificationCheck* trialFailure=nullptr;
            for(auto it=result.cycles.back().evidence.checks.rbegin();it!=result.cycles.back().evidence.checks.rend();++it)
                if(!it->passed){trialFailure=&*it;break;}
            int64_t trialObserved=0,outputChange=0,remaining=0;
            if(seed.mutationEnabled("empirical-numeric-calibration")&&trialFailure&&
               trialFailure->kind=="behavior"&&trialFailure->name==failure->name&&
               integer(read(trialFailure->standardOutput),trialObserved)&&
               difference(trialObserved,observed,outputChange)&&outputChange!=0&&
               difference(expected,trialObserved,remaining)) {
                const long double adjustment=static_cast<long double>(remaining)*static_cast<long double>(delta)/
                    static_cast<long double>(outputChange);
                const long double rounded=std::round(adjustment);
                if(std::fabs(adjustment-rounded)<1e-12L&&rounded>=static_cast<long double>(std::numeric_limits<int64_t>::min())&&
                   rounded<=static_cast<long double>(std::numeric_limits<int64_t>::max())) {
                    int64_t calibrated=0;
                    if(sum(trialLiteral,static_cast<int64_t>(rounded),calibrated)&&calibrated!=trialLiteral) {
                        auto refined=seed;auto refinedSource=source;
                        refinedSource.replace(offset,token.text.size(),std::to_string(calibrated));
                        refined.replaceNodeContent(nodeId,refinedSource);
                        ProjectDevelopmentCycle refinedCycle;refinedCycle.number=result.cycles.size()+1;
                        refinedCycle.hypothesis="numeric-calibrated-"+std::to_string(++mutation);
                        refinedCycle.rationale="Empirical output sensitivity "+std::to_string(outputChange)+"/"+
                            std::to_string(delta)+" calibrated token in node "+nodeId;
                        refinedCycle.evidence=verifyGenerativeProject(refined,contract,
                            config.workspace/("cycle_"+std::to_string(refinedCycle.number)),config.compiler,config.sandboxRunner);
                        result.cycles.push_back(std::move(refinedCycle));
                        if(result.cycles.back().evidence.success) {
                            result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
                            result.transformations.push_back(transformation(seed,refined,ProjectTransformationKind::ReplaceExactToken,
                                nodeId,"empirical-numeric-calibration",token.text,std::to_string(calibrated),result.cycles.back().evidence));
                            writeCampaign(result,config);return result;
                        }
                        if(result.cycles.size()>=config.maxCycles){writeCampaign(result,config);return result;}
                    }
                }
            }
        }
    }
    writeCampaign(result,config);return result;
}

AutonomousProjectResult runExperienceTransferredProjectDevelopment(const GenerativeProjectGraph& seed,
    const ProjectVerificationContract& contract,const AutonomousProjectConfig& config,
    const ProjectExperienceStore& experience) {
    if(config.maxCycles<1||config.maxCycles>16)throw std::invalid_argument("Experience transfer requires 1..16 cycles");
    if(config.workspace.empty()||(std::filesystem::exists(config.workspace)&&!std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Experience transfer workspace must be new or empty");
    std::filesystem::create_directories(config.workspace);
    AutonomousProjectResult result;result.immutableGenomeHash=seed.genomeHash();
    const auto learned=experience.load();std::unordered_set<uint64_t> attempted;
    for(const auto& first:learned) {
        if(first.genomeHash!=seed.genomeHash()||!attempted.insert(first.verifiedGraphHash).second)continue;
        auto candidate=seed;std::vector<VerifiedProjectTransformation> group;
        bool applicable=true;
        for(const auto& item:learned) {
            if(item.genomeHash!=first.genomeHash||item.originGraphHash!=first.originGraphHash||
               item.verifiedGraphHash!=first.verifiedGraphHash||item.evidenceHash!=first.evidenceHash)continue;
            try {candidate=applyVerifiedTransformation(candidate,item);group.push_back(item);}
            catch(const std::exception&) {applicable=false;break;}
        }
        if(!applicable||group.empty())continue;
        ProjectDevelopmentCycle cycle;cycle.number=result.cycles.size()+1;
        cycle.hypothesis="experience-"+std::to_string(first.verifiedGraphHash);
        cycle.rationale="Transfer a provenance-linked transformation group and independently reverify it";
        cycle.evidence=verifyGenerativeProject(candidate,contract,
            config.workspace/("cycle_"+std::to_string(cycle.number)),config.compiler,config.sandboxRunner);
        result.cycles.push_back(std::move(cycle));
        if(result.cycles.back().evidence.success) {
            result.success=true;result.selectedHypothesis=result.cycles.back().hypothesis;
            result.experienceTransferred=true;result.experienceOriginGraphHash=first.originGraphHash;
            result.transformations=std::move(group);break;
        }
        if(result.cycles.size()>=config.maxCycles)break;
    }
    writeCampaign(result,config);return result;
}
} // namespace genomic::gpi
