add_executable(sim_tests sim_tests.cpp)
target_include_directories(sim_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sim_tests PRIVATE simnodus::sim simnodus::warnings)
add_test(NAME sim-unit COMMAND sim_tests)

add_executable(engine_tests engine_tests.cpp library_engine_tests.cpp session_engine_tests.cpp session_mcu_tests.cpp)
target_include_directories(engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_tests PRIVATE simnodus::session simnodus::netlist simnodus::editing simnodus::io simnodus::warnings)
target_compile_definitions(engine_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library"
    SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples" SIMNODUS_TEST_MCU_DIR="${PROJECT_SOURCE_DIR}/resources/renode")
if(SIMNODUS_RENODE)
    target_compile_definitions(engine_tests PRIVATE SIMNODUS_TEST_RENODE="${SIMNODUS_RENODE}")
endif()
if(SIMNODUS_NGSPICE_LIBRARY)
    add_test(NAME engine-ngspice
        COMMAND engine_tests "${SIMNODUS_NGSPICE_LIBRARY}" "${PROJECT_SOURCE_DIR}/resources/ngspice")
    set_tests_properties(engine-ngspice PROPERTIES TIMEOUT 900)
else()
    message(STATUS "engine-ngspice test skipped: SIMNODUS_NGSPICE_LIBRARY is not set")
endif()

add_executable(model_tests model_tests.cpp library_tests.cpp io_tests.cpp operations_tests.cpp netlist_tests.cpp
    examples_tests.cpp
    test_main.cpp)
target_include_directories(model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(model_tests PRIVATE simnodus::model simnodus::library simnodus::io simnodus::editing
    simnodus::netlist simnodus::warnings)
target_compile_definitions(model_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library"
    SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
add_test(NAME model-unit COMMAND model_tests)

add_executable(emu_tests emu_tests.cpp)
target_include_directories(emu_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(emu_tests PRIVATE simnodus::emu simnodus::warnings)
add_test(NAME emu-unit COMMAND emu_tests "${PROJECT_SOURCE_DIR}/examples")

# Differential test against Renode (E-07): batches from
# tests/experiments/emu-differential/generate.py; the committed corpus runs in ctest.
add_executable(emu_differential emu_differential.cpp)
target_link_libraries(emu_differential PRIVATE simnodus::emu simnodus::mcu simnodus::warnings)
add_executable(emu_firmware_compare emu_firmware_compare.cpp)
target_link_libraries(emu_firmware_compare PRIVATE simnodus::emu simnodus::mcu simnodus::warnings)
if(SIMNODUS_RENODE)
    add_test(NAME emu-firmware COMMAND emu_firmware_compare "${SIMNODUS_RENODE}" "${PROJECT_SOURCE_DIR}/resources/renode"
        "${PROJECT_SOURCE_DIR}/examples")
    set_tests_properties(emu-firmware PROPERTIES TIMEOUT 900)
endif()
set(_emu_corpus "${PROJECT_SOURCE_DIR}/tests/experiments/emu-differential/corpus")
if(SIMNODUS_RENODE AND EXISTS "${_emu_corpus}/seed1-512.elf")
    add_test(NAME emu-differential COMMAND emu_differential "${SIMNODUS_RENODE}" "${PROJECT_SOURCE_DIR}/resources/renode"
        "${PROJECT_SOURCE_DIR}/tests/experiments/emu-differential/diff.repl" 512 "${_emu_corpus}/seed1-512.elf")
    set_tests_properties(emu-differential PROPERTIES TIMEOUT 900)
endif()

add_executable(renode_tests renode_tests.cpp)
target_include_directories(renode_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(renode_tests PRIVATE simnodus::mcu simnodus::warnings)
target_compile_definitions(renode_tests PRIVATE SIMNODUS_TEST_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
if(SIMNODUS_RENODE)
    add_test(NAME engine-renode COMMAND renode_tests "${SIMNODUS_RENODE}" "${PROJECT_SOURCE_DIR}/resources/renode")
    set_tests_properties(engine-renode PROPERTIES TIMEOUT 1200)
elseif(SIMNODUS_REQUIRE_RENODE)
    message(FATAL_ERROR "SIMNODUS_REQUIRE_RENODE is set but Renode was not found (tools/fetch_backends.py --renode)")
else()
    message(STATUS "engine-renode test skipped: Renode not found (tools/fetch_backends.py --renode)")
endif()
