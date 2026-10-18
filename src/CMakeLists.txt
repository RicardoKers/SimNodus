# Qt-free libraries (docs/architecture/PRODUCT_ARCHITECTURE.md).
add_library(simnodus_json INTERFACE)
target_include_directories(simnodus_json SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/third_party")

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
    library/component_library.cpp
    library/spice_model.cpp
    library/spice_template.cpp)
add_library(simnodus::library ALIAS simnodus_library)
target_link_libraries(simnodus_library PUBLIC simnodus::model PRIVATE simnodus_json simnodus::warnings)
# Last-resort library location for running from a build tree.
target_compile_definitions(simnodus_library PRIVATE
    SIMNODUS_SOURCE_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/resources/library")

add_library(simnodus_io STATIC io/project_file.cpp)
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
target_link_libraries(simnodus_session PUBLIC simnodus::sim simnodus::netlist simnodus::mcu PRIVATE simnodus::warnings)
