#include "gpi/compositional_synthesis.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

genomic::gpi::CompositionalSynthesisResult run(const std::filesystem::path& root,
                                               const std::filesystem::path& workspace,
                                               const std::filesystem::path& memory,
                                               const std::string& fixture,
                                               const char* cmake) {
    genomic::gpi::CompositionalSynthesisConfig config;
    config.protectedProject = root / "examples" / fixture;
    config.isolatedWorkspace = workspace / fixture;
    config.experienceStore = memory;
    config.nativeBuild.cmakeExecutable = cmake;
    config.nativeBuild.ctestExecutable = std::filesystem::path(cmake).parent_path() / "ctest.exe";
    config.nativeBuild.runTests = true;
    config.nativeBuild.timeoutMilliseconds = 180000;
    return genomic::gpi::runCompositionalSynthesis(config);
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("usage: test_compositional_synthesis <root> <workspace> <cmake>");
        const std::filesystem::path root = argv[1];
        const std::filesystem::path workspace = argv[2];
        std::error_code error;
        std::filesystem::remove_all(workspace, error);
        const auto memory = workspace / "compositional_experience.gdi";

        const auto loop = run(root, workspace, memory, "compositional_triangle", argv[3]);
        require(loop.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "loop synthesis failed: " + loop.reason);
        require(loop.originalUntouched && loop.experiencePersisted && !loop.attempts.empty()
                    && loop.attempts.back().testsPassed,
                "loop result lacks protected native evidence");
        require(loop.attempts.back().program.find("for (") != std::string::npos,
                "loop task was not solved by a generated loop");

        const auto branch = run(root, workspace, memory, "compositional_branch", argv[3]);
        require(branch.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "branch synthesis failed: " + branch.reason);
        require(branch.originalUntouched && branch.experienceUsed && !branch.attempts.empty()
                    && branch.attempts.back().testsPassed,
                "branch result did not use experience or native evidence");
        require(branch.attempts.back().program.find("if (") != std::string::npos,
                "branch task was not solved by a generated branch");

        const auto reduction = run(root, workspace, memory, "compositional_reduction", argv[3]);
        require(reduction.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "stateful reduction synthesis failed: " + reduction.reason);
        require(reduction.originalUntouched && reduction.experienceUsed && !reduction.attempts.empty()
                    && reduction.attempts.back().testsPassed,
                "stateful reduction lacks protected native evidence");
        require(reduction.attempts.back().program.find("while (") != std::string::npos
                    && reduction.attempts.back().program.find("state0 = state1") != std::string::npos,
                "reduction task was not solved by generated mutable control flow");

        const auto container = run(root, workspace, memory, "compositional_container", argv[3]);
        require(container.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "container traversal synthesis failed: " + container.reason);
        require(container.originalUntouched && !container.attempts.empty()
                    && container.attempts.back().testsPassed,
                "container traversal lacks protected native evidence");
        require(container.attempts.back().program.find("for (const int item") != std::string::npos,
                "container task was not solved by a generated traversal");

        const auto recursive = run(root, workspace, memory, "compositional_recursive", argv[3]);
        require(recursive.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "recursive synthesis failed: " + recursive.reason);
        require(recursive.originalUntouched && recursive.experienceUsed && !recursive.attempts.empty()
                    && recursive.attempts.back().testsPassed,
                "recursive result lacks protected native evidence");
        require(recursive.attempts.back().program.find("factorialWeight(") != std::string::npos,
                "recursive task was not solved by a generated self-call");

        const auto multi = run(root, workspace, memory, "compositional_multifunction", argv[3]);
        require(multi.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "joint multi-function synthesis failed: " + multi.reason);
        require(multi.originalUntouched && !multi.attempts.empty() && multi.attempts.back().testsPassed,
                "joint multi-function result lacks protected native evidence");
        require(multi.attempts.back().program.find("---") != std::string::npos,
                "multi-function plan did not contain multiple generated programs");

        const auto ownership = run(root, workspace, memory, "compositional_ownership", argv[3]);
        require(ownership.outcome == genomic::gpi::CompositionalSynthesisOutcome::Complete,
                "unique ownership synthesis failed: " + ownership.reason);
        require(ownership.originalUntouched && !ownership.attempts.empty() && ownership.attempts.back().testsPassed,
                "unique ownership result lacks protected native evidence");
        require(ownership.attempts.back().program.find("make_unique") != std::string::npos,
                "ownership task was not solved by generated ownership-safe code");

        std::cout << "Compositional synthesis: PASSED\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "COMPOSITIONAL SYNTHESIS TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
