#include "gpi/inductive_synthesis.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

genomic::gpi::InductiveSynthesisResult synthesize(const std::filesystem::path& root,
                                                   const std::filesystem::path& workspace,
                                                   const std::filesystem::path& experience,
                                                   const std::string& fixture,
                                                   const char* cmake) {
    genomic::gpi::InductiveSynthesisConfig config;
    config.protectedProject = root / "examples" / fixture;
    config.isolatedWorkspace = workspace / fixture;
    config.experienceStore = experience;
    config.nativeBuild.cmakeExecutable = cmake;
    config.nativeBuild.ctestExecutable = std::filesystem::path(cmake).parent_path() / "ctest.exe";
    config.nativeBuild.runTests = true;
    config.nativeBuild.timeoutMilliseconds = 180000;
    config.maxCandidates = 50;
    return genomic::gpi::runInductiveSynthesis(config);
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            throw std::invalid_argument("usage: test_inductive_synthesis <root> <workspace> <cmake>");
        }
        const std::filesystem::path root = argv[1];
        const std::filesystem::path workspace = argv[2];
        std::error_code error;
        std::filesystem::remove_all(workspace, error);
        const auto experience = workspace / "inductive_experience.gdi";

        const auto first = synthesize(root, workspace, experience, "inductive_alpha", argv[3]);
        require(first.outcome == genomic::gpi::InductiveSynthesisOutcome::Complete,
                "new arithmetic behaviour was not synthesized: " + first.reason);
        require(first.originalUntouched && first.experiencePersisted,
                "first induction did not preserve source or experience");
        require(!first.attempts.empty() && first.attempts.back().testsPassed,
                "first induction has no verified candidate");

        const auto transfer = synthesize(root, workspace, experience, "inductive_beta", argv[3]);
        require(transfer.outcome == genomic::gpi::InductiveSynthesisOutcome::Complete,
                "learned induction did not solve independently named project: " + transfer.reason);
        require(transfer.originalUntouched && transfer.experienceUsed,
                "transfer did not use persisted generic synthesis experience");
        require(!transfer.attempts.empty() && transfer.attempts.back().testsPassed,
                "transferred candidate was not verified by native tests");

        std::cout << "Inductive synthesis: PASSED\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "INDUCTIVE SYNTHESIS TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
