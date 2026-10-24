#include "gpi/compositional_synthesis.hpp"

#include "gpi/filesystem_safety.hpp"
#include "gpi/foreign_project_intelligence.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
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
    std::vector<std::string> vectorParameters;
    std::vector<std::string> ownedVectorParameters;
    std::vector<std::string> uniquePointerParameters;
    std::string functionName;
};

std::string functionNameBefore(const std::string& source, std::size_t open) {
    while (open > 0 && std::isspace(static_cast<unsigned char>(source[open - 1]))) --open;
    const auto end = open;
    while (open > 0 && (std::isalnum(static_cast<unsigned char>(source[open - 1])) || source[open - 1] == '_')) --open;
    return source.substr(open, end - open);
}

std::vector<std::string> vectorParameterNames(const std::string& parameters) {
    std::vector<std::string> result;
    std::size_t begin{};
    while (begin < parameters.size()) {
        const auto end = parameters.find(',', begin);
        const auto part = parameters.substr(begin, end == std::string::npos ? end : end - begin);
        if (part.find("vector") != std::string::npos) {
            const auto names = parameterNames(part);
            if (!names.empty()) result.push_back(names.front());
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

std::vector<std::string> parameterNamesContaining(const std::string& parameters, const std::string& token) {
    std::vector<std::string> result;
    std::size_t begin{};
    while (begin < parameters.size()) {
        const auto end = parameters.find(',', begin);
        const auto part = parameters.substr(begin, end == std::string::npos ? end : end - begin);
        if (part.find(token) != std::string::npos) {
            const auto names = parameterNames(part);
            if (!names.empty()) result.push_back(names.front());
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

std::vector<std::string> parameterNamesContainingBoth(const std::string& parameters,
                                                       const std::string& first,
                                                       const std::string& second) {
    std::vector<std::string> result;
    std::size_t begin{};
    while (begin < parameters.size()) {
        const auto end = parameters.find(',', begin);
        const auto part = parameters.substr(begin, end == std::string::npos ? end : end - begin);
        if (part.find(first) != std::string::npos && part.find(second) != std::string::npos) {
            const auto names = parameterNames(part);
            if (!names.empty()) result.push_back(names.front());
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

std::vector<Stub> discoverStubs(const std::filesystem::path& root) {
    std::vector<Stub> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || !isCpp(entry.path())) continue;
        const auto source = readFile(entry.path());
        for (const std::string marker : {"return 0;", "return nullptr;"}) {
        std::size_t position{};
        while ((position = source.find(marker, position)) != std::string::npos) {
            const auto brace = source.rfind('{', position);
            const auto close = source.rfind(')', brace);
            const auto open = close == std::string::npos ? std::string::npos : matchingOpenParen(source, close);
            if (brace != std::string::npos && open != std::string::npos && source.substr(open, brace - open).find("main") == std::string::npos) {
                const auto parameterText = source.substr(open + 1, close - open - 1);
                const auto parameters = parameterNames(parameterText);
                if (!parameters.empty()) result.push_back({entry.path(), position, position + marker.size(), parameters,
                    vectorParameterNames(parameterText), parameterNamesContainingBoth(parameterText, "vector", "unique_ptr")
                        , parameterNamesContaining(parameterText, "unique_ptr"), functionNameBefore(source, open)});
            }
            position += marker.size();
        }
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

std::vector<std::string> reductionPrograms(const Stub& stub) {
    std::vector<std::string> result;
    for (const auto& left : stub.parameters) {
        for (const auto& right : stub.parameters) {
            if (left == right) continue;
            for (const std::string operation : {"%", "-", "+"}) {
                for (const std::string returned : {"state0", "state1"}) {
                    result.push_back(
                        "int state0 = " + left + "; int state1 = " + right
                        + "; while (state1 != 0) { int next = state0 " + operation
                        + " state1; state0 = state1; state1 = next; } return " + returned + ";");
                }
            }
        }
    }
    return result;
}

std::vector<std::string> vectorPrograms(const Stub& stub) {
    std::vector<std::string> result;
    for (const auto& container : stub.ownedVectorParameters) {
        result.push_back("int result = 0; for (const auto& item : " + container + ") { if (item) { result += *item; } } return result;");
    }
    for (const auto& container : stub.vectorParameters) {
        result.push_back("int result = 0; for (const int item : " + container + ") { result += item; } return result;");
        result.push_back("int result = 0; for (const int item : " + container + ") { if (item > 0) { ++result; } } return result;");
    }
    return result;
}

std::vector<std::string> uniquePointerPrograms(const Stub& stub) {
    std::vector<std::string> result;
    for (const auto& pointer : stub.uniquePointerParameters) {
        result.push_back("if (" + pointer + ") { return std::make_unique<int>(*" + pointer + "); } return nullptr;");
    }
    return result;
}

std::vector<std::string> recursivePrograms(const Stub& stub) {
    if (stub.parameters.size() != 1 || stub.functionName.empty() || !stub.vectorParameters.empty()) return {};
    const auto& value = stub.parameters.front();
    return {
        "if (" + value + " <= 1) { return 1; } return " + value + " * " + stub.functionName + "(" + value + " - 1);",
        "if (" + value + " <= 1) { return " + value + "; } return " + stub.functionName + "(" + value + " - 1) + " + stub.functionName + "(" + value + " - 2);"
    };
}

std::vector<std::string> programs(const Stub& stub, const bool experience) {
    auto branches = branchPrograms(stub);
    auto loops = loopPrograms(stub);
    auto reductions = reductionPrograms(stub);
    auto vectors = vectorPrograms(stub);
    auto pointers = uniquePointerPrograms(stub);
    auto recursions = recursivePrograms(stub);
    if (!pointers.empty() && stub.vectorParameters.empty()) return pointers;
    if (!vectors.empty()) {
        vectors.insert(vectors.end(), branches.begin(), branches.end());
        vectors.insert(vectors.end(), loops.begin(), loops.end());
        return vectors;
    }
    if (experience) {
        branches.insert(branches.end(), loops.begin(), loops.end());
        branches.insert(branches.end(), reductions.begin(), reductions.end());
        branches.insert(branches.end(), recursions.begin(), recursions.end());
        return branches;
    }
    loops.insert(loops.end(), branches.begin(), branches.end());
    loops.insert(loops.end(), reductions.begin(), reductions.end());
    loops.insert(loops.end(), recursions.begin(), recursions.end());
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
            auto stubs = discoverStubs(original);
            std::sort(stubs.begin(), stubs.end(), [](const Stub& left, const Stub& right) {
                return left.source == right.source ? left.begin < right.begin : left.source.string() < right.source.string();
            });
            using Patch = std::pair<const Stub*, std::string>;
            using Plan = std::vector<Patch>;
            std::vector<Plan> plans(1);
            for (const auto& stub : stubs) {
                const auto choices = programs(stub, result.experienceUsed);
                std::vector<Plan> next;
                for (const auto& plan : plans) {
                    for (const auto& program : choices) {
                        if (next.size() == config.maxCandidates) break;
                        auto expanded = plan;
                        expanded.emplace_back(&stub, program);
                        next.push_back(std::move(expanded));
                    }
                    if (next.size() == config.maxCandidates) break;
                }
                plans = std::move(next);
                if (plans.empty()) break;
            }
            std::size_t count{};
            for (const auto& plan : plans) {
                ++count;
                const auto candidate = workspace / ("candidate_" + std::to_string(count));
                copyTree(original, candidate);
                std::map<std::filesystem::path, std::vector<const Patch*>> perFile;
                std::string program;
                for (const auto& patch : plan) {
                    perFile[std::filesystem::relative(patch.first->source, original)].push_back(&patch);
                    if (!program.empty()) program += "\n---\n";
                    program += patch.second;
                }
                for (auto& [relative, patches] : perFile) {
                    std::sort(patches.begin(), patches.end(), [](const Patch* left, const Patch* right) {
                        return left->first->begin > right->first->begin;
                    });
                    auto source = readFile(candidate / relative);
                    for (const auto* patch : patches) {
                        source.replace(patch->first->begin, patch->first->end - patch->first->begin, patch->second);
                    }
                    writeFile(candidate / relative, source);
                }
                NativeBuildConfig candidateConfig = config.nativeBuild;
                candidateConfig.originalProject = candidate;
                candidateConfig.isolatedWorkspace = workspace / ("candidate_build_" + std::to_string(count));
                const auto evidence = executeNativeBuild(discoverBuildContract(candidate), candidateConfig);
                const auto relative = std::filesystem::relative(plan.front().first->source, original);
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
