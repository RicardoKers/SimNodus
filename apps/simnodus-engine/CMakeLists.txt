# Engine process of the desktop application (SN-151, ADR 0101): runs the
# simulation for the editor over its standard input and output. Qt-free.
add_executable(simnodus-engine main.cpp)
target_link_libraries(simnodus-engine PRIVATE simnodus::engine_link simnodus::warnings)
# Next to simnodus(.exe) in the build tree, where the application looks for it.
set_target_properties(simnodus-engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/simnodus")
