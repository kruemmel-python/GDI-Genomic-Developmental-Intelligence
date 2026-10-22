if(NOT DEFINED GPI_RESEARCH OR NOT DEFINED CPP_COMPILER OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing GPI research test arguments")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Bestimme den Median eines unbekannt langen Zahlenstroms"
    --workspace "${OUTPUT_DIR}/median" --compiler "${CPP_COMPILER}" --cycles 3
    --memory "${OUTPUT_DIR}/research.memory"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "CYCLES=2" OR
   NOT output MATCHES "SELECTED_STRATEGY=two_heaps[+]streaming_balance" OR
   NOT output MATCHES "BEHAVIOR_VERIFIED=YES" OR NOT output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "Research improvement cycle failed: ${output}\n${error}")
endif()
if(NOT EXISTS "${OUTPUT_DIR}/median/solution.cpp" OR
   NOT EXISTS "${OUTPUT_DIR}/median/research_report.txt" OR
   NOT EXISTS "${OUTPUT_DIR}/median/research_provenance.json")
    message(FATAL_ERROR "Research artifacts are incomplete")
endif()
file(READ "${OUTPUT_DIR}/median/research_provenance.json" provenance)
if(NOT provenance MATCHES "GPI-research-v1.0" OR
   NOT provenance MATCHES "behavior_verified.*true" OR NOT provenance MATCHES "genome_hash" OR
   NOT provenance MATCHES "total_compiled_candidates" OR NOT provenance MATCHES "total_decision_errors")
    message(FATAL_ERROR "Genomic research provenance is incomplete: ${provenance}")
endif()
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Berechne erneut den Median eines Zahlenstroms"
    --workspace "${OUTPUT_DIR}/median_control" --compiler "${CPP_COMPILER}" --cycles 1
    --memory "${OUTPUT_DIR}/research.memory" --development-state control
    RESULT_VARIABLE control_result OUTPUT_VARIABLE control_output ERROR_VARIABLE control_error)
if(NOT control_result EQUAL 0 OR NOT control_output MATCHES "SELECTED_STRATEGY=vector[+]full_sort" OR
   control_output MATCHES "Reused verified strategy from research memory")
    message(FATAL_ERROR "CONTROL research condition failed: ${control_output}\n${control_error}")
endif()
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Berechne erneut den Median eines Zahlenstroms"
    --workspace "${OUTPUT_DIR}/median_learned" --compiler "${CPP_COMPILER}" --cycles 1
    --memory "${OUTPUT_DIR}/research.memory" --development-state developed
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "CYCLES=1" OR
   NOT output MATCHES "SELECTED_STRATEGY=two_heaps[+]streaming_balance" OR
   NOT output MATCHES "Reused verified strategy from research memory")
    message(FATAL_ERROR "Persistent research learning failed: ${output}\n${error}")
endif()
string(REGEX MATCH "GENOME_HASH=[0-9]+" control_hash "${control_output}")
string(REGEX MATCH "GENOME_HASH=[0-9]+" developed_hash "${output}")
if(NOT control_hash STREQUAL developed_hash)
    message(FATAL_ERROR "CONTROL and DEVELOPED did not use the same immutable genome")
endif()
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne den Median"
    --workspace "${OUTPUT_DIR}/composite" --compiler "${CPP_COMPILER}" --cycles 3
    --development-state explore
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "RESULT=PASS" OR
   NOT output MATCHES "SELECTED_STRATEGY=hash[+]component_pipeline" OR
   NOT output MATCHES "GRAPH_NODE=operation_1 OPERATION=filter_gt" OR
   NOT output MATCHES "REGULATION: STRATEGY=median" OR
   NOT output MATCHES "BEHAVIOR_VERIFIED=YES")
    message(FATAL_ERROR "Generative composite development failed: ${output}\n${error}")
