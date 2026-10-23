foreach(required GDI_INTELLIGENCE KR_PROJECT OUTPUT_DIR CPP_COMPILER KR_JAVA KR_COMPILER_JAR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(
    COMMAND "${GDI_INTELLIGENCE}" --kr-e2e
        --foreign-project "${KR_PROJECT}"
        --workspace "${OUTPUT_DIR}"
        --compiler "${CPP_COMPILER}"
        --kr-java "${KR_JAVA}"
        --kr-compiler-jar "${KR_COMPILER_JAR}"
        --runtime-ready-marker "${RUNTIME_READY_MARKER}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 900)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "KR end-to-end command failed (${result})\n${output}\n${error}")
endif()
foreach(marker
        "KR_BUILD=PASS"
        "RUST_LINKED=YES"
        "GO_LINKED=YES"
        "SDL_LINKED=YES"
        "RUNTIME_READY=YES"
        "ABI_FUNCTIONAL_TESTS=PASS"
        "ORIGINAL_UNTOUCHED=YES"
        "RESULT=PASS")
    string(FIND "${output}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing KR end-to-end evidence marker '${marker}'\n${output}")
    endif()
endforeach()
