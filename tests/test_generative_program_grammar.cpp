#include "gpi/generative_program_grammar.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        genomic::gpi::GenerativeProgramContext context;
        context.scalarValues = {"levels"};
        context.selfFunction = "staircaseTiles";
        const auto programs = genomic::gpi::GenerativeProgramGrammar::enumerate(context, 50);
        const auto hasInclusiveAccumulation = std::any_of(programs.begin(), programs.end(), [](const std::string& program) {
            return program.find("index <= levels") != std::string::npos && program.find("result += index") != std::string::npos;
        });
        if (programs.size() < 8 || !hasInclusiveAccumulation) {
            throw std::runtime_error("grammar did not compose independent expression and loop structures");
        }
        std::cout << "Generative program grammar: PASSED\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GENERATIVE PROGRAM GRAMMAR TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
