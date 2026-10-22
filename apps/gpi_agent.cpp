#include "genome/genome_bootstrap.hpp"
#include "gpi/agent_runner.hpp"
#include "gpi/experience_store.hpp"
#include "gpi/task_ir.hpp"
#include "gpi/task_genome_policy.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace {
std::string readFile(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read task file");
    const std::string s(std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{});
    if (s.size()>8192) throw std::runtime_error("Task file exceeds 8192 bytes");
    return s;
}
void usage() {
    std::cerr << "Usage: gpi_agent --task 'map=x*x+3; filter=y>10; reduce=sum; samples=4' "
                 "[--out result.cpp] [--compiler g++] [--memory experience.log]\n"
                 "       gpi_agent --task-file specification.txt [--repair-probe]\n";
}
}
int main(int argc, char** argv) {
    std::string task, taskFile, compiler="g++", out="gpi_task.cpp", memory="gpi_experience.log";
    bool repairProbe=false;
    for (int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if (arg=="--repair-probe") { repairProbe=true; continue; }
        if (i+1>=argc) { usage(); return 2; }
        const std::string value=argv[++i];
        if (arg=="--task") task=value;
        else if (arg=="--task-file") taskFile=value;
        else if (arg=="--compiler") compiler=value;
        else if (arg=="--out") out=value;
        else if (arg=="--memory") memory=value;
        else { usage(); return 2; }
    }
    if ((task.empty() && taskFile.empty()) || (!task.empty() && !taskFile.empty())) { usage(); return 2; }
    try {
        if (!taskFile.empty()) task=readFile(taskFile);
        const auto parsed=genomic::gpi::parseTask(task);
        if (!parsed.task) { std::cerr << "BLOCKED: " << parsed.error << '\n'; return 3; }
        const auto genome=genomic::GenomeBootstrap::buildCompleteSystem();
        const auto hash=genome.strand->getGenomeHash();
        const genomic::gpi::TaskGenomePolicy policy(*genome.strand);
        const auto policyError=policy.validate(*parsed.task);
        if (!policyError.empty()) { std::cerr << "BLOCKED: " << policyError << '\n'; return 3; }
        std::cout << "MOLECULAR_TASK_ACTIVATION=PASS REGION=" << policy.regionId
                  << " SIGNATURE=" << policy.signature << '\n';
        genomic::gpi::ExperienceStore store(memory);
        const auto experiences=store.load();
        std::size_t prior=0;
        bool learnedMathHeaderRepair=false;
        for (const auto& e : experiences)
            if (e.genomeHash==hash && e.taskHash==genomic::gpi::fingerprint(task)) {
                ++prior;
                if (e.outcome=="PASS" && e.attempts>1) learnedMathHeaderRepair=true;
            }
        std::cout << "IMMUTABLE_GENOME_HASH=" << hash << "\nPRIOR_EXPERIENCES=" << prior << '\n';
        for (const auto& step : parsed.task->plan) std::cout << "PLAN: " << step << '\n';
        const genomic::gpi::AgentConfig config{out,compiler,repairProbe,learnedMathHeaderRepair};
        const auto result=genomic::gpi::runAgent(*parsed.task,hash,config);
        for (const auto& event : result.events) std::cout << "AGENT: " << event << '\n';
        store.append({hash,genomic::gpi::fingerprint(task),genomic::gpi::fingerprint(result.source),
                      result.success ? "PASS" : "BLOCKED",result.attempts});
        std::cout << "ATTEMPTS=" << result.attempts << "\nBEHAVIOR_VERIFIED=" << (result.behaviorVerified?"YES":"NO") << '\n';
        std::cout << "SOURCE=" << out << "\nBINARY=" << result.executable.string() << '\n';
        std::cout << "RESULT=" << (result.success?"PASS":"BLOCKED") << '\n';
        return result.success ? 0 : 4;
    } catch (const std::exception& e) {
        std::cerr << "GPI AGENT ERROR: " << e.what() << '\n';
        return 1;
    }
}
