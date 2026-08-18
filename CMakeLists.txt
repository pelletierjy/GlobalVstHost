cmake_minimum_required(VERSION 3.24)

project(JyGlobalVST
    VERSION 1.0.13.0
    DESCRIPTION "Windows system-wide VST3 host (testable-dev build)"
    LANGUAGES CXX)

# Constitution §VI: x64 Windows only. 32-bit and ARM64 are out of scope for v1.
if(NOT WIN32)
    message(FATAL_ERROR "JyGlobalVST targets Windows only. Current platform: ${CMAKE_SYSTEM_NAME}")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "JyGlobalVST is x64-only. Configure with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Per project style: namespace jyglobalvst, 4-space indent, 120-col limit (.clang-format).
# Per Constitution §V: warnings-as-errors on every audio-thread-reachable target.
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /MP)
    # Suppress warnings from third-party headers (JUCE harfbuzz, VST3 SDK) that we cannot fix.
    add_compile_options(/wd5105 /wd4458)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
else()
    message(WARNING "Non-MSVC compiler detected; release builds require MSVC. Continuing for tooling.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(JYGLOBALVST_BUILD_TESTS "Build unit and integration tests" ON)
option(JYGLOBALVST_ENABLE_COVERAGE "Enable code coverage instrumentation (MSVC only)" OFF)

# ASIO support is mandatory (not a toggle): this host requires ASIO output.
# The SDK can be provided via:
#   1. CMake variable: -DJYGLOBALVST_ASIO_SDK_PATH=...
#   2. Environment variable: JYGLOBALVST_ASIO_SDK_PATH
#   3. Vcpkg / system path (future)
#
# WARNING: The Steinberg ASIO SDK is provided under a proprietary license.
# See THIRD_PARTY_LICENSES.md for details. Legal review is required before
# shipping with ASIO enabled.
message(WARNING "ASIO support enabled. The Steinberg ASIO SDK is proprietary; "
    "see THIRD_PARTY_LICENSES.md for licensing details and legal requirements before shipping.")
if(DEFINED JYGLOBALVST_ASIO_SDK_PATH AND EXISTS "${JYGLOBALVST_ASIO_SDK_PATH}/common/iasiodrv.h")
    set(ASIO_SDK_PATH "${JYGLOBALVST_ASIO_SDK_PATH}" CACHE PATH "Steinberg ASIO SDK root" FORCE)
elseif(DEFINED ENV{JYGLOBALVST_ASIO_SDK_PATH} AND EXISTS "$ENV{JYGLOBALVST_ASIO_SDK_PATH}/common/iasiodrv.h")
    set(ASIO_SDK_PATH "$ENV{JYGLOBALVST_ASIO_SDK_PATH}" CACHE PATH "Steinberg ASIO SDK root" FORCE)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/asiosdk/common/iasiodrv.h")
    set(ASIO_SDK_PATH "${CMAKE_SOURCE_DIR}/third_party/asiosdk" CACHE PATH "Steinberg ASIO SDK root" FORCE)
else()
    message(FATAL_ERROR "Steinberg ASIO SDK not found (ASIO support is mandatory). "
        "Set JYGLOBALVST_ASIO_SDK_PATH to the SDK root (containing common/iasiodrv.h), "
        "or place the SDK at ${CMAKE_SOURCE_DIR}/third_party/asiosdk")
endif()
message(STATUS "ASIO SDK found: ${ASIO_SDK_PATH}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/third_party)

# T003: JUCE 8.x — UI, VST3 hosting, AudioProcessorGraph, audio device abstraction.
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)

# T004: nlohmann/json — presets, settings, scan cache, update manifest, IPC envelopes.
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)

# T005: GoogleTest — unit tests.
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(juce nlohmann_json)

if(JYGLOBALVST_BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT (matches our targets)" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

add_subdirectory(src/shared)
add_subdirectory(src/audio-engine)
add_subdirectory(src/tray-app)

# WiX MSI Installer (requires WiX Toolset v4+)
option(JYGLOBALVST_BUILD_INSTALLER "Build WiX MSI installer (requires WiX Toolset)" ON)
if(JYGLOBALVST_BUILD_INSTALLER)
    add_subdirectory(src/installer/wix)
endif()

# T114–T121 — Service mode (optional; release installs only).
option(JYGLOBALVST_BUILD_SERVICE "Build Windows Service host (service-mode)" OFF)
if(JYGLOBALVST_BUILD_SERVICE)
    add_subdirectory(src/service)
endif()

if(JYGLOBALVST_BUILD_TESTS)
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "")
message(STATUS "JyGlobalVST configuration:")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests:            ${JYGLOBALVST_BUILD_TESTS}")
message(STATUS "  Coverage:         ${JYGLOBALVST_ENABLE_COVERAGE}")
message(STATUS "")
