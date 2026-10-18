# Microcontroller engine (SimNodus's own STM32F103 emulator), Qt-free.
add_library(simnodus_mcu STATIC
    mcu_runner.cpp
    native_mcu_runner.cpp)
add_library(simnodus::mcu ALIAS simnodus_mcu)
target_include_directories(simnodus_mcu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(Threads REQUIRED)
target_link_libraries(simnodus_mcu PUBLIC Threads::Threads PRIVATE simnodus::emu simnodus::warnings)
