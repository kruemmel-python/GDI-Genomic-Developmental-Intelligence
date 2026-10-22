cmake_minimum_required(VERSION 3.20)
project(GenomicCpp20System VERSION 1.1.0 LANGUAGES CXX)

# A relocatable Windows GCC toolchain may need DLLs from its own bin directory
# when the compiler starts cc1plus and the linker. Keep that dependency local to
# compiler child processes instead of requiring callers to modify their PATH.
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(GENOMIC_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(GENOMIC_COMPILER_LAUNCHER
        "${CMAKE_COMMAND};-E;env;--modify;PATH=path_list_prepend:${GENOMIC_COMPILER_BIN};--")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${GENOMIC_COMPILER_LAUNCHER}")
    set(CMAKE_CXX_LINKER_LAUNCHER "${GENOMIC_COMPILER_LAUNCHER}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags with zero-warning tolerance as required by Section 21
if (MSVC)
    add_compile_options(/W4 /WX /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2)
endif()

include_directories(include)

set(GENOMIC_SOURCES
    src/runtime/substrate_kernel.cpp
    src/runtime/genome_ir.cpp
    src/runtime/genome_vm.cpp
    src/runtime/genome_persistence.cpp
    src/runtime/genome_stdlib.cpp
    src/genome/genome_integrity.cpp
    src/genome/genome_strand.cpp
    src/genome/genome_decoder.cpp
    src/genome/genomic_rule_engine.cpp
    src/genome/genome_bootstrap.cpp
    src/gpi/prompt_regulatory_resolver.cpp
    src/gpi/prompt_program_synthesizer.cpp
    src/gpi/task_ir.cpp
    src/gpi/task_genome_policy.cpp
    src/gpi/agent_runner.cpp
    src/gpi/experience_store.cpp
    src/gpi/natural_task.cpp
    src/gpi/project_emitter.cpp
    src/gpi/process_runner.cpp
    src/gpi/project_agent.cpp
    src/gpi/research_developer.cpp
    src/gpi/integrated_development.cpp
    src/gpi/generative_project.cpp
    src/gpi/project_verifier.cpp
    src/gpi/autonomous_project_developer.cpp
    src/gpi/project_experience.cpp
    src/gpi/project_manifest.cpp
    src/gpi/project_blueprint.cpp
    src/cpp20/cpp20_lexicon.cpp
    src/cpp20/cpp20_preprocessor.cpp
    src/cpp20/genomic_expression_grammar.cpp
    src/cpp20/cpp20_source_genome.cpp
    src/cpp20/cpp20_parser.cpp
    src/cpp20/cpp20_types.cpp
    src/cpp20/cpp20_ast.cpp
    src/cpp20/cpp20_semantics.cpp
    src/cpp20/cpp20_classes.cpp
    src/cpp20/cpp20_templates.cpp
    src/cpp20/cpp20_constexpr.cpp
    src/cpp20/cpp20_runtime_features.cpp
    src/cpp20/cpp20_memory_model.cpp
    src/cpp20/cpp20_compiler.cpp
    src/regulation/molecular_pool.cpp
    src/regulation/regulatory_resolver.cpp
    src/expression/splice_graph.cpp
    src/expression/semantic_validator.cpp
    src/expression/program_synthesizer.cpp
    src/development/phenotype_builder.cpp
    src/evolution/genome_evolution.cpp
    src/research/causal_trace.cpp
    src/research/genome_inspector.cpp
)

add_library(genomic_core STATIC ${GENOMIC_SOURCES})
target_include_directories(genomic_core PUBLIC include)

# Main Executable
add_executable(genomic_system_app apps/main.cpp)
target_link_libraries(genomic_system_app PRIVATE genomic_core)

# Test Executable
add_executable(test_genomic_system tests/test_genomic_system.cpp)
target_link_libraries(test_genomic_system PRIVATE genomic_core)

add_executable(genomic_parse apps/genomic_parse.cpp)
target_link_libraries(genomic_parse PRIVATE genomic_core)

add_executable(gpi_prompt apps/gpi_prompt.cpp)
target_link_libraries(gpi_prompt PRIVATE genomic_core)

add_executable(gpi_agent apps/gpi_agent.cpp)
target_link_libraries(gpi_agent PRIVATE genomic_core)

add_executable(gpi_project apps/gpi_project.cpp)
target_link_libraries(gpi_project PRIVATE genomic_core)

add_executable(gpi_research apps/gpi_research.cpp)
target_link_libraries(gpi_research PRIVATE genomic_core)

add_executable(gpi_integrated apps/gpi_integrated.cpp)
target_link_libraries(gpi_integrated PRIVATE genomic_core)

add_executable(gpi_develop apps/gpi_develop.cpp)
target_link_libraries(gpi_develop PRIVATE genomic_core)

add_executable(gpi_synthesize apps/gpi_synthesize.cpp)
target_link_libraries(gpi_synthesize PRIVATE genomic_core)

if(WIN32)
    add_executable(gpi_gui WIN32 apps/gpi_gui.cpp)
    target_link_libraries(gpi_gui PRIVATE genomic_core comctl32 comdlg32 shell32 ole32)
    if(MINGW)
        target_link_options(gpi_gui PRIVATE -municode)
    endif()
endif()

add_executable(gpi_sandbox apps/gpi_sandbox.cpp)
if(WIN32)
    target_link_libraries(gpi_sandbox PRIVATE advapi32 ole32 userenv)
endif()

add_executable(test_gpi_v03 tests/test_gpi_v03.cpp)
target_link_libraries(test_gpi_v03 PRIVATE genomic_core)

add_executable(test_generative_project tests/test_generative_project.cpp)
target_link_libraries(test_generative_project PRIVATE genomic_core)

enable_testing()
add_test(NAME GPIv03UnitTests COMMAND test_gpi_v03)

add_test(NAME GPIProjectEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_PROJECT=$<TARGET_FILE:gpi_project>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_project_e2e"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_project_end_to_end.cmake")
add_test(NAME ComprehensiveSection43Tests COMMAND test_genomic_system)

add_test(NAME GPIPromptEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_EXECUTABLE=$<TARGET_FILE:gpi_prompt>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCPP_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_e2e"
        "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_end_to_end.cmake")

add_test(NAME GPIAgentEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_AGENT=$<TARGET_FILE:gpi_agent>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_agent_e2e"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_agent_end_to_end.cmake")

add_test(NAME GPIResearchEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_RESEARCH=$<TARGET_FILE:gpi_research>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_research_e2e"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_research_end_to_end.cmake")

add_test(NAME GPIIntegratedEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_INTEGRATED=$<TARGET_FILE:gpi_integrated>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_integrated_e2e"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_integrated_end_to_end.cmake")

if(WIN32)
    add_test(NAME GPIGuiSelfTest COMMAND gpi_gui --self-test)
endif()

if(WIN32)
    add_test(NAME GPIGenerativeProjectEndToEnd
        COMMAND test_generative_project "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_BINARY_DIR}/gpi_generative_project_e2e" "$<TARGET_FILE:gpi_sandbox>")
else()
    add_test(NAME GPIGenerativeProjectEndToEnd
        COMMAND test_generative_project "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_BINARY_DIR}/gpi_generative_project_e2e")
endif()