endif()
file(READ "${OUTPUT_DIR}/composite/cycle_2.runtime.out" composite_output)
if(NOT composite_output MATCHES "MEDIAN=4.000")
    message(FATAL_ERROR "Novel component composition produced wrong behavior: ${composite_output}")
endif()
# v0.7: learn an abstract component path, then transfer it to a related graph
# with a different terminal operation. CONTROL receives the same immutable
# genome but cannot use developmental experience.
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne den Median"
    --workspace "${OUTPUT_DIR}/transfer_training" --compiler "${CPP_COMPILER}" --cycles 2
    --memory "${OUTPUT_DIR}/transfer.memory" --development-state explore
    RESULT_VARIABLE training_result OUTPUT_VARIABLE training_output ERROR_VARIABLE training_error)
if(NOT training_result EQUAL 0 OR NOT training_output MATCHES "SELECTED_STRATEGY=hash[+]component_pipeline" OR
   NOT training_output MATCHES "DEVELOPED_COMPONENT=component_[0-9a-f]+" OR
   NOT EXISTS "${OUTPUT_DIR}/transfer.memory.components")
    message(FATAL_ERROR "Developmental transfer training failed: ${training_output}\n${training_error}")
endif()
set(transfer_prompt "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne die Summe")
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "${transfer_prompt}" --workspace "${OUTPUT_DIR}/transfer_control"
    --compiler "${CPP_COMPILER}" --cycles 1 --memory "${OUTPUT_DIR}/transfer.memory"
    --development-state control
    RESULT_VARIABLE transfer_control_result OUTPUT_VARIABLE transfer_control_output ERROR_VARIABLE transfer_control_error)
if(NOT transfer_control_result EQUAL 0 OR
   NOT transfer_control_output MATCHES "SELECTED_STRATEGY=vector[+]component_pipeline" OR
   NOT transfer_control_output MATCHES "SCORE=140" OR
   transfer_control_output MATCHES "REUSED_COMPONENT=component_" OR
   transfer_control_output MATCHES "Transferred developmental experience")
    message(FATAL_ERROR "v0.7 CONTROL transfer condition failed: ${transfer_control_output}\n${transfer_control_error}")
endif()
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "${transfer_prompt}" --workspace "${OUTPUT_DIR}/transfer_developed"
    --compiler "${CPP_COMPILER}" --cycles 1 --memory "${OUTPUT_DIR}/transfer.memory"
    --development-state developed
    RESULT_VARIABLE transfer_result OUTPUT_VARIABLE transfer_output ERROR_VARIABLE transfer_error)
if(NOT transfer_result EQUAL 0 OR
   NOT transfer_output MATCHES "SELECTED_STRATEGY=hash[+]component_pipeline" OR
   NOT transfer_output MATCHES "SCORE=85" OR
   NOT transfer_output MATCHES "EXPERIENCE_TRANSFERRED=YES" OR
   NOT transfer_output MATCHES "EXPERIENCE_SIMILARITY=0.6" OR
   NOT transfer_output MATCHES "REUSED_COMPONENT=component_[0-9a-f]+" OR
   NOT transfer_output MATCHES "Transferred developmental experience")
    message(FATAL_ERROR "v0.7 DEVELOPED transfer condition failed: ${transfer_output}\n${transfer_error}")
endif()
string(REGEX MATCH "GENOME_HASH=[0-9]+" transfer_control_hash "${transfer_control_output}")
string(REGEX MATCH "GENOME_HASH=[0-9]+" transfer_developed_hash "${transfer_output}")
if(NOT transfer_control_hash STREQUAL transfer_developed_hash)
    message(FATAL_ERROR "v0.7 CONTROL and DEVELOPED did not use the same immutable genome")
