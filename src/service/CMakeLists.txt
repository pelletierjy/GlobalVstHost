# src/service/CMakeLists.txt
# Windows Service host for JyGlobalVST (service-mode installs only).
# Builds jyglobalvst_service.exe.

option(JYGLOBALVST_BUILD_SERVICE "Build Windows Service host (service-mode)" OFF)

if(NOT JYGLOBALVST_BUILD_SERVICE)
    message(STATUS "service: JYGLOBALVST_BUILD_SERVICE=OFF — service target skipped")
    add_library(jyglobalvst_service_placeholder INTERFACE)
    return()
endif()

add_executable(jyglobalvst_service
    host/service_main.cpp
    ipc-server/pipe_server.cpp
    ipc-server/session_auth.cpp
    ipc-server/meter_pusher.cpp)

target_include_directories(jyglobalvst_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(jyglobalvst_service PRIVATE
    jyglobalvst_audio_engine
    jyglobalvst_shared
    nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(jyglobalvst_service PRIVATE
        advapi32    # SCM APIs
        ws2_32)
endif()

set_target_properties(jyglobalvst_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
