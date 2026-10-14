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
    main_window.cpp
    main_window.hpp
    messages_panel.cpp
    messages_panel.hpp
    properties_panel.cpp
    properties_panel.hpp
    schematic_view.cpp
    schematic_view.hpp
    scope_widget.cpp
    scope_widget.hpp
    theme.cpp
    theme.hpp)
target_include_directories(simnodus_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(simnodus_ui PUBLIC Qt6::Widgets simnodus::sim PRIVATE simnodus::warnings)

add_executable(simnodus main.cpp)
set_target_properties(simnodus PROPERTIES WIN32_EXECUTABLE ON)
target_link_libraries(simnodus PRIVATE simnodus_ui simnodus::warnings)

# Layout of an installed (or packaged) application:
#   Windows: simnodus.exe, Qt runtime (windeployqt), ngspice/ with the engine
#   Linux:   bin/simnodus, share/simnodus/ngspice/spinit; system libngspice
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
    else()
        message(STATUS "ngspice runtime not staged; the install tree will not contain the engine")
    endif()
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/resources/package/README.txt"
        "${PROJECT_SOURCE_DIR}/resources/package/THIRD-PARTY-NOTICES.txt" DESTINATION .)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/package/licenses/" DESTINATION licenses)
    install(FILES "${PROJECT_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT" DESTINATION licenses RENAME nlohmann-json-MIT.txt)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION library)
    # MSVC C++ runtime next to the executable (app-local deployment).
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
else()
    install(TARGETS simnodus RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${PROJECT_SOURCE_DIR}/resources/ngspice/spinit"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/ngspice")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/library/" DESTINATION "${CMAKE_INSTALL_DATADIR}/simnodus/library")
endif()

if(BUILD_TESTING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/tests/gui" "${CMAKE_BINARY_DIR}/tests/gui")
endif()
