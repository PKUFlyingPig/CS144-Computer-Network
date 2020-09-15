find_package (Doxygen)
if (DOXYGEN_FOUND)
    if (Doxygen_dot_FOUND)
        set (DOXYGEN_DOT_FOUND YES)
    else (NOT Doxygen_dot_FOUND)
        set (DOXYGEN_DOT_FOUND NO)
    endif (Doxygen_dot_FOUND)
    configure_file ("${PROJECT_SOURCE_DIR}/etc/Doxyfile.in" "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
    add_custom_target (doc "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
                           WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
                           COMMENT "Generate docs using Doxygen" VERBATIM)
endif ()
