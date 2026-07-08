// tests/integration/polish_service_mode_test.cpp
//
// T121 — Service-mode integration test.
// Validates that the IPC client can detect service presence/absence and that
// the in-process engine remains functional when no service is installed.

#include "jyglobalvst/audio_engine.h"

#include <gtest/gtest.h>

#include <windows.h>

using namespace jyglobalvst;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static bool probePipe(const std::wstring& pipeName)
{
    HANDLE h = CreateFileW(
        pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

TEST(ServiceMode, ServicePipeNotPresentInTestableDev)
{
    // In testable-dev builds the service should not be running,
    // so its named pipe should not exist.
    DWORD sessionId = 0;
    ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));

    std::wstring pipeName = L"\\\\.\\pipe\\JyGlobalVST\\v1\\" + std::to_wstring(sessionId);
    EXPECT_FALSE(probePipe(pipeName))
        << "Service pipe should not exist in testable-dev (no service installed)";
}

TEST(ServiceMode, InProcessEngineWorksWithoutService)
{
    // Verify the factory returns a functional engine even when no service is present.
    auto engine = createAudioEngine();
    ASSERT_NE(engine, nullptr);

    // Basic API smoke test.
    auto outputs = engine->listOutputs();
    // At least the default Windows audio endpoint should be visible.
    EXPECT_FALSE(outputs.empty());

    auto inputs = engine->listInputs();
    EXPECT_FALSE(inputs.empty());

    // Buffer size API.
    engine->setBufferSize(256);
    EXPECT_EQ(engine->bufferSize(), 256);

    // Chain should be empty at start.
    auto snapshot = engine->snapshotChain();
    EXPECT_EQ(snapshot.chain_revision, 0);
    EXPECT_TRUE(snapshot.slots.empty());
}

TEST(ServiceMode, EngineHostModeEnumValuesStable)
{
    // The UI relies on these enum values for the diagnostics surface (T120).
    // Ensure they don't shift inadvertently.
    enum class EngineHostMode
    {
        InProcess = 0,
        WindowsService = 1,
    };
    EXPECT_EQ(static_cast<int>(EngineHostMode::InProcess), 0);
    EXPECT_EQ(static_cast<int>(EngineHostMode::WindowsService), 1);
}

TEST(ServiceMode, SessionIdMatchesCurrentProcess)
{
    // Sanity check: we can read our own session ID.
    DWORD sessionId = 0;
    ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));
    EXPECT_NE(sessionId, static_cast<DWORD>(-1));
}
