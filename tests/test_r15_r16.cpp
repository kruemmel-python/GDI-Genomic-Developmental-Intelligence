#include "gpi/build_contract.hpp"
#include "gpi/development_strategy.hpp"
#include "gpi/filesystem_safety.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
}

int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::invalid_argument("usage: test_r15_r16 <source-root> <workspace>");
        const std::filesystem::path source=argv[1],workspace=argv[2];
        std::error_code error;std::filesystem::remove_all(workspace,error);std::filesystem::create_directories(workspace);
        auto contract=genomic::gpi::discoverBuildContract(source/"examples"/"build_contract_smoke");
        require(contract.buildSystem==genomic::gpi::BuildSystemKind::CMake,"CMake discovery failed");
        require(contract.targets.size()==1&&contract.targets.front().name=="build_contract_smoke","target discovery failed");
        require(!contract.targets.front().sources.empty(),"target source discovery failed");
        require(contract.languageStandard.value=="C++20","language standard discovery failed");
        require(contract.testTargets.size()==1,"CTest discovery failed");
        contract.windowsSdk={"10.0.26100.0",genomic::gpi::ContractEvidence::Verified,"probe"};
        contract.environment.emplace("R15_TEST","enabled");contract.verified=true;
        const auto contractFile=workspace/"roundtrip.gdibc";genomic::gpi::saveBuildContract(contract,contractFile);
        const auto loaded=genomic::gpi::loadBuildContract(contractFile);
        require(loaded.projectRoot==contract.projectRoot&&loaded.targets.front().sources==contract.targets.front().sources,"GDIBC1 roundtrip lost fields");
        require(loaded.windowsSdk.value=="10.0.26100.0"&&loaded.windowsSdk.evidence==genomic::gpi::ContractEvidence::Verified,"GDIBC1 evidence roundtrip failed");
        require(loaded.environment.at("R15_TEST")=="enabled"&&loaded.verified,"GDIBC1 state roundtrip failed");
        {std::ofstream corrupt(contractFile,std::ios::binary|std::ios::app);corrupt<<"tampered\n";}
        bool corruptRejected=false;try{(void)genomic::gpi::loadBuildContract(contractFile);}catch(const std::exception&){corruptRejected=true;}
        require(corruptRejected,"corrupt GDIBC1 was accepted");
        bool isolationRejected=false;try{genomic::gpi::validateIsolatedWorkspace(contract.projectRoot,contract.projectRoot/"unsafe_workspace");}catch(const std::exception&){isolationRejected=true;}
        require(isolationRejected,"workspace nested in protected project was accepted");
        const auto diagnostics=genomic::gpi::classifyBuildDiagnostics(contract,
            "fatal error: generated/config.hpp: No such file or directory\nerror LNK2019: unresolved external symbol x\nThe following tests FAILED:\n");
        require(diagnostics.size()==3,"causal diagnostic classification count failed");
        require(diagnostics[0].classification==genomic::gpi::BuildDiagnosticClass::MissingConfiguredInclude,"include diagnosis failed");
        require(diagnostics[1].classification==genomic::gpi::BuildDiagnosticClass::MissingLinkLibrary,"link diagnosis failed");
        require(diagnostics[2].classification==genomic::gpi::BuildDiagnosticClass::TestFailure,"test diagnosis failed");
        genomic::gpi::StrategyPlanningRequest request;request.goal="repair composed defect";request.contract=contract;request.diagnostics=diagnostics;request.searchBudget=8;
        const auto plan=genomic::gpi::AutonomousStrategyPlanner().plan(request);
        require(plan.executable&&plan.candidates.size()>=2,"strategy induction did not create alternatives");
        const auto& composed=plan.candidates.back();require(composed.operators.size()>=5,"strategy operator composition is incomplete");
        const auto strategyFile=workspace/"strategies.gdis";genomic::gpi::DevelopmentStrategyStore store(strategyFile);store.append(composed);
        const auto restored=store.load();require(restored.size()==1&&restored.front().operators.size()==composed.operators.size(),"GDIS1 operator graph roundtrip failed");
        require(restored.front().operators[1].verification==composed.operators[1].verification,"GDIS1 verification metadata lost");
        {std::ofstream corrupt(strategyFile,std::ios::binary|std::ios::app);corrupt<<"tampered\n";}
        corruptRejected=false;try{(void)store.load();}catch(const std::exception&){corruptRejected=true;}
        require(corruptRejected,"corrupt GDIS1 was accepted");
        genomic::gpi::DevelopmentStrategy unsafe;unsafe.id="unsafe";unsafe.goal="must block";unsafe.origin="negative test";unsafe.requiredBuildSystem="CMake";unsafe.status=genomic::gpi::StrategyStatus::Executable;unsafe.operators.push_back({"mutate","UPDATE_TARGET_GRAPH",{}, {},{}, {},true});
        genomic::gpi::NativeBuildConfig execution;execution.originalProject=contract.projectRoot;execution.isolatedWorkspace=workspace/"unsafe_execution";
        const auto blocked=genomic::gpi::executeDevelopmentStrategy(unsafe,contract,execution);
        require(!blocked.success&&blocked.rolledBack&&blocked.terminationReason=="unauthorized_source_mutation_blocked","unsafe mutation was not blocked");
        auto incompatible=unsafe;incompatible.requiredCompilerFamily="MSVC";contract.compilerFamily={"GNU",genomic::gpi::ContractEvidence::Verified,"test"};
        require(genomic::gpi::strategyTransferScore(incompatible,contract,{})<0.0,"incompatible strategy transfer was accepted");
        std::cout<<"R15/R16 unit and persistence checks: PASSED\n";return 0;
    }catch(const std::exception& exception){std::cerr<<"R15/R16 TEST FAILURE: "<<exception.what()<<'\n';return 1;}
}
