if(NOT DEFINED GPI_INTEGRATED OR NOT DEFINED CPP_COMPILER OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing integrated GPI test arguments")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
set(goal "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne die Summe")
execute_process(COMMAND "${GPI_INTEGRATED}"
    --prompt "${goal}" --workspace "${OUTPUT_DIR}/comparison"
    --compiler "${CPP_COMPILER}" --architecture compare --cycles 2 --autonomous-cycles 3
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "Integrated A-D comparison failed: ${output}\n${error}")
endif()
foreach(architecture SINGLE ISOLATED COMMUNICATING COMMUNICATING_EXPERIENCED)
    if(NOT output MATCHES "ARCHITECTURE=${architecture}")
        message(FATAL_ERROR "Missing architecture ${architecture}: ${output}")
    endif()
    if(NOT EXISTS "${OUTPUT_DIR}/comparison/${architecture}/integrated_report.txt")
        message(FATAL_ERROR "Missing integrated report for ${architecture}")
    endif()
endforeach()
if(NOT EXISTS "${OUTPUT_DIR}/comparison/architecture_comparison.csv")
    message(FATAL_ERROR "Missing combined A-D comparison metrics")
endif()
file(READ "${OUTPUT_DIR}/comparison/architecture_comparison.csv" comparison_csv)
if(NOT comparison_csv MATCHES "compiled_candidates,semantic_checks,compile_ms,execution_ms,decision_errors,successful_transfers" OR
   NOT comparison_csv MATCHES "COMMUNICATING_EXPERIENCED,[0-9]+,1,1,5,10,[0-9]+,[0-9]+,0,1,SOLVABLE_OPTIMIZATION_EVIDENCE")
    message(FATAL_ERROR "A-D comparison lacks measured evidence: ${comparison_csv}")
endif()
string(REGEX MATCHALL "GOAL_FULFILLED=YES" fulfilled "${output}")
list(LENGTH fulfilled fulfilled_count)
if(NOT fulfilled_count EQUAL 4)
    message(FATAL_ERROR "Not every A-D architecture fulfilled the goal: ${output}")
endif()
string(REGEX MATCHALL "GENOME_HASH=[0-9]+" hashes "${output}")
list(REMOVE_DUPLICATES hashes)
list(LENGTH hashes hash_count)
if(NOT hash_count EQUAL 1)
    message(FATAL_ERROR "A-D architectures did not share exactly one immutable genome: ${output}")
endif()
if(NOT output MATCHES "CAPABILITY_DIAGNOSIS=SOLVABLE_WITHOUT_CROSS_COMPARTMENT_EVIDENCE" OR
   NOT output MATCHES "CAPABILITY_DIAGNOSIS=SOLVABLE_OPTIMIZATION_EVIDENCE" OR
   NOT output MATCHES "SUCCESSFUL_TRANSFERS=1" OR
   NOT output MATCHES "DELIVERED=NO" OR NOT output MATCHES "DELIVERED=YES")
    message(FATAL_ERROR "Integrated specialization, communication or transfer evidence is incomplete: ${output}")
endif()
if(NOT EXISTS "${OUTPUT_DIR}/comparison/COMMUNICATING_EXPERIENCED/campaign/diagnosis_probe/goal_contract.expected" OR
   NOT EXISTS "${OUTPUT_DIR}/comparison/COMMUNICATING_EXPERIENCED/campaign/experiment_1/research_provenance.json" OR
   NOT EXISTS "${OUTPUT_DIR}/comparison/COMMUNICATING_EXPERIENCED/campaign/target/developed_component.hpp")
    message(FATAL_ERROR "Experienced multi-compartment campaign artifacts are incomplete")
endif()
file(READ "${OUTPUT_DIR}/comparison/COMMUNICATING_EXPERIENCED/campaign/autonomous_provenance.json" autonomous_provenance)
if(NOT autonomous_provenance MATCHES "total_compiled_candidates[^0-9]+4" OR
   NOT autonomous_provenance MATCHES "total_semantic_checks[^0-9]+8" OR
   NOT autonomous_provenance MATCHES "total_decision_errors[^0-9]+0")
    message(FATAL_ERROR "Autonomous evidence totals are incomplete: ${autonomous_provenance}")
endif()
execute_process(COMMAND "${GPI_INTEGRATED}"
    --prompt "Sortiere Zahlen absteigend und entferne Duplikate"
    --workspace "${OUTPUT_DIR}/ambiguous" --compiler "${CPP_COMPILER}" --architecture communicating
    RESULT_VARIABLE ambiguous_result OUTPUT_VARIABLE ambiguous_output ERROR_VARIABLE ambiguous_error)
if(ambiguous_result EQUAL 0 OR NOT ambiguous_output MATCHES "UNRESOLVED_REQUIREMENT=" OR
   NOT ambiguous_output MATCHES "RESULT=BLOCKED" OR EXISTS "${OUTPUT_DIR}/ambiguous")
    message(FATAL_ERROR "Unresolved original goal did not fail closed: ${ambiguous_output}\n${ambiguous_error}")
endif()
message(STATUS "Integrated goal contract, multi-compartment A-D comparison, evidence diagnosis and systemic reuse PASS")
