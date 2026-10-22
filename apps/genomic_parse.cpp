#include "cpp20/cpp20_parser.hpp"
#include "genome/genome_bootstrap.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
void printAst(const genomic::AstNode& node, int depth = 0) {
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "kind=" << static_cast<unsigned>(node.kind);
    if (!node.name.empty()) std::cout << " name=" << node.name;
    if (!node.op.empty()) std::cout << " op=" << node.op;
    if (node.kind == genomic::AstNodeKind::LiteralExpr) {
        std::cout << " literal=" << node.intVal;
    }
    std::cout << '\n';
    for (const auto& child : node.children) {
        if (child) printAst(*child, depth + 1);
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: genomic_parse <source.cpp>\n";
        return 2;
    }
    std::ifstream source(argv[1], std::ios::binary);
    if (!source) {
        std::cerr << "Cannot open source file: " << argv[1] << '\n';
        return 2;
    }
    const std::string input((std::istreambuf_iterator<char>(source)),
                            std::istreambuf_iterator<char>());
    try {
        const auto system = genomic::GenomeBootstrap::buildCompleteSystem();
        const auto ast = genomic::Cpp20Parser::parseWithGenome(input, *system.ruleEngine);
        std::cout << "Genome hash: " << system.strand->getGenomeHash() << '\n';
        std::cout << "Genomic binary grammar rules: "
                  << system.ruleEngine->getExpressionGrammar().binaryRuleCount() << '\n';
        printAst(*ast);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Genomic parser rejected input: " << error.what() << '\n';
        return 1;
    }
}
