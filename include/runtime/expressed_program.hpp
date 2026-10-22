#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "runtime/genome_ir.hpp"
#include "cpp20/cpp20_ast.hpp"

namespace genomic {

struct ExpressedProgram {
    uint64_t programId{0};
    uint64_t genomeHash{0};
    uint64_t contextHash{0};
    uint64_t astHash{0};
    uint64_t irHash{0};

    std::string name;
    std::shared_ptr<AstNode> ast;
    std::shared_ptr<IrModule> irModule;
    GenomeProvenance provenance;
};

} // namespace genomic
