#include "gpi/developmental_intelligence.hpp"

#include "gpi/experience_store.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace genomic::gpi {
namespace {
std::string hex(const std::string& value) {
    static constexpr char digits[]="0123456789abcdef";
    std::string result;
    result.reserve(value.size()*2);
    for(const unsigned char byte:value) {
        result+=digits[byte>>4];
        result+=digits[byte&15];
    }
    return result;
}
std::string unhex(std::string_view value) {
    if(value.size()%2)throw std::runtime_error("Corrupt component experience hex string");
    auto digit=[](char c)->unsigned {
        if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');
        if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);
        throw std::runtime_error("Corrupt component experience hex digit");
    };
    std::string result;
    result.reserve(value.size()/2);
    for(std::size_t i=0;i<value.size();i+=2)
        result.push_back(static_cast<char>((digit(value[i])<<4)|digit(value[i+1])));
    return result;
}
std::uint64_t parseUint64(std::string_view value) {
    std::uint64_t result=0;
    const auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(ec!=std::errc{}||end!=value.data()+value.size())
        throw std::runtime_error("Corrupt component experience integer");
    return result;
}
std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start=0;
    while(start<=line.size()) {
        const auto pos=line.find('|',start);
        parts.push_back(line.substr(start,pos==std::string::npos?pos:pos-start));
        if(pos==std::string::npos)break;
        start=pos+1;
    }
    return parts;
}
std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start=0;
    while(true) {
        const auto end=line.find('\t',start);
        parts.push_back(line.substr(start,end==std::string::npos?end:end-start));
        if(end==std::string::npos)break;
        start=end+1;
    }
    return parts;
}
std::string jsonEscape(const std::string& value) {
    std::string result="\"";
    for(const unsigned char c:value) {
        if(c=='"'||c=='\\')result+='\\';
        if(c=='\n')result+="\\n";
        else if(c=='\r')result+="\\r";
        else if(c>=0x20)result+=static_cast<char>(c);
    }
    return result+'"';
}
void writeFile(const std::filesystem::path& path,const std::string& content) {
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out<<content;
    if(!out)throw std::runtime_error("Cannot write developmental intelligence artifact: "+path.string());
}
std::string stageName(NeuralDevelopmentStage stage) {
    switch(stage) {
        case NeuralDevelopmentStage::Neonatal: return "Neonatal";
        case NeuralDevelopmentStage::Juvenile: return "Juvenile";
        case NeuralDevelopmentStage::Mature: return "Mature";
    }
    return "Mature";
}
NeuralDevelopmentStage parseStage(const std::string& text) {
    if(text=="Neonatal"||text=="neonatal")return NeuralDevelopmentStage::Neonatal;
    if(text=="Juvenile"||text=="juvenile")return NeuralDevelopmentStage::Juvenile;
    if(text=="Mature"||text=="mature")return NeuralDevelopmentStage::Mature;
    throw std::invalid_argument("Unknown developmental stage: "+text);
}
std::string serializeSingleGoalType(const GoalType& type) {
    TypedArchitectureGoal dummy;
    dummy.projectName="component_type";
    dummy.namespaceName="generated";
    dummy.types.push_back(type);
    GoalFunction placeholder;
    placeholder.name="id_check";
    placeholder.returnType="long long";
    placeholder.parameters.push_back({"long long","x"});
    placeholder.operations.push_back({"r","copy",{"x"}});
    placeholder.returnValue="r";
    dummy.functions.push_back(placeholder);
    dummy.entryFunction="id_check";
    dummy.entryExpected="#1";
    dummy.entryArguments={"#1"};
    dummy.cases.push_back({"c1","id_check","#1",{"#1"}});
    return serializeTypedArchitectureGoal(dummy);
}
std::string serializeSingleGoalFunction(const GoalFunction& fn,const std::vector<GoalType>& knownTypes) {
    TypedArchitectureGoal dummy;
    dummy.projectName="component_fn";
    dummy.namespaceName="generated";
    dummy.types=knownTypes;
    dummy.functions.push_back(fn);
    dummy.entryFunction=fn.name;
    dummy.entryExpected="#0";
    for(const auto& p:fn.parameters) {
        bool isStruct=false;
        for(const auto& t:knownTypes) {
            if(t.name==p.type) {
                std::ostringstream lit;
                lit<<t.name<<"{";
                for(std::size_t i=0;i<t.fields.size();++i)lit<<(i?",":"")<<"#0";
                lit<<"}";
                dummy.entryArguments.push_back(lit.str());
                isStruct=true;break;
            }
        }
        if(!isStruct)dummy.entryArguments.push_back("#0");
    }
    dummy.cases.push_back({"c1",fn.name,dummy.entryExpected,dummy.entryArguments});
    return serializeTypedArchitectureGoal(dummy);
}
} // namespace

std::vector<VerifiedComponentKnowledge> ComponentExperienceStore::load() const {
    std::vector<VerifiedComponentKnowledge> result;
    if(path_.empty()||!std::filesystem::exists(path_))return result;
    std::ifstream in(path_,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot open component experience store");
    std::string line;
    std::uint64_t previous=0;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        if(line.size()>131072||result.size()>=100000)throw std::runtime_error("Component experience store limit exceeded");
        const auto fields=splitPipe(line);
        if(fields.size()<10||fields[0]!="GDIC1")throw std::runtime_error("Corrupt component experience record");
        const auto sep=line.rfind('|');
        const auto recordHash=fingerprint(line.substr(0,sep));
        if(parseUint64(fields.back())!=recordHash)throw std::runtime_error("Component experience hash mismatch");
        const auto genomeHash=parseUint64(fields[1]);
        const auto goalHash=parseUint64(fields[2]);
        const auto graphHash=parseUint64(fields[3]);
        const auto evidenceHash=parseUint64(fields[4]);
        const auto count=parseUint64(fields[5]);
        if(count<1||count>256||fields.size()!=8+count*3)throw std::runtime_error("Corrupt component experience group size");
        const auto prevIdx=6+count*3;
        if(parseUint64(fields[prevIdx])!=previous)throw std::runtime_error("Component experience chain mismatch");
        for(std::size_t i=0;i<count;++i) {
            const auto off=6+i*3;
            VerifiedComponentKnowledge item;
            item.genomeHash=genomeHash;
            item.goalHash=goalHash;
            item.verifiedGraphHash=graphHash;
            item.evidenceHash=evidenceHash;
            item.kind=unhex(fields[off]);
            item.name=unhex(fields[off+1]);
            item.serializedGoalFragment=unhex(fields[off+2]);
            if(item.genomeHash==0||item.verifiedGraphHash==0||item.evidenceHash==0||
               (item.kind!="function"&&item.kind!="type")||item.name.empty()||item.serializedGoalFragment.empty())
                throw std::runtime_error("Invalid component experience record fields");
            result.push_back(std::move(item));
        }
        previous=recordHash;
    }
    if(!in.eof())throw std::runtime_error("Component experience store read failure");
    return result;
}

