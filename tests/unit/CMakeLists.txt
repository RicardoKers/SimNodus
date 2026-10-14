add_executable(sim_tests sim_tests.cpp)
target_include_directories(sim_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sim_tests PRIVATE simnodus::sim simnodus::warnings)
add_test(NAME sim-unit COMMAND sim_tests)

add_executable(engine_tests engine_tests.cpp)
target_include_directories(engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_tests PRIVATE simnodus::sim simnodus::warnings)
if(SIMNODUS_NGSPICE_LIBRARY)
    add_test(NAME engine-ngspice
        COMMAND engine_tests "${SIMNODUS_NGSPICE_LIBRARY}" "${PROJECT_SOURCE_DIR}/resources/ngspice")
    set_tests_properties(engine-ngspice PROPERTIES TIMEOUT 120)
else()
    message(STATUS "engine-ngspice test skipped: SIMNODUS_NGSPICE_LIBRARY is not set")
endif()

add_executable(model_tests model_tests.cpp library_tests.cpp io_tests.cpp operations_tests.cpp test_main.cpp)
target_include_directories(model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(model_tests PRIVATE simnodus::model simnodus::library simnodus::io simnodus::editing
    simnodus::warnings)
target_compile_definitions(model_tests PRIVATE SIMNODUS_TEST_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library")
add_test(NAME model-unit COMMAND model_tests)
