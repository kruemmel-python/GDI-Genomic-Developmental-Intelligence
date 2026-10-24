#include "gpi/compositional_synthesis.hpp"

#include "gpi/filesystem_safety.hpp"
#include "gpi/foreign_project_intelligence.hpp"
#include "gpi/generative_program_grammar.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
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
    std::string pointeeType{"int"};
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

std::string uniquePointerPointee(const std::string& parameters) {
    const auto start = parameters.find("unique_ptr<");
    if (start == std::string::npos) return "int";
    const auto typeBegin = start + std::string("unique_ptr<").size();
    const auto typeEnd = parameters.find('>', typeBegin);
    if (typeEnd == std::string::npos || typeEnd == typeBegin) return "int";
    return parameters.substr(typeBegin, typeEnd - typeBegin);
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
            const auto name = open == std::string::npos ? std::string{} : functionNameBefore(source, open);
            const bool controlScope = name == "if" || name == "for" || name == "while" || name == "switch" || name == "catch";
            if (brace != std::string::npos && open != std::string::npos && !controlScope
                && source.substr(open, brace - open).find("main") == std::string::npos) {
                const auto parameterText = source.substr(open + 1, close - open - 1);
                const auto parameters = parameterNames(parameterText);
                if (!parameters.empty()) result.push_back({entry.path(), position, position + marker.size(), parameters,
                    vectorParameterNames(parameterText), parameterNamesContainingBoth(parameterText, "vector", "unique_ptr")
                        , parameterNamesContaining(parameterText, "unique_ptr"), name,
                        uniquePointerPointee(parameterText)});
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

std::vector<std::string> programs(const Stub& stub, const bool experience) {
    GenerativeProgramContext context;
    context.scalarValues = stub.parameters;
    for (const auto& vector : stub.vectorParameters) {
        if (std::find(stub.ownedVectorParameters.begin(), stub.ownedVectorParameters.end(), vector)
            == stub.ownedVectorParameters.end()) context.integerVectors.push_back(vector);
    }
    context.ownedIntegerVectors = stub.ownedVectorParameters;
    if (stub.vectorParameters.empty()) context.uniquePointers = stub.uniquePointerParameters;
    context.pointeeType = stub.pointeeType;
    context.selfFunction = stub.functionName;
    auto generated = GenerativeProgramGrammar::enumerate(context, 50);
    if (experience) {
        const auto structuralScore = [](const std::string& program) {
            return static_cast<int>(program.find("for (") != std::string::npos)
                + static_cast<int>(program.find("while (") != std::string::npos)
                + static_cast<int>(program.find("if (") != std::string::npos);
        };
        std::stable_sort(generated.begin(), generated.end(), [&structuralScore](const std::string& left, const std::string& right) {
            return structuralScore(left) > structuralScore(right);
        });
    }
    return generated;
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
    if (config.protectedProject.empty() || config.isolatedWorkspace.empty() || config.maxCandidates == 0 || config.maxCandidates > 50
        || config.timeLimitMilliseconds == 0 || config.timeLimitMilliseconds > 1'500'000) {
        throw std::invalid_argument("compositional synthesis requires 1..50 candidates and a 1..25 minute time limit");
    }
    CompositionalSynthesisResult result;
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&]() {
        return static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
    };
    const auto boundedBuildTimeout = [&]() {
        const auto spent = elapsed();
        if (spent >= config.timeLimitMilliseconds) return std::size_t{};
        const auto remaining = config.timeLimitMilliseconds - spent;
        return std::min(config.nativeBuild.timeoutMilliseconds, std::max<std::size_t>(1, remaining / 3));
    };
    const auto original = std::filesystem::absolute(config.protectedProject);
    const auto workspace = std::filesystem::absolute(config.isolatedWorkspace);
    const auto before = computeDirectoryTreeHash(original);
    try {
        validateIsolatedWorkspace(original, workspace);
        NativeBuildConfig baselineConfig = config.nativeBuild;
        baselineConfig.originalProject = original;
        baselineConfig.isolatedWorkspace = workspace / "baseline";
        baselineConfig.timeoutMilliseconds = boundedBuildTimeout();
        if (baselineConfig.timeoutMilliseconds == 0) {
            result.outcome = CompositionalSynthesisOutcome::Blocked;
            result.timeLimitReached = true;
            result.reason = "compositional_time_budget_exhausted";
            result.originalUntouched = before == computeDirectoryTreeHash(original);
            result.elapsedMilliseconds = elapsed();
            return result;
        }
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
            struct Patch {
                Stub stub;
                std::string program;
            };
            using Plan = std::vector<Patch>;
            std::vector<Plan> plans(1);
            for (const auto& stub : stubs) {
                const auto choices = programs(stub, result.experienceUsed);
                std::vector<Plan> next;
                for (const auto& plan : plans) {
                    for (const auto& program : choices) {
                        if (next.size() == config.maxCandidates) break;
                        auto expanded = plan;
                        expanded.push_back({stub, program});
                        next.push_back(std::move(expanded));
                    }
                    if (next.size() == config.maxCandidates) break;
                }
                plans = std::move(next);
                if (plans.empty()) break;
            }
            std::size_t count{};
            for (const auto& plan : plans) {
                const auto timeout = boundedBuildTimeout();
                if (timeout == 0) {
                    result.outcome = CompositionalSynthesisOutcome::Blocked;
                    result.timeLimitReached = true;
                    result.reason = "compositional_time_budget_exhausted";
                    break;
                }
                ++count;
                const auto candidate = workspace / ("candidate_" + std::to_string(count));
                copyTree(original, candidate);
                std::map<std::filesystem::path, std::vector<const Patch*>> perFile;
                std::string program;
                for (const auto& patch : plan) {
                    perFile[std::filesystem::relative(patch.stub.source, original)].push_back(&patch);
                    if (!program.empty()) program += "\n---\n";
                    program += patch.program;
                }
                for (auto& [relative, patches] : perFile) {
                    std::sort(patches.begin(), patches.end(), [](const Patch* left, const Patch* right) {
                        return left->stub.begin > right->stub.begin;
                    });
                    auto source = readFile(candidate / relative);
                    for (const auto* patch : patches) {
                        source.replace(patch->stub.begin, patch->stub.end - patch->stub.begin, patch->program);
                    }
                    writeFile(candidate / relative, source);
                }
                NativeBuildConfig candidateConfig = config.nativeBuild;
                candidateConfig.originalProject = candidate;
                candidateConfig.isolatedWorkspace = workspace / ("candidate_build_" + std::to_string(count));
                candidateConfig.timeoutMilliseconds = timeout;
                const auto evidence = executeNativeBuild(discoverBuildContract(candidate), candidateConfig);
                const auto relative = std::filesystem::relative(plan.front().stub.source, original);
                result.attempts.push_back({relative, program, evidence.built, evidence.testsPassed});
                if (evidence.built && evidence.testsPassed) {
                    result.developedProject = candidate;
                    result.outcome = CompositionalSynthesisOutcome::Complete;
                    result.reason = "compositional_program_verified";
                    remember(config.experienceStore, "success", program);
                    result.experiencePersisted = !config.experienceStore.empty();
                    result.originalUntouched = before == computeDirectoryTreeHash(original);
                    result.elapsedMilliseconds = elapsed();
                    return result;
                }
                remember(config.experienceStore, "failed", program);
            }
            if (!result.timeLimitReached) {
                result.outcome = CompositionalSynthesisOutcome::Blocked;
                result.reason = "compositional_grammar_exhausted";
            }
        }
    } catch (const std::exception& error) {
        result.outcome = CompositionalSynthesisOutcome::Failed;
        result.reason = error.what();
    }
    result.originalUntouched = before == computeDirectoryTreeHash(original);
    result.elapsedMilliseconds = elapsed();
    return result;
}

} // namespace genomic::gpi
