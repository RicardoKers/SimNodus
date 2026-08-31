cmake_minimum_required(VERSION 3.24)
project(SimNodusBackendProbe LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This dependency probe requires 64-bit Windows.")
endif()

set(NGSPICE_ROOT "" CACHE PATH "Verified, extracted Spice64_dll directory")
if(NOT EXISTS "${NGSPICE_ROOT}/include/ngspice/sharedspice.h")
    message(FATAL_ERROR "Set NGSPICE_ROOT to the verified ngspice DLL package.")
endif()

add_executable(ngspice-probe ngspice_probe.cpp)
target_compile_features(ngspice-probe PRIVATE cxx_std_20)
set_target_properties(ngspice-probe PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(ngspice-probe PRIVATE "${NGSPICE_ROOT}/include")
target_compile_definitions(ngspice-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(ngspice-probe PRIVATE /W4 /WX /permissive-)
endif()

# Deliberately separate from the foundation build. No downloads or circuit loading.
