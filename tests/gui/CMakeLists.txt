find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(ui_tests ui_tests.cpp)
set_target_properties(ui_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ui_tests PRIVATE simnodus_ui Qt6::Test simnodus::warnings)
target_compile_definitions(ui_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library")
add_test(NAME gui-smoke COMMAND ui_tests)
# Runs go to the engine process (SN-151), which the tests find by this variable.
set_tests_properties(gui-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SIMNODUS_ENGINE=$<TARGET_FILE:simnodus-engine>" TIMEOUT 120)
add_dependencies(ui_tests simnodus-engine)

add_executable(editor_tests editor_tests.cpp)
set_target_properties(editor_tests PROPERTIES AUTOMOC ON)
target_link_libraries(editor_tests PRIVATE simnodus_ui Qt6::Test simnodus::warnings)
if(SIMNODUS_NGSPICE_LIBRARY)
    target_compile_definitions(editor_tests PRIVATE
        SIMNODUS_TEST_NGSPICE_LIBRARY="${SIMNODUS_NGSPICE_LIBRARY}"
        SIMNODUS_TEST_NGSPICE_SCRIPTS="${PROJECT_SOURCE_DIR}/resources/ngspice")
endif()
target_compile_definitions(editor_tests PRIVATE SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
if(SIMNODUS_TRANSLATIONS_DIR)
    target_compile_definitions(editor_tests PRIVATE SIMNODUS_TEST_TRANSLATIONS_DIR="${SIMNODUS_TRANSLATIONS_DIR}")
    add_dependencies(editor_tests simnodus) # the .qm files are built with the application
endif()
add_test(NAME gui-editor COMMAND editor_tests)
set_tests_properties(gui-editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SIMNODUS_ENGINE=$<TARGET_FILE:simnodus-engine>" TIMEOUT 900)
add_dependencies(editor_tests simnodus-engine)

# The real executable's built-in checks, offscreen.
add_test(NAME gui-app-smoke
    COMMAND simnodus --smoke-test -platform offscreen --settings "${CMAKE_CURRENT_BINARY_DIR}/app-smoke.ini")
set_tests_properties(gui-app-smoke PROPERTIES TIMEOUT 60)

if(WIN32)
    # Run against the Qt kit's DLLs without deploying them into the build tree.
    get_filename_component(_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
    set_tests_properties(gui-smoke gui-editor gui-app-smoke PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()