endif()
file(READ "${OUTPUT_DIR}/transfer_developed/cycle_1.runtime.out" transfer_runtime)
file(READ "${OUTPUT_DIR}/transfer_developed/research_provenance.json" transfer_provenance)
file(READ "${OUTPUT_DIR}/transfer_developed/solution.cpp" transfer_source)
if(NOT transfer_runtime MATCHES "SUM=8.000" OR
   NOT transfer_provenance MATCHES "experience_transferred.*true" OR
   NOT transfer_provenance MATCHES "filter_gt,deduplicate,sort_ascending,median" OR
   NOT transfer_provenance MATCHES "reused_component.*component_" OR
   NOT transfer_source MATCHES "developed_component.hpp" OR
   NOT EXISTS "${OUTPUT_DIR}/transfer_developed/developed_component.hpp")
    message(FATAL_ERROR "v0.7 transferred behavior/provenance is incomplete: ${transfer_runtime}\n${transfer_provenance}")
endif()
# v0.9: starting with empty memory, the user supplies only the final goal.
# GPI must detect the missing component, derive and verify an intermediate
# experiment, and then solve the original target within a bounded campaign.
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "${transfer_prompt}" --workspace "${OUTPUT_DIR}/autonomous"
    --compiler "${CPP_COMPILER}" --cycles 1 --memory "${OUTPUT_DIR}/autonomous.memory"
    --autonomous-cycles 3
    RESULT_VARIABLE autonomous_result OUTPUT_VARIABLE autonomous_output ERROR_VARIABLE autonomous_error)
if(NOT autonomous_result EQUAL 0 OR
   NOT autonomous_output MATCHES "RESEARCH_MODE=AUTONOMOUS_SAFE" OR
   NOT autonomous_output MATCHES "AUTONOMOUS_MODE=YES" OR
   NOT autonomous_output MATCHES "AUTONOMOUS_CYCLES=3" OR
   NOT autonomous_output MATCHES "CAPABILITY_DIAGNOSIS=SOLVABLE_OPTIMIZATION_EVIDENCE" OR
   NOT autonomous_output MATCHES "BASELINE_VERIFIED=YES" OR
   NOT autonomous_output MATCHES "DERIVED_GOAL=.*median oracle" OR
   NOT autonomous_output MATCHES "REUSED_COMPONENT=component_[0-9a-f]+" OR
   NOT autonomous_output MATCHES "BEHAVIOR_VERIFIED=YES" OR
   NOT autonomous_output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "v0.9 autonomous development failed: ${autonomous_output}\n${autonomous_error}")
endif()
if(NOT EXISTS "${OUTPUT_DIR}/autonomous/experiment_1/research_provenance.json" OR
   NOT EXISTS "${OUTPUT_DIR}/autonomous/diagnosis_probe/research_provenance.json" OR
   NOT EXISTS "${OUTPUT_DIR}/autonomous/target/developed_component.hpp" OR
   NOT EXISTS "${OUTPUT_DIR}/autonomous/autonomous_research_report.txt" OR
   NOT EXISTS "${OUTPUT_DIR}/autonomous/autonomous_provenance.json")
    message(FATAL_ERROR "v0.9 autonomous campaign artifacts are incomplete")
endif()
file(READ "${OUTPUT_DIR}/autonomous/target/cycle_1.runtime.out" autonomous_runtime)
file(READ "${OUTPUT_DIR}/autonomous/target/solution.cpp" autonomous_source)
file(READ "${OUTPUT_DIR}/autonomous/autonomous_provenance.json" autonomous_provenance)
if(NOT autonomous_runtime MATCHES "SUM=8.000" OR
   NOT autonomous_source MATCHES "GpiDeveloped_component_" OR
   NOT autonomous_provenance MATCHES "GPI-autonomous-v1.0" OR
   NOT autonomous_provenance MATCHES "behavior_verified.*true" OR
   NOT autonomous_provenance MATCHES "capability_diagnosis.*SOLVABLE_OPTIMIZATION_EVIDENCE" OR
   NOT autonomous_provenance MATCHES "derived_goals")
    message(FATAL_ERROR "v0.9 autonomous evidence is incomplete: ${autonomous_runtime}\n${autonomous_provenance}")