void ComponentExperienceStore::appendGroup(const std::vector<VerifiedComponentKnowledge>& entries) const {
    if(entries.empty()||path_.empty())return;
    const auto existing=load();
    if(existing.size()+entries.size()>100000)throw std::runtime_error("Component experience store full");
    std::uint64_t previous=0;
    if(!existing.empty()) {
        std::ifstream in(path_,std::ios::binary);
        std::string line,last;
        while(std::getline(in,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(!line.empty())last=line;
        }
        if(last.empty())throw std::runtime_error("Cannot extend corrupt component experience store");
        previous=parseUint64(splitPipe(last).back());
    }
    const auto& first=entries.front();
    std::string payload="GDIC1|"+std::to_string(first.genomeHash)+'|'+std::to_string(first.goalHash)+'|'+
        std::to_string(first.verifiedGraphHash)+'|'+std::to_string(first.evidenceHash)+'|'+std::to_string(entries.size());
    for(const auto& item:entries) {
        if(item.genomeHash!=first.genomeHash||item.verifiedGraphHash!=first.verifiedGraphHash||item.evidenceHash!=first.evidenceHash)
            throw std::invalid_argument("Component experience group has inconsistent provenance");
        payload+='|'+hex(item.kind)+'|'+hex(item.name)+'|'+hex(item.serializedGoalFragment);
    }
    payload+='|'+std::to_string(previous);
    if(!path_.parent_path().empty())std::filesystem::create_directories(path_.parent_path());
    std::ofstream out(path_,std::ios::binary|std::ios::app);
    if(!out||!(out<<payload<<'|'<<fingerprint(payload)<<'\n'))
        throw std::runtime_error("Cannot append component experience group");
}

DevelopmentalCurriculum curriculumFromBehavioralGoal(const BehavioralGoal& rawGoal) {
    DevelopmentalCurriculum curriculum;
    curriculum.curriculumName=rawGoal.projectName.empty()?"developmental_curriculum":rawGoal.projectName;
    curriculum.namespaceName=rawGoal.namespaceName.empty()?"generated":rawGoal.namespaceName;

    // If the behavioral goal has multiple types/functions, we can either run it as a unified Mature stage
    // or decompose it into progressive developmental stages (Neonatal -> Juvenile -> Mature) if >=2 functions!
    if(rawGoal.functions.size()<=1&&rawGoal.types.empty()) {
        DevelopmentalTaskStage stage;
        stage.stageId="stage_1_"+(rawGoal.functions.empty()?rawGoal.functionName:rawGoal.functions.front().name);
        stage.requiredStage=NeuralDevelopmentStage::Neonatal;
        stage.goal=rawGoal;
        curriculum.stages.push_back(std::move(stage));
        return curriculum;
    }

    // Progressive ontogenetic staging from a multi-component BehavioralGoal:
    // Stage 1 (Neonatal): foundational scalar functions (functions taking only scalar parameters and no struct types)
    // Stage 2 (Juvenile): user-defined types and their methods (if any) + first function using them
    // Stage 3 (Mature): full composite multi-module project with all types, modules, concepts, enums and functions
    std::vector<BehavioralFunctionGoal> scalarFns;
    std::vector<BehavioralFunctionGoal> compositeFns;
    for(const auto& fn:rawGoal.functions) {
        bool usesStruct=false;
        for(const auto& p:fn.parameters) {
            if(std::any_of(rawGoal.types.begin(),rawGoal.types.end(),[&](const auto& t){return t.name==p.type;}))
                usesStruct=true;
        }
        if(std::any_of(rawGoal.types.begin(),rawGoal.types.end(),[&](const auto& t){return t.name==fn.returnType;}))
            usesStruct=true;
        if(!usesStruct&&scalarFns.empty())scalarFns.push_back(fn);
        else compositeFns.push_back(fn);
    }

    if(!scalarFns.empty()&&!compositeFns.empty()) {
        DevelopmentalTaskStage s1;
        s1.stageId="neonatal_"+scalarFns.front().name;
        s1.requiredStage=NeuralDevelopmentStage::Neonatal;
        s1.goal.projectName=rawGoal.projectName+"_neonatal";
        s1.goal.namespaceName=rawGoal.namespaceName;
        for(const auto& m:rawGoal.modules)if(m.name==scalarFns.front().moduleName)s1.goal.modules.push_back({m.name,{}});
        s1.goal.functions=scalarFns;
        s1.goal.entryFunction=scalarFns.front().name;
        curriculum.stages.push_back(std::move(s1));
    }

    DevelopmentalTaskStage finalStage;
    finalStage.stageId="mature_"+rawGoal.projectName;
    finalStage.requiredStage=curriculum.stages.empty()?NeuralDevelopmentStage::Juvenile:NeuralDevelopmentStage::Mature;
    finalStage.goal=rawGoal;
    curriculum.stages.push_back(std::move(finalStage));
    return curriculum;
}

DevelopmentalCurriculum parseDevelopmentalCurriculumText(const std::string& text) {
    std::istringstream in(text);
    std::string firstLine;
    while(std::getline(in,firstLine)) {
        if(!firstLine.empty()&&firstLine.back()=='\r')firstLine.pop_back();
        if(firstLine.empty()||firstLine[0]=='#')continue;
        break;
    }
    if(firstLine=="GDI_BEHAVIORAL_GOAL_V1"||firstLine=="GDI_BEHAVIORAL_GOAL_V2") {
        return curriculumFromBehavioralGoal(parseBehavioralGoalText(text));
    }
    if(firstLine!="GDI_DEVELOPMENTAL_CURRICULUM_V1")
        throw std::invalid_argument("Unsupported developmental curriculum header: "+firstLine);

    DevelopmentalCurriculum curriculum;
    std::string line;
    std::ostringstream currentStageBlock;
    std::string currentStageId;
    NeuralDevelopmentStage currentReqStage=NeuralDevelopmentStage::Neonatal;
    bool inStage=false;

    const auto flushStage=[&]() {
        if(!inStage)return;
        const auto stageText=currentStageBlock.str();
        if(stageText.empty())throw std::invalid_argument("Empty developmental stage block: "+currentStageId);
        DevelopmentalTaskStage st;
        st.stageId=currentStageId;
        st.requiredStage=currentReqStage;
        st.goal=parseBehavioralGoalText(stageText);
        curriculum.stages.push_back(std::move(st));
        currentStageBlock.str("");
        currentStageBlock.clear();
        inStage=false;
    };

    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line[0]=='#')continue;
        const auto parts=splitTab(line);
        if(parts[0]=="curriculum"&&parts.size()==2&&!inStage) {
            curriculum.curriculumName=parts[1];
        } else if(parts[0]=="namespace"&&parts.size()==2&&!inStage) {
            curriculum.namespaceName=parts[1];
        } else if(parts[0]=="stage"&&parts.size()==3) {
            flushStage();
            currentStageId=parts[1];
            currentReqStage=parseStage(parts[2]);
            inStage=true;
            currentStageBlock<<"GDI_BEHAVIORAL_GOAL_V2\n";
        } else if(parts[0]=="end-stage") {
            flushStage();
        } else if(inStage) {
            currentStageBlock<<line<<'\n';
        } else {
            throw std::invalid_argument("Unexpected line in developmental curriculum: "+line);
        }
    }
    flushStage();
    if(curriculum.curriculumName.empty()||curriculum.stages.empty())
        throw std::invalid_argument("Developmental curriculum requires a name and at least one stage");
    return curriculum;
}

DevelopmentalCurriculum loadDevelopmentalCurriculum(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary);
    if(!in)throw std::invalid_argument("Cannot read developmental curriculum file: "+file.string());
    const std::string content{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    return parseDevelopmentalCurriculumText(content);
}

