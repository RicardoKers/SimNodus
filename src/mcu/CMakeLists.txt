# Microcontroller engine (Renode), Qt-free. Renode itself is a separate
# process found at run time; nothing here downloads or starts it at build time.
add_library(simnodus_mcu STATIC
    child_process.cpp
    mcu_check.cpp
    mcu_runner.cpp
    native_mcu_runner.cpp
    renode_engine.cpp)
add_library(simnodus::mcu ALIAS simnodus_mcu)
target_include_directories(simnodus_mcu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(Threads REQUIRED)
target_link_libraries(simnodus_mcu PUBLIC Threads::Threads PRIVATE simnodus::emu simnodus::renode_client simnodus::warnings)
# Last-resort locations for running from a build tree.
target_compile_definitions(simnodus_mcu PRIVATE SIMNODUS_SOURCE_MCU_DIR="${PROJECT_SOURCE_DIR}/resources/renode")
if(SIMNODUS_DEV_RENODE_DIR)
    target_compile_definitions(simnodus_mcu PRIVATE SIMNODUS_DEV_RENODE_DIR="${SIMNODUS_DEV_RENODE_DIR}")
endif()
