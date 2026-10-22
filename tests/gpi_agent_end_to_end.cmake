if(NOT DEFINED GPI_AGENT OR NOT DEFINED CPP_COMPILER OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing GPI agent test arguments")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(MEMORY "${OUTPUT_DIR}/experience.log")
file(REMOVE "${MEMORY}")
set(TASK "map=x*x+3*x-7; filter=y>10; reduce=sum; samples=4; emit=true")
execute_process(COMMAND "${GPI_AGENT}" --task "${TASK}"
    --out "${OUTPUT_DIR}/novel.cpp" --memory "${MEMORY}" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "BEHAVIOR_VERIFIED=YES" OR
   NOT output MATCHES "MOLECULAR_TASK_ACTIVATION=PASS")
    message(FATAL_ERROR "GPI new-expression E2E failed: ${output}\n${error}")
endif()
file(READ "${OUTPUT_DIR}/novel.cpp.smoke.out" values)
if(NOT values MATCHES "SUM=32.000" OR NOT values MATCHES "SELECTED=2")
    message(FATAL_ERROR "Unexpected result: ${values}")
endif()
execute_process(COMMAND "${GPI_AGENT}" --task "${TASK}"
    --out "${OUTPUT_DIR}/novel.cpp" --memory "${MEMORY}" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "PRIOR_EXPERIENCES=1")
    message(FATAL_ERROR "Persistent experience retrieval failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_AGENT}" --task
    "map=(x+2)*(x-1); filter=y>=0 && x<4; reduce=mean; samples=4"
    --out "${OUTPUT_DIR}/repair.cpp" --memory "${MEMORY}" --compiler "${CPP_COMPILER}" --repair-probe
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "ATTEMPTS=2" OR
   NOT output MATCHES "Repair: add required" OR NOT output MATCHES "BEHAVIOR_VERIFIED=YES")
    message(FATAL_ERROR "Bounded compiler-diagnostic repair failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_AGENT}" --task
    "map=(x+2)*(x-1); filter=y>=0 && x<4; reduce=mean; samples=4"
    --out "${OUTPUT_DIR}/repair.cpp" --memory "${MEMORY}" --compiler "${CPP_COMPILER}" --repair-probe
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "ATTEMPTS=1" OR
   NOT output MATCHES "Reused prior successful header repair")
    message(FATAL_ERROR "Experience did not adapt later repair decision: ${output}\n${error}")
endif()
file(READ "${OUTPUT_DIR}/repair.cpp.smoke.out" values)
if(NOT values MATCHES "MEAN=4.667")
    message(FATAL_ERROR "Unexpected repaired result: ${values}")
endif()
execute_process(COMMAND "${GPI_AGENT}" --task
    "map=system(x); reduce=sum" --out "${OUTPUT_DIR}/unsafe.cpp"
    --memory "${MEMORY}" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/unsafe.cpp")
    message(FATAL_ERROR "Unsupported function call was not blocked: ${output}\n${error}")
endif()
execute_process(COMMAND "${GPI_AGENT}" --task
    "map=x+1; filter=x; reduce=sum" --out "${OUTPUT_DIR}/bad_type.cpp"
    --memory "${MEMORY}" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/bad_type.cpp")
    message(FATAL_ERROR "Invalid predicate type was not blocked: ${output}\n${error}")
endif()
# Corrupt one byte of the stored append-only chain; the next run must fail closed.
file(READ "${MEMORY}" original)
string(REPLACE "GPI1|" "GPI2|" corrupted "${original}")
file(WRITE "${MEMORY}" "${corrupted}")
execute_process(COMMAND "${GPI_AGENT}" --task "${TASK}"
    --out "${OUTPUT_DIR}/after_corruption.cpp" --memory "${MEMORY}" --compiler "${CPP_COMPILER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/after_corruption.cpp")
    message(FATAL_ERROR "Corrupted experience chain was accepted: ${output}\n${error}")
endif()
message(STATUS "GPI agent E2E: novel expression, molecular activation, compile/run/oracle, repair, persistence, fail-closed PASS")
