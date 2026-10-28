# Microcontroller engine (SimNodus's own emulator: STM32F103, ESP32-C3), Qt-free.
add_library(simnodus_mcu STATIC
    device_bus.cpp
    mcu_machine.cpp
    mcu_runner.cpp
    native_mcu_runner.cpp)
add_library(simnodus::mcu ALIAS simnodus_mcu)
target_include_directories(simnodus_mcu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(Threads REQUIRED)
target_link_libraries(simnodus_mcu PUBLIC Threads::Threads simnodus::devices PRIVATE simnodus::emu simnodus::warnings)
# The ESP32-C3's ROM ELF in the source tree, for runs from the build directory.
target_compile_definitions(simnodus_mcu PRIVATE
    SIMNODUS_SOURCE_ROM_DIR="${PROJECT_SOURCE_DIR}/third_party/esp-rom-elfs")
