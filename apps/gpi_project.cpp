#include "genome/genome_bootstrap.hpp"
#include "gpi/experience_store.hpp"
#include "gpi/natural_task.hpp"
#include "gpi/project_agent.hpp"
#include "gpi/task_genome_policy.hpp"
#include "gpi/task_ir.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace {
std::string readFile(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read task file");
    const std::string s{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    if (s.size()>1024*1024) throw std::runtime_error("File exceeds 1 MiB");
    return s;
}
void usage() {
    std::cerr<<"Usage: gpi_project --task 'map=x*x; reduce=sum; samples=4' --project-dir DIR [--compiler g++] [--memory FILE]\n"
                "       gpi_project --natural 'Berechne die Summe der Quadrate der ersten 4 Zahlen' --project-dir DIR\n"
                "       gpi_project --task-file FILE --project-dir DIR [--repair-probe]\n";
}
}
int main(int argc,char** argv) {
    std::string task,natural,taskFile,projectDir,compiler="g++",memory;
    bool repairProbe=false;
    for (int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if (key=="--repair-probe") { repairProbe=true; continue; }
        if (i+1>=argc) { usage(); return 2; }
        const std::string value=argv[++i];
        if (key=="--task") task=value;
        else if (key=="--natural") natural=value;
        else if (key=="--task-file") taskFile=value;
        else if (key=="--project-dir") projectDir=value;
        else if (key=="--compiler") compiler=value;
        else if (key=="--memory") memory=value;
        else { usage(); return 2; }
    }
    if (projectDir.empty() || (!task.empty())+(!natural.empty())+(!taskFile.empty())!=1) { usage(); return 2; }
    try {
        if (!taskFile.empty()) { task=readFile(taskFile); if (task.size()>8192) throw std::runtime_error("Task file exceeds 8192 bytes"); }
        if (!natural.empty()) {
            const auto translated=genomic::gpi::translateNaturalTask(natural);
            if (!translated.error.empty()) { std::cerr<<"BLOCKED: "<<translated.error<<'\n'; return 3; }
            task=translated.structured;
            std::cout<<"NATURAL_TASK_NORMALIZED="<<task<<'\n';
        }
        const auto parsed=genomic::gpi::parseTask(task);
        if (!parsed.task) { std::cerr<<"BLOCKED: "<<parsed.error<<'\n'; return 3; }
        const auto genome=genomic::GenomeBootstrap::buildCompleteSystem();
        const auto hash=genome.strand->getGenomeHash();
        const genomic::gpi::TaskGenomePolicy policy(*genome.strand);
        const auto error=policy.validate(*parsed.task);
        if (!error.empty()) { std::cerr<<"BLOCKED: "<<error<<'\n'; return 3; }
        std::cout<<"MOLECULAR_TASK_ACTIVATION=PASS REGION="<<policy.regionId<<" SIGNATURE="<<policy.signature<<'\n';
        bool learnedRepair=false;
        if (!memory.empty()) {
            const genomic::gpi::ExperienceStore store(memory);
            for (const auto& experience:store.load()) {
                if (experience.genomeHash==hash && experience.taskHash==genomic::gpi::fingerprint(task) &&
                    experience.outcome=="PASS" && experience.attempts>1) learnedRepair=true;
            }
        }
        for (const auto& step:parsed.task->plan) std::cout<<"PLAN: "<<step<<'\n';
        const auto result=genomic::gpi::runProjectAgent(*parsed.task,hash,
            {projectDir,compiler,repairProbe,learnedRepair});
        for (const auto& event:result.events) std::cout<<"AGENT: "<<event<<'\n';
        if (!memory.empty() && result.attempts>0) {
            const auto sourcePath=std::filesystem::path(projectDir)/"src"/"pipeline.cpp";
            const auto source=std::filesystem::exists(sourcePath)?readFile(sourcePath.string()):std::string{};
            genomic::gpi::ExperienceStore(memory).append({hash,genomic::gpi::fingerprint(task),
                genomic::gpi::fingerprint(source),
                result.success?"PASS":"BLOCKED",result.attempts});
        }
        std::cout<<"ATTEMPTS="<<result.attempts<<"\nBEHAVIOR_VERIFIED="
                 <<(result.behaviorVerified?"YES":"NO")<<"\nRESULT="<<(result.success?"PASS":"BLOCKED")<<'\n';
        return result.success?0:4;
    } catch (const std::exception& e) {
        std::cerr<<"GPI PROJECT ERROR: "<<e.what()<<'\n'; return 1;
    }
}
