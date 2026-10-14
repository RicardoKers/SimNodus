# Renode external control client (MIT, Antmicro), adapted for a native
# loopback transport on Windows and POSIX. See README.md for provenance.
enable_language(C)
add_library(renode_external_control STATIC renode_api.c transport.c)
add_library(simnodus::renode_client ALIAS renode_external_control)
set_target_properties(renode_external_control PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(renode_external_control SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(WIN32)
    target_link_libraries(renode_external_control PUBLIC ws2_32)
    target_compile_definitions(renode_external_control PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
