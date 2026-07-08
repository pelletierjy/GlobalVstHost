# src/shared/CMakeLists.txt
# Cross-component code: concurrency primitives, JSON validators, Windows platform helpers.
# Linked into audio-engine, tray-app, and (release) service.

add_library(jyglobalvst_shared STATIC)

target_include_directories(jyglobalvst_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/audio-engine/include)

target_link_libraries(jyglobalvst_shared PUBLIC
    nlohmann_json::nlohmann_json)

# Windows platform link deps for IMMDeviceEnumerator, named mutexes, known folders.
if(WIN32)
    target_link_libraries(jyglobalvst_shared PUBLIC
        ole32
        oleaut32
        uuid
        ksuser
        avrt
        propsys
        shell32
        user32)
endif()

# Collect source files added by Phase 2 tasks. Empty list is OK during early scaffolding;
# the lib becomes header-only-equivalent until sources land.
file(GLOB_RECURSE _shared_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/concurrency/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/json/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ipc/*.cpp")

if(_shared_sources)
    target_sources(jyglobalvst_shared PRIVATE ${_shared_sources})
else()
    # Placeholder so the static lib is buildable before any .cpp lands.
    set(_placeholder ${CMAKE_CURRENT_BINARY_DIR}/_shared_placeholder.cpp)
    file(WRITE ${_placeholder} "namespace jyglobalvst::shared { void _placeholder() {} }\n")
    target_sources(jyglobalvst_shared PRIVATE ${_placeholder})
endif()
