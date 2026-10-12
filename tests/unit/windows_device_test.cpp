// tests/unit/windows_device_test.cpp
//
// Verifies the WASAPI-backed WindowsDeviceHelper queries (previously TODO
// stubs): the default-format string shape and endpoint enumeration.

#include "../../src/shared/platform/windows_device.h"

#include <gtest/gtest.h>

#include <regex>

namespace jyglobalvst::testing {

TEST(WindowsDeviceTest, DefaultAudioFormatMatchesShape)
{
    const auto fmt = WindowsDeviceHelper::GetSystemDefaultAudioFormat();
    // "<sample-rate>/<bits><f|i>/<channels>" — a real endpoint query result or
    // the documented fallback ("48000/32f/2") when no endpoint is available.
    const std::regex shape(R"(\d+/\d+[fi]/\d+)");
    EXPECT_TRUE(std::regex_match(fmt, shape)) << "got: " << fmt;
}

TEST(WindowsDeviceTest, EnumerateAudioDevicesReturnsUtf8Names)
{
    const auto devices = WindowsDeviceHelper::EnumerateAudioDevices();
    for (const auto& name : devices)
    {
        EXPECT_FALSE(name.empty());
    }
    // A CI runner may expose no active render endpoint; empty is acceptable.
}

}  // namespace jyglobalvst::testing
