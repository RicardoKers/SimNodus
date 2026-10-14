add_executable(sim_tests sim_tests.cpp)
target_include_directories(sim_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sim_tests PRIVATE simnodus::sim simnodus::warnings)
add_test(NAME sim-unit COMMAND sim_tests)

add_executable(engine_tests engine_tests.cpp library_engine_tests.cpp session_engine_tests.cpp)
target_include_directories(engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_tests PRIVATE simnodus::session simnodus::netlist simnodus::editing simnodus::warnings)
target_compile_definitions(engine_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library")
if(SIMNODUS_NGSPICE_LIBRARY)
    add_test(NAME engine-ngspice
        COMMAND engine_tests "${SIMNODUS_NGSPICE_LIBRARY}" "${PROJECT_SOURCE_DIR}/resources/ngspice")
    set_tests_properties(engine-ngspice PROPERTIES TIMEOUT 120)
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

add_executable(renode_tests renode_tests.cpp)
target_include_directories(renode_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(renode_tests PRIVATE simnodus::mcu simnodus::warnings)
if(SIMNODUS_RENODE)
    add_test(NAME engine-renode COMMAND renode_tests "${SIMNODUS_RENODE}" "${PROJECT_SOURCE_DIR}/resources/renode")
    set_tests_properties(engine-renode PROPERTIES TIMEOUT 300)
elseif(SIMNODUS_REQUIRE_RENODE)
    message(FATAL_ERROR "SIMNODUS_REQUIRE_RENODE is set but Renode was not found (tools/fetch_backends.py --renode)")
else()
    message(STATUS "engine-renode test skipped: Renode not found (tools/fetch_backends.py --renode)")
endif()
