# Engine spikes S1 (SN-103) and S2 (SN-104). Results: docs/experiments/S1-*.md, S2-*.md.
add_executable(s1_continuous s1_continuous.cpp)
target_link_libraries(s1_continuous PRIVATE simnodus::sim simnodus::warnings $<$<PLATFORM_ID:Windows>:winmm>)

add_executable(s2_reference s2_reference.cpp)
target_link_libraries(s2_reference PRIVATE simnodus::sim simnodus::warnings)

if(BUILD_TESTING AND SIMNODUS_NGSPICE_LIBRARY)
    file(GLOB _s2_netlists CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/s2/*.cir")
    # Every platform is compared with the committed Windows ngspice-47 result.
    add_test(NAME spike-s2-reference
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/s2_compare.py" run
            --exe "$<TARGET_FILE:s2_reference>"
            --library "${SIMNODUS_NGSPICE_LIBRARY}"
            --scripts "${PROJECT_SOURCE_DIR}/resources/ngspice"
            --output "${CMAKE_BINARY_DIR}/spikes/s2-result.json"
            --reference "${CMAKE_CURRENT_SOURCE_DIR}/s2/reference-windows-ngspice47.json"
            ${_s2_netlists})
    set_tests_properties(spike-s2-reference PROPERTIES TIMEOUT 300)
    # Short bounded-memory check of the segmented interactive run.
    add_test(NAME spike-s1-segments
        COMMAND s1_continuous "${SIMNODUS_NGSPICE_LIBRARY}" "${PROJECT_SOURCE_DIR}/resources/ngspice"
            "${CMAKE_BINARY_DIR}/spikes/s1-segments-smoke.json" segments sim=1 segment=0.25)
    set_tests_properties(spike-s1-segments PROPERTIES TIMEOUT 120)
endif()
