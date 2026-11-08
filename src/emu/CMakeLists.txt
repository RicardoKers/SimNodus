# SimNodus microcontroller emulator (docs/architecture/MCU_EMULATOR.md; the
# RISC-V core of experiment E-08: docs/experiments/E-08-plan.md; the AVR core
# of experiment E-10: docs/experiments/E-10-plan.md), Qt-free. The Wi-Fi
# gateway (E-09, SN-268) uses lwIP from third_party/lwip, unmodified (BSD-3).
enable_language(C)
set(SIMNODUS_LWIP_DIR "${PROJECT_SOURCE_DIR}/third_party/lwip/src")
file(GLOB SIMNODUS_LWIP_SOURCES CONFIGURE_DEPENDS "${SIMNODUS_LWIP_DIR}/core/*.c" "${SIMNODUS_LWIP_DIR}/core/ipv4/*.c")
add_library(simnodus_lwip STATIC ${SIMNODUS_LWIP_SOURCES})
target_include_directories(simnodus_lwip SYSTEM PUBLIC "${SIMNODUS_LWIP_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/lwip_port")
set_target_properties(simnodus_lwip PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(simnodus_lwip PRIVATE /wd4103) # lwIP's packed-structure check
endif()

add_library(simnodus_emu STATIC
    atmega.cpp
    avr_cpu.cpp
    bus.cpp
    cpu.cpp
    elf.cpp
    riscv_cpu.cpp
    riscv_test_machine.cpp
    esp32c3.cpp
    esp32c3_rom.cpp
    sha.cpp
    gdb_server.cpp
    gdb_socket.cpp
    stm32f103.cpp
    stm32f1_adc.cpp
    stm32f1_dma.cpp
    stm32f1_i2c.cpp
    stm32f1_spi.cpp
    stm32f1_system.cpp
    stm32f1_timer.cpp
    host_socket.cpp
    wifi_air.cpp
    wifi_crypto.cpp
    wifi_gateway.cpp
    wifi_network.cpp)
add_library(simnodus::emu ALIAS simnodus_emu)
target_include_directories(simnodus_emu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(simnodus_emu PRIVATE simnodus::warnings simnodus_lwip)
if(WIN32)
    target_link_libraries(simnodus_emu PRIVATE ws2_32 iphlpapi) # GDB server and Wi-Fi gateway sockets, pings
endif()
