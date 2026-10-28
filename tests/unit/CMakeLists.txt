add_executable(sim_tests sim_tests.cpp)
target_include_directories(sim_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sim_tests PRIVATE simnodus::sim simnodus::warnings)
add_test(NAME sim-unit COMMAND sim_tests)

add_executable(engine_tests engine_tests.cpp library_engine_tests.cpp session_engine_tests.cpp session_mcu_tests.cpp
    remote_session_tests.cpp device_tests.cpp automation_engine_tests.cpp)
target_include_directories(engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_tests PRIVATE simnodus::session simnodus::engine_link simnodus::emu simnodus::netlist simnodus::editing simnodus::io simnodus::automation simnodus::warnings)
target_compile_definitions(engine_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library"
    SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
if(SIMNODUS_NGSPICE_LIBRARY)
    add_test(NAME engine-ngspice
        COMMAND engine_tests "${SIMNODUS_NGSPICE_LIBRARY}" "${PROJECT_SOURCE_DIR}/resources/ngspice")
    set_tests_properties(engine-ngspice PROPERTIES TIMEOUT 900)
else()
    message(STATUS "engine-ngspice test skipped: SIMNODUS_NGSPICE_LIBRARY is not set")
endif()

add_executable(model_tests model_tests.cpp library_tests.cpp io_tests.cpp operations_tests.cpp netlist_tests.cpp block_tests.cpp
    examples_tests.cpp automation_tests.cpp
    test_main.cpp)
target_include_directories(model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(model_tests PRIVATE simnodus::model simnodus::library simnodus::io simnodus::editing
    simnodus::netlist simnodus::automation simnodus::warnings)
target_compile_definitions(model_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library"
    SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
add_test(NAME model-unit COMMAND model_tests)

add_executable(emu_tests emu_tests.cpp)
target_include_directories(emu_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(emu_tests PRIVATE simnodus::emu simnodus::warnings)
add_test(NAME emu-unit COMMAND emu_tests "${PROJECT_SOURCE_DIR}/examples")

# RV32IMC core of experiment E-08: directed cases, the architecture tests and
# the differential corpus recorded from QEMU (docs/experiments/E-08-plan.md).
add_executable(riscv_tests riscv_tests.cpp)
target_include_directories(riscv_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(riscv_tests PRIVATE simnodus::emu simnodus::warnings)
add_test(NAME emu-riscv-unit COMMAND riscv_tests "${PROJECT_SOURCE_DIR}" "riscv: ")
add_test(NAME emu-riscv-arch COMMAND riscv_tests "${PROJECT_SOURCE_DIR}" "riscv-arch: ")
add_test(NAME emu-riscv-differential COMMAND riscv_tests "${PROJECT_SOURCE_DIR}" "riscv-diff: ")

# ESP32-C3 machine of experiment E-08 (SN-244): unmodified ESP-IDF and
# MicroPython images from tests/experiments/esp32c3/firmware.
add_executable(esp32c3_tests esp32c3_tests.cpp)
target_include_directories(esp32c3_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(esp32c3_tests PRIVATE simnodus::emu simnodus::warnings)
add_test(NAME emu-esp32c3 COMMAND esp32c3_tests "${PROJECT_SOURCE_DIR}")
