cmake_minimum_required(VERSION 3.20)
project(GenomicCpp20System VERSION 1.8.0 LANGUAGES CXX)

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
    src/gpi/goal_architect.cpp
    src/gpi/behavioral_induction.cpp
    src/gpi/developmental_intelligence.cpp
    src/gpi/foreign_project_intelligence.cpp
    src/gpi/filesystem_safety.cpp
    src/gpi/build_contract.cpp
    src/gpi/development_strategy.cpp
    src/gpi/autonomous_software_engineering.cpp
    src/gpi/autonomous_behavioral_evolution.cpp
    src/gpi/autonomous_semantic_integration.cpp
    src/gpi/adaptive_experimentation.cpp
    src/gpi/inductive_synthesis.cpp
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

add_executable(gdi_create apps/gdi_create.cpp)
target_link_libraries(gdi_create PRIVATE genomic_core)

add_executable(gdi_intelligence apps/gdi_intelligence.cpp)
target_link_libraries(gdi_intelligence PRIVATE genomic_core)

add_executable(gpi_architect apps/gpi_architect.cpp)
target_link_libraries(gpi_architect PRIVATE genomic_core)

if(WIN32)
    add_executable(gpi_gui WIN32 apps/gpi_gui.cpp)
    target_link_libraries(gpi_gui PRIVATE genomic_core comctl32 comdlg32 shell32 ole32)
    target_compile_definitions(gpi_gui PRIVATE GDI_ENGINE_VERSION="${PROJECT_VERSION}")
    if(MINGW)
        target_link_options(gpi_gui PRIVATE -municode)
    endif()
    # Preserve the existing gpi_gui.exe entry point and provide the new GDI name (tolerate if gdi_gui.exe is currently open).
    add_custom_command(TARGET gpi_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -D_SRC=$<TARGET_FILE:gpi_gui> -D_DST=$<TARGET_FILE_DIR:gpi_gui>/gdi_gui.exe
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_gui_optional.cmake"
        VERBATIM)
endif()

add_executable(gpi_sandbox apps/gpi_sandbox.cpp)
if(WIN32)
    target_link_libraries(gpi_sandbox PRIVATE advapi32 ole32 userenv)
endif()

add_executable(test_gpi_v03 tests/test_gpi_v03.cpp)
target_link_libraries(test_gpi_v03 PRIVATE genomic_core)

add_executable(test_behavioral_induction tests/test_behavioral_induction.cpp)
target_link_libraries(test_behavioral_induction PRIVATE genomic_core)

add_executable(test_generative_project tests/test_generative_project.cpp)
target_link_libraries(test_generative_project PRIVATE genomic_core)

add_executable(test_r15_r16 tests/test_r15_r16.cpp)
target_link_libraries(test_r15_r16 PRIVATE genomic_core)

add_executable(test_r17_asei tests/test_r17_asei.cpp)
target_link_libraries(test_r17_asei PRIVATE genomic_core)

add_executable(test_r18_behavioral tests/test_r18_behavioral.cpp)
target_link_libraries(test_r18_behavioral PRIVATE genomic_core)

add_executable(test_r19_semantic tests/test_r19_semantic.cpp)
target_link_libraries(test_r19_semantic PRIVATE genomic_core)

add_executable(test_adaptive_experimentation tests/test_adaptive_experimentation.cpp)
target_link_libraries(test_adaptive_experimentation PRIVATE genomic_core)

add_executable(test_inductive_synthesis tests/test_inductive_synthesis.cpp)
target_link_libraries(test_inductive_synthesis PRIVATE genomic_core)

enable_testing()

option(GDI_ENABLE_KR_E2E_TEST "Enable the external KR/Go/Rust/SDL project end-to-end test" OFF)
set(GDI_KR_E2E_PROJECT "" CACHE PATH "Protected KR project used by the opt-in end-to-end test")
set(GDI_KR_JAVA "" CACHE FILEPATH "Java executable bundled with the KR compiler")
set(GDI_KR_COMPILER_JAR "" CACHE FILEPATH "KR Compiler IDE jar")
set(GDI_KR_RUNTIME_READY_MARKER "" CACHE STRING "Runtime log marker required by the KR end-to-end test")
if(WIN32)
    add_test(NAME GDIBehavioralInductionEndToEnd
        COMMAND test_behavioral_induction "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_BINARY_DIR}/gdi_behavioral_e2e"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples" "$<TARGET_FILE:gpi_sandbox>")
else()
    # The generated source comes only from a bounded genome-authorized expression grammar.
    # No arbitrary prompt source is executed by this regression test.
    add_test(NAME GDIBehavioralInductionEndToEnd
        COMMAND test_behavioral_induction "${CMAKE_CXX_COMPILER}"
            "${CMAKE_CURRENT_BINARY_DIR}/gdi_behavioral_e2e"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples")
endif()

add_test(NAME GPIv03UnitTests COMMAND test_gpi_v03)

add_test(NAME GPIProjectEndToEnd
    COMMAND ${CMAKE_COMMAND}
        "-DGPI_PROJECT=$<TARGET_FILE:gpi_project>"
        "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gpi_project_e2e"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpi_project_end_to_end.cmake")
add_test(NAME ComprehensiveSection43Tests COMMAND test_genomic_system)
add_test(NAME GDIR15R16UnitTests COMMAND test_r15_r16
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_r15_r16_unit")
add_test(NAME GDIR17AseiTests COMMAND test_r17_asei
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_r17_asei")
add_test(NAME GDIR18BehavioralEvolutionTests COMMAND test_r18_behavioral
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_r18_behavioral" "${CMAKE_COMMAND}")
add_test(NAME GDIR19SemanticIntegrationTests COMMAND test_r19_semantic
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_r19_semantic" "${CMAKE_COMMAND}")
add_test(NAME GDIAdaptiveExperimentationTests COMMAND test_adaptive_experimentation
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_adaptive_experimentation" "${CMAKE_COMMAND}")
add_test(NAME GDIInductiveSynthesisTests COMMAND test_inductive_synthesis
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gdi_inductive_synthesis" "${CMAKE_COMMAND}")

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

if(GDI_ENABLE_KR_E2E_TEST)
    if(NOT EXISTS "${GDI_KR_E2E_PROJECT}" OR NOT EXISTS "${GDI_KR_JAVA}" OR NOT EXISTS "${GDI_KR_COMPILER_JAR}")
        message(FATAL_ERROR "GDI_ENABLE_KR_E2E_TEST requires GDI_KR_E2E_PROJECT, GDI_KR_JAVA, and GDI_KR_COMPILER_JAR")
    endif()
    add_test(NAME GDIKrProjectEndToEnd
        COMMAND ${CMAKE_COMMAND}
            "-DGDI_INTELLIGENCE=$<TARGET_FILE:gdi_intelligence>"
            "-DKR_PROJECT=${GDI_KR_E2E_PROJECT}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/gdi_kr_project_e2e"
            "-DCPP_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DKR_JAVA=${GDI_KR_JAVA}"
            "-DKR_COMPILER_JAR=${GDI_KR_COMPILER_JAR}"
            "-DRUNTIME_READY_MARKER=${GDI_KR_RUNTIME_READY_MARKER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gdi_kr_end_to_end.cmake")
    set_tests_properties(GDIKrProjectEndToEnd PROPERTIES TIMEOUT 900)
endif()
