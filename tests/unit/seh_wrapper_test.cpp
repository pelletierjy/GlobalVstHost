// tests/unit/seh_wrapper_test.cpp
//
// Unit tests for SEHPluginWrapper::invokeGuarded — the guard used to isolate
// plugin loading during a scan so a faulty VST3 cannot crash the host. Verifies
// the success path, C++-exception path, null guard, and (on Windows) that a
// structured exception (access violation) is caught rather than terminating the
// test process.

#include "../../src/audio-engine/vst-host/seh_wrapper.h"

#include <gtest/gtest.h>

#include <stdexcept>

using jyglobalvst::engine::SEHPluginWrapper;

TEST(SehWrapperInvokeGuarded, RunsCallbackAndReportsSuccess)
{
    int value = 0;
    const bool ok = SEHPluginWrapper::invokeGuarded(
        [](void* p) { *static_cast<int*>(p) = 42; }, &value);

    EXPECT_TRUE(ok);
    EXPECT_EQ(value, 42);
}

TEST(SehWrapperInvokeGuarded, NullCallbackReturnsFalse)
{
    EXPECT_FALSE(SEHPluginWrapper::invokeGuarded(nullptr, nullptr));
}

TEST(SehWrapperInvokeGuarded, CatchesCppException)
{
    bool reached_after = false;
    const bool ok = SEHPluginWrapper::invokeGuarded(
        [](void*) { throw std::runtime_error("boom"); }, nullptr);

    reached_after = true;  // we only get here because the guard swallowed it
    EXPECT_FALSE(ok);
    EXPECT_TRUE(reached_after);
}

#if defined(_WIN32)
TEST(SehWrapperInvokeGuarded, CatchesStructuredException)
{
    // A null-pointer write raises a structured exception (access violation) that a
    // plain C++ catch(...) cannot stop. The guard must catch it and return false —
    // if it does not, this whole test process crashes, which is itself the failure.
    const bool ok = SEHPluginWrapper::invokeGuarded(
        [](void*) {
            volatile int* p = nullptr;
            *p = 1;
        },
        nullptr);

    EXPECT_FALSE(ok);
}
#endif
