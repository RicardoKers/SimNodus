# Command-line automation (SN-215, ADR 0104): the JSON commands of
# src/automation without a window, for AI agents, scripts and CI. Qt-free.
add_executable(simnodus-cli main.cpp)
target_link_libraries(simnodus-cli PRIVATE simnodus::automation simnodus::warnings)
target_compile_definitions(simnodus-cli PRIVATE
    SIMNODUS_CLI_VERSION="${PROJECT_VERSION}$<$<BOOL:${SIMNODUS_VERSION_SUFFIX}>:-${SIMNODUS_VERSION_SUFFIX}>")
# Next to simnodus(.exe) in the build tree, so it finds ngspice and the library like the application.
set_target_properties(simnodus-cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/simnodus")

if(BUILD_TESTING)
    # The command line itself: a project check, and a run when ngspice is known.
    add_test(NAME cli-check COMMAND simnodus-cli check "${PROJECT_SOURCE_DIR}/examples/01_LED_and_push_button.snx")
    set_tests_properties(cli-check PROPERTIES PASS_REGULAR_EXPRESSION "\"errors\":0")
    if(SIMNODUS_NGSPICE_LIBRARY)
        add_test(NAME cli-run COMMAND simnodus-cli --ngspice "${SIMNODUS_NGSPICE_LIBRARY}"
            --ngspice-scripts "${PROJECT_SOURCE_DIR}/resources/ngspice"
            run "${PROJECT_SOURCE_DIR}/examples/01_LED_and_push_button.snx" --time 20m --event "10m SW1 1")
        set_tests_properties(cli-run PROPERTIES PASS_REGULAR_EXPRESSION "\"applied_at\"" TIMEOUT 120)
    endif()
endif()
