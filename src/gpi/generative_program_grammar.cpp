#include "gpi/generative_program_grammar.hpp"

#include <algorithm>

namespace genomic::gpi {
namespace {

void append(std::vector<std::string>& programs, const std::string& program, const std::size_t limit) {
    if (programs.size() < limit && std::find(programs.begin(), programs.end(), program) == programs.end()) {
        programs.push_back(program);
    }
}

} // namespace

std::vector<std::string> GenerativeProgramGrammar::enumerate(const GenerativeProgramContext& context,
                                                               const std::size_t limit) {
    std::vector<std::string> programs;
    if (limit == 0) return programs;

    // Primitive expression and branch productions.
    for (const auto& left : context.scalarValues) {
        append(programs, "return " + left + ";", limit);
        for (const auto& right : context.scalarValues) {
            if (left == right) continue;
            for (const std::string op : {"+", "-", "*", "%"}) {
                append(programs, "return " + left + " " + op + " " + right + ";", limit);
            }
            append(programs, "if (" + left + " < " + right + ") { return " + left + "; } return " + left + " - " + right + ";", limit);
            append(programs, "if (" + left + " > " + right + ") { return " + left + " - " + right + "; } return " + left + ";", limit);
        }
    }

    // General counted-loop and mutable two-state productions.  No domain name
    // or target algorithm is encoded; operands, operations and bounds come
    // entirely from the recovered signature.
    for (const auto& bound : context.scalarValues) {
        for (const std::string seed : {"0", "1"}) {
            for (const std::string accumulation : {"+", "*"}) {
                append(programs, "int result = " + seed + "; for (int index = 0; index < " + bound
                    + "; ++index) { result " + accumulation + "= index; } return result;", limit);
                append(programs, "int result = " + seed + "; for (int index = 1; index <= " + bound
                    + "; ++index) { result " + accumulation + "= index; } return result;", limit);
            }
        }
    }
    for (const auto& left : context.scalarValues) {
        for (const auto& right : context.scalarValues) {
            if (left == right) continue;
            for (const std::string op : {"%", "-", "+"}) {
                for (const std::string returned : {"state0", "state1"}) {
                    append(programs, "int state0 = " + left + "; int state1 = " + right
                        + "; while (state1 != 0) { int next = state0 " + op
                        + " state1; state0 = state1; state1 = next; } return " + returned + ";", limit);
                }
            }
        }
    }

    // Generic range and ownership productions are enabled only by types found
    // in the target signature.
    for (const auto& values : context.integerVectors) {
        for (const std::string seed : {"0", "1"}) {
            for (const std::string accumulation : {"+", "*"}) {
                append(programs, "int result = " + seed + "; for (const int item : " + values
                    + ") { result " + accumulation + "= item; } return result;", limit);
            }
        }
    }
    for (const auto& values : context.ownedIntegerVectors) {
        for (const std::string seed : {"0", "1"}) {
            for (const std::string accumulation : {"+", "*"}) {
                append(programs, "int result = " + seed + "; for (const auto& item : " + values
                    + ") { if (item) { result " + accumulation + "= *item; } } return result;", limit);
            }
        }
    }
    for (const auto& pointer : context.uniquePointers) {
        append(programs, "if (" + pointer + ") { return std::make_unique<" + context.pointeeType
            + ">(*" + pointer + "); } return nullptr;", limit);
    }

    // Structural recursion is produced by combining generic base conditions,
    // decrements, return expressions and binary operators.
    if (context.scalarValues.size() == 1 && !context.selfFunction.empty()) {
        const auto& value = context.scalarValues.front();
        for (const std::string base : {"0", "1"}) {
            for (const std::string& baseReturn : {base, value}) {
                for (const std::string delta : {"1", "2"}) {
                    const auto recursive = context.selfFunction + "(" + value + " - " + delta + ")";
                    for (const std::string op : {"+", "*"}) {
                        append(programs, "if (" + value + " <= " + base + ") { return " + baseReturn
                            + "; } return " + value + " " + op + " " + recursive + ";", limit);
                        append(programs, "if (" + value + " <= " + base + ") { return " + baseReturn
                            + "; } return " + recursive + " " + op + " " + context.selfFunction
                            + "(" + value + " - " + (delta == "1" ? "2" : "1") + ");", limit);
                    }
                }
            }
        }
    }
    return programs;
}

} // namespace genomic::gpi
