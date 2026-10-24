#include "gpi/adaptive_experimentation.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace { void require(const bool value,const std::string& message){if(!value)throw std::runtime_error(message);} }

int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("usage: test_adaptive_experimentation <root> <workspace> <cmake>");
    const std::filesystem::path root=argv[1],workspace=argv[2];std::error_code error;std::filesystem::remove_all(workspace,error);
    const auto experience=workspace/"experience.gdiad";
    genomic::gpi::AdaptiveTrainingConfig training;training.protectedProjects={root/"examples"/"adaptive_alpha",root/"examples"/"adaptive_beta"};training.modelPath=experience;
    const auto trained=genomic::gpi::trainAdaptiveExperienceModel(training);
    require(trained.projectsAnalysed==2&&trained.observableLiteralContexts>=2&&trained.modelPersisted&&trained.rejectedProjects.empty(),"independent adaptive training failed");
    auto run=[&](const std::string& fixture,const std::string& expected,const std::string& name){genomic::gpi::AdaptiveExperimentConfig config;config.protectedProject=root/"examples"/fixture;config.isolatedWorkspace=workspace/name;config.experienceStore=experience;config.nativeBuild.cmakeExecutable=argv[3];config.nativeBuild.ctestExecutable="ctest";config.nativeBuild.runTests=false;config.nativeBuild.timeoutMilliseconds=180000;config.oracle.requiredOutput=expected;config.maxAttempts=50;return genomic::gpi::runAdaptiveExperimentation(config);};
    const auto first=run("adaptive_alpha","PROFILE=modern","alpha");
    require(first.outcome==genomic::gpi::AdaptiveExperimentOutcome::Complete&&first.originalUntouched&&first.experiencePersisted&&first.experienceUsed,"first previously-unsupported adaptive repair failed: "+first.reason);
    const auto second=run("adaptive_beta","STATE=active","beta");
    require(second.outcome==genomic::gpi::AdaptiveExperimentOutcome::Complete&&second.originalUntouched&&second.experienceUsed,"transfer of learned generic experimentation principle failed: "+second.reason);
    genomic::gpi::AdaptiveExperimentConfig negativeConfig;negativeConfig.protectedProject=root/"examples"/"adaptive_beta";negativeConfig.isolatedWorkspace=workspace/"negative";negativeConfig.experienceStore=experience;negativeConfig.nativeBuild.cmakeExecutable=argv[3];negativeConfig.nativeBuild.ctestExecutable="ctest";negativeConfig.nativeBuild.runTests=false;negativeConfig.nativeBuild.timeoutMilliseconds=180000;negativeConfig.oracle.requiredOutput="STATE=active";negativeConfig.oracle.forbiddenOutput="STATE=active";negativeConfig.maxAttempts=50;
    const auto negative=genomic::gpi::runAdaptiveExperimentation(negativeConfig);
    require(negative.outcome==genomic::gpi::AdaptiveExperimentOutcome::Blocked&&negative.originalUntouched,"adaptive negative control was not fail-closed");
    negativeConfig.isolatedWorkspace=workspace/"negative_repeat";
    const auto repeatedNegative=genomic::gpi::runAdaptiveExperimentation(negativeConfig);
    require(repeatedNegative.outcome==genomic::gpi::AdaptiveExperimentOutcome::Blocked&&repeatedNegative.suppressedFailedHypotheses>0&&repeatedNegative.originalUntouched,"failed-hypothesis learning did not suppress a repeated attempt");
    std::cout<<"Adaptive experimentation: PASSED\n";return 0;
}catch(const std::exception& error){std::cerr<<"ADAPTIVE TEST FAILURE: "<<error.what()<<'\n';return 1;}}
