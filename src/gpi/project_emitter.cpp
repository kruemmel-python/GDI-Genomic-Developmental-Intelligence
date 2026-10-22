#include "gpi/project_emitter.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace genomic::gpi {
namespace {
void write(const std::filesystem::path& file, const std::string& content) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary|std::ios::trunc);
    if (!out || !(out<<content)) throw std::runtime_error("Cannot write generated project file: "+file.string());
}
void replaceAll(std::string& source,const std::string& before,const std::string& after) {
    std::size_t pos=0;
    while ((pos=source.find(before,pos))!=std::string::npos) {
        source.replace(pos,before.size(),after);
        pos+=after.size();
    }
}
}
GeneratedProject emitProject(const TaskIR& task, uint64_t genomeHash,
                             const std::filesystem::path& directory, bool includeMathHeader) {
    if (directory.empty()) throw std::invalid_argument("Project directory is empty");
    if (std::filesystem::exists(directory) && !std::filesystem::is_empty(directory))
        throw std::invalid_argument("Refusing to overwrite a nonempty project directory");
    const auto count=task.samples.value_or(4);
    if (count>32) throw std::invalid_argument("Project verification supports 1..32 samples");
    std::vector<double> fixture;
    std::string inputText;
    for (std::size_t i=1;i<=count;++i) {
        fixture.push_back(static_cast<double>(i));
        inputText+=std::to_string(i)+"\n";
    }
    const auto expected=expectedOutputForInputs(task,fixture);
    GeneratedProject project;
    project.root=std::filesystem::absolute(directory).lexically_normal();
    project.includeDirectory=project.root/"include";
    project.mainSource=project.root/"src"/"main.cpp";
    project.pipelineSource=project.root/"src"/"pipeline.cpp";
    project.testSource=project.root/"tests"/"test_pipeline.cpp";
    const auto header=project.includeDirectory/"gpi_generated"/"pipeline.hpp";
    write(header,"#pragma once\n#include <iosfwd>\nnamespace gpi_generated {\nint runPipeline(std::istream&, std::ostream&, std::ostream&);\n}\n");
    std::string pipeline=generateTaskSource(task,genomeHash,includeMathHeader);
    if (pipeline.find("int main() {")==std::string::npos)
        throw std::runtime_error("Generator entry point changed; cannot split into modules");
    replaceAll(pipeline,"int main() {","namespace gpi_generated {\nint runPipeline(std::istream& input, std::ostream& output, std::ostream& errors) {");
    replaceAll(pipeline,"std::cin","input");
    replaceAll(pipeline,"std::cout","output");
    replaceAll(pipeline,"std::cerr","errors");
    pipeline="#include \"gpi_generated/pipeline.hpp\"\n"+pipeline+"\n} // namespace gpi_generated\n";
    write(project.pipelineSource,pipeline);
    write(project.mainSource,"#include \"gpi_generated/pipeline.hpp\"\n#include <iostream>\nint main() { return gpi_generated::runPipeline(std::cin,std::cout,std::cerr); }\n");
    auto quoted=[](const std::string& value) {
        std::string result="\"";
        for (char c:value) {
            if (c=='\\' || c=='\"') result+='\\';
            if (c=='\n') result+="\\n";
            else if (c=='\r') result+="\\r";
            else result+=c;
        }
        return result+"\"";
    };
    write(project.testSource,
        "#include \"gpi_generated/pipeline.hpp\"\n#include <sstream>\n#include <iostream>\n#include <string>\n"
        "int main() {\n"
        "  std::istringstream input("+quoted(inputText)+");\n"
        "  std::ostringstream output, errors;\n"
        "  const int status = gpi_generated::runPipeline(input,output,errors);\n"
        "  if (status != 0) { std::cerr << errors.str(); return 1; }\n"
        "  if (output.str() != "+quoted(expected)+") { std::cerr << output.str(); return 2; }\n"
        "  return 0;\n}\n");
    // The project is independent of the original GPI build tree.
    write(project.root/"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(GPIGenerated LANGUAGES CXX)\n"
        "set(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "add_library(gpi_pipeline STATIC src/pipeline.cpp)\n"
        "target_include_directories(gpi_pipeline PUBLIC include)\n"
        "add_executable(gpi_program src/main.cpp)\n"
        "target_link_libraries(gpi_program PRIVATE gpi_pipeline)\n"
        "add_executable(gpi_pipeline_test tests/test_pipeline.cpp)\n"
        "target_link_libraries(gpi_pipeline_test PRIVATE gpi_pipeline)\n"
        "enable_testing()\nadd_test(NAME GPIGeneratedPipeline COMMAND gpi_pipeline_test)\n");
    write(project.root/"README.md",
        "# GPI generated multi-file C++20 project\n\n"
        "Created from a typed AST after immutable-genome policy validation.\n"
        "src/pipeline.cpp contains generated implementation; src/main.cpp is a thin adapter.\n"
        "The generated test checks exact output against the AST oracle for the canonical fixture.\n"
        "GPI also verifies two input permutations using direct compiler/runtime execution.\n"
        "Build: cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure\n");
    project.files={header,project.pipelineSource,project.mainSource,project.testSource,
                   project.root/"CMakeLists.txt",project.root/"README.md"};
    return project;
}
}
