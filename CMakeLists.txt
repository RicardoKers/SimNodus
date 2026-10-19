cmake_minimum_required(VERSION 3.24)

project(SimNodus
    VERSION 0.4.0
    DESCRIPTION "Interactive circuit and STM32 simulator for teaching"
    HOMEPAGE_URL "https://github.com/RicardoKers/SimNodus"
    LANGUAGES CXX)

# Pre-release label appended to the version shown by the application and the
# package name. Empty for a release build.
set(SIMNODUS_VERSION_SUFFIX "dev" CACHE STRING "Pre-release label, for example dev or rc1")

option(SIMNODUS_BUILD_APP "Build the Qt desktop application" ON)
option(SIMNODUS_BUILD_SPIKES "Build the engine spike programs (SN-103, SN-104)" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings as errors for project code; third-party headers are included as system headers.
add_library(simnodus_warnings INTERFACE)
add_library(simnodus::warnings ALIAS simnodus_warnings)
if(MSVC)
    target_compile_options(simnodus_warnings INTERFACE /W4 /WX /permissive- /utf-8 /EHsc)
    target_compile_definitions(simnodus_warnings INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
else()
    target_compile_options(simnodus_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_custom_target(check-repository
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_repository.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking repository documentation and structure"
    VERBATIM)

include(CTest)
include(GNUInstallDirs)

# Optional ngspice shared library used by engine tests and spikes. Never
# required to build; tests that need it are skipped when it is not found.
set(SIMNODUS_NGSPICE_LIBRARY "" CACHE FILEPATH
    "ngspice shared library for engine tests (ngspice.dll or libngspice.so)")
if(NOT SIMNODUS_NGSPICE_LIBRARY)
    if(WIN32)
        set(_simnodus_ngspice_candidate "${CMAKE_CURRENT_SOURCE_DIR}/build/deps/ngspice-runtime/ngspice.dll")
        if(EXISTS "${_simnodus_ngspice_candidate}")
            set(SIMNODUS_NGSPICE_LIBRARY "${_simnodus_ngspice_candidate}" CACHE FILEPATH "" FORCE)
        endif()
    else()
        find_library(_simnodus_ngspice_found NAMES ngspice libngspice.so.0)
        if(_simnodus_ngspice_found)
            set(SIMNODUS_NGSPICE_LIBRARY "${_simnodus_ngspice_found}" CACHE FILEPATH "" FORCE)
        endif()
    endif()
endif()
message(STATUS "ngspice for engine tests: ${SIMNODUS_NGSPICE_LIBRARY}")

add_subdirectory(src)
add_subdirectory(apps/simnodus-emu)

if(BUILD_TESTING)
    add_subdirectory(tests/unit)
endif()

if(SIMNODUS_BUILD_SPIKES)
    add_subdirectory(tests/spikes)
endif()

if(SIMNODUS_BUILD_APP)
    add_subdirectory(apps/simnodus)
endif()
