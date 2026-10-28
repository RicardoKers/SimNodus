# SimNodus microcontroller emulator (docs/architecture/MCU_EMULATOR.md; the
# RISC-V core of experiment E-08: docs/experiments/E-08-plan.md), Qt-free, no
# third-party code.
add_library(simnodus_emu STATIC
    bus.cpp
    cpu.cpp
    elf.cpp
    riscv_cpu.cpp
    riscv_test_machine.cpp
    gdb_server.cpp
    gdb_socket.cpp
    stm32f103.cpp
    stm32f1_adc.cpp
    stm32f1_dma.cpp
    stm32f1_i2c.cpp
    stm32f1_spi.cpp
    stm32f1_system.cpp
    stm32f1_timer.cpp)
add_library(simnodus::emu ALIAS simnodus_emu)
target_include_directories(simnodus_emu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(simnodus_emu PRIVATE simnodus::warnings)
if(WIN32)
    target_link_libraries(simnodus_emu PRIVATE ws2_32) # GDB server socket
endif()
