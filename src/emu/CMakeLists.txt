# SimNodus STM32 emulator (docs/architecture/MCU_EMULATOR.md), Qt-free, no
# third-party code.
add_library(simnodus_emu STATIC
    bus.cpp
    cpu.cpp
    elf.cpp
    gdb_server.cpp
    gdb_socket.cpp
    stm32f103.cpp
    stm32f1_adc.cpp
    stm32f1_dma.cpp
    stm32f1_system.cpp
    stm32f1_timer.cpp)
add_library(simnodus::emu ALIAS simnodus_emu)
target_include_directories(simnodus_emu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(simnodus_emu PRIVATE simnodus::warnings)
if(WIN32)
    target_link_libraries(simnodus_emu PRIVATE ws2_32) # GDB server socket
endif()
