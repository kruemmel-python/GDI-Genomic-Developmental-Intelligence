#include "genome/genome_bootstrap.hpp"
#include "gpi/prompt_regulatory_resolver.hpp"
#include "gpi/prompt_program_synthesizer.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

bool safeShellPath(const std::string& path) {
    if (path.empty()) return false;
    for (const unsigned char c : path) {
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.' &&
            c != '/' && c != '\\' && c != ':' && c != '+' && c != ' ')
            return false;
    }
    return true;
}

std::string loadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read prompt file: " + path);
    const std::string result((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (result.size() > 8192) throw std::runtime_error("Prompt file exceeds 8192 bytes");
    return result;
}

void usage() {
    std::cerr << "Usage: gpi_prompt --prompt \"...\" [--out generated.cpp] [--check] [--compiler g++]\n"
                 "       gpi_prompt --prompt-file task.txt [--out generated.cpp] [--check]\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string prompt;
    std::string promptFile;
    std::string out = "gpi_generated.cpp";
    std::string compiler = "g++";
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--prompt" || arg == "--prompt-file" || arg == "--out" || arg == "--compiler") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (arg == "--prompt") prompt = value;
            else if (arg == "--prompt-file") promptFile = value;
            else if (arg == "--out") out = value;
            else compiler = value;
        } else if (arg == "--check") {
            check = true;
        } else {
            usage();
            return 2;
        }
    }
    if ((prompt.empty() && promptFile.empty()) || (!prompt.empty() && !promptFile.empty())) {
        usage();
        return 2;
    }
    try {
        if (!promptFile.empty()) prompt = loadText(promptFile);
        const auto system = genomic::GenomeBootstrap::buildCompleteSystem();
        const genomic::gpi::PromptRegulatoryResolver resolver(*system.strand);
        const auto resolution = resolver.resolve(prompt);
        std::cout << "GPI GENOME HASH: " << resolution.genomeHash << '\n';
        std::cout << "REGULATORY CAPABILITIES: " << resolution.capabilities.size() << '\n';
        for (const auto& capability : resolution.capabilities) {
            std::cout << "  " << capability.id << " | molecule=" << capability.moleculeSignature
                      << " | region=" << capability.regionId << " | activity=" << capability.activity
                      << " | evidence=" << capability.matchedTerm << '\n';
        }
        const auto artifact = genomic::gpi::PromptProgramSynthesizer::synthesize(resolution);
        if (!artifact.generated) {
            for (const auto& error : artifact.diagnostics) std::cerr << "UNSUPPORTED: " << error << '\n';
            return 3;
        }
        for (const auto& step : artifact.plan) std::cout << "PLAN: " << step << '\n';
        std::ofstream output(out, std::ios::binary | std::ios::trunc);
        if (!output || !(output << artifact.source)) {
            std::cerr << "Cannot write source: " << out << '\n';
            return 4;
        }
        output.close();
        std::cout << "GENERATED: " << out << " (" << artifact.source.size() << " bytes)\n";
        if (check) {
            if (!safeShellPath(out) || !safeShellPath(compiler)) {
                std::cerr << "Unsafe compiler or output path; refusing shell invocation\n";
                return 5;
            }
            const std::string diagnostics = out + ".diagnostics.txt";
            const std::string command = '"' + compiler + '"' +
                " -std=c++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \"" + out +
                "\" > \"" + diagnostics + "\" 2>&1";
            const int status = std::system(command.c_str());
            if (status != 0) {
                std::cerr << "EXTERNAL_CPP20_CHECK_FAILED (" << status << "): " << diagnostics << '\n';
                return 6;
            }
            std::cout << "EXTERNAL_CPP20_CHECK: PASSED\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GPI ERROR: " << error.what() << '\n';
        return 1;
    }
}
