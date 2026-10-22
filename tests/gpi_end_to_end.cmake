# Real black-box test: natural language -> frozen genome -> molecules ->
# generated standalone C++20 -> external compiler -> program execution.
if(NOT DEFINED GPI_EXECUTABLE OR NOT DEFINED CPP_COMPILER OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing GPI_EXECUTABLE, CPP_COMPILER or OUTPUT_DIR")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(SOURCE "${OUTPUT_DIR}/generated_statistics.cpp")
set(EXECUTABLE "${OUTPUT_DIR}/generated_statistics${EXE_SUFFIX}")
execute_process(
    COMMAND "${GPI_EXECUTABLE}" --prompt
        "Build a program to read 5 numbers and calculate the average and maximum"
        --out "${SOURCE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE trace ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Prompt resolution failed: ${trace}\n${diagnostic}")
endif()
if(CPP_COMPILER_ID STREQUAL "MSVC")
    set(FLAGS /std:c++20 /W4 /WX)
else()
    set(FLAGS -std=c++20 -Wall -Wextra -Wpedantic -Werror)
endif()
get_filename_component(CPP_COMPILER_DIR "${CPP_COMPILER}" DIRECTORY)
set(COMPILER_ENV_COMMAND "${CMAKE_COMMAND}" -E env --modify
    "PATH=path_list_prepend:${CPP_COMPILER_DIR}" --)
if(CPP_COMPILER_ID STREQUAL "MSVC")
    execute_process(COMMAND ${COMPILER_ENV_COMMAND} "${CPP_COMPILER}" ${FLAGS} "${SOURCE}" "/Fe:${EXECUTABLE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE compiler_output ERROR_VARIABLE compiler_error)
else()
    execute_process(COMMAND ${COMPILER_ENV_COMMAND} "${CPP_COMPILER}" ${FLAGS} "${SOURCE}" -o "${EXECUTABLE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE compiler_output ERROR_VARIABLE compiler_error)
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Generated source rejected by external compiler: ${compiler_output}\n${compiler_error}")
endif()
file(WRITE "${OUTPUT_DIR}/numbers.txt" "1\n2\n3\n4\n10\n")
execute_process(COMMAND "${EXECUTABLE}"
    INPUT_FILE "${OUTPUT_DIR}/numbers.txt"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0 OR NOT output MATCHES "MEAN=4\\.00" OR NOT output MATCHES "MAX=10")
    message(FATAL_ERROR "Wrong result from generated program: ${output}\n${diagnostic}")
endif()

set(SOURCE "${OUTPUT_DIR}/generated_sensor.cpp")
set(EXECUTABLE "${OUTPUT_DIR}/generated_sensor${EXE_SUFFIX}")
execute_process(
    COMMAND "${GPI_EXECUTABLE}" --prompt
        "Monitor 3 temperature sensors, store samples as CSV and alert above 80"
        --out "${SOURCE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE trace ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Sensor prompt resolution failed: ${trace}\n${diagnostic}")
endif()
if(CPP_COMPILER_ID STREQUAL "MSVC")
    execute_process(COMMAND ${COMPILER_ENV_COMMAND} "${CPP_COMPILER}" ${FLAGS} "${SOURCE}" "/Fe:${EXECUTABLE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE compiler_output ERROR_VARIABLE compiler_error)
else()
    execute_process(COMMAND ${COMPILER_ENV_COMMAND} "${CPP_COMPILER}" ${FLAGS} "${SOURCE}" -o "${EXECUTABLE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE compiler_output ERROR_VARIABLE compiler_error)
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Sensor source rejected by external compiler: ${compiler_output}\n${compiler_error}")
endif()
file(WRITE "${OUTPUT_DIR}/sensor_data.txt" "10\n81\n90\n")
execute_process(COMMAND "${EXECUTABLE}"
    INPUT_FILE "${OUTPUT_DIR}/sensor_data.txt"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0 OR NOT output MATCHES "ALARMS=2" OR NOT output MATCHES "SAMPLES=3")
    message(FATAL_ERROR "Wrong sensor results: ${output}\n${diagnostic}")
endif()
file(READ "${OUTPUT_DIR}/gpi_measurements.csv" csv)
if(NOT csv MATCHES "index,value" OR NOT csv MATCHES "3,90")
    message(FATAL_ERROR "Generated CSV is missing data: ${csv}")
endif()

execute_process(COMMAND "${GPI_EXECUTABLE}" --prompt
    "Create an HTTP server with 5 connections" --out "${OUTPUT_DIR}/unsupported.cpp"
    RESULT_VARIABLE result OUTPUT_VARIABLE trace ERROR_VARIABLE diagnostic)
if(result EQUAL 0 OR EXISTS "${OUTPUT_DIR}/unsupported.cpp")
    message(FATAL_ERROR "Unsupported task must be rejected without generating source")
endif()
message(STATUS "GPI E2E: statistics, sensor CSV/alarm, and unsupported-prompt rejection passed")
