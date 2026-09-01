cmake_minimum_required(VERSION 3.24)
project(SimNodusE03 LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "E-03 currently requires native Windows x64.")
endif()

set(PREPARED_ROOT "" CACHE PATH "Verified output of the SN-019 preparation step")
set(FIRMWARE_ROOT "" CACHE PATH "Output of the owned E-03 firmware build")
set(NGSPICE_ROOT "" CACHE PATH "Verified Spice64_dll directory")

if(NOT EXISTS "${FIRMWARE_ROOT}/firmware_layout.h")
    message(FATAL_ERROR "Build the owned E-03 firmware and set FIRMWARE_ROOT.")
endif()
if(NOT EXISTS "${NGSPICE_ROOT}/include/ngspice/sharedspice.h")
    message(FATAL_ERROR "Set NGSPICE_ROOT to the verified DLL package.")
endif()

add_subdirectory(../renode-client "${CMAKE_CURRENT_BINARY_DIR}/client")
add_executable(e03_probe probe.cpp)
target_compile_features(e03_probe PRIVATE cxx_std_20)
set_target_properties(e03_probe PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(e03_probe PRIVATE
    "${FIRMWARE_ROOT}"
    "${NGSPICE_ROOT}/include"
    "../../../tools/backend_probe")
target_compile_definitions(e03_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(e03_probe PRIVATE client_normal)

if(MSVC)
    target_compile_options(e03_probe PRIVATE /W4 /WX /permissive- /EHsc)
endif()
