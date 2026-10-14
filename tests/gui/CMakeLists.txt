find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(ui_tests ui_tests.cpp)
set_target_properties(ui_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ui_tests PRIVATE simnodus_ui Qt6::Test simnodus::warnings)
add_test(NAME gui-smoke COMMAND ui_tests)
set_tests_properties(gui-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)

# The real executable's built-in checks, offscreen.
add_test(NAME gui-app-smoke
    COMMAND simnodus --smoke-test -platform offscreen --settings "${CMAKE_CURRENT_BINARY_DIR}/app-smoke.ini")
set_tests_properties(gui-app-smoke PROPERTIES TIMEOUT 60)

if(WIN32)
    # Run against the Qt kit's DLLs without deploying them into the build tree.
    get_filename_component(_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
    set_tests_properties(gui-smoke gui-app-smoke PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()
