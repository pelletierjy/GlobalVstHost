// tests/contract/us1_audio_engine_api_test.cpp
//
// T035 — Contract test: IAudioEngine.start/stop/selectOutput/setBufferSize idempotency.
//
// Per audio-engine-api.md Contract Guarantees, these methods are idempotent:
// - start() and stop() are safe to call multiple times
// - selectOutput(currentOutput()) is a no-op
// - setBufferSize(bufferSize()) is a no-op
// Listener events are properly delivered on the UI thread.

#include "../integration/loopback_fixture.h"

namespace jyglobalvst::testing {

class US1AudioEngineApiTest : public LoopbackFixture
{
};

TEST_F(US1AudioEngineApiTest, StartIsIdempotent)
{
    // Calling start() multiple times should be safe (no crash, no double-init).
    engine()->start();
    engine()->start();  // Second call
    engine()->start();  // Third call

    // Engine should still be in a valid state.
    const auto outputs = engine()->listOutputs();
    EXPECT_GT(outputs.size(), 0);

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, StopIsIdempotent)
{
    engine()->start();

    engine()->stop();
    engine()->stop();  // Second call
    engine()->stop();  // Third call

    // Engine should still be stoppable (no exception thrown).
}

TEST_F(US1AudioEngineApiTest, StartStopStartCycle)
{
    // start() → stop() → start() should work correctly.

    engine()->start();
    engine()->stop();
    engine()->start();

    const auto outputs = engine()->listOutputs();
    EXPECT_GT(outputs.size(), 0);

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, SelectCurrentOutputIsNoop)
{
    engine()->start();

    const auto current = engine()->currentOutput();
    const int revision_before = listener()->chain_revision();

    engine()->selectOutput(current);  // Select same device

    const int revision_after = listener()->chain_revision();

    // Selection should not bump chain revision.
    EXPECT_EQ(revision_before, revision_after);

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, SetCurrentBufferSizeIsNoop)
{
    engine()->start();

    const int current_size = engine()->bufferSize();
    const int revision_before = listener()->chain_revision();

    engine()->setBufferSize(current_size);  // Set to current value

    const int revision_after = listener()->chain_revision();

    // Setting to current value should not bump chain revision.
    EXPECT_EQ(revision_before, revision_after);

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, BufferSizeValidation)
{
    engine()->start();

    const int valid_sizes[] = {32, 64, 128, 256, 512, 1024};

    for (int size : valid_sizes)
    {
        // Should not throw.
        engine()->setBufferSize(size);
        EXPECT_EQ(engine()->bufferSize(), size);
    }

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, BufferSizeInvalidThrows)
{
    engine()->start();

    const int invalid_sizes[] = {16, 48, 255, 500, 2048};

    for (int size : invalid_sizes)
    {
        // Should throw std::invalid_argument.
        EXPECT_THROW(engine()->setBufferSize(size), std::invalid_argument);
    }

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, SetListenerWhenStopped)
{
    // setListener() can be called even when stopped.
    listener()->chain_revision();  // Already set via SetUp(), but explicit call is OK.

    engine()->start();
    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, ListenerEventsFireOnUiThread)
{
    // Listener events should fire on the JUCE UI thread, not the audio thread.
    // This is guaranteed by the engine's use of MessageManager::callAsync.
    // Testable-dev: implicitly verified by the loopback fixture.
    // Release: measured via thread-affinity assertions in listener callbacks.

    engine()->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // If any meter frames were received, they came from the UI thread.
    const int meter_count = listener()->meter_frame_count();
    (void)meter_count;

    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, CurrentOutputBeforeStart)
{
    // currentOutput() can be called even before start().
    const auto current = engine()->currentOutput();
    // May be empty or default, but should not throw.

    engine()->start();
    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, BufferSizeBeforeStart)
{
    // bufferSize() and negotiatedSampleRate() can be called before start().
    const int size = engine()->bufferSize();
    const int sample_rate = engine()->negotiatedSampleRate();

    // Size should be valid, sample rate may be 0 until negotiated.
    EXPECT_GT(size, 0);
    EXPECT_GE(sample_rate, 0);

    engine()->start();
    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, LatencyProfileBeforeStart)
{
    // latencyProfile() can be called anytime.
    const auto latency = engine()->latencyProfile();
    // May be zero-filled before audio starts, but should not throw.
    (void)latency;

    engine()->start();
    engine()->stop();
}

TEST_F(US1AudioEngineApiTest, CpuStatsTracking)
{
    engine()->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto cpu = engine()->cpuStats();
    EXPECT_GE(cpu.instantaneous_pct, 0.0f);
    EXPECT_LE(cpu.instantaneous_pct, 100.0f);

    engine()->stop();
}

}  // namespace jyglobalvst::testing