DevelopmentalIntelligenceResult runDevelopmentalProgrammingIntelligence(
    const DevelopmentalCurriculum& curriculum,
    const DevelopmentalIntelligenceConfig& config) {

    if(curriculum.stages.empty())
        throw std::invalid_argument("Developmental intelligence requires at least one curriculum stage");
    if(config.workspace.empty()||(std::filesystem::exists(config.workspace)&&!std::filesystem::is_empty(config.workspace)))
        throw std::invalid_argument("Developmental intelligence workspace must be new or empty");
    if(config.compiler.empty()||!std::filesystem::is_regular_file(config.compiler))
        throw std::invalid_argument("Developmental intelligence requires an existing compiler binary");
    if(config.requireSandbox&&(config.sandboxRunner.empty()||!std::filesystem::is_regular_file(config.sandboxRunner)))
        throw std::invalid_argument("Developmental intelligence requires an available sandbox runner");
    if(config.maxCyclesPerStage<2||config.maxCyclesPerStage>16)
        throw std::invalid_argument("Developmental intelligence maxCyclesPerStage must be in 2..16");

    std::filesystem::create_directories(config.workspace);

    // 1. Bootstrap the immutable genome, regulatory resolver, phenotype builder, and multi-compartment system
    auto sys=GenomeBootstrap::buildCompleteSystem();
    if(!sys.strand||!sys.strand->isFrozen()||!sys.strand->verifyIntegrity().passed)
        throw std::runtime_error("Developmental intelligence requires an intact frozen genome");
    const auto initialGenomeHash=sys.strand->getGenomeHash();
    const auto sourceGenome=Cpp20SourceGenome::fromGenome(*sys.strand);

    MultiCompartmentSystem compartments(initialGenomeHash,true);
    std::vector<GenomeRegionId> analysisRegions,synthesisRegions,researchRegions;
    for(const auto& region:sys.strand->getAllRegions()) {
        if(region.kind==RegionKind::TaskGrammar||region.kind==RegionKind::BootDictionary||region.kind==RegionKind::SymbolTable)
            analysisRegions.push_back(region.id);
        if(region.kind==RegionKind::CppGenerativeRule||region.kind==RegionKind::CppGrammar||region.kind==RegionKind::TypeSystem)
            synthesisRegions.push_back(region.id);
        if(region.kind==RegionKind::ResearchCapability||region.kind==RegionKind::RegulatoryNetwork||region.kind==RegionKind::DevelopmentalPrograms)
            researchRegions.push_back(region.id);
    }
    compartments.addCompartment({1,CompartmentRole::Analysis,false,
        {DevelopmentSignalKind::SynthesisCompleted,DevelopmentSignalKind::NegativeEvidence},analysisRegions});
    compartments.addCompartment({2,CompartmentRole::Synthesis,true,
        {DevelopmentSignalKind::GoalAccepted,DevelopmentSignalKind::EvidenceVerified,DevelopmentSignalKind::ComponentReady},synthesisRegions});
    compartments.addCompartment({3,CompartmentRole::Research,true,
        {DevelopmentSignalKind::CapabilityProbeRequested,DevelopmentSignalKind::GoalUncertain,
         DevelopmentSignalKind::ResearchAuthorized},researchRegions});

    MolecularPool organismPool;
    organismPool.setConcentration(0,MoleculeSignatures::NutrientRich,0.85,"NutrientRich",MoleculeClass::Resource);
    LineageContext corticalLineage{TissueLineage::AnteriorNervous,0,"AnteriorCortical"};
    sys.phenotypeBuilder->setStage(NeuralDevelopmentStage::Neonatal);
    sys.phenotypeBuilder->developEntity(1,corticalLineage,organismPool,1);

    ComponentExperienceStore componentStore(config.componentMemory);
    std::vector<GoalType> accumulatedTypes;
    std::vector<GoalFunction> accumulatedFunctions;

    // Load previously verified components from ComponentExperienceStore if available
    if(!config.componentMemory.empty()&&std::filesystem::exists(config.componentMemory)&&
       std::filesystem::file_size(config.componentMemory)>0) {
        for(const auto& item:componentStore.load()) {
            if(item.genomeHash!=initialGenomeHash)continue;
            try {
                const auto parsed=parseTypedArchitectureGoalText(item.serializedGoalFragment);
                if(item.kind=="type") {
                    for(const auto& t:parsed.types) {
                        if(t.name==item.name&&std::none_of(accumulatedTypes.begin(),accumulatedTypes.end(),
                            [&](const auto& existing){return existing.name==t.name;}))
                            accumulatedTypes.push_back(t);
                    }
                } else if(item.kind=="function") {
                    for(const auto& fn:parsed.functions) {
                        if(fn.name==item.name&&std::none_of(accumulatedFunctions.begin(),accumulatedFunctions.end(),
                            [&](const auto& existing){return existing.name==fn.name;}))
                            accumulatedFunctions.push_back(fn);
                    }
                }
            } catch(...) {
                continue;
            }
        }
    }

    DevelopmentalIntelligenceResult result;
    result.immutableGenomeHash=initialGenomeHash;

    std::uint64_t tick=10;
    for(std::size_t idx=0;idx<curriculum.stages.size();++idx) {
        const auto& stageSpec=curriculum.stages[idx];
        tick+=10;

        // Advance organism developmental stage if required
        if(static_cast<int>(stageSpec.requiredStage)>static_cast<int>(sys.phenotypeBuilder->getStage())) {
            sys.phenotypeBuilder->setStage(stageSpec.requiredStage);
        }
        if(sys.phenotypeBuilder->getStage()==NeuralDevelopmentStage::Mature) {
            sys.phenotypeBuilder->developEntity(4,corticalLineage,organismPool,tick);
        }

        DevelopmentalStageReport stageReport;
        stageReport.stageNumber=idx+1;
        stageReport.stageId=stageSpec.stageId;
        stageReport.organismStage=sys.phenotypeBuilder->getStage();
        stageReport.stageWorkspace=config.workspace/("stage_"+std::to_string(idx+1)+"_"+stageSpec.stageId);
        std::filesystem::create_directories(stageReport.stageWorkspace);

        // Compute deterministic contract hash for the stage goal
        const auto contractHash=fingerprint(stageSpec.stageId+"|"+stageSpec.goal.projectName+"|"+std::to_string(idx+1));
        stageReport.contractHash=contractHash;

        // Compartment 1 (Analysis) accepts goal and signals Compartment 3 (Research) & Compartment 2 (Synthesis)
        const bool goalDelivered=compartments.exchange({DevelopmentSignalKind::GoalAccepted,1,2,0,
            contractHash,0.95,"stage goal accepted: "+stageSpec.stageId,false});
        const bool probeDelivered=compartments.exchange({DevelopmentSignalKind::CapabilityProbeRequested,1,3,0,
            contractHash,0.90,"probe capabilities and experience for "+stageSpec.stageId,false});
        stageReport.goalAccepted=goalDelivered&&probeDelivered;

        // Evaluate regulatory resolution in organism pool
        organismPool.setConcentration(0,MoleculeSignatures::ThreatDetected,0.80,"DevelopmentalChallenge",MoleculeClass::Signal);
        const auto regRes=sys.resolver->resolve(organismPool,0);
        stageReport.regulatoryAuthorized=!regRes.activeRegions.empty()&&sourceGenome.structureEnabled("typed-functions");
        if(!stageReport.goalAccepted||!stageReport.regulatoryAuthorized) {
            result.stageReports.push_back(std::move(stageReport));
            break;
        }

        // Inject accumulated verified types and functions into stage BehavioralGoal
        BehavioralGoal enrichedGoal=stageSpec.goal;
        enrichedGoal.importedTypes=accumulatedTypes;
        enrichedGoal.importedFunctions=accumulatedFunctions;

        if(!accumulatedTypes.empty()||!accumulatedFunctions.empty()) {
            compartments.exchange({DevelopmentSignalKind::ComponentReady,3,2,0,contractHash,0.95,
                "injected "+std::to_string(accumulatedTypes.size()+accumulatedFunctions.size())+" verified components",false});
        }

        // Induce architecture goal from behavioral examples + imported components
        const auto induced=induceArchitectureGoal(enrichedGoal,sourceGenome);
        stageReport.totalSearchNodes=induced.totalSearchNodes;
        stageReport.reusedComponents=induced.reusedComponents;
        stageReport.reusedNames=induced.reusedComponentNames;
        for(const auto& t:stageSpec.goal.types)stageReport.inducedNames.push_back("type:"+t.name);
        for(const auto& f:stageSpec.goal.functions)stageReport.inducedNames.push_back("function:"+f.name);
        stageReport.newlyInducedTypes=stageSpec.goal.types.size();
        stageReport.newlyInducedFunctions=stageSpec.goal.functions.size();

        writeFile(stageReport.stageWorkspace/"induction.json",induced.report);
        writeFile(stageReport.stageWorkspace/"inferred.goal",serializeTypedArchitectureGoal(induced.goal));

        // Architect typed goal into multi-module project blueprint & graph
        auto architected=architectTypedGoal(induced.goal,sourceGenome);
        architected.project.contract.requireSandbox=config.requireSandbox;
        writeFile(stageReport.stageWorkspace/"architecture.json",architected.architectureReport);
        writeFile(stageReport.stageWorkspace/"project.blueprint",serializeProjectBlueprint(architected.blueprint));

        compartments.exchange({DevelopmentSignalKind::EvidenceVerified,3,2,0,contractHash,0.92,
            "induced operations graph hash="+std::to_string(architected.goalHash),false});

        // Verify or autonomously repair the generated multi-module C++20 project in the sandbox
        AutonomousProjectConfig repairCfg{
            stageReport.stageWorkspace/"development",
            config.compiler,
            config.sandboxRunner,
            config.maxCyclesPerStage,
            config.projectRepairMemory
        };
        const auto devResult=runCumulativeAutonomousProjectRepair(
            architected.project.graph,architected.project.contract,repairCfg);

        stageReport.verificationCycles=devResult.cycles.size();
        stageReport.verified=devResult.success;
        if(!devResult.cycles.empty()) {
            stageReport.verifiedGraphHash=devResult.cycles.back().evidence.graphHash;
            stageReport.evidenceReport=devResult.cycles.back().evidence.report;
        }

        // Build full causal provenance trace from genome strand -> regulatory state -> compartment signals -> induction -> sandbox evidence
        stageReport.causalChain.push_back("GENOME[hash="+std::to_string(initialGenomeHash)+",region=Cpp20UniversalSourceSpace]");
        stageReport.causalChain.push_back("ONTOGENY[stage="+stageName(stageReport.organismStage)+",active_regions="+std::to_string(regRes.activeRegions.size())+"]");
        stageReport.causalChain.push_back("COMPARTMENTS[analysis->research->synthesis,contract_hash="+std::to_string(contractHash)+"]");
        stageReport.causalChain.push_back("INDUCTION[reused="+std::to_string(stageReport.reusedComponents)+",new_types="+
            std::to_string(stageReport.newlyInducedTypes)+",new_functions="+std::to_string(stageReport.newlyInducedFunctions)+
            ",search_nodes="+std::to_string(stageReport.totalSearchNodes)+"]");
        stageReport.causalChain.push_back("VERIFICATION[cycles="+std::to_string(stageReport.verificationCycles)+
            ",graph_hash="+std::to_string(stageReport.verifiedGraphHash)+",status="+(stageReport.verified?"PASS":"BLOCKED")+"]");

        if(!stageReport.verified) {
            compartments.exchange({DevelopmentSignalKind::NegativeEvidence,3,1,0,contractHash,0.95,
                "stage verification failed: "+stageSpec.stageId,false});
            result.stageReports.push_back(std::move(stageReport));
            break;
        }

        compartments.exchange({DevelopmentSignalKind::SynthesisCompleted,2,1,0,contractHash,0.98,
            "stage verified in sandbox: "+stageSpec.stageId,false});

        // Emitted recovery signal updates organism molecular pool after successful verification
        organismPool.setConcentration(0,MoleculeSignatures::RecoverySignal,0.90,"StageVerifiedRecovery",MoleculeClass::Development);

        // Promote newly verified types and functions into accumulated library and persistent ComponentExperienceStore
        const auto evHash=projectEvidenceHash(stageReport.evidenceReport);
        std::vector<VerifiedComponentKnowledge> newKnowledge;
        for(const auto& gType:induced.goal.types) {
            if(std::none_of(accumulatedTypes.begin(),accumulatedTypes.end(),[&](const auto& t){return t.name==gType.name;})) {
                accumulatedTypes.push_back(gType);
                newKnowledge.push_back({
                    initialGenomeHash,architected.goalHash,stageReport.verifiedGraphHash,evHash,
                    "type",gType.name,serializeSingleGoalType(gType)
                });
            }
        }
        for(const auto& gFn:induced.goal.functions) {
            if(std::none_of(accumulatedFunctions.begin(),accumulatedFunctions.end(),[&](const auto& f){return f.name==gFn.name;})) {
                accumulatedFunctions.push_back(gFn);
                newKnowledge.push_back({
                    initialGenomeHash,architected.goalHash,stageReport.verifiedGraphHash,evHash,
                    "function",gFn.name,serializeSingleGoalFunction(gFn,accumulatedTypes)
                });
            }
        }
        if(!newKnowledge.empty()&&!config.componentMemory.empty()) {
            componentStore.appendGroup(newKnowledge);
        }

        // Enforce strict genome immutability invariant after every developmental stage
        if(sys.strand->getGenomeHash()!=initialGenomeHash||!sys.strand->isFrozen())
            throw std::runtime_error("Genome immutability invariant violated during developmental intelligence run");

        result.totalStagesCompleted++;
        result.totalReusedComponents+=stageReport.reusedComponents;
        result.totalNewlyInducedComponents+=stageReport.newlyInducedTypes+stageReport.newlyInducedFunctions;
        result.stageReports.push_back(std::move(stageReport));
    }

    result.finalStage=sys.phenotypeBuilder->getStage();
    result.compartments=compartments.snapshots();
    result.signals=compartments.signals();
    result.success=(result.totalStagesCompleted==curriculum.stages.size());

    // Capture deterministic SystemSnapshot of the developed organism
    result.finalSnapshot=GenomePersistence::captureSnapshot(*sys.strand,organismPool,*sys.phenotypeBuilder,tick);
    result.snapshotPath=config.workspace/"organism_snapshot.gdi";
    writeFile(result.snapshotPath,GenomePersistence::serialize(result.finalSnapshot));

    // Write comprehensive developmental intelligence JSON report
    result.reportPath=config.workspace/"developmental_intelligence.json";
    std::ostringstream rep;
    rep<<"{\n  \"format\": \"gdi-developmental-intelligence-v1\",\n"
       <<"  \"curriculum\": "<<jsonEscape(curriculum.curriculumName)<<",\n"
       <<"  \"immutable_genome_hash\": "<<result.immutableGenomeHash<<",\n"
       <<"  \"final_developmental_stage\": "<<jsonEscape(stageName(result.finalStage))<<",\n"
       <<"  \"snapshot_state_hash\": "<<result.finalSnapshot.stateHash<<",\n"
       <<"  \"success\": "<<(result.success?"true":"false")<<",\n"
       <<"  \"stages_completed\": "<<result.totalStagesCompleted<<",\n"
       <<"  \"total_stages\": "<<curriculum.stages.size()<<",\n"
       <<"  \"total_reused_components\": "<<result.totalReusedComponents<<",\n"
       <<"  \"total_newly_induced_components\": "<<result.totalNewlyInducedComponents<<",\n"
       <<"  \"compartments\": "<<result.compartments.size()<<",\n"
       <<"  \"signals_exchanged\": "<<result.signals.size()<<",\n"
       <<"  \"stages\": [\n";
    for(std::size_t i=0;i<result.stageReports.size();++i) {
        const auto& s=result.stageReports[i];
        rep<<"    {\n"
           <<"      \"stage_number\": "<<s.stageNumber<<",\n"
           <<"      \"stage_id\": "<<jsonEscape(s.stageId)<<",\n"
           <<"      \"organism_stage\": "<<jsonEscape(stageName(s.organismStage))<<",\n"
           <<"      \"contract_hash\": "<<s.contractHash<<",\n"
           <<"      \"verified_graph_hash\": "<<s.verifiedGraphHash<<",\n"
           <<"      \"verified\": "<<(s.verified?"true":"false")<<",\n"
           <<"      \"reused_components\": "<<s.reusedComponents<<",\n"
           <<"      \"newly_induced_types\": "<<s.newlyInducedTypes<<",\n"
           <<"      \"newly_induced_functions\": "<<s.newlyInducedFunctions<<",\n"
           <<"      \"search_nodes\": "<<s.totalSearchNodes<<",\n"
           <<"      \"verification_cycles\": "<<s.verificationCycles<<",\n"
           <<"      \"causal_chain\": [";
        for(std::size_t c=0;c<s.causalChain.size();++c) {
            if(c)rep<<", ";
            rep<<jsonEscape(s.causalChain[c]);
        }
        rep<<"]\n    }"<<(i+1<result.stageReports.size()?",\n":"\n");
    }
    rep<<"  ]\n}\n";
    writeFile(result.reportPath,rep.str());

    return result;
}

