#include "genome/genome_bootstrap.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/project_blueprint.hpp"
#include "gpi/generative_project.hpp"
#include "gpi/goal_architect.hpp"
#include "gpi/project_manifest.hpp"
#include "gpi/project_verifier.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    using namespace genomic;
    using namespace genomic::gpi;
    using namespace std::chrono_literals;
    if(argc!=3&&argc!=4){std::cerr<<"usage: test_generative_project COMPILER OUTPUT [SANDBOX]\n";return 2;}
    try {
        const std::filesystem::path compiler=argv[1],root=argv[2];
        std::error_code error;std::filesystem::remove_all(root,error);
        const auto system=GenomeBootstrap::buildCompleteSystem();
        const auto manifestRoot=std::filesystem::path(root.string()+"_manifest_input");
        std::filesystem::remove_all(manifestRoot,error);std::filesystem::create_directories(manifestRoot/"src");
        std::filesystem::create_directories(manifestRoot/"fixtures");
        {std::ofstream file(manifestRoot/"src/main.cpp",std::ios::binary);file<<"#include <iostream>\nint main(){std::cout<<42<<'\\n';}\n";}
        {std::ofstream file(manifestRoot/"fixtures/out.txt",std::ios::binary);file<<"42\n";}
        const auto manifestPath=manifestRoot/"project.gpi";
        {std::ofstream file(manifestPath,std::ios::binary);file<<
            "GPI_PROJECT_CONTRACT_V1\n"
            "node\tapplication\tsource\tsrc/main.cpp\n"
            "target\tapp\texecutable\tapplication\n"
            "build\tapp\n"
            "behavior\toutput\tapp\t-\tfixtures/out.txt\t-\t0\n"
            "timeout-seconds\t5\n"
            "require-sandbox\ttrue\n";}
        const auto loaded=loadProjectManifest(manifestPath,manifestRoot,Cpp20SourceGenome::fromGenome(*system.strand));
        if(loaded.graph.nodes().size()!=1||loaded.graph.targets().size()!=1||loaded.contract.behaviors.size()!=1||
           loaded.contract.behaviors.front().expectedOutput!="42\n"||!loaded.contract.requireSandbox)return 17;
        const auto escapeManifest=manifestRoot/"escape.gpi";
        {std::ofstream file(escapeManifest,std::ios::binary);file<<
            "GPI_PROJECT_CONTRACT_V1\nnode\tbad\tsource\t../outside.cpp\n"
            "target\tbad\texecutable\tbad\n";}
        bool escapeRejected=false;try {static_cast<void>(loadProjectManifest(escapeManifest,manifestRoot,
            Cpp20SourceGenome::fromGenome(*system.strand)));}catch(const std::invalid_argument&){escapeRejected=true;}
        if(!escapeRejected)return 18;
        const auto blueprintInput=manifestRoot/"blueprint";std::filesystem::create_directories(blueprintInput/"fixtures");
        {std::ofstream file(blueprintInput/"fixtures/expected.txt",std::ios::binary);file<<"55\n";}
        const auto blueprintPath=blueprintInput/"project.blueprint";
        {std::ofstream file(blueprintPath,std::ios::binary);file<<
            "GPI_PROJECT_BLUEPRINT_V1\n"
            "project\tfibonacci_blueprint\n"
            "namespace\tgenerated\n"
            "function\tfibonacci\tlong long\tn < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2)\n"
            "parameter\tfibonacci\tint\tn\n"
            "entry\tlong long\tfibonacci(10)\n"
            "test\tbase_zero\tfibonacci(0) == 0\n"
            "test\tbase_one\tfibonacci(1) == 1\n"
            "test\trecursive_ten\tfibonacci(10) == 55\n"
            "expected-output\tfixtures/expected.txt\nexpected-error\t-\ninput\t-\nexpected-exit\t0\n"
            "timeout-seconds\t20\nrequire-sandbox\t"<<(argc==4?"true":"false")<<"\n";}
        auto synthesized=synthesizeProjectBlueprint(loadProjectBlueprint(blueprintPath),
            Cpp20SourceGenome::fromGenome(*system.strand));
        if(synthesized.graph.nodes().size()!=5||synthesized.graph.targets().size()!=3||
           synthesized.contract.behaviors.size()!=2)return 23;
        const auto blueprintOutput=std::filesystem::path(root.string()+"_blueprint");
        std::filesystem::remove_all(blueprintOutput,error);
        const auto blueprintEvidence=verifyGenerativeProject(synthesized.graph,synthesized.contract,
            blueprintOutput,compiler,argc==4?std::filesystem::path(argv[3]):std::filesystem::path{});
        if(!blueprintEvidence.success||blueprintEvidence.checks.size()!=5)return 24;
        const auto goalPath=blueprintInput/"factorial.goal";
        {std::ofstream file(goalPath,std::ios::binary);file<<
            "GPI_TYPED_ARCHITECTURE_GOAL_V1\nproject\ttyped_factorial\nnamespace\tgenerated\n"
            "function\tfactorial\tlong long\nparameter\tfactorial\tint\tn\n"
            "operation\tfactorial\tbase\tless-equal\tn\t#1\n"
            "operation\tfactorial\tprevious\tsubtract\tn\t#1\n"
            "operation\tfactorial\trecursive\tcall:factorial\tprevious\n"
            "operation\tfactorial\tproduct\tmultiply\tn\trecursive\n"
            "operation\tfactorial\tresult\tselect\tbase\t#1\tproduct\n"
            "return\tfactorial\tresult\nentry\tfactorial\t#720\t#6\n"
            "case\tzero\tfactorial\t#1\t#0\ncase\tone\tfactorial\t#1\t#1\n"
            "case\tfive\tfactorial\t#120\t#5\ntimeout-seconds\t20\nrequire-sandbox\t"
            <<(argc==4?"true":"false")<<"\n";}
        auto architected=architectTypedGoal(loadTypedArchitectureGoal(goalPath),
            Cpp20SourceGenome::fromGenome(*system.strand));
        if(architected.blueprint.functions.size()!=1||architected.blueprint.tests.size()!=3||
           architected.blueprint.entryExpression!="factorial(6)"||architected.blueprint.expectedOutput!="720\n"||
           architected.goalHash==0||architected.architectureReport.find("\"operations\": 5")==std::string::npos)return 26;
        const auto architectureOutput=std::filesystem::path(root.string()+"_architecture");
        std::filesystem::remove_all(architectureOutput,error);
        const auto architectureEvidence=verifyGenerativeProject(architected.project.graph,architected.project.contract,
            architectureOutput,compiler,argc==4?std::filesystem::path(argv[3]):std::filesystem::path{});
        if(!architectureEvidence.success||architectureEvidence.checks.size()!=5)return 27;
        const auto multiGoalPath=blueprintInput/"multi.goal";
        {std::ofstream file(multiGoalPath,std::ios::binary);file<<
            "GPI_TYPED_ARCHITECTURE_GOAL_V1\nproject\ttyped_components\nnamespace\tgenerated\n"
            "function\tsquare\tlong long\nparameter\tsquare\tlong long\tx\n"
            "operation\tsquare\tproduct\tmultiply\tx\tx\nreturn\tsquare\tproduct\n"
            "function\tsum_squares\tlong long\nparameter\tsum_squares\tlong long\ta\n"
            "parameter\tsum_squares\tlong long\tb\noperation\tsum_squares\tleft\tcall:square\ta\n"
            "operation\tsum_squares\tright\tcall:square\tb\noperation\tsum_squares\ttotal\tadd\tleft\tright\n"
            "return\tsum_squares\ttotal\nentry\tsum_squares\t#25\t#3\t#4\n"
            "case\tsquare_three\tsquare\t#9\t#3\ncase\tpythagorean\tsum_squares\t#25\t#3\t#4\n"
            "timeout-seconds\t20\nrequire-sandbox\t"<<(argc==4?"true":"false")<<"\n";}
        auto multi=architectTypedGoal(loadTypedArchitectureGoal(multiGoalPath),Cpp20SourceGenome::fromGenome(*system.strand));
        if(multi.blueprint.functions.size()!=2||multi.blueprint.tests.size()!=2||
           multi.architectureReport.find("\"dependencies\": [\"square\"]")==std::string::npos)return 29;
        const auto multiOutput=std::filesystem::path(root.string()+"_architecture_multi");
        std::filesystem::remove_all(multiOutput,error);
        const auto multiEvidence=verifyGenerativeProject(multi.project.graph,multi.project.contract,multiOutput,
            compiler,argc==4?std::filesystem::path(argv[3]):std::filesystem::path{});
        if(!multiEvidence.success||multiEvidence.checks.size()!=5)return 30;
        auto cyclicGoal=loadTypedArchitectureGoal(goalPath);
        cyclicGoal.functions.front().operations.front().operands.front()="result";
        bool cycleRejected=false;try {static_cast<void>(architectTypedGoal(cyclicGoal,Cpp20SourceGenome::fromGenome(*system.strand)));}
        catch(const std::invalid_argument&){cycleRejected=true;}
        if(!cycleRejected)return 28;
        auto contradictoryGoal=loadTypedArchitectureGoal(goalPath);
        contradictoryGoal.cases.push_back({"wrong_six","factorial","#721",{"#6"}});
        bool contradictionRejected=false;
        try {static_cast<void>(architectTypedGoal(contradictoryGoal,Cpp20SourceGenome::fromGenome(*system.strand)));}
        catch(const std::invalid_argument&){contradictionRejected=true;}
        if(!contradictionRejected)return 31;
        const auto unsafeBlueprint=blueprintInput/"unsafe.blueprint";
        {std::ofstream file(unsafeBlueprint,std::ios::binary);file<<
            "GPI_PROJECT_BLUEPRINT_V1\nproject\tunsafe\nnamespace\tgenerated\n"
            "function\tf\tint\t0; dangerous()\nentry\tint\tf()\ntest\tcheck\tf()==0\n";}
        bool statementRejected=false;try {static_cast<void>(loadProjectBlueprint(unsafeBlueprint));}
        catch(const std::invalid_argument&){statementRejected=true;}
        if(!statementRejected)return 25;
        const auto makeGraph=[&](const std::string& argument,bool includeIostream=true,
                                 bool includeApplicationLink=true,const std::string& testExpected="18") {
            GenerativeProjectGraph graph(Cpp20SourceGenome::fromGenome(*system.strand));
            graph.addNode("api",ProjectNodeKind::Header,"include/generated/api.hpp",
                "#pragma once\n#include <concepts>\nnamespace generated { template<class T> concept Number=std::integral<T>; template<Number T>T twice(T); }\n");
            graph.addNode("implementation",ProjectNodeKind::Source,"src/api.cpp",
                "#include \"generated/api.hpp\"\nnamespace generated { template<Number T>T twice(T x){return x+x;} template int twice<int>(int); }\n");
            graph.addNode("application",ProjectNodeKind::Source,"src/main.cpp",
                "#include \"generated/api.hpp\"\n"+std::string(includeIostream?"#include <iostream>\n":"")+
                "int main(){std::cout<<generated::twice("+argument+")<<'\\n';}\n");
            graph.addNode("test",ProjectNodeKind::Test,"tests/api_test.cpp",
                "#include \"generated/api.hpp\"\nint main(){return generated::twice(9)=="+testExpected+"?0:1;}\n");
            graph.addNode("config",ProjectNodeKind::Resource,"resources/profile.json","{\"mode\":\"verified\"}\n");
            graph.addNode("build",ProjectNodeKind::BuildManifest,"CMakeLists.txt",
                "cmake_minimum_required(VERSION 3.20)\nproject(generated LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\n");
            graph.addDependency("implementation","api",ProjectDependencyKind::Includes);
            graph.addDependency("application","api",ProjectDependencyKind::Includes);
            if(includeApplicationLink)graph.addDependency("application","implementation",ProjectDependencyKind::Links);
            graph.addDependency("application","config",ProjectDependencyKind::UsesResource);
            graph.addDependency("test","implementation",ProjectDependencyKind::Tests);
            graph.addDependency("implementation","build",ProjectDependencyKind::Generates);
            graph.addTarget("core",ProjectTargetKind::Library,{"implementation"});
            graph.addTarget("app",ProjectTargetKind::Executable,{"application"});
            graph.addTarget("behavior",ProjectTargetKind::Test,{"test"});return graph;
        };
        auto graph=makeGraph("21");
        ProjectVerificationContract contract;contract.buildTargets={"core","app","behavior"};
        contract.includeRoots={"include"};
        contract.executionTimeout=std::chrono::seconds(30);
        contract.behaviors={{"program-output","app","","42\n","",0},
                            {"generated-unit-test","behavior","","","",0}};
        std::filesystem::path sandbox;
        if(argc==4){sandbox=argv[3];contract.requireSandbox=true;}
        const auto evidence=verifyGenerativeProject(graph,contract,root,compiler,sandbox);
        if(!evidence.success||evidence.checks.size()!=5||!std::filesystem::is_regular_file(evidence.report))return 3;
        if(argc==4&&(evidence.checks[3].sandboxed==false||evidence.checks[4].sandboxed==false))return 6;
        if(evidence.genomeHash!=system.strand->getGenomeHash()||evidence.graphHash!=graph.graphHash())return 4;
        auto negativeContract=contract;negativeContract.requireSandbox=false;
        negativeContract.behaviors={{"wrong-oracle","app","","41\n","",0}};
        const auto negativeRoot=std::filesystem::path(root.string()+"_negative");
        std::filesystem::remove_all(negativeRoot,error);
        const auto negative=verifyGenerativeProject(graph,negativeContract,negativeRoot,compiler);
        if(negative.success||negative.checks.empty()||negative.checks.back().passed||
           !std::filesystem::is_regular_file(negative.report))return 7;
        bool sandboxRequirementRejected=false;
        try {
            auto sandboxContract=contract;sandboxContract.requireSandbox=true;
            static_cast<void>(verifyGenerativeProject(graph,sandboxContract,root.string()+"_sandbox_required",compiler,{}));
        } catch(const std::invalid_argument&) { sandboxRequirementRejected=true; }
        if(!sandboxRequirementRejected)return 5;
        const auto campaignRoot=std::filesystem::path(root.string()+"_campaign");
        std::filesystem::remove_all(campaignRoot,error);
        AutonomousProjectConfig campaignConfig{campaignRoot,compiler,sandbox,4,{}};
        const auto campaign=runAutonomousProjectRepair(makeGraph("20"),contract,campaignConfig);
        if(!campaign.success||campaign.selectedHypothesis!="numeric-calibrated-2"||campaign.cycles.size()!=3||
           campaign.cycles.front().evidence.success||!campaign.cycles.back().evidence.success||
           !std::filesystem::is_regular_file(campaign.report))return 8;
        const auto headerCampaignRoot=std::filesystem::path(root.string()+"_header_campaign");
        std::filesystem::remove_all(headerCampaignRoot,error);
        const auto memory=std::filesystem::path(root.string()+"_experience.memory");
        std::filesystem::remove(memory,error);
        campaignConfig.workspace=headerCampaignRoot;campaignConfig.maxCycles=3;campaignConfig.experienceStore=memory;
        const auto headerCampaign=runAutonomousProjectRepair(makeGraph("21",false),contract,campaignConfig);
        if(!headerCampaign.success||headerCampaign.selectedHypothesis!="genomic-header-1"||
           headerCampaign.cycles.size()!=2||headerCampaign.cycles.front().evidence.success||
           !headerCampaign.cycles.back().evidence.success||headerCampaign.transformations.size()!=1)return 9;
        ProjectExperienceStore store(memory);const auto learned=store.load();
        if(learned.size()!=1||learned.front().kind!=ProjectTransformationKind::InsertHeader)return 13;
        auto transferContract=contract;transferContract.behaviors.front().expectedOutput="44\n";
        const auto transferRoot=std::filesystem::path(root.string()+"_experience_transfer");
        std::filesystem::remove_all(transferRoot,error);
        AutonomousProjectConfig transferConfig{transferRoot,compiler,sandbox,1,{}};
        const auto transfer=runExperienceTransferredProjectDevelopment(makeGraph("22",false),transferContract,transferConfig,store);
        if(!transfer.success||!transfer.experienceTransferred||transfer.cycles.size()!=1||
           transfer.experienceOriginGraphHash!=learned.front().originGraphHash)return 14;
        auto wrongGenome=learned.front();++wrongGenome.genomeHash;bool wrongGenomeRejected=false;
        try { static_cast<void>(applyVerifiedTransformation(makeGraph("22",false),wrongGenome)); }
        catch(const std::invalid_argument&) { wrongGenomeRejected=true; }
        if(!wrongGenomeRejected)return 15;
        const auto tampered=std::filesystem::path(root.string()+"_tampered.memory");
        std::filesystem::copy_file(memory,tampered,std::filesystem::copy_options::overwrite_existing);
        {std::ofstream corrupt(tampered,std::ios::binary|std::ios::app);corrupt<<"tampered\n";}
        bool tamperRejected=false;try { static_cast<void>(ProjectExperienceStore(tampered).load()); }
        catch(const std::runtime_error&) { tamperRejected=true; }
        if(!tamperRejected)return 16;
        campaignConfig.experienceStore.clear();
        const auto linkCampaignRoot=std::filesystem::path(root.string()+"_link_campaign");
        std::filesystem::remove_all(linkCampaignRoot,error);
        campaignConfig.workspace=linkCampaignRoot;campaignConfig.maxCycles=3;
        const auto linkCampaign=runAutonomousProjectRepair(makeGraph("21",true,false),contract,campaignConfig);
        if(!linkCampaign.success||linkCampaign.selectedHypothesis!="genomic-link-1"||
           linkCampaign.cycles.size()!=2||linkCampaign.cycles.front().evidence.success||
           !linkCampaign.cycles.back().evidence.success)return 10;
        const auto cumulativeRoot=std::filesystem::path(root.string()+"_cumulative_campaign");
        const auto cumulativeMemory=std::filesystem::path(root.string()+"_cumulative.memory");
        std::filesystem::remove_all(cumulativeRoot,error);std::filesystem::remove(cumulativeMemory,error);
        campaignConfig.workspace=cumulativeRoot;campaignConfig.maxCycles=5;campaignConfig.experienceStore=cumulativeMemory;
        const auto cumulative=runCumulativeAutonomousProjectRepair(makeGraph("\"21\"",false,false),contract,campaignConfig);
        if(!cumulative.success||cumulative.cycles.size()!=4||cumulative.transformations.size()!=3||
           cumulative.cycles[1].hypothesis!="genomic-header-1"||
           cumulative.cycles[2].hypothesis!="genomic-type-1"||cumulative.cycles[3].hypothesis!="genomic-link-1"||
           cumulative.cycles[1].evidence.success||cumulative.cycles[2].evidence.success||
           !cumulative.cycles[3].evidence.success)return 19;
        const auto cumulativeLearned=ProjectExperienceStore(cumulativeMemory).load();
        if(cumulativeLearned.size()!=3||cumulativeLearned[0].originGraphHash!=cumulativeLearned[1].originGraphHash||
           cumulativeLearned[1].originGraphHash!=cumulativeLearned[2].originGraphHash||
           cumulativeLearned[0].verifiedGraphHash!=cumulativeLearned[2].verifiedGraphHash||
           cumulativeLearned[0].evidenceHash!=cumulativeLearned[2].evidenceHash)return 20;
        {std::ifstream atomicGroup(cumulativeMemory,std::ios::binary);std::string first,second;
         std::getline(atomicGroup,first);std::getline(atomicGroup,second);
         if(first.rfind("GPIG2|",0)!=0||!second.empty())return 22;}
        const auto cumulativeTransferRoot=std::filesystem::path(root.string()+"_cumulative_transfer");
        std::filesystem::remove_all(cumulativeTransferRoot,error);
        auto cumulativeTransferContract=contract;cumulativeTransferContract.behaviors.front().expectedOutput="44\n";
        AutonomousProjectConfig cumulativeTransferConfig{cumulativeTransferRoot,compiler,sandbox,1,{}};
        const auto cumulativeTransfer=runExperienceTransferredProjectDevelopment(makeGraph("\"22\"",false,false),
            cumulativeTransferContract,cumulativeTransferConfig,ProjectExperienceStore(cumulativeMemory));
        if(!cumulativeTransfer.success||!cumulativeTransfer.experienceTransferred||
           cumulativeTransfer.transformations.size()!=3)return 21;
        campaignConfig.experienceStore.clear();
        const auto typeCampaignRoot=std::filesystem::path(root.string()+"_type_campaign");
        std::filesystem::remove_all(typeCampaignRoot,error);
        campaignConfig.workspace=typeCampaignRoot;campaignConfig.maxCycles=3;
        const auto typeCampaign=runAutonomousProjectRepair(makeGraph("\"21\""),contract,campaignConfig);
        if(!typeCampaign.success||typeCampaign.selectedHypothesis!="genomic-type-1"||
           typeCampaign.cycles.size()!=2||typeCampaign.cycles.front().evidence.success||
           !typeCampaign.cycles.back().evidence.success)return 11;
        const auto testCampaignRoot=std::filesystem::path(root.string()+"_test_campaign");
        std::filesystem::remove_all(testCampaignRoot,error);
        campaignConfig.workspace=testCampaignRoot;campaignConfig.maxCycles=6;
        auto testContract=contract;testContract.buildTargets={"behavior"};
        testContract.behaviors={{"generated-unit-test","behavior","","","",0}};
        const auto testCampaign=runAutonomousProjectRepair(makeGraph("21",true,true,"17"),testContract,campaignConfig);
        if(!testCampaign.success||testCampaign.selectedHypothesis!="integer-neighbor-4"||
           testCampaign.cycles.size()!=5||testCampaign.cycles.front().evidence.success||
           !testCampaign.cycles.back().evidence.success)return 12;
        return 0;
    } catch(const std::exception& exception) {
        std::cerr<<exception.what()<<'\n';return 1;
    }
}
