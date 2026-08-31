cmake_minimum_required(VERSION 3.24)

project(SimNodus VERSION 0.0.0 DESCRIPTION "Mixed-signal and embedded systems simulator" LANGUAGES CXX)

# This foundation does not build a simulator or download runtime dependencies.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simnodus_build_options INTERFACE)
add_library(simnodus::build_options ALIAS simnodus_build_options)
target_compile_features(simnodus_build_options INTERFACE cxx_std_20)

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_custom_target(check-repository
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_repository.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking repository documentation and structure"
    VERBATIM
)

message(STATUS "SimNodus foundation only: no simulation application is implemented yet.")
