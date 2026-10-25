# Qt-free libraries (docs/architecture/PRODUCT_ARCHITECTURE.md).
add_library(simnodus_json INTERFACE)
target_include_directories(simnodus_json SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/third_party")

# Digital devices on a board's nets (SN-203, ADR 0102).
add_library(simnodus_devices STATIC devices/devices.cpp)
add_library(simnodus::devices ALIAS simnodus_devices)
target_include_directories(simnodus_devices PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(simnodus_devices PRIVATE simnodus::warnings)

add_subdirectory(sim)
add_subdirectory(mcu)
add_subdirectory(emu)

add_library(simnodus_model STATIC
    model/edit.cpp
    model/engineering.cpp
    model/schematic.cpp)
add_library(simnodus::model ALIAS simnodus_model)
target_include_directories(simnodus_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(simnodus_model PRIVATE simnodus::warnings)

add_library(simnodus_library STATIC
    library/blocks.cpp
    library/component_library.cpp
    library/spice_model.cpp
    library/spice_template.cpp)
add_library(simnodus::library ALIAS simnodus_library)
target_link_libraries(simnodus_library PUBLIC simnodus::model PRIVATE simnodus_json simnodus::warnings)
# Last-resort library location for running from a build tree.
target_compile_definitions(simnodus_library PRIVATE
    SIMNODUS_SOURCE_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library")

add_library(simnodus_io STATIC io/memory_file.cpp io/project_file.cpp)
add_library(simnodus::io ALIAS simnodus_io)
target_link_libraries(simnodus_io PUBLIC simnodus::model PRIVATE simnodus_json simnodus::warnings)

add_library(simnodus_editing STATIC editing/operations.cpp)
add_library(simnodus::editing ALIAS simnodus_editing)
target_link_libraries(simnodus_editing PUBLIC simnodus::model simnodus::library PRIVATE simnodus::io simnodus::warnings)

add_library(simnodus_netlist STATIC
    netlist/connectivity.cpp
    netlist/netlist.cpp)
add_library(simnodus::netlist ALIAS simnodus_netlist)
target_link_libraries(simnodus_netlist PUBLIC simnodus::model simnodus::library PRIVATE simnodus::warnings)

add_library(simnodus_session STATIC sim/simulation_session.cpp)
add_library(simnodus::session ALIAS simnodus_session)
target_link_libraries(simnodus_session PUBLIC simnodus::sim simnodus::netlist simnodus::mcu PRIVATE simnodus::io simnodus::warnings)

# The editor/engine process boundary (SN-151, ADR 0101).
add_library(simnodus_engine_link STATIC
    sim/engine_protocol.cpp
    sim/engine_server.cpp
    sim/remote_session.cpp)
add_library(simnodus::engine_link ALIAS simnodus_engine_link)
target_link_libraries(simnodus_engine_link PUBLIC simnodus::session simnodus::library PRIVATE simnodus::io simnodus::warnings)

# Automation commands for agents and scripts (SN-215, ADR 0104).
add_library(simnodus_automation STATIC automation/automation.cpp)
add_library(simnodus::automation ALIAS simnodus_automation)
target_link_libraries(simnodus_automation PUBLIC simnodus::session simnodus::editing simnodus::library simnodus_json
    PRIVATE simnodus::io simnodus::netlist simnodus::warnings)
