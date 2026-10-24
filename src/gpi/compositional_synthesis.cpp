#include "gpi/compositional_synthesis.hpp"

#include "gpi/filesystem_safety.hpp"
#include "gpi/foreign_project_intelligence.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace genomic::gpi {
namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write " + path.string());
    output << content;
}

bool isCpp(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    return extension == ".cpp" || extension == ".cc" || extension == ".cxx";
}

void copyTree(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code error;
    std::filesystem::create_directories(destination);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
        if (entry.is_symlink()) throw std::runtime_error("symlink in protected project");
        const auto target = destination / std::filesystem::relative(entry.path(), source);
        if (entry.is_directory()) std::filesystem::create_directories(target);
        else if (entry.is_regular_file()) {
            std::filesystem::create_directories(target.parent_path());
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, error);
            if (error) throw std::runtime_error("cannot create isolated candidate");
        }
    }
}

std::size_t matchingOpenParen(const std::string& source, const std::size_t close) {
    int depth{};
    for (std::size_t index = close + 1; index-- > 0;) {
        if (source[index] == ')') ++depth;
        else if (source[index] == '(' && --depth == 0) return index;
    }
    return std::string::npos;
}

std::vector<std::string> parameterNames(const std::string& parameters) {
    std::vector<std::string> result;
    std::size_t begin{};
    while (begin < parameters.size()) {
        const auto end = parameters.find(',', begin);
        const auto part = parameters.substr(begin, end == std::string::npos ? end : end - begin);
        std::size_t cursor = part.size();
        while (cursor > 0 && !std::isalnum(static_cast<unsigned char>(part[cursor - 1])) && part[cursor - 1] != '_') --cursor;
        const auto finish = cursor;
        while (cursor > 0 && (std::isalnum(static_cast<unsigned char>(part[cursor - 1])) || part[cursor - 1] == '_')) --cursor;
        if (finish > cursor && part.substr(cursor, finish - cursor) != "void") result.push_back(part.substr(cursor, finish - cursor));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

struct Stub {
    std::filesystem::path source;
    std::size_t begin{};
    std::size_t end{};
    std::vector<std::string> parameters;
};

std::vector<Stub> discoverStubs(const std::filesystem::path& root) {
    std::vector<Stub> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || !isCpp(entry.path())) continue;
        const auto source = readFile(entry.path());
        std::size_t position{};
        while ((position = source.find("return 0;", position)) != std::string::npos) {
            const auto brace = source.rfind('{', position);
            const auto close = source.rfind(')', brace);
            const auto open = close == std::string::npos ? std::string::npos : matchingOpenParen(source, close);
            if (brace != std::string::npos && open != std::string::npos && source.substr(open, brace - open).find("main") == std::string::npos) {
                const auto parameters = parameterNames(source.substr(open + 1, close - open - 1));
                if (!parameters.empty()) result.push_back({entry.path(), position, position + 9, parameters});
            }
            position += 9;
        }
    }
    return result;
}

bool hasExperience(const std::filesystem::path& store) {
    return !store.empty() && std::filesystem::exists(store)
        && readFile(store).find("GDIIS2|success|compositional_control_flow") != std::string::npos;
}

void remember(const std::filesystem::path& store, const std::string& status, const std::string& program) {
    if (store.empty()) return;
    std::filesystem::create_directories(store.parent_path());
    std::ofstream output(store, std::ios::binary | std::ios::app);
    if (!output) throw std::runtime_error("cannot persist compositional experience");
    output << "GDIIS2|" << status << "|compositional_control_flow|" << program << '\n';
}

std::vector<std::string> branchPrograms(const Stub& stub) {
    std::vector<std::string> result;
    for (const auto& left : stub.parameters) {
        for (const auto& right : stub.parameters) {
            if (left == right) continue;
            result.push_back("if (" + left + " < " + right + ") { return " + left + "; } return " + left + " - " + right + ";");
            result.push_back("if (" + left + " > " + right + ") { return " + left + " - " + right + "; } return " + left + ";");
        }
    }
    return result;
}

