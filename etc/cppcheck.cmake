if (NOT CPPCHECK)
    if (DEFINED ENV{CPPCHECK})
        set (CPPCHECK_TMP $ENV{CPPCHECK})
    else (NOT DEFINED ENV{CPPCHECK})
        set (CPPCHECK_TMP cppcheck)
    endif ()

    # is cppcheck available?
    execute_process (COMMAND ${CPPCHECK_TMP} --version RESULT_VARIABLE CPPCHECK_RESULT OUTPUT_VARIABLE CPPCHECK_OUTPUT)
    if (${CPPCHECK_RESULT} EQUAL 0)
        message (STATUS "Found cppcheck")
        set (CPPCHECK ${CPPCHECK_TMP} CACHE STRING "cppcheck executable name")
    endif()
endif (NOT CPPCHECK)

if (DEFINED CPPCHECK)
    add_custom_target (cppcheck ${CPPCHECK} --enable=all --project="${PROJECT_BINARY_DIR}/compile_commands.json")
endif (DEFINED CPPCHECK)
