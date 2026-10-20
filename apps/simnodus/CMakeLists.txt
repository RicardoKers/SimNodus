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
    engine_process.cpp
    engine_process.hpp
    main_window.cpp
    main_window.hpp
    memory_window.cpp
    memory_window.hpp
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
    user_models.cpp
    user_models.hpp
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
target_link_libraries(simnodus_ui PUBLIC Qt6::Widgets simnodus::session simnodus::engine_link simnodus::editing simnodus::io
    simnodus::netlist simnodus::mcu PRIVATE simnodus::warnings)
# Last-resort examples location for running from a build tree.
target_compile_definitions(simnodus_ui PRIVATE SIMNODUS_SOURCE_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")

# Application icon: Qt resource for the windows, Windows resource for the .exe.
set(SIMNODUS_ICON_FILE "${PROJECT_SOURCE_DIR}/resources/icons/simnodus.ico")
set(_simnodus_icon_sources "${PROJECT_SOURCE_DIR}/resources/icons/icons.qrc")
if(WIN32)
    configure_file(simnodus.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/simnodus.rc" @ONLY)
    list(APPEND _simnodus_icon_sources "${CMAKE_CURRENT_BINARY_DIR}/generated/simnodus.rc")
endif()
add_executable(simnodus main.cpp ${_simnodus_icon_sources})
set_target_properties(simnodus PROPERTIES WIN32_EXECUTABLE ON AUTORCC ON)
target_link_libraries(simnodus PRIVATE simnodus_ui simnodus::warnings)
target_compile_definitions(simnodus PRIVATE SIMNODUS_SOURCE_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
# The engine process (SN-151) sits next to the application.
add_dependencies(simnodus simnodus-engine)

# Translations (UI-16): resources/translations/*.ts become .qm files in
# translations/ next to the executable. Without Qt's LinguistTools the
# application runs in English.
find_package(Qt6 6.4 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
    file(GLOB _simnodus_ts_files "${PROJECT_SOURCE_DIR}/resources/translations/*.ts")
    set_source_files_properties(${_simnodus_ts_files} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations")
    qt_add_lrelease(simnodus TS_FILES ${_simnodus_ts_files} QM_FILES_OUTPUT_VARIABLE SIMNODUS_QM_FILES)
    add_custom_command(TARGET simnodus POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:simnodus>/translations"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SIMNODUS_QM_FILES} "$<TARGET_FILE_DIR:simnodus>/translations"
        VERBATIM)
    set(SIMNODUS_TRANSLATIONS_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations" PARENT_SCOPE)
else()
    message(STATUS "Qt LinguistTools not found: the application is built without translations")
endif()

# Layout of an installed (or packaged) application:
#   Windows: simnodus.exe, Qt runtime (windeployqt), ngspice/ with the engine
#   Linux:   bin/simnodus, share/simnodus/ngspice; system libngspice
if(WIN32)
    install(TARGETS simnodus simnodus-engine RUNTIME DESTINATION .)
    if(SIMNODUS_QM_FILES)
        install(FILES ${SIMNODUS_QM_FILES} DESTINATION translations)
    endif()
    install(FILES "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit" DESTINATION ngspice)
    set(SIMNODUS_NGSPICE_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/build/deps/ngspice-runtime" CACHE PATH
        "Staged ngspice runtime to bundle (tools/fetch_backends.py)")
    if(EXISTS "${SIMNODUS_NGSPICE_RUNTIME_DIR}/ngspice.dll")
        install(FILES
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/ngspice.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/libomp140.x86_64.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/sndfile.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/samplerate.dll"
            "${SIMNODUS_NGSPICE_RUNTIME_DIR}/spice2poly.cm"
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
    install(FILES "${PROJECT_SOURCE_DIR}/docs/user/USER_GUIDE.md" DESTINATION . RENAME USER-GUIDE.md)
    install(FILES "${PROJECT_SOURCE_DIR}/docs/user/MICROCONTROLLER.md" DESTINATION .)
    install(FILES "${PROJECT_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT" DESTINATION licenses RENAME nlohmann-json-MIT.txt)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION library)
    # Example projects with their firmware (ELF, C source and linker script) and memory contents.
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION examples
        FILES_MATCHING PATTERN "*.snx" PATTERN "*.elf" PATTERN "*.c" PATTERN "*.ld" PATTERN "*.md" PATTERN "*.hex" PATTERN "*.bin")
    # MSVC C++ runtime next to the executable (app-local deployment).
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
else()
    install(TARGETS simnodus simnodus-engine RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(SIMNODUS_QM_FILES)
        install(FILES ${SIMNODUS_QM_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/translations")
    endif()
    install(FILES "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/ngspice")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/library")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/examples"
        FILES_MATCHING PATTERN "*.snx" PATTERN "*.elf" PATTERN "*.c" PATTERN "*.ld" PATTERN "*.hex" PATTERN "*.bin")
    install(FILES "${PROJECT_SOURCE_DIR}/resources/package/simnodus.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    foreach(_size 16 24 32 48 64 128 256 512)
        install(FILES "${PROJECT_SOURCE_DIR}/resources/icons/simnodus_${_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps" RENAME simnodus.png)
    endforeach()
endif()

if(BUILD_TESTING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/tests/gui" "${CMAKE_BINARY_DIR}/tests/gui")
endif()
