add_library(simnodus_sim STATIC
    engine_check.cpp
    engine_host.cpp
    ngspice_runtime.cpp
    process_memory.cpp)
add_library(simnodus::sim ALIAS simnodus_sim)
target_include_directories(simnodus_sim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(Threads REQUIRED)
target_link_libraries(simnodus_sim PUBLIC Threads::Threads ${CMAKE_DL_LIBS} PRIVATE simnodus::warnings)