namespace {

std::string foldAndLower(const std::string& input) {
    std::string s=input;
    const auto repAll=[&](std::string_view from,std::string_view to) {
        std::size_t pos=0;
        while((pos=s.find(from,pos))!=std::string::npos) {
            s.replace(pos,from.size(),to);
            pos+=to.size();
        }
    };
    repAll("Ä","ae"); repAll("Ö","oe"); repAll("Ü","ue");
    repAll("ä","ae"); repAll("ö","oe"); repAll("ü","ue");
    repAll("ß","ss");
    for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool containsAnyTerm(const std::string& text,const std::vector<std::string_view>& terms) {
    return std::any_of(terms.begin(),terms.end(),[&](std::string_view term) {
        return text.find(term)!=std::string::npos;
    });
}

void appendBehavioralTypeBlock(std::ostringstream& out,const BehavioralTypeGoal& t) {
    out<<"type\t"<<t.name;
    if(!t.moduleName.empty())out<<'\t'<<t.moduleName;
    out<<'\n';
    for(const auto& f:t.fields)out<<"field\t"<<t.name<<'\t'<<f.type<<'\t'<<f.name<<'\n';
    for(const auto& m:t.methods) {
        out<<"method\t"<<t.name<<'\t'<<m.name<<'\t'<<m.returnType<<'\n';
        for(const auto& p:m.parameters)out<<"method-parameter\t"<<t.name<<'\t'<<m.name<<'\t'<<p.type<<'\t'<<p.name<<'\n';
        for(const auto& s:m.samples) {
            out<<"method-sample\t"<<t.name<<'\t'<<m.name;
            for(const auto& a:s.rawArguments)out<<'\t'<<a;
            out<<'\t'<<s.rawExpected<<'\n';
        }
        for(const auto& c:m.checks) {
            out<<"method-check\t"<<t.name<<'\t'<<m.name;
            for(const auto& a:c.rawArguments)out<<'\t'<<a;
            out<<'\t'<<c.rawExpected<<'\n';
        }
    }
}

void appendBehavioralFunctionBlock(std::ostringstream& out,const BehavioralFunctionGoal& fn) {
    out<<"function\t"<<fn.name<<'\t'<<fn.returnType;
    if(!fn.moduleName.empty())out<<'\t'<<fn.moduleName;
    out<<'\n';
    for(const auto& p:fn.parameters)out<<"parameter\t"<<fn.name<<'\t'<<p.type<<'\t'<<p.name<<'\n';
    out<<"max-depth\t"<<fn.name<<'\t'<<fn.maxDepth<<'\n';
    for(const auto& s:fn.samples) {
        out<<"sample\t"<<fn.name;
        for(const auto& a:s.rawArguments)out<<'\t'<<a;
        out<<'\t'<<s.rawExpected<<'\n';
    }
    for(const auto& c:fn.checks) {
        out<<"check\t"<<fn.name;
        for(const auto& a:c.rawArguments)out<<'\t'<<a;
        out<<'\t'<<c.rawExpected<<'\n';
    }
}

std::string serializeDevelopmentalCurriculum(const DevelopmentalCurriculum& curriculum) {
    std::ostringstream out;
    out<<"GDI_DEVELOPMENTAL_CURRICULUM_V1\n"
       <<"curriculum\t"<<curriculum.curriculumName<<'\n'
       <<"namespace\t"<<curriculum.namespaceName<<"\n\n";
    for(const auto& st:curriculum.stages) {
        out<<"stage\t"<<st.stageId<<'\t'<<stageName(st.requiredStage)<<'\n'
           <<"project\t"<<st.goal.projectName<<'\n'
           <<"namespace\t"<<st.goal.namespaceName<<'\n';
        for(const auto& m:st.goal.modules) {
            out<<"module\t"<<m.name;
            for(const auto& d:m.dependencies)out<<'\t'<<d;
            out<<'\n';
        }
        for(const auto& t:st.goal.types)appendBehavioralTypeBlock(out,t);
        for(const auto& fn:st.goal.functions)appendBehavioralFunctionBlock(out,fn);
        out<<"entry-function\t"<<st.goal.entryFunction<<'\n'
           <<"end-stage\n\n";
    }
    return out.str();
}

BehavioralFunctionGoal makeSquareSpec() {
    BehavioralFunctionGoal fn;
    fn.name="square";
    fn.returnType="long long";
    fn.moduleName="math";
    fn.parameters={{"long long","v"}};
    fn.maxDepth=2;
    fn.samples={
        {{0},0,{"0"},"0"},
        {{2},4,{"2"},"4"},
        {{-3},9,{"-3"},"9"},
        {{5},25,{"5"},"25"}
    };
    fn.checks={
        {{7},49,{"7"},"49"},
        {{-8},64,{"-8"},"64"}
    };
    return fn;
}

BehavioralTypeGoal makePoint2DSpec(bool includeManhattanMethod) {
    BehavioralTypeGoal t;
    t.name="Point2D";
    t.moduleName="geometry";
    t.fields={{"long long","x"},{"long long","y"}};
    if(includeManhattanMethod) {
        BehavioralMethodGoal m;
        m.name="manhattan";
        m.returnType="long long";
        m.maxDepth=3;
        m.samples={
            {{0,0},0,{"0","0"},"0"},
            {{3,4},7,{"3","4"},"7"},
            {{2,5},7,{"2","5"},"7"},
            {{6,8},14,{"6","8"},"14"}
        };
        m.checks={
            {{5,12},17,{"5","12"},"17"},
            {{10,3},13,{"10","3"},"13"}
        };
        t.methods.push_back(std::move(m));
    }
    return t;
}

BehavioralFunctionGoal makeNormSqSpec() {
    BehavioralFunctionGoal fn;
    fn.name="norm_sq";
    fn.returnType="long long";
    fn.moduleName="geometry";
    fn.parameters={{"Point2D","p"}};
    fn.maxDepth=2;
    fn.samples={
        {{0},0,{"Point2D{0,0}"},"0"},
        {{0},25,{"Point2D{3,4}"},"25"},
        {{0},29,{"Point2D{-2,5}"},"29"},
        {{0},100,{"Point2D{6,8}"},"100"}
    };
    fn.checks={
        {{0},169,{"Point2D{5,12}"},"169"},
        {{0},25,{"Point2D{-4,-3}"},"25"}
    };
    return fn;
}

BehavioralFunctionGoal makePointEnergySpec() {
    BehavioralFunctionGoal fn;
    fn.name="point_energy";
    fn.returnType="long long";
    fn.moduleName="analytics";
    fn.parameters={{"Point2D","p"}};
    fn.maxDepth=2;
    fn.samples={
        {{0},0,{"Point2D{0,0}"},"0"},
        {{0},32,{"Point2D{3,4}"},"32"},
        {{0},36,{"Point2D{2,5}"},"36"},
        {{0},114,{"Point2D{6,8}"},"114"}
    };
    fn.checks={
        {{0},186,{"Point2D{5,12}"},"186"},
        {{0},8,{"Point2D{1,2}"},"8"}
    };
    return fn;
}

BehavioralFunctionGoal makeStoreGeometryRecordSpec(bool hasPointEnergy) {
    BehavioralFunctionGoal fn;
    fn.name="store_geometry_record";
    fn.returnType="long long";
    fn.moduleName="storage";
    fn.parameters={{"Point2D","p"}};
    fn.maxDepth=2;
    if(hasPointEnergy) {
        // Checksum/storage record over point_energy(p) + norm_sq(p)
        fn.samples={
            {{0},0,{"Point2D{0,0}"},"0"},
            {{0},57,{"Point2D{3,4}"},"57"},
            {{0},65,{"Point2D{2,5}"},"65"},
            {{0},214,{"Point2D{6,8}"},"214"}
        };
        fn.checks={
            {{0},355,{"Point2D{5,12}"},"355"},
            {{0},13,{"Point2D{1,2}"},"13"}
        };
    } else {
        fn.samples={
            {{0},0,{"Point2D{0,0}"},"0"},
            {{0},25,{"Point2D{3,4}"},"25"},
            {{0},29,{"Point2D{-2,5}"},"29"},
            {{0},100,{"Point2D{6,8}"},"100"}
        };
        fn.checks={
            {{0},169,{"Point2D{5,12}"},"169"},
            {{0},25,{"Point2D{-4,-3}"},"25"}
        };
    }
    return fn;
}

} // namespace

AutonomousOntogeneticPlan planAutonomousOntogenyFromObjective(
    const std::string& highLevelObjective,
    const std::filesystem::path& componentMemoryPath) {

    AutonomousOntogeneticPlan plan;
    plan.audit.highLevelObjective=highLevelObjective;
    plan.audit.objectiveSemanticHash=fingerprint(highLevelObjective);

    if(highLevelObjective.empty()||highLevelObjective.size()>8192) {
        plan.audit.diagnostics.push_back("Overarching objective must contain 1..8192 bytes");
        return plan;
    }

    auto sys=GenomeBootstrap::buildCompleteSystem();
    const auto genomeHash=sys.strand->getGenomeHash();
    const auto sourceGenome=Cpp20SourceGenome::fromGenome(*sys.strand);

    const auto text=foldAndLower(highLevelObjective);

    // Reject contradictory or unencoded external capabilities (fail closed)
    if(containsAnyTerm(text,{"netzwerk","http","socket","SQL","datenbank-server","3d-mesh","opengl","vulkan"})) {
        plan.audit.diagnostics.push_back("Objective requests external I/O or 3D graphics capabilities not encoded in the genome");
        return plan;
    }

    const bool has2DGeometry=containsAnyTerm(text,{
        "zweidimensional","2d","geometrisch","geometrie","geometric","geometry","punkt","point","objekt","object"
    });
    const bool hasProperties=containsAnyTerm(text,{
        "eigenschaft","berechn","calculat","comput","property","properties","norm","abstand","energie","distanz","metrik"
    });
    const bool hasStorage=containsAnyTerm(text,{
        "speicher","store","persist","archiv","ergebnis","protokoll","record","verwalt","manage"
    });

    if(!has2DGeometry||(!hasProperties&&!hasStorage)) {
        plan.audit.diagnostics.push_back(
            "Overarching objective is outside the genomically verifiable domain (expected 2D geometric objects, property calculation, and/or result storage)");
        return plan;
    }

    if(!sourceGenome.structureEnabled("user-defined-types")||
       !sourceGenome.structureEnabled("multi-module-components")||
       !sourceGenome.structureEnabled("independent-behavior-tests")) {
        plan.audit.diagnostics.push_back("Immutable genome does not authorize required multi-module or user-defined type structures");
        return plan;
    }

    plan.audit.activatedCapabilities.push_back("domain:2d_geometric_objects(struct Point2D, module geometry)");
    plan.audit.activatedCapabilities.push_back("foundation:scalar_quadratic_primitive(square, module math)");
    if(hasProperties) {
        plan.audit.activatedCapabilities.push_back("computation:geometric_metrics(Point2D::manhattan, norm_sq, point_energy)");
    }
    if(hasStorage) {
        plan.audit.activatedCapabilities.push_back("storage:deterministic_result_record(store_geometry_record, module storage)");
    }

    // Build dependency graph of required components for the overarching goal
    plan.audit.requiredComponents.push_back({
        "function:square","function","math",{},NeuralDevelopmentStage::Neonatal
    });
    plan.audit.requiredComponents.push_back({
        "type:Point2D","type","geometry",{},NeuralDevelopmentStage::Juvenile
    });
    plan.audit.requiredComponents.push_back({
        "method:Point2D::manhattan","method","geometry",{"type:Point2D"},NeuralDevelopmentStage::Juvenile
    });
    plan.audit.requiredComponents.push_back({
        "function:norm_sq","function","geometry",{"function:square","type:Point2D"},NeuralDevelopmentStage::Juvenile
    });
    if(hasProperties) {
        plan.audit.requiredComponents.push_back({
            "function:point_energy","function","analytics",
            {"function:square","type:Point2D","method:Point2D::manhattan","function:norm_sq"},
            NeuralDevelopmentStage::Mature
        });
    }
    if(hasStorage) {
        std::vector<std::string> deps={"function:square","type:Point2D","function:norm_sq"};
        if(hasProperties)deps.push_back("function:point_energy");
        plan.audit.requiredComponents.push_back({
            "function:store_geometry_record","function","storage",std::move(deps),NeuralDevelopmentStage::Mature
        });
    }

    // Inspect persistent ComponentExperienceStore to see which required components GDI already knows!
    std::unordered_set<std::string> knownIds;
    if(!componentMemoryPath.empty()&&std::filesystem::exists(componentMemoryPath)&&
       std::filesystem::file_size(componentMemoryPath)>0) {
        ComponentExperienceStore store(componentMemoryPath);
        for(const auto& item:store.load()) {
            if(item.genomeHash!=genomeHash)continue;
            if(item.kind=="type") {
                knownIds.insert("type:"+item.name);
                try {
                    const auto parsed=parseTypedArchitectureGoalText(item.serializedGoalFragment);
                    for(const auto& t:parsed.types) {
                        if(t.name==item.name) {
                            for(const auto& m:t.methods)knownIds.insert("method:"+t.name+"::"+m.name);
                        }
                    }
                } catch(...) {}
            } else if(item.kind=="function") {
                knownIds.insert("function:"+item.name);
            }
        }
    }

    for(const auto& req:plan.audit.requiredComponents) {
        if(knownIds.contains(req.id))plan.audit.satisfiedFromMemory.push_back(req.id);
        else plan.audit.knowledgeGaps.push_back(req.id);
    }

    plan.derivedCurriculum.curriculumName="self_planned_geometry_ontogeny";
    plan.derivedCurriculum.namespaceName="generated";

    const auto isMissing=[&](const std::string& id) {
        return !knownIds.contains(id);
    };

    // Stage 1 (Neonatal): Only scheduled if foundational scalar primitive `square` is a knowledge gap!
    if(isMissing("function:square")) {
        DevelopmentalTaskStage s1;
        s1.stageId="planned_neonatal_math_foundation";
        s1.requiredStage=NeuralDevelopmentStage::Neonatal;
        s1.goal.projectName="planned_stage_math";
        s1.goal.namespaceName="generated";
        s1.goal.modules.push_back({"math",{}});
        s1.goal.functions.push_back(makeSquareSpec());
        s1.goal.entryFunction="square";
        plan.derivedCurriculum.stages.push_back(std::move(s1));
    }

    // Stage 2 (Juvenile): Only scheduled if `Point2D`, `Point2D::manhattan`, or `norm_sq` is a knowledge gap!
    if(isMissing("type:Point2D")||isMissing("method:Point2D::manhattan")||isMissing("function:norm_sq")) {
        DevelopmentalTaskStage s2;
        s2.stageId="planned_juvenile_geometry_objects";
        s2.requiredStage=NeuralDevelopmentStage::Juvenile;
        s2.goal.projectName="planned_stage_geometry";
        s2.goal.namespaceName="generated";
        s2.goal.modules.push_back({"math",{}});
        s2.goal.modules.push_back({"geometry",{"math"}});
        if(isMissing("type:Point2D")||isMissing("method:Point2D::manhattan")) {
            s2.goal.types.push_back(makePoint2DSpec(isMissing("method:Point2D::manhattan")));
        }
        if(isMissing("function:norm_sq")) {
            s2.goal.functions.push_back(makeNormSqSpec());
            s2.goal.entryFunction="norm_sq";
        }
        if(!s2.goal.functions.empty()||!s2.goal.types.empty()) {
            plan.derivedCurriculum.stages.push_back(std::move(s2));
        }
    }

    // Stage 3 (Mature): Synthesize missing high-level property analytics and/or storage verification
    {
        DevelopmentalTaskStage s3;
        s3.stageId="planned_mature_geometry_application";
        s3.requiredStage=NeuralDevelopmentStage::Mature;
        s3.goal.projectName="planned_stage_application";
        s3.goal.namespaceName="generated";
        s3.goal.modules.push_back({"math",{}});
        s3.goal.modules.push_back({"geometry",{"math"}});
        s3.goal.modules.push_back({"analytics",{"geometry","math"}});
        if(hasStorage) {
            s3.goal.modules.push_back({"storage",{"analytics","geometry","math"}});
        }

        if(hasProperties&&isMissing("function:point_energy")) {
            s3.goal.functions.push_back(makePointEnergySpec());
        }
        if(hasStorage&&isMissing("function:store_geometry_record")) {
            s3.goal.functions.push_back(makeStoreGeometryRecordSpec(hasProperties));
        }
        // If every component was already in memory, still emit a final verification stage for the top-level target
        if(s3.goal.functions.empty()) {
            if(hasStorage)s3.goal.functions.push_back(makeStoreGeometryRecordSpec(hasProperties));
            else s3.goal.functions.push_back(makePointEnergySpec());
        }
        s3.goal.entryFunction=s3.goal.functions.back().name;
        plan.derivedCurriculum.stages.push_back(std::move(s3));
    }

    plan.audit.objectiveSupported=true;
    plan.serializedCurriculum=serializeDevelopmentalCurriculum(plan.derivedCurriculum);

    std::ostringstream rat;
    rat<<"AUTONOMER ONTOGENETISCHER ENTWICKLUNGSPLANER (GDI)\n"
       <<"Uebergeordnetes Ziel: "<<highLevelObjective<<"\n"
       <<"Ziel-Hash: "<<plan.audit.objectiveSemanticHash<<"\n"
       <<"Benoetigte Fähigkeiten ("<<plan.audit.requiredComponents.size()<<"): ";
    for(std::size_t i=0;i<plan.audit.requiredComponents.size();++i)
        rat<<(i?", ":"")<<plan.audit.requiredComponents[i].id;
    rat<<"\nVorhandene Komponenten im Gedächtnis ("<<plan.audit.satisfiedFromMemory.size()<<"): ";
    if(plan.audit.satisfiedFromMemory.empty())rat<<"keine (naiver Zustand)";
    else for(std::size_t i=0;i<plan.audit.satisfiedFromMemory.size();++i)
        rat<<(i?", ":"")<<plan.audit.satisfiedFromMemory[i];
    rat<<"\nIdentifizierte Wissensluecken ("<<plan.audit.knowledgeGaps.size()<<"): ";
    if(plan.audit.knowledgeGaps.empty())rat<<"keine (nur Zielverifikation erforderlich)";
    else for(std::size_t i=0;i<plan.audit.knowledgeGaps.size();++i)
        rat<<(i?", ":"")<<plan.audit.knowledgeGaps[i];
    rat<<"\nAbgeleitetes Curriculum: "<<plan.derivedCurriculum.stages.size()<<" Entwicklungsstufe(n):\n";
    for(std::size_t i=0;i<plan.derivedCurriculum.stages.size();++i) {
        const auto& st=plan.derivedCurriculum.stages[i];
        rat<<"  "<<(i+1)<<". ["<<stageName(st.requiredStage)<<"] "<<st.stageId
           <<" -> "<<st.goal.types.size()<<" Typ(en), "<<st.goal.functions.size()
           <<" Funktion(en), Einstieg: "<<st.goal.entryFunction<<"\n";
    }
    plan.plannerRationale=rat.str();
    return plan;
}

DevelopmentalIntelligenceResult runAutonomousGoalDirectedOntogeny(
    const std::string& highLevelObjective,
    const DevelopmentalIntelligenceConfig& config) {

    const auto plan=planAutonomousOntogenyFromObjective(highLevelObjective,config.componentMemory);
    if(!plan.audit.objectiveSupported) {
        std::string msg="Overarching goal rejected by ontogenetic planner";
        if(!plan.audit.diagnostics.empty())msg+=": "+plan.audit.diagnostics.front();
        throw std::invalid_argument(msg);
    }
    auto res=runDevelopmentalProgrammingIntelligence(plan.derivedCurriculum,config);
    writeFile(config.workspace/"derived_ontogeny.curriculum",plan.serializedCurriculum);
    writeFile(config.workspace/"ontogenetic_plan_rationale.txt",plan.plannerRationale);
    return res;
}

OntogeneticControlComparison runOntogeneticControlExperiment(
    const std::string& highLevelObjective,
    const DevelopmentalIntelligenceConfig& baseConfig,
    const std::filesystem::path& priorComponentMemory) {

    if(baseConfig.workspace.empty()||(std::filesystem::exists(baseConfig.workspace)&&!std::filesystem::is_empty(baseConfig.workspace)))
        throw std::invalid_argument("Control experiment workspace must be new or empty");
    std::filesystem::create_directories(baseConfig.workspace);

    OntogeneticControlComparison cmp;
    cmp.highLevelObjective=highLevelObjective;

    // 1. Naive run (empty component memory: organism must discover all intermediate stages itself)
    const auto naiveMem=baseConfig.workspace/"naive_components.memory";
    cmp.naivePlan=planAutonomousOntogenyFromObjective(highLevelObjective,naiveMem);
    if(!cmp.naivePlan.audit.objectiveSupported)
        throw std::invalid_argument("Control experiment objective rejected by planner");

    DevelopmentalIntelligenceConfig naiveCfg=baseConfig;
    naiveCfg.workspace=baseConfig.workspace/"naive_without_memory";
    naiveCfg.componentMemory=naiveMem;
    naiveCfg.projectRepairMemory=baseConfig.workspace/"naive_repair.memory";
    cmp.naiveResult=runDevelopmentalProgrammingIntelligence(cmp.naivePlan.derivedCurriculum,naiveCfg);
    writeFile(naiveCfg.workspace/"derived_ontogeny.curriculum",cmp.naivePlan.serializedCurriculum);
    writeFile(naiveCfg.workspace/"ontogenetic_plan_rationale.txt",cmp.naivePlan.plannerRationale);

    // 2. Experienced run (seeded with priorComponentMemory, or if empty, seeded with the memory acquired in a prior run)
    const auto expMem=baseConfig.workspace/"experienced_components.memory";
    if(!priorComponentMemory.empty()&&std::filesystem::exists(priorComponentMemory)) {
        std::filesystem::copy_file(priorComponentMemory,expMem,std::filesystem::copy_options::overwrite_existing);
    } else if(std::filesystem::exists(naiveMem)) {
        std::filesystem::copy_file(naiveMem,expMem,std::filesystem::copy_options::overwrite_existing);
    }

    cmp.experiencedPlan=planAutonomousOntogenyFromObjective(highLevelObjective,expMem);
    DevelopmentalIntelligenceConfig expCfg=baseConfig;
    expCfg.workspace=baseConfig.workspace/"experienced_with_memory";
    expCfg.componentMemory=expMem;
    expCfg.projectRepairMemory=baseConfig.workspace/"experienced_repair.memory";
    cmp.experiencedResult=runDevelopmentalProgrammingIntelligence(cmp.experiencedPlan.derivedCurriculum,expCfg);
    writeFile(expCfg.workspace/"derived_ontogeny.curriculum",cmp.experiencedPlan.serializedCurriculum);
    writeFile(expCfg.workspace/"ontogenetic_plan_rationale.txt",cmp.experiencedPlan.plannerRationale);

    cmp.memoryAlteredDevelopmentalPath=
        (cmp.experiencedPlan.derivedCurriculum.stages.size()<cmp.naivePlan.derivedCurriculum.stages.size())||
        (cmp.experiencedPlan.audit.knowledgeGaps.size()<cmp.naivePlan.audit.knowledgeGaps.size());
    cmp.stagesSavedByExperience=
        cmp.naivePlan.derivedCurriculum.stages.size()>cmp.experiencedPlan.derivedCurriculum.stages.size()
        ?(cmp.naivePlan.derivedCurriculum.stages.size()-cmp.experiencedPlan.derivedCurriculum.stages.size()):0;
    cmp.componentsReusedFromExperience=cmp.experiencedPlan.audit.satisfiedFromMemory.size();

    cmp.comparisonReportPath=baseConfig.workspace/"ontogenetic_control_experiment.json";
    std::ostringstream rep;
    rep<<"{\n  \"format\": \"gdi-ontogenetic-control-experiment-v1\",\n"
       <<"  \"objective\": "<<jsonEscape(highLevelObjective)<<",\n"
       <<"  \"immutable_genome_hash\": "<<cmp.naiveResult.immutableGenomeHash<<",\n"
       <<"  \"memory_altered_developmental_path\": "<<(cmp.memoryAlteredDevelopmentalPath?"true":"false")<<",\n"
       <<"  \"stages_saved_by_experience\": "<<cmp.stagesSavedByExperience<<",\n"
       <<"  \"components_satisfied_from_memory\": "<<cmp.componentsReusedFromExperience<<",\n"
       <<"  \"naive_run\": {\n"
       <<"    \"knowledge_gaps\": "<<cmp.naivePlan.audit.knowledgeGaps.size()<<",\n"
       <<"    \"derived_stages\": "<<cmp.naivePlan.derivedCurriculum.stages.size()<<",\n"
       <<"    \"newly_induced_components\": "<<cmp.naiveResult.totalNewlyInducedComponents<<",\n"
       <<"    \"reused_components\": "<<cmp.naiveResult.totalReusedComponents<<",\n"
       <<"    \"success\": "<<(cmp.naiveResult.success?"true":"false")<<"\n"
       <<"  },\n"
       <<"  \"experienced_run\": {\n"
       <<"    \"satisfied_from_memory\": "<<cmp.experiencedPlan.audit.satisfiedFromMemory.size()<<",\n"
       <<"    \"knowledge_gaps\": "<<cmp.experiencedPlan.audit.knowledgeGaps.size()<<",\n"
       <<"    \"derived_stages\": "<<cmp.experiencedPlan.derivedCurriculum.stages.size()<<",\n"
       <<"    \"newly_induced_components\": "<<cmp.experiencedResult.totalNewlyInducedComponents<<",\n"
       <<"    \"reused_components\": "<<cmp.experiencedResult.totalReusedComponents<<",\n"
       <<"    \"success\": "<<(cmp.experiencedResult.success?"true":"false")<<"\n"
       <<"  }\n}\n";
    writeFile(cmp.comparisonReportPath,rep.str());
    return cmp;
}

} // namespace genomic::gpi
