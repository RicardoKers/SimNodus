# Command-line runner of the SimNodus STM32 emulator (experiment E-07): runs an
# ELF for a given time and prints pin changes, UART bytes and diagnostics.
add_executable(simnodus-emu main.cpp)
target_link_libraries(simnodus-emu PRIVATE simnodus::emu simnodus::devices simnodus::warnings)
