#include "gpi/task_genome_policy.hpp"
#include "genome/genome_decoder.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace genomic::gpi {
namespace {
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::size_t pos=0;
    while (pos<=s.size()) {
        const auto end=s.find(delim,pos);
        out.push_back(s.substr(pos,end==std::string::npos ? end : end-pos));
        if (end==std::string::npos) break;
        pos=end+1;
    }
    return out;
}
std::string opcode(const std::string& op) {
    if (op=="+") return "add";
    if (op=="-") return "sub";
    if (op=="*") return "mul";
    if (op=="/") return "div";
    if (op=="%") return "mod";
    if (op=="<") return "lt";
    if (op==">") return "gt";
    if (op=="<=") return "le";
    if (op==">=") return "ge";
    if (op=="==") return "eq";
    if (op=="!=") return "ne";
    if (op=="&&") return "and";
    if (op=="||") return "or";
    if (op=="unary-") return "neg";
    if (op=="unary+") return "pos";
    if (op=="unary!") return "not";
    return {};
}
std::string validateTree(const ExprNode& node, const std::set<std::string>& ops) {
    if (node.op!="literal" && node.op!="x" && node.op!="y") {
        const auto name=opcode(node.op);
        if (name.empty() || !ops.contains(name)) return "Genomic policy disallows expression opcode: " + node.op;
    }
    if (node.left) {
        const auto error=validateTree(*node.left,ops);
        if (!error.empty()) return error;
    }
    if (node.right) {
        const auto error=validateTree(*node.right,ops);
        if (!error.empty()) return error;
    }
    return {};
}
}
TaskGenomePolicy::TaskGenomePolicy(const GenomeStrand& genome) {
    if (!genome.isFrozen() || !genome.verifyIntegrity().passed)
        throw std::invalid_argument("Task grammar requires frozen intact genome");
    bool found=false;
    for (const auto& region : genome.getAllRegions()) {
        if (region.kind != RegionKind::TaskGrammar) continue;
        if (found) throw std::invalid_argument("Duplicate task grammar loci");
        found=true;
        const auto decoded=GenomeDecoder::parseRegionsFromStrand(region.bases);
        if (decoded.size()!=1 || decoded[0].kind!=region.kind || decoded[0].name!=region.name ||
            decoded[0].payload!=region.payload) throw std::invalid_argument("Task grammar physical bases mismatch");
        const std::string payload(region.payload.begin(),region.payload.end());
        const auto fields=split(payload,'|');
        if (fields.size()!=4 || fields[0]!="gpi-task-v1")
            throw std::invalid_argument("Invalid genomic task grammar format");
        const auto [p,ec]=std::from_chars(fields[1].data(),fields[1].data()+fields[1].size(),signature);
        if (ec!=std::errc{} || p!=fields[1].data()+fields[1].size() || !signature)
            throw std::invalid_argument("Invalid task grammar molecule signature");
        for (const auto& op : split(fields[2],',')) {
            if (op.empty() || !operations.insert(op).second) throw std::invalid_argument("Invalid genomic task opcode");
        }
        for (const auto& red : split(fields[3],',')) {
            if (red.empty() || !reductions.insert(red).second) throw std::invalid_argument("Invalid genomic reduction");
        }
        regionId=region.id;
    }
    if (!found || operations.empty() || reductions.empty())
        throw std::invalid_argument("Genome missing task grammar");
}
bool TaskGenomePolicy::activatedByTaskMolecule() const {
    MolecularPool pool;
    pool.setConcentration(0,signature,0.9,"GPI_TASK_INTENT",MoleculeClass::Activator);
    RegulatoryResolver resolver;
    GenomeEnhancer enhancer;
    enhancer.id=regionId;
    enhancer.targetRegion=regionId;
    enhancer.name="GPI_TASK_EXPRESSION_ENHANCER";
    enhancer.activationThreshold=0.4;
    enhancer.cooperativity=1.0;
    enhancer.sites.push_back(GenomeBindingSite{signature,0.2,1.0,0.0,0,false,true});
    resolver.addEnhancer(enhancer);
    return resolver.resolve(pool).activeRegions.contains(regionId);
}
std::string TaskGenomePolicy::validate(const TaskIR& task) const {
    if (!activatedByTaskMolecule()) return "Task locus not molecularly activated";
    if (!reductions.contains(task.reduction)) return "Genomic policy disallows reduction: " + task.reduction;
    if (const auto err=validateTree(*task.map,operations); !err.empty()) return err;
    if (task.filter) return validateTree(*task.filter,operations);
    return {};
}
} // namespace genomic::gpi
