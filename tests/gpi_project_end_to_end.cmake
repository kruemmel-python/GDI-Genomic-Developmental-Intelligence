if(NOT DEFINED GPI_PROJECT OR NOT DEFINED CPP_COMPILER OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing GPI v0.3 test arguments")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(MEMORY "${OUTPUT_DIR}/experience.log")
execute_process(COMMAND "${GPI_PROJECT}" --natural "Berechne die Summe der Quadrate der ersten 4 Zahlen"
    --project-dir "${OUTPUT_DIR}/de" --compiler "${CPP_COMPILER}" --memory "${MEMORY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "RESULT=PASS" OR
   NOT output MATCHES "BEHAVIOR_VERIFIED=YES" OR NOT output MATCHES "MOLECULAR_TASK_ACTIVATION=PASS")
    message(FATAL_ERROR "German NL multi-file project failed: ${output}\n${error}")
endif()
if(NOT EXISTS "${OUTPUT_DIR}/de/src/main.cpp" OR NOT EXISTS "${OUTPUT_DIR}/de/src/pipeline.cpp" OR
   NOT EXISTS "${OUTPUT_DIR}/de/include/gpi_generated/pipeline.hpp" OR NOT EXISTS "${OUTPUT_DIR}/de/CMakeLists.txt")
    message(FATAL_ERROR "Generated multi-file project incomplete")
endif()
if(NOT EXISTS "${OUTPUT_DIR}/de/verification/report.json")
    message(FATAL_ERROR "Provenance report missing")
endif()
file(READ "${OUTPUT_DIR}/de/verification/report.json" report)
if(NOT report MATCHES "independent_oracle_fixtures.*2" OR NOT report MATCHES "behavior_verified.*true")
    message(FATAL_ERROR "Provenance report does not describe verified execution")
endif()
file(READ "${OUTPUT_DIR}/de/verification/output.txt" values)
if(NOT values MATCHES "SUM=30.000")
    message(FATAL_ERROR "Wrong generated behavior: ${values}")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --natural "compute the average of the first 5 squares"
    --project-dir "${OUTPUT_DIR}/en" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "English NL multi-file project failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --task "map=(x+2)*(x-1); filter=y>=0 && x<4; reduce=mean; samples=4"
    --project-dir "${OUTPUT_DIR}/repair" --compiler "${CPP_COMPILER}" --memory "${MEMORY}" --repair-probe
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "ATTEMPTS=2" OR
   NOT output MATCHES "Repair: add <cmath>" OR NOT output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "Project compiler repair failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --task "map=(x+2)*(x-1); filter=y>=0 && x<4; reduce=mean; samples=4"
    --project-dir "${OUTPUT_DIR}/repair_reused" --compiler "${CPP_COMPILER}" --memory "${MEMORY}" --repair-probe
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "ATTEMPTS=1")
    message(FATAL_ERROR "Learned project repair failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --natural
    "Berechne die Summe der ersten 4 Zahlen mit Formel x*x+3*x mit Filter y>10"
    --project-dir "${OUTPUT_DIR}/novel_formula" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "RESULT=PASS")
    message(FATAL_ERROR "Novel natural-language formula failed: ${output}\n${error}")
endif()
file(READ "${OUTPUT_DIR}/novel_formula/verification/output.txt" values)
if(NOT values MATCHES "SUM=46.000")
    message(FATAL_ERROR "Novel formula output mismatch: ${values}")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --natural "Create a full operating system"
    --project-dir "${OUTPUT_DIR}/blocked" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/blocked")
    message(FATAL_ERROR "Unsupported NL request was not blocked")
endif()
execute_process(COMMAND "${GPI_PROJECT}" --task "map=system(x); reduce=sum; samples=4"
    --project-dir "${OUTPUT_DIR}/unsafe" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/unsafe")
    message(FATAL_ERROR "Unsafe expression was not blocked")
endif()
message(STATUS "GPI v0.3: German/English NL, multi-file generation, compile/test/dual-oracle, repair, persistence, fail-closed PASS")
