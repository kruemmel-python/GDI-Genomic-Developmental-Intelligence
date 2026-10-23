#include "gpi/autonomous_behavioral_evolution.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace { void require(bool value,const std::string& text){if(!value)throw std::runtime_error(text);} }
int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::invalid_argument("usage: test_r18_behavioral <source-root> <workspace> <cmake>");
        const std::filesystem::path root=argv[1],base=argv[2];std::error_code ec;std::filesystem::remove_all(base,ec);
        auto run=[&](const std::string& fixture,const std::string& requirement){
            genomic::gpi::BehavioralEvolutionConfig c;c.originalProject=root/"examples"/fixture;c.isolatedWorkspace=base/fixture;c.execute=true;
            c.nativeBuild.cmakeExecutable=argv[3];c.nativeBuild.ctestExecutable="ctest";c.nativeBuild.timeoutMilliseconds=180000;c.nativeBuild.runTests=false;
            return genomic::gpi::runAutonomousBehavioralEvolution(c,requirement);
        };
        const auto records=run("r18_csv_application","For CSV records skip invalid rows and write an error report.");
        require(records.outcome==genomic::gpi::BehavioralEvolutionOutcome::Complete,"record integration incomplete");
        require(records.transformations.size()==1&&records.evidence.originalUntouched,"record safety evidence missing");
        const auto state=run("r18_state_application","Resume state and reject inconsistent state data.");
        require(state.outcome==genomic::gpi::BehavioralEvolutionOutcome::Complete,"state integration incomplete");
        require(state.evidence.tests.size()==2&&state.evidence.persistent,"state evidence incomplete");
        genomic::gpi::BehavioralEvolutionConfig blocked;blocked.originalProject=root/"examples"/"r18_csv_application";blocked.isolatedWorkspace=base/"blocked";blocked.execute=true;
        const auto no=genomic::gpi::runAutonomousBehavioralEvolution(blocked,"make all behavior magical");
        require(no.outcome==genomic::gpi::BehavioralEvolutionOutcome::Blocked&&!std::filesystem::exists(blocked.isolatedWorkspace/"developed_project"),"unsupported change was not fail-closed");
        std::cout<<"R18 behavioral integration end-to-end: PASSED\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"R18 TEST FAILURE: "<<e.what()<<'\n';return 1;}
}
