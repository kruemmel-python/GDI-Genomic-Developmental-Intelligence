execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_SRC}" "${_DST}"
    RESULT_VARIABLE _copy_res
    ERROR_QUIET
)
if(NOT _copy_res EQUAL 0)
    message(FATAL_ERROR "Failed to create verified gdi_gui executable alias")
endif()
