#include "genome/genome_bootstrap.hpp"
#include "gpi/behavioral_induction.hpp"
#include "gpi/developmental_intelligence.hpp"
#include "gpi/foreign_project_intelligence.hpp"
#include "gpi/project_verifier.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    using namespace genomic;
    using namespace genomic::gpi;
    if(argc<4||argc>5)return 2;
    try {
        const std::filesystem::path compiler=argv[1],root=argv[2],examples=argv[3];
        const std::filesystem::path sandbox=argc==5?std::filesystem::path(argv[4]):std::filesystem::path{};
        const auto system=GenomeBootstrap::buildCompleteSystem();
        const auto genome=Cpp20SourceGenome::fromGenome(*system.strand);
        for(const auto& name:{"max","square","absolute"}) {
            const auto input=examples/(std::string("behavioral_")+name)/(std::string(name)+".behavior");
            const auto specification=loadBehavioralGoal(input);
            std::ifstream textStream(input,std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(textStream)),std::istreambuf_iterator<char>());
            const auto parsedText=parseBehavioralGoalText(text);
            if(parsedText.samples.size()!=specification.samples.size()||
               parsedText.checks.size()!=specification.checks.size()||
               parsedText.functionName!=specification.functionName)return 8;
            const auto induced=induceArchitectureGoal(specification,genome);
            if(induced.goal.functions.size()!=1||induced.goal.functions.front().operations.empty()||
               induced.goal.cases.size()!=specification.samples.size()+specification.checks.size()||
               induced.report.find("\"withheld_checks_passed\": true")==std::string::npos)return 3;
            const auto goal=induced.goal;
            const auto architected=architectTypedGoal(goal,genome);
            auto contract=architected.project.contract;
            contract.requireSandbox=!sandbox.empty();
            contract.executionTimeout=std::chrono::seconds(30);
            std::error_code error;
            const auto workspace=root/name;
            std::filesystem::remove_all(workspace,error);
            const auto result=verifyGenerativeProject(architected.project.graph,contract,workspace,compiler,sandbox);
            if(!result.success) {
                std::cerr<<"Failed synthesized project: "<<name<<" report="<<result.report<<'\n';
                return 4;
            }
            const auto serialized=serializeTypedArchitectureGoal(goal);
            if(serialized.find("operation\t")==std::string::npos)return 5;
        }
        auto contradiction=loadBehavioralGoal(examples/"behavioral_max"/"max.behavior");
        contradiction.samples.push_back({{1,5},99,{"1","5"},"99"});
        if(!contradiction.functions.empty())
            contradiction.functions.front().samples.push_back({{1,5},99,{"1","5"},"99"});
        bool rejected=false;
        try {static_cast<void>(induceArchitectureGoal(contradiction,genome));}
        catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)return 6;
        auto withheld=loadBehavioralGoal(examples/"behavioral_square"/"square.behavior");
        withheld.checks.front().expected=100;
        withheld.checks.front().rawExpected="100";
        if(!withheld.functions.empty()) {
            withheld.functions.front().checks.front().expected=100;
            withheld.functions.front().checks.front().rawExpected="100";
        }
        rejected=false;
        try {static_cast<void>(induceArchitectureGoal(withheld,genome));}
        catch(const std::runtime_error&){rejected=true;}
        if(!rejected)return 7;

        // Multi-component, user-defined type, multi-module behavioral induction end-to-end check
        const auto geomInput=examples/"behavioral_geometry"/"geometry.behavior";
        if(std::filesystem::is_regular_file(geomInput)) {
            const auto geomSpec=loadBehavioralGoal(geomInput);
            const auto geomInduced=induceArchitectureGoal(geomSpec,genome);
            if(geomInduced.goal.functions.size()!=2||geomInduced.goal.types.size()!=1||geomInduced.goal.modules.size()!=2)return 9;
            const auto geomArch=architectTypedGoal(geomInduced.goal,genome);
            auto geomContract=geomArch.project.contract;
            geomContract.requireSandbox=!sandbox.empty();
            geomContract.executionTimeout=std::chrono::seconds(30);
            std::error_code error;
            const auto geomWorkspace=root/"geometry_v2";
            std::filesystem::remove_all(geomWorkspace,error);
            const auto geomResult=verifyGenerativeProject(geomArch.project.graph,geomContract,geomWorkspace,compiler,sandbox);
            if(!geomResult.success) {
                std::cerr<<"Failed multi-component behavioral project: geometry_v2 report="<<geomResult.report<<'\n';
                return 10;
            }
        }

        // Multi-way switch dispatch synthesis from behavioral examples + C++20 compilation & sandbox verification
        {
            const std::string switchBehaviorText=
                "GDI_BEHAVIORAL_GOAL_V2\n"
                "project\tswitch_dispatch_demo\n"
                "namespace\tgenerated\n"
                "function\tdispatch_op\tlong long\n"
                "parameter\tdispatch_op\tlong long\tmode\n"
                "parameter\tdispatch_op\tlong long\ta\n"
                "parameter\tdispatch_op\tlong long\tb\n"
                "sample\tdispatch_op\t0\t10\t20\t10\n"
                "sample\tdispatch_op\t1\t10\t20\t20\n"
                "sample\tdispatch_op\t2\t10\t20\t0\n"
                "sample\tdispatch_op\t0\t7\t3\t7\n"
                "sample\tdispatch_op\t1\t7\t3\t3\n"
                "sample\tdispatch_op\t2\t7\t3\t0\n"
                "check\tdispatch_op\t0\t42\t99\t42\n"
                "check\tdispatch_op\t1\t42\t99\t99\n"
                "check\tdispatch_op\t2\t42\t99\t0\n";
            const auto swSpec=parseBehavioralGoalText(switchBehaviorText);
            const auto swInduced=induceArchitectureGoal(swSpec,genome);
            if(swInduced.goal.functions.size()!=1||
               swInduced.goal.functions.front().operations.empty()||
               swInduced.goal.functions.front().operations.front().operation!="switch") {
                std::cerr<<"Failed multi-way switch behavioral induction\n";
                return 17;
            }
            const auto swArch=architectTypedGoal(swInduced.goal,genome);
            if(swArch.blueprint.functions.front().expression.find("switch (mode)")==std::string::npos) {
                std::cerr<<"Expected emitted C++20 switch statement in synthesized blueprint\n";
                return 18;
            }
            auto swContract=swArch.project.contract;
            swContract.requireSandbox=!sandbox.empty();
            swContract.executionTimeout=std::chrono::seconds(30);
            std::error_code error;
            const auto swWorkspace=root/"switch_dispatch_v2";
            std::filesystem::remove_all(swWorkspace,error);
            const auto swResult=verifyGenerativeProject(swArch.project.graph,swContract,swWorkspace,compiler,sandbox);
            if(!swResult.success) {
                std::cerr<<"Failed synthesized switch project verification: "<<swResult.report<<'\n';
                return 19;
            }
        }

        // End-to-end verification of the Autonomous Genomically Organized Developmental Programming Intelligence
        // across a 3-stage ontogenetic curriculum (Neonatal -> Juvenile -> Mature) with persistent ComponentExperienceStore
        const auto currInput=examples/"developmental_curriculum"/"ontogeny.curriculum";
        if(std::filesystem::is_regular_file(currInput)) {
            const auto curriculum=loadDevelopmentalCurriculum(currInput);
            if(curriculum.stages.size()!=3)return 11;
            std::error_code error;
            const auto devWorkspace=root/"ontogeny_run";
            const auto compMem=root/"ontogeny_components.memory";
            const auto repMem=root/"ontogeny_repairs.memory";
            std::filesystem::remove_all(devWorkspace,error);
            std::filesystem::remove(compMem,error);
            std::filesystem::remove(repMem,error);
            DevelopmentalIntelligenceConfig devCfg{
                devWorkspace,compiler,sandbox,compMem,repMem,4,!sandbox.empty()
            };
            const auto devRes=runDevelopmentalProgrammingIntelligence(curriculum,devCfg);
            if(!devRes.success||devRes.totalStagesCompleted!=3||devRes.totalReusedComponents<3||
               devRes.finalStage!=NeuralDevelopmentStage::Mature||
               devRes.immutableGenomeHash!=system.strand->getGenomeHash()||
               !std::filesystem::is_regular_file(devRes.reportPath)||
               !std::filesystem::is_regular_file(devRes.snapshotPath)) {
                std::cerr<<"Failed developmental intelligence curriculum run\n";
                return 12;
            }
            const auto storedComponents=ComponentExperienceStore(compMem).load();
            if(storedComponents.size()<3)return 13;

            // Autonomous Ontogenetic Developmental Planner & Controlled Memory Experiment:
            // Give GDI only the overarching high-level objective (no manual curriculum!).
            // Run 1 (with prior memory from ontogeny_run): should detect square, Point2D, manhattan, norm_sq, point_energy in memory
            // and derive a 1-stage curriculum only for the missing storage capability (`store_geometry_record`).
            // Run 2 (control experiment: naive vs experienced): proves that experience alters the derived developmental path.
            const std::string overarchingGoal=
                "Entwickle eine C++20-Anwendung, die zweidimensionale geometrische Objekte verwaltet, "
                "deren Eigenschaften berechnet und die Ergebnisse speichert.";
            const auto naivePlan=planAutonomousOntogenyFromObjective(overarchingGoal);
            const auto expPlan=planAutonomousOntogenyFromObjective(overarchingGoal,compMem);
            if(!naivePlan.audit.objectiveSupported||naivePlan.derivedCurriculum.stages.size()!=3||
               naivePlan.audit.knowledgeGaps.size()!=6||!naivePlan.audit.satisfiedFromMemory.empty()) {
                std::cerr<<"Failed naive autonomous ontogenetic planning\n";
                return 14;
            }
            if(!expPlan.audit.objectiveSupported||expPlan.derivedCurriculum.stages.size()!=1||
               expPlan.audit.satisfiedFromMemory.size()<5||expPlan.audit.knowledgeGaps.size()!=1) {
                std::cerr<<"Failed experienced autonomous ontogenetic planning (memory did not alter derived curriculum)\n";
                return 15;
            }
            // Execute the experienced 1-stage plan in the sandbox to prove the newly planned capability compiles and passes behavioral checks
            const auto expWorkspace=root/"autonomous_planned_with_memory";
            std::filesystem::remove_all(expWorkspace,error);
            DevelopmentalIntelligenceConfig expCfg{
                devWorkspace,compiler,sandbox,compMem,repMem,4,!sandbox.empty()
            };
            expCfg.workspace=expWorkspace;
            const auto expRun=runAutonomousGoalDirectedOntogeny(overarchingGoal,expCfg);
            if(!expRun.success||expRun.totalStagesCompleted!=1||expRun.totalReusedComponents<4) {
                std::cerr<<"Failed autonomous goal-directed ontogeny execution\n";
                return 16;
            }
        }

        // Foreign C++20 Software Project Intelligence:
        // 1. Read-Only Architecture Discovery + Simultaneous Hypothesis & Goal Formation on Foreign Project A (foreign_telemetry_project)
        // 2. Synthesis of missing module capability (telemetry::total_signal), sandbox verification, and Read-Only Boundary Check
        // 3. Dual Memory Consolidation: ProjectMemoryStore (GDIPM1) + DevelopmentalPrincipleStore (GDIDP1)
        // 4. Cross-Project Principle Transfer to an unknown Foreign Project B (inventory_ledger_project)
        const auto foreignTelemetryDir=examples/"foreign_telemetry_project";
        if(std::filesystem::is_directory(foreignTelemetryDir)) {
            std::error_code error;
            const auto fpWorkspaceA=root/"foreign_telemetry_isolated";
            const auto fpWorkspaceB=root/"foreign_inventory_isolated";
            const auto fpProjectBSource=root/"foreign_inventory_source_readonly";
            const auto projMemFile=root/"foreign_projects.gdipm";
            const auto devMemFile=root/"developmental_principles.gdidp";
            std::filesystem::remove_all(fpWorkspaceA,error);
            std::filesystem::remove_all(fpWorkspaceB,error);
            std::filesystem::remove_all(fpProjectBSource,error);
            std::filesystem::remove(projMemFile,error);
            std::filesystem::remove(devMemFile,error);

            const auto hashBeforeA=computeDirectoryTreeHash(foreignTelemetryDir);
            const auto initialAnalysisA=analyzeForeignCpp20Project(foreignTelemetryDir);
            if(initialAnalysisA.modules.empty()||initialAnalysisA.symbols.empty()||initialAnalysisA.hypotheses.empty()) {
                std::cerr<<"Failed foreign project A initial AST/architecture/hypothesis analysis\n";
                return 20;
            }

            ForeignProjectDevelopmentConfig fpCfgA;
            fpCfgA.foreignProjectRoot=foreignTelemetryDir;
            fpCfgA.isolatedWorkspace=fpWorkspaceA;
            fpCfgA.compiler=compiler;
            fpCfgA.sandboxRunner=sandbox;
            fpCfgA.projectMemoryPath=projMemFile;
            fpCfgA.developmentalMemoryPath=devMemFile;
            fpCfgA.allowWritebackToOriginal=false;
            fpCfgA.requireSandbox=!sandbox.empty();
            fpCfgA.maxIterations=6;

            const auto fpResA=runForeignProjectDevelopmentalLoop(fpCfgA);
            const auto hashAfterA=computeDirectoryTreeHash(foreignTelemetryDir);
            if(!fpResA.developmentVerified||!fpResA.originalProjectUntouched||
               hashBeforeA!=hashAfterA||fpResA.learnedPrinciples.empty()) {
                std::cerr<<"Failed foreign project A developmental loop or violated read-only safety boundary\n";
                return 21;
            }

            // Verify ProjectMemoryStore (GDIPM1) and DevelopmentalPrincipleStore (GDIDP1)
            const auto storedProjects=ProjectMemoryStore(projMemFile).load();
            const auto storedPrinciples=DevelopmentalPrincipleStore(devMemFile).load();
            if(storedProjects.empty()||storedProjects.back().verifiedGraphHash==0||storedPrinciples.empty()) {
                std::cerr<<"Failed dual memory consolidation (GDIPM1 / GDIDP1)\n";
                return 22;
            }

            // Construct a completely different Foreign Project B (inventory domain) with an unimplemented interface function
            std::filesystem::create_directories(fpProjectBSource/"include"/"inventory");
            std::filesystem::create_directories(fpProjectBSource/"src");
            std::filesystem::create_directories(fpProjectBSource/"tests");
            std::filesystem::create_directories(fpProjectBSource/"fixtures");
            {
                std::ofstream hdr(fpProjectBSource/"include"/"inventory"/"stock.hpp",std::ios::binary);
                hdr<<"#pragma once\nnamespace inventory {\nstruct StockItem {\n    long long on_hand;\n    long long reserved;\n};\nlong long available_units(StockItem item);\nlong long total_committed(StockItem item);\n} // namespace inventory\n";
            }
            {
                std::ofstream src(fpProjectBSource/"src"/"stock.cpp",std::ios::binary);
                src<<"#include \"inventory/stock.hpp\"\nnamespace inventory {\nlong long available_units(StockItem item) {\n    return item.on_hand - item.reserved;\n}\n} // namespace inventory\n";
            }
            {
                std::ofstream mainSrc(fpProjectBSource/"src"/"main.cpp",std::ios::binary);
                mainSrc<<"#include \"inventory/stock.hpp\"\n#include <iostream>\nint main() {\n    inventory::StockItem item{120, 35};\n    std::cout << \"AVAILABLE=\" << inventory::available_units(item) << \"\\n\";\n    std::cout << \"COMMITTED=\" << inventory::total_committed(item) << \"\\n\";\n    return 0;\n}\n";
            }
            {
                std::ofstream tst(fpProjectBSource/"tests"/"stock_test.cpp",std::ios::binary);
                tst<<"#include \"inventory/stock.hpp\"\nint main() {\n    if (inventory::available_units(inventory::StockItem{120, 35}) != 85) return 1;\n    if (inventory::total_committed(inventory::StockItem{120, 35}) != 155) return 2;\n    if (inventory::total_committed(inventory::StockItem{20, 8}) != 28) return 3;\n    return 0;\n}\n";
            }
            {
                std::ofstream expOut(fpProjectBSource/"fixtures"/"expected.txt",std::ios::binary);
                expOut<<"AVAILABLE=85\nCOMMITTED=155\n";
            }

            ForeignProjectDevelopmentConfig fpCfgB;
            fpCfgB.foreignProjectRoot=fpProjectBSource;
            fpCfgB.isolatedWorkspace=fpWorkspaceB;
            fpCfgB.compiler=compiler;
            fpCfgB.sandboxRunner=sandbox;
            fpCfgB.projectMemoryPath=projMemFile;
            fpCfgB.developmentalMemoryPath=devMemFile;
            fpCfgB.allowWritebackToOriginal=false;
            fpCfgB.requireSandbox=!sandbox.empty();
            fpCfgB.maxIterations=6;

            const auto fpResB=runForeignProjectDevelopmentalLoop(fpCfgB);
            if(!fpResB.developmentVerified||!fpResB.originalProjectUntouched||
               !fpResB.transferredPrincipleApplied||fpResB.appliedTransferredPrinciples.empty()) {
                std::cerr<<"Failed foreign project B cross-project developmental principle transfer\n";
                return 23;
            }
            const auto storedProjectsAfterB=ProjectMemoryStore(projMemFile).load();
            if(storedProjectsAfterB.size()<2) {
                std::cerr<<"Expected both foreign projects recorded in ProjectMemoryStore\n";
                return 24;
            }

            // 5. Closed-loop compiler diagnostic repair of deprecated C++20 API (std::filesystem::u8path)
            //    with KR source separation, zero original writeback, and transferable API modernization!
            const auto fpProjectCSource=root/"foreign_kr_dumper_source";
            const auto fpWorkspaceC=root/"foreign_kr_dumper_isolated";
            const auto fpProjectDSource=root/"foreign_logger_u8path_source";
            const auto fpWorkspaceD=root/"foreign_logger_isolated";
            std::filesystem::remove_all(fpProjectCSource,error);
            std::filesystem::remove_all(fpWorkspaceC,error);
            std::filesystem::remove_all(fpProjectDSource,error);
            std::filesystem::remove_all(fpWorkspaceD,error);

            // Set up Project C (KR structure with .kr primary source and generated KR_BUILD C++20 code)
            std::filesystem::create_directories(fpProjectCSource/"workspace");
            std::filesystem::create_directories(fpProjectCSource/"KR_BUILD"/"modules");
            std::filesystem::create_directories(fpProjectCSource/"tests");
            {
                std::ofstream krSrc(fpProjectCSource/"workspace"/"dumper.kr",std::ios::binary);
                krSrc<<"// KR Primary source\nfn dump(p: str) -> i32 { return 42; }\n";
            }
            {
                std::ofstream krHdr(fpProjectCSource/"KR_BUILD"/"kr_bridge.h",std::ios::binary);
                krHdr<<"#pragma once\n#include <string>\n#include <filesystem>\n#include <cstdint>\nnamespace fs = std::filesystem;\nstd::int32_t workspace_dump_path(const std::string& path);\n";
            }
            {
                std::ofstream dumperCpp(fpProjectCSource/"KR_BUILD"/"modules"/"workspace_dumper.cpp",std::ios::binary);
                dumperCpp<<"// Automatisch generiertes C++20-Modul aus workspace/dumper.kr\n"
                         <<"#include \"kr_bridge.h\"\n"
                         <<"#include <iostream>\n"
                         <<"#include <string>\n"
                         <<"#include <filesystem>\n"
                         <<"namespace fs = std::filesystem;\n"
                         <<"std::int32_t workspace_dump_path(const std::string& path) {\n"
                         <<"    fs::path input = fs::u8path(path);\n"
                         <<"    return static_cast<std::int32_t>(input.string().size());\n"
                         <<"}\n";
            }
            {
                std::ofstream dumperTest(fpProjectCSource/"tests"/"dumper_test.cpp",std::ios::binary);
                dumperTest<<"#include \"kr_bridge.h\"\n"
                          <<"#include <iostream>\n"
                          <<"int main() {\n"
                          <<"    std::string p = \"test_file.txt\";\n"
                          <<"    auto res = workspace_dump_path(p);\n"
                          <<"    if (res <= 0) return 1;\n"
                          <<"    return 0;\n"
                          <<"}\n";
            }

            const auto hashBeforeC=computeDirectoryTreeHash(fpProjectCSource);
            const auto initialAnalysisC=analyzeForeignCpp20Project(fpProjectCSource);
            if(initialAnalysisC.discoveredKrSourceFiles!=1 || initialAnalysisC.discoveredGeneratedKrBuildFiles<2) {
                std::cerr<<"Failed KR project C primary vs generated source separation\n";
                return 25;
            }
            bool foundDeprecatedHyp=false;
            for(const auto& h : initialAnalysisC.hypotheses) {
                if(h.kind==ArchitecturalHypothesisKind::DeprecatedCpp20ApiUsage) foundDeprecatedHyp=true;
            }
            if(!foundDeprecatedHyp) {
                std::cerr<<"Failed to generate DeprecatedCpp20ApiUsage hypothesis for u8path in Project C\n";
                return 26;
            }

            ForeignProjectDevelopmentConfig fpCfgC;
            fpCfgC.foreignProjectRoot=fpProjectCSource;
            fpCfgC.isolatedWorkspace=fpWorkspaceC;
            fpCfgC.compiler=compiler;
            fpCfgC.sandboxRunner=sandbox;
            fpCfgC.projectMemoryPath=projMemFile;
            fpCfgC.developmentalMemoryPath=devMemFile;
            fpCfgC.allowWritebackToOriginal=false;
            fpCfgC.requireSandbox=!sandbox.empty();
            fpCfgC.maxIterations=6;

            const auto fpResC=runForeignProjectDevelopmentalLoop(fpCfgC);
            const auto hashAfterC=computeDirectoryTreeHash(fpProjectCSource);
            if(!fpResC.developmentVerified || !fpResC.originalProjectUntouched || hashBeforeC!=hashAfterC) {
                std::cerr<<"Failed closed-loop autonomous repair of deprecated u8path in Project C or violated safety boundary\n";
                return 27;
            }
            bool learnedModernization=false;
            for(const auto& p : fpResC.learnedPrinciples) {
                if(p.patternCategory=="api_modernization" || p.actionTemplate.find("modernize-deprecated-cpp20-api")!=std::string::npos) {
                    learnedModernization=true;
                }
            }
            if(!learnedModernization) {
                std::cerr<<"Expected learned api_modernization principle from Project C repair\n";
                return 28;
            }

            // Set up Project D (a different foreign project also using u8path) to verify immediate principle transfer!
            std::filesystem::create_directories(fpProjectDSource/"src");
            std::filesystem::create_directories(fpProjectDSource/"tests");
            {
                std::ofstream loggerHdr(fpProjectDSource/"src"/"logger.hpp",std::ios::binary);
                loggerHdr<<"#pragma once\n#include <string>\n#include <filesystem>\n#include <cstdint>\nnamespace fs = std::filesystem;\nstd::int32_t log_to_path(const std::string& path);\n";
            }
            {
                std::ofstream loggerCpp(fpProjectDSource/"src"/"logger.cpp",std::ios::binary);
                loggerCpp<<"#include \"logger.hpp\"\n"
                         <<"#include <iostream>\n"
                         <<"#include <filesystem>\n"
                         <<"namespace fs = std::filesystem;\n"
                         <<"std::int32_t log_to_path(const std::string& path) {\n"
                         <<"    fs::path p = fs::u8path(path);\n"
                         <<"    return static_cast<std::int32_t>(p.string().length());\n"
                         <<"}\n";
            }
            {
                std::ofstream loggerTest(fpProjectDSource/"tests"/"logger_test.cpp",std::ios::binary);
                loggerTest<<"#include \"logger.hpp\"\n"
                          <<"#include <iostream>\n"
                          <<"int main() {\n"
                          <<"    if (log_to_path(\"log.txt\") <= 0) return 1;\n"
                          <<"    return 0;\n"
                          <<"}\n";
            }

            ForeignProjectDevelopmentConfig fpCfgD;
            fpCfgD.foreignProjectRoot=fpProjectDSource;
            fpCfgD.isolatedWorkspace=fpWorkspaceD;
            fpCfgD.compiler=compiler;
            fpCfgD.sandboxRunner=sandbox;
            fpCfgD.projectMemoryPath=projMemFile;
            fpCfgD.developmentalMemoryPath=devMemFile;
            fpCfgD.allowWritebackToOriginal=false;
            fpCfgD.requireSandbox=!sandbox.empty();
            fpCfgD.maxIterations=6;

            const auto fpResD=runForeignProjectDevelopmentalLoop(fpCfgD);
            if(!fpResD.developmentVerified || !fpResD.originalProjectUntouched || !fpResD.transferredPrincipleApplied) {
                std::cerr<<"Failed cross-project transfer of api_modernization principle to Project D\n";
                return 29;
            }

            // Regression: conventional `if (fn(...) != expected) return ...` guards must
            // provide scalar behavioral samples for missing-function synthesis.
            const auto fpProjectESource=root/"foreign_scalar_contract_source";
            const auto fpWorkspaceE=root/"foreign_scalar_contract_isolated";
            std::filesystem::remove_all(fpProjectESource,error);
            std::filesystem::remove_all(fpWorkspaceE,error);
            std::filesystem::create_directories(fpProjectESource/"include"/"calc");
            std::filesystem::create_directories(fpProjectESource/"src");
            std::filesystem::create_directories(fpProjectESource/"tests");
            {
                std::ofstream hdr(fpProjectESource/"include"/"calc"/"api.hpp",std::ios::binary);
                hdr<<"#pragma once\nnamespace calc {\nlong long identity(long long value);\nlong long combine(long long left, long long right);\n}\n";
            }
            {
                std::ofstream src(fpProjectESource/"src"/"api.cpp",std::ios::binary);
                src<<"#include \"calc/api.hpp\"\nnamespace calc { long long identity(long long value) { return value; } }\n";
            }
            {
                std::ofstream tst(fpProjectESource/"tests"/"combine_test.cpp",std::ios::binary);
                tst<<"#include \"calc/api.hpp\"\nint main() {\n"
                      "  if (calc::combine(7, 5) != 19) return 1;\n"
                      "  if (calc::combine(-2, 4) != 0) return 2;\n"
                      "  return 0;\n}\n";
            }
            ForeignProjectDevelopmentConfig fpCfgE;
            fpCfgE.foreignProjectRoot=fpProjectESource;
            fpCfgE.isolatedWorkspace=fpWorkspaceE;
            fpCfgE.compiler=compiler;
            fpCfgE.sandboxRunner=sandbox;
            fpCfgE.projectMemoryPath=projMemFile;
            fpCfgE.developmentalMemoryPath=devMemFile;
            fpCfgE.allowWritebackToOriginal=false;
            fpCfgE.requireSandbox=!sandbox.empty();
            fpCfgE.maxIterations=6;
            const auto fpResE=runForeignProjectDevelopmentalLoop(fpCfgE);
            if(!fpResE.developmentVerified || !fpResE.originalProjectUntouched) {
                std::cerr<<"Failed scalar !=-guard behavioral synthesis regression\n";
                return 30;
            }

            // General cross-language ABI regression with previously unseen names.
            const auto fpProjectFSource=root/"foreign_generic_abi_source";
            const auto fpWorkspaceF=root/"foreign_generic_abi_isolated";
            std::filesystem::remove_all(fpProjectFSource,error);
            std::filesystem::remove_all(fpWorkspaceF,error);
            std::filesystem::create_directories(fpProjectFSource/"workspace");
            std::filesystem::create_directories(fpProjectFSource/"KR_BUILD");
            std::filesystem::create_directories(fpProjectFSource/"tests");
            {
                std::ofstream go(fpProjectFSource/"workspace"/"limit.kr",std::ios::binary);
                go<<"func abi_limit_ok(value int32) bool { return value >= -3 && value <= 9 }\n";
            }
            {
                std::ofstream rust(fpProjectFSource/"workspace"/"step.kr",std::ios::binary);
                rust<<"fn abi_step_count(count: i32, enabled: bool) -> i32 { if enabled { count + 2 } else { count } }\n";
            }
            {
                std::ofstream hdr(fpProjectFSource/"KR_BUILD"/"bridge.h",std::ios::binary);
                hdr<<"#pragma once\n#include <cstdint>\nextern \"C\" {\n"
                      "bool abi_limit_ok(std::int32_t value);\n"
                      "std::int32_t abi_step_count(std::int32_t count, bool enabled);\n}\n";
            }
            {
                std::ofstream tst(fpProjectFSource/"tests"/"abi_test.cpp",std::ios::binary);
                tst<<"#include \"bridge.h\"\nint main() {\n"
                      "  if (!abi_limit_ok(-3) || !abi_limit_ok(9) || abi_limit_ok(-4) || abi_limit_ok(10)) return 1;\n"
                      "  if (abi_step_count(5, true) != 7 || abi_step_count(5, false) != 5) return 2;\n"
                      "  return 0;\n}\n";
            }
            ForeignProjectDevelopmentConfig fpCfgF;
            fpCfgF.foreignProjectRoot=fpProjectFSource;
            fpCfgF.isolatedWorkspace=fpWorkspaceF;
            fpCfgF.compiler=compiler;
            fpCfgF.sandboxRunner=sandbox;
            fpCfgF.projectMemoryPath=projMemFile;
            fpCfgF.developmentalMemoryPath=devMemFile;
            fpCfgF.allowWritebackToOriginal=false;
            fpCfgF.requireSandbox=!sandbox.empty();
            fpCfgF.maxIterations=6;
            const auto fpResF=runForeignProjectDevelopmentalLoop(fpCfgF);
            if(!fpResF.developmentVerified || !fpResF.originalProjectUntouched ||
               fpResF.initialAnalysis.abiContracts.size()<2) {
                for(const auto& abi:fpResF.initialAnalysis.abiContracts)
                    std::cerr<<"ABI "<<abi.functionName<<" return="<<abi.returnType
                             <<" params="<<abi.parameters.size()<<" derivable="<<abi.semanticallyDerivable
                             <<" expr="<<abi.cppExpression<<" diagnostic="<<abi.diagnostic<<'\n';
                std::cerr<<"Failed generic cross-language scalar ABI synthesis\n";
                return 31;
            }
        }
        std::cout<<"GDI behavioral induction, developmental intelligence & foreign project intelligence (read-only boundary + dual memory transfer) verified\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Behavioral induction test failed: "<<error.what()<<'\n';
        return 1;
    }
}
