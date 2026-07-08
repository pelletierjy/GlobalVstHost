# src/audio-engine/CMakeLists.txt
# Real-time audio routing + VST3 hosting. Built as a static library so both the
# tray app (user-mode install) and the Windows Service (release: service-mode)
# link the same binary code path. Identical engine, different host.

add_library(jyglobalvst_audio_engine STATIC)

target_include_directories(jyglobalvst_audio_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(jyglobalvst_audio_engine PUBLIC
    jyglobalvst_shared
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_graphics)

target_compile_definitions(jyglobalvst_audio_engine PUBLIC
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_USE_WINRT_MIDI=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=0
    JUCE_STANDALONE_APPLICATION=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

if(JYGLOBALVST_BUILD_ASIO)
    target_compile_definitions(jyglobalvst_audio_engine PUBLIC JUCE_ASIO=1)
    target_include_directories(jyglobalvst_audio_engine SYSTEM PUBLIC ${ASIO_SDK_PATH}/common)
else()
    target_compile_definitions(jyglobalvst_audio_engine PUBLIC JUCE_ASIO=0)
endif()

file(GLOB_RECURSE _engine_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/builtin-effects/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/routing/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/chain/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vst-host/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/monitoring/*.cpp")

if(MSVC)
    # JUCE 8.0.4 + VS 2022+ emit warnings our /WX policy does not apply to third-party code.
    target_compile_options(jyglobalvst_audio_engine PRIVATE /wd5105 /wd4458 /wd4324)
endif()

if(_engine_sources)
    target_sources(jyglobalvst_audio_engine PRIVATE ${_engine_sources})
else()
    set(_placeholder ${CMAKE_CURRENT_BINARY_DIR}/_engine_placeholder.cpp)
    file(WRITE ${_placeholder} "namespace jyglobalvst::engine { void _placeholder() {} }\n")
    target_sources(jyglobalvst_audio_engine PRIVATE ${_placeholder})
endif()
