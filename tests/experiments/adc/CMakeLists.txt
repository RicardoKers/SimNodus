cmake_minimum_required(VERSION 3.24)
project(SimNodusE04 LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "E-04 currently requires native Windows x64.")
endif()

set(PREPARED_ROOT "" CACHE PATH "Verified output of the SN-019 preparation step")
set(FIRMWARE_ROOT "" CACHE PATH "Output of the owned E-04 firmware build")
if(NOT EXISTS "${FIRMWARE_ROOT}/firmware_layout.h")
    message(FATAL_ERROR "Build the owned E-04 firmware and set FIRMWARE_ROOT.")
endif()

add_subdirectory(../renode-client "${CMAKE_CURRENT_BINARY_DIR}/client")
add_executable(e04_probe probe.cpp)
target_compile_features(e04_probe PRIVATE cxx_std_20)
set_target_properties(e04_probe PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(e04_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${FIRMWARE_ROOT}")
target_compile_definitions(e04_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(e04_probe PRIVATE client_normal)

if(MSVC)
    target_compile_options(e04_probe PRIVATE /W4 /WX /permissive- /EHsc)
endif()
