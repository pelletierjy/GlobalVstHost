# src/tray-app/CMakeLists.txt
# JUCE-based tray UI. Always installed in both user-mode and (release)
# service-mode. The GUI target is gated on the presence of main.cpp so that
# Phase 1/Phase 2 builds can configure cleanly before US1 (T045) wires the
# minimal tray UI.

set(_tray_main "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

if(NOT EXISTS "${_tray_main}")
    message(STATUS "tray-app: main.cpp not present — GUI target skipped (US1/T045 enables it)")
    add_library(jyglobalvst_tray_placeholder INTERFACE)
    return()
endif()

juce_add_gui_app(jyglobalvst_tray
    PRODUCT_NAME "JyGlobalVST"
    COMPANY_NAME "JyGlobalVST"
    BUNDLE_ID "io.jyglobalvst.tray"
    VERSION "${PROJECT_VERSION}"
    MICROPHONE_PERMISSION_ENABLED FALSE
    DOCUMENT_BROWSER_ENABLED FALSE)

target_include_directories(jyglobalvst_tray PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(tray_app_icons
    HEADER_NAME "BinaryData.h"
    NAMESPACE "jyglobalvst::BinaryData"
    SOURCES
        ${CMAKE_SOURCE_DIR}/assets/app_icon.png
        ${CMAKE_SOURCE_DIR}/assets/userguide.html)

target_link_libraries(jyglobalvst_tray PRIVATE
    jyglobalvst_audio_engine
    jyglobalvst_shared
    tray_app_icons
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra)

target_compile_definitions(jyglobalvst_tray PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=0
    JUCE_DISPLAY_SPLASH_SCREEN=1
    JUCE_USE_DARK_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

file(GLOB_RECURSE _tray_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/settings/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/single-instance/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ipc-client/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/updates/*.cpp"
    "${_tray_main}")

# Add custom resource file with icon
list(APPEND _tray_sources "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")

target_sources(jyglobalvst_tray PRIVATE ${_tray_sources})
