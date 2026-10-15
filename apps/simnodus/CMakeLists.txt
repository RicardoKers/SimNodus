# SimNodus desktop application (Qt 6 Widgets, ADR 0081).
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

set(SIMNODUS_VERSION_FULL "${PROJECT_VERSION}")
if(SIMNODUS_VERSION_SUFFIX)
    string(APPEND SIMNODUS_VERSION_FULL "-${SIMNODUS_VERSION_SUFFIX}")
endif()
set(SIMNODUS_VERSION_FULL "${SIMNODUS_VERSION_FULL}" PARENT_SCOPE)
configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/simnodus/version.hpp" @ONLY)

# Everything except main() lives in a library so the GUI tests use the same code.
add_library(simnodus_ui STATIC
    analyzer_window.cpp
    analyzer_window.hpp
    app_settings.cpp
    app_settings.hpp
    component_panel.cpp
    component_panel.hpp
    document.cpp
    document.hpp
    main_window.cpp
    main_window.hpp
    messages_panel.cpp
    messages_panel.hpp
    properties_panel.cpp
    properties_panel.hpp
    run_controller.cpp
    run_controller.hpp
    schematic_scene.cpp
    schematic_scene.hpp
    schematic_view.cpp
    schematic_view.hpp
    scope_widget.cpp
    logic_widget.cpp
    logic_widget.hpp
    uart_terminal.cpp
    uart_terminal.hpp
    scope_widget.hpp
    settings_dialog.cpp
    settings_dialog.hpp
    symbol_painter.cpp
    symbol_painter.hpp
    theme.cpp
    theme.hpp)
target_include_directories(simnodus_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(simnodus_ui PUBLIC Qt6::Widgets simnodus::session simnodus::editing simnodus::io
    simnodus::netlist simnodus::mcu PRIVATE simnodus::warnings)
# Last-resort examples location for running from a build tree.
target_compile_definitions(simnodus_ui PRIVATE SIMNODUS_SOURCE_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")

add_executable(simnodus main.cpp)
set_target_properties(simnodus PROPERTIES WIN32_EXECUTABLE ON)
target_link_libraries(simnodus PRIVATE simnodus_ui simnodus::warnings)

# Layout of an installed (or packaged) application:
#   Windows: simnodus.exe, Qt runtime (windeployqt), ngspice/ with the engine,
#            renode/ (unmodified Renode 1.16.1 portable bundle) and mcu/
#   Linux:   bin/simnodus, share/simnodus/{ngspice,mcu}; system libngspice;
#            Renode found through SIMNODUS_RENODE
if(WIN32)
    install(TARGETS simnodus RUNTIME DESTINATION .)
    install(FILES "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit" DESTINATION ngspice)
    set(SIMNODUS_NGSPICE_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/build/deps/ngspice-runtime" CACHE PATH
        "Staged ngspice runtime to bundle (tools/fetch_backends.py)")
    if(EXISTS "${SIMNODUS_NGSPICE_RUNTIME_DIR}/ngspice.dll")
        install(FILES
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/ngspice.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/libomp140.x86_64.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/sndfile.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/samplerate.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/COPYING"
            DESTINATION ngspice)
        # Same layout next to the build-tree executable, for running during development.
        add_custom_command(TARGET simnodus POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SIMNODUS_NGSPICE_RUNTIME_DIR}" "$<TARGET_FILE_DIR:simnodus>/ngspice"
            COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit" "$<TARGET_FILE_DIR:simnodus>/ngspice/spinit"
            VERBATIM)
    else()
        message(STATUS "ngspice runtime not staged; the install tree will not contain the engine")
    endif()
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/resources/package/README.txt"
        "${PROJECT_SOURCE_DIR}/resources/package/THIRD-PARTY-NOTICES.txt" DESTINATION .)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/package/licenses/" DESTINATION licenses)
    install(FILES "${PROJECT_SOURCE_DIR}/docs/user/QUICK_START.md" DESTINATION . RENAME QUICK-START.md)
    install(FILES "${PROJECT_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT" DESTINATION licenses RENAME nlohmann-json-MIT.txt)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION library)
    # Example projects with their firmware (ELF, C source and linker script).
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION examples
        FILES_MATCHING PATTERN "*.snx" PATTERN "*.elf" PATTERN "*.c" PATTERN "*.ld" PATTERN "*.md")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/renode/" DESTINATION mcu)
    if(SIMNODUS_RENODE)
        # The whole portable bundle with its licenses/, without Renode's own test suite.
        get_filename_component(_simnodus_renode_bundle "${SIMNODUS_RENODE}" DIRECTORY)
        install(DIRECTORY "${_simnodus_renode_bundle}/" DESTINATION renode PATTERN "tests" EXCLUDE)
    else()
        message(STATUS "Renode not staged; the install tree will not contain the microcontroller engine")
    endif()
    # MSVC C++ runtime next to the executable (app-local deployment).
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
else()
    install(TARGETS simnodus RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/ngspice")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/library")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/examples"
        FILES_MATCHING PATTERN "*.snx" PATTERN "*.elf" PATTERN "*.c" PATTERN "*.ld")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/renode/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/mcu")
endif()

if(BUILD_TESTING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/tests/gui" "${CMAKE_BINARY_DIR}/tests/gui")
endif()
