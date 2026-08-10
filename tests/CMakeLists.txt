# tests/CMakeLists.txt
# Unit + integration + contract + audit tests. GoogleTest as the runner.

include(GoogleTest)

# Coverage support (MSVC + Visual Studio's /PROFILE coverage, or gcov-compatible)
if(JYGLOBALVST_ENABLE_COVERAGE)
    message(STATUS "Code coverage instrumentation enabled (JYGLOBALVST_ENABLE_COVERAGE)")
    if(MSVC)
        # Use /PROFILE for coverage (VS2022 17.9+) or fall back to legacy /Zi
        add_compile_options(/PROFILE)
        add_compile_options(/Zi)
        add_compile_definitions(COVERAGE_ENABLED)
    endif()
endif()

function(jyglobalvst_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        jyglobalvst_audio_engine
        jyglobalvst_shared
        GTest::gtest
        GTest::gtest_main)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    if(MSVC)
        # JUCE 8.0.4 + VS 2022+ third-party warnings (see audio-engine/CMakeLists.txt).
        target_compile_options(${name} PRIVATE /wd5105 /wd4458 /wd4324)
    endif()
    gtest_discover_tests(${name}
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
endfunction()

file(GLOB _latency_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/latency/*_runner.cpp")
foreach(_src IN LISTS _latency_test_sources)
    get_filename_component(_name "${_src}" NAME_WE)
    jyglobalvst_add_test(${_name} ${_src})
endforeach()

# T028 — hardware-loopback harness is compiled but skipped at runtime
# unless JYGLOBALVST_LOOPBACK_IFACE is set.
file(GLOB _unit_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*_test.cpp")

foreach(_src IN LISTS _unit_test_sources)
    get_filename_component(_name "${_src}" NAME_WE)
    jyglobalvst_add_test(${_name} ${_src})
endforeach()

file(GLOB _integration_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/*_test.cpp")
foreach(_src IN LISTS _integration_test_sources)
    get_filename_component(_name "${_src}" NAME_WE)
    jyglobalvst_add_test(${_name} ${_src})
endforeach()

file(GLOB _contract_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/contract/*_test.cpp")
foreach(_src IN LISTS _contract_test_sources)
    get_filename_component(_name "${_src}" NAME_WE)
    jyglobalvst_add_test(${_name} ${_src})
endforeach()

# polish_accessibility_test unity-includes chain_editor.cpp to exercise
# ChainSlotRow directly; chain_editor.cpp needs HorizontalMeterPanel's
# implementation, which must be compiled as its own translation unit here
# (rather than also unity-included) since both .cpp files define an
# identically-named linearToDb() in an anonymous namespace.
if(TARGET polish_accessibility_test)
    target_sources(polish_accessibility_test PRIVATE
        "${CMAKE_SOURCE_DIR}/src/tray-app/ui/horizontal_meter_panel.cpp")
endif()