std::vector<std::string> loopPrograms(const Stub& stub) {
    std::vector<std::string> result;
    for (const auto& bound : stub.parameters) {
        result.push_back("int result = 0; for (int index = 0; index < " + bound + "; ++index) { result += index; } return result;");
        result.push_back("int result = 0; for (int index = 1; index <= " + bound + "; ++index) { result += index; } return result;");
        for (const auto& operand : stub.parameters) {
            if (operand == bound) continue;
            result.push_back("int result = 1; for (int index = 0; index < " + bound + "; ++index) { result *= " + operand + "; } return result;");
        }
    }
    return result;
}

std::vector<std::string> programs(const Stub& stub, const bool experience) {
    auto branches = branchPrograms(stub);
    auto loops = loopPrograms(stub);
    if (experience) {
        branches.insert(branches.end(), loops.begin(), loops.end());
        return branches;
    }
    loops.insert(loops.end(), branches.begin(), branches.end());
    return loops;
}

} // namespace

const char* toString(const CompositionalSynthesisOutcome outcome) noexcept {
    switch (outcome) {
        case CompositionalSynthesisOutcome::Complete: return "COMPLETE";
        case CompositionalSynthesisOutcome::AlreadySatisfied: return "ALREADY_SATISFIED";
        case CompositionalSynthesisOutcome::Blocked: return "BLOCKED";
        case CompositionalSynthesisOutcome::Failed: return "FAILED";
    }
    return "FAILED";
}

CompositionalSynthesisResult runCompositionalSynthesis(const CompositionalSynthesisConfig& config) {
    if (config.protectedProject.empty() || config.isolatedWorkspace.empty() || config.maxCandidates == 0 || config.maxCandidates > 50) {
        throw std::invalid_argument("compositional synthesis requires 1..50 candidates");
    }
    CompositionalSynthesisResult result;
    const auto original = std::filesystem::absolute(config.protectedProject);
    const auto workspace = std::filesystem::absolute(config.isolatedWorkspace);
    const auto before = computeDirectoryTreeHash(original);
    try {
        validateIsolatedWorkspace(original, workspace);
        NativeBuildConfig baselineConfig = config.nativeBuild;
        baselineConfig.originalProject = original;
        baselineConfig.isolatedWorkspace = workspace / "baseline";
        result.baseline = executeNativeBuild(discoverBuildContract(original), baselineConfig);
        if (!result.baseline.built) {
            result.outcome = CompositionalSynthesisOutcome::Blocked;
            result.reason = "baseline_does_not_compile";
        } else if (result.baseline.testsPassed) {
            result.outcome = CompositionalSynthesisOutcome::AlreadySatisfied;
            result.reason = "baseline_tests_already_satisfied";
        } else {
            result.experienceUsed = hasExperience(config.experienceStore);
            std::size_t count{};
            for (const auto& stub : discoverStubs(original)) {
                for (const auto& program : programs(stub, result.experienceUsed)) {
                    if (++count > config.maxCandidates) break;
                    const auto candidate = workspace / ("candidate_" + std::to_string(count));
                    copyTree(original, candidate);
                    const auto relative = std::filesystem::relative(stub.source, original);
                    auto source = readFile(candidate / relative);
                    source.replace(stub.begin, stub.end - stub.begin, program);
                    writeFile(candidate / relative, source);
                    NativeBuildConfig candidateConfig = config.nativeBuild;
                    candidateConfig.originalProject = candidate;
                    candidateConfig.isolatedWorkspace = workspace / ("candidate_build_" + std::to_string(count));
                    const auto evidence = executeNativeBuild(discoverBuildContract(candidate), candidateConfig);
                    result.attempts.push_back({relative, program, evidence.built, evidence.testsPassed});
                    if (evidence.built && evidence.testsPassed) {
                        result.developedProject = candidate;
                        result.outcome = CompositionalSynthesisOutcome::Complete;
                        result.reason = "compositional_program_verified";
                        remember(config.experienceStore, "success", program);
                        result.experiencePersisted = !config.experienceStore.empty();
                        result.originalUntouched = before == computeDirectoryTreeHash(original);
                        return result;
                    }
                    remember(config.experienceStore, "failed", program);
                }
                if (count >= config.maxCandidates) break;
            }
            result.outcome = CompositionalSynthesisOutcome::Blocked;
            result.reason = "compositional_grammar_exhausted";
        }
    } catch (const std::exception& error) {
        result.outcome = CompositionalSynthesisOutcome::Failed;
        result.reason = error.what();
    }
    result.originalUntouched = before == computeDirectoryTreeHash(original);
    return result;
}

} // namespace genomic::gpi