endif()
# A registered component is executable code, so reuse must fail closed if its
# materialized artifact no longer matches the bounded declarative component.
file(GLOB developed_headers "${OUTPUT_DIR}/transfer.memory.components/component_*.hpp")
file(GLOB developed_provenance "${OUTPUT_DIR}/transfer.memory.components/component_*.provenance")
list(LENGTH developed_headers developed_header_count)
list(LENGTH developed_provenance developed_provenance_count)
if(NOT developed_header_count EQUAL 1 OR NOT developed_provenance_count EQUAL 1)
    message(FATAL_ERROR "v0.8 expected exactly one developed component artifact")
endif()
list(GET developed_provenance 0 developed_provenance_file)
file(READ "${developed_provenance_file}" developed_provenance_text)
if(NOT developed_provenance_text MATCHES "ORIGIN_CONTRACT_HASH=[0-9]+" OR
   NOT developed_provenance_text MATCHES "STRUCTURE_RATIONALE=maximal typed dataflow cohesion" OR
   NOT developed_provenance_text MATCHES "ACCESS_RULE=same_genome" OR
   NOT developed_provenance_text MATCHES "ACCESS_RULE=typed_contiguous_graph" OR
   NOT developed_provenance_text MATCHES "SEMANTIC_CHECKS=[1-9]")
    message(FATAL_ERROR "Developed component origin or access rules are incomplete: ${developed_provenance_text}")
endif()
list(GET developed_headers 0 developed_header)
file(APPEND "${developed_header}" "\n// tampered\n")
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "${transfer_prompt}" --workspace "${OUTPUT_DIR}/transfer_tampered"
    --compiler "${CPP_COMPILER}" --cycles 1 --memory "${OUTPUT_DIR}/transfer.memory"
    --development-state developed
    RESULT_VARIABLE tampered_result OUTPUT_VARIABLE tampered_output ERROR_VARIABLE tampered_error)
if(tampered_result EQUAL 0 OR NOT tampered_error MATCHES "integrity verification")
    message(FATAL_ERROR "Tampered v0.8 component did not fail closed: ${tampered_output}\n${tampered_error}")
endif()
execute_process(COMMAND "${GPI_RESEARCH}"
    --prompt "Entwickle einen Quantenbrowser" --workspace "${OUTPUT_DIR}/unsupported"
    --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT output MATCHES "RESULT=BLOCKED" OR EXISTS "${OUTPUT_DIR}/unsupported")
    message(FATAL_ERROR "Ungrounded research goal did not fail closed: ${output}\n${error}")
endif()
if(WIN32)
    file(WRITE "${OUTPUT_DIR}/unsafe_prompt.txt"
"sandbox experiment
```cpp
#include <iostream>
extern \"C\" int system(const char*);
int main(){ (void)system(\"cmd /c echo HARM>${OUTPUT_DIR}/outside.txt\"); std::cout << \"SANDBOX_OK\\n\"; }
```
")
    file(WRITE "${OUTPUT_DIR}/unsafe.in" "")
    file(WRITE "${OUTPUT_DIR}/unsafe.expected" "SANDBOX_OK\n")
    execute_process(COMMAND "${GPI_RESEARCH}"
        --prompt-file "${OUTPUT_DIR}/unsafe_prompt.txt" --workspace "${OUTPUT_DIR}/arbitrary"
        --compiler "${CPP_COMPILER}" --cycles 2 --research-unsafe
        --fixture-input "${OUTPUT_DIR}/unsafe.in" --fixture-expected "${OUTPUT_DIR}/unsafe.expected"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "BEHAVIOR_VERIFIED=YES" OR
       EXISTS "${OUTPUT_DIR}/outside.txt")
        message(FATAL_ERROR "Integrated AppContainer sandbox failed: ${output}\n${error}")
    endif()
endif()
message(STATUS "GPI research: decomposition, strategy selection, multi-cycle improvement and fail-closed sandbox gate PASS")
