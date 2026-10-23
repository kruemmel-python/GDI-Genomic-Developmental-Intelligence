#include "gpi/autonomous_software_engineering.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace { void require(const bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);} }

int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::invalid_argument("usage: test_r17_asei <source-root> <workspace>");
        const std::filesystem::path root=argv[1],workspace=argv[2],project=root/"examples"/"r17_neutral_project";
        std::error_code error;std::filesystem::remove_all(workspace,error);std::filesystem::create_directories(workspace);
        const auto contract=genomic::gpi::interpretDevelopmentRequirement("Add persistent session state that is saved, restored after restart, and rejected when corrupted.");
        require(contract.status==genomic::gpi::RequirementStatus::Clarified&&contract.capabilityClass=="persistent_state","requirement interpretation failed");
        const auto serialized=workspace/"requirement.gdir17c";genomic::gpi::saveRequirementContract(contract,serialized);
        require(genomic::gpi::loadRequirementContract(serialized).id==contract.id,"requirement persistence failed");
        const auto build=genomic::gpi::discoverBuildContract(project);
        const auto impact=genomic::gpi::analyzeChangeImpact(project,build,contract);
        require(!impact.nodes.empty(),"impact analysis found no nodes");
        const auto architecture=genomic::gpi::synthesizeArchitectureChange(contract,impact,build);
        require(architecture.executable&&architecture.cmakeMutation.find("add_test")!=std::string::npos,"architecture synthesis failed");
        const auto blocked=genomic::gpi::interpretDevelopmentRequirement("Implement an unspecified artificial intelligence universe.");
        require(blocked.status==genomic::gpi::RequirementStatus::Blocked,"unsupported requirement was not fail-closed");
        std::cout<<"R17 requirement, impact, architecture and persistence checks: PASSED\n";
        return 0;
    } catch(const std::exception& exception) {std::cerr<<"R17 TEST FAILURE: "<<exception.what()<<'\n';return 1;}
}
