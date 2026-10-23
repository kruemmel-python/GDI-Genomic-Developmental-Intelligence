#include "gpi/autonomous_semantic_integration.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace { void require(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);} }
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("usage: test_r19_semantic <root> <workspace> <cmake>");
    const std::filesystem::path root=argv[1],work=argv[2];std::error_code ec;std::filesystem::remove_all(work,ec);
    auto run=[&](const std::string& fixture,const std::string& requirement,const std::string& name){genomic::gpi::SemanticIntegrationConfig c;c.originalProject=root/"examples"/fixture;c.isolatedWorkspace=work/name;c.execute=true;c.nativeBuild.cmakeExecutable=argv[3];c.nativeBuild.ctestExecutable="ctest";c.nativeBuild.runTests=false;c.nativeBuild.timeoutMilliseconds=180000;return genomic::gpi::runAutonomousSemanticIntegration(c,requirement);};
    const auto csv=run("r19_csv_semantic","For CSV records skip invalid rows and write an error report.","csv");
    require(csv.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete&&csv.plan.selected.kind=="pre_processor_validation","hook-free CSV discovery failed: "+csv.reason);
    const auto csvGerman=run("r19_csv_semantic","Bei CSV-Datensaetzen ungueltige Zeilen ueberspringen und einen Fehlerbericht schreiben.","csv_german");
    require(csvGerman.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete&&csvGerman.plan.selected.kind=="pre_processor_validation","German record semantics failed: "+csvGerman.reason);
    const auto state=run("r19_state_semantic","Einen gespeicherten Zustand wiederherstellen und beschaedigte Zustandsdaten ablehnen.","state");
    require(state.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete&&state.plan.selected.kind=="state_transition_validation","hook-free state discovery failed");
    const auto stateEnglish=run("r19_state_semantic","Resume state and reject corrupt state data.","state_english");
    require(stateEnglish.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete&&stateEnglish.plan.selected.kind=="state_transition_validation","English state semantics failed");
    const auto transfer=run("r19_transfer_semantic","For records skip invalid rows and write an error report.","transfer");
    require(transfer.outcome==genomic::gpi::SemanticIntegrationOutcome::Complete,"transfer semantic discovery failed: "+transfer.reason);
    genomic::gpi::SemanticIntegrationConfig blocked;blocked.originalProject=root/"examples"/"r19_csv_semantic";blocked.isolatedWorkspace=work/"negative";blocked.execute=true;
    const auto negative=genomic::gpi::runAutonomousSemanticIntegration(blocked,"Change an ambiguous unknown protocol without any observable contract.");
    require(negative.outcome==genomic::gpi::SemanticIntegrationOutcome::Blocked&&negative.originalUntouched&&!std::filesystem::exists(blocked.isolatedWorkspace/"developed_project"),"negative control was not blocked");
    std::cout<<"R19 hook-free semantic integration: PASSED\n";return 0;
}catch(const std::exception& e){std::cerr<<"R19 TEST FAILURE: "<<e.what()<<'\n';return 1;}}
