// tests/latency/hardware_loopback_runner.cpp
//
// T028 — Hardware-loopback latency harness scaffolding.
//
// Physical setup: USB audio interface with TRS cable connecting output 1/2
// back to input 1/2. The interface MUST support duplex operation at the
// target sample rate.
//
// This executable is NOT run in CI. It is executed manually on a reference
// machine with the loopback cable attached. Results are written to stdout
// and to a Markdown report file.
//
// Gates (per spec.md):
//   - Scenario 4 (single plugin): ≤ 10 ms round-trip at 256-sample buffer.
//   - Scenario 4 (heavy chain):   ≤ 20 ms round-trip at 256-sample buffer.
//   - Scenario 8 (CPU):           ≤ 5 % rolling-1s CPU on i5-8th-gen class.
//   - Scenario 14 (soak):         0 xruns over 12 hours.
//   - Scenario 7 (sample rates):  no audible artifacts on 44.1 ↔ 48 ↔ 96
//                                 ↔ 176.4 ↔ 192 kHz transitions.

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace jyglobalvst::testing {

// -------------------------------------------------------------------------
// Loopback hardware detection
// -------------------------------------------------------------------------

struct LoopbackDeviceInfo
{
    std::string output_name;
    std::string input_name;
    bool detected = false;
};

static LoopbackDeviceInfo DetectLoopbackHardware()
{
    LoopbackDeviceInfo info;
    // TODO: enumerate WASAPI endpoints and look for a known loopback
    // interface (e.g., device description contains "JyGlobalVST Loopback"
    // or a specific USB VID/PID pair).
    //
    // For now, the harness expects the tester to set the environment
    // variable JYGLOBALVST_LOOPBACK_IFACE to the friendly name of the
    // USB interface being used.
    const char* env = std::getenv("JYGLOBALVST_LOOPBACK_IFACE");
    if (env != nullptr && std::strlen(env) > 0)
    {
        info.output_name = env;
        info.input_name = env;
        info.detected = true;
    }
    return info;
}

// -------------------------------------------------------------------------
// Report generation
// -------------------------------------------------------------------------

class LatencyReport
{
public:
    void AddSection(const std::string& title, const std::string& body)
    {
        sections_.push_back("## " + title + "\n\n" + body + "\n");
    }

    void WriteToFile(const std::filesystem::path& path) const
    {
        std::ofstream f(path);
        f << "# JyGlobalVST Hardware Loopback Report\n\n";
        f << "Date: " __DATE__ " " __TIME__ "\n\n";
        for (const auto& s : sections_)
            f << s;
    }

private:
    std::vector<std::string> sections_;
};

// -------------------------------------------------------------------------
// Scenario 4 — Single-plugin loopback latency
// -------------------------------------------------------------------------

TEST(HardwareLoopback, Scenario4_SinglePlugin_LatencyGate)
{
    auto hw = DetectLoopbackHardware();
    if (!hw.detected)
    {
        GTEST_SKIP() << "No loopback hardware detected. Set "
                           "JYGLOBALVST_LOOPBACK_IFACE to the USB interface "
                           "friendly name.";
    }

    // TODO: start engine with a single VST3 plugin (e.g., ReaEQ).
    // TODO: inject impulse, measure round-trip time via loopback input.
    // TODO: gate: ≤ 10 ms at 256-sample buffer.

    SUCCEED() << "Placeholder — loopback latency measurement not yet "
                      "implemented. Hardware detected: "
                   << hw.output_name;
}

// -------------------------------------------------------------------------
// Scenario 4 — Heavy-chain loopback latency
// -------------------------------------------------------------------------

TEST(HardwareLoopback, Scenario4_HeavyChain_LatencyGate)
{
    auto hw = DetectLoopbackHardware();
    if (!hw.detected)
    {
        GTEST_SKIP() << "No loopback hardware detected.";
    }

    // TODO: load 5-plugin chain (e.g., ARC X → Sonarworks → ReaEQ →
    // FabFilter Pro-Q 3 → saturation).
    // TODO: measure worst-case round-trip latency.
    // TODO: gate: ≤ 20 ms at 256-sample buffer.

    SUCCEED() << "Placeholder — heavy-chain latency measurement not yet "
                      "implemented.";
}

// -------------------------------------------------------------------------
// Scenario 8 — CPU profiling
// -------------------------------------------------------------------------

TEST(HardwareLoopback, Scenario8_CpuProfile)
{
    auto hw = DetectLoopbackHardware();
    if (!hw.detected)
    {
        GTEST_SKIP() << "No loopback hardware detected.";
    }

    // TODO: run typical 3-plugin chain for 60 s.
    // TODO: record rolling-1s CPU usage from IAudioEngineListener.
    // TODO: gate: ≤ 5 % on reference hardware (Intel i5-8th-gen or
    //       Ryzen 5 3000-class).

    SUCCEED() << "Placeholder — CPU profiling not yet implemented.";
}

// -------------------------------------------------------------------------
// Scenario 14 — 12-hour soak
// -------------------------------------------------------------------------

TEST(HardwareLoopback, Scenario14_TwelveHourSoak)
{
    auto hw = DetectLoopbackHardware();
    if (!hw.detected)
    {
        GTEST_SKIP() << "No loopback hardware detected.";
    }

    // TODO: run typical chain with looping playlist for 12 hours.
    // TODO: monitor xrun count, memory growth, drift.
    // TODO: gate: 0 xruns, no drift, memory growth ≤ 200 MB.

    SUCCEED() << "Placeholder — 12-hour soak not yet implemented.";
}

// -------------------------------------------------------------------------
// Scenario 7 — Sample-rate transitions
// -------------------------------------------------------------------------

TEST(HardwareLoopback, Scenario7_SampleRateTransitions)
{
    auto hw = DetectLoopbackHardware();
    if (!hw.detected)
    {
        GTEST_SKIP() << "No loopback hardware detected.";
    }

    // TODO: iterate through 44.1 → 48 → 96 → 176.4 → 192 kHz.
    // TODO: at each rate, play 30 s of audio.
    // TODO: gate: no audible artifacts (measured via THD+N spike
    //       detection on loopback capture).

    SUCCEED() << "Placeholder — sample-rate transition test not yet "
                      "implemented.";
}

// -------------------------------------------------------------------------
// Main — emit Markdown report when run outside gtest
// -------------------------------------------------------------------------

static void EmitReport()
{
    LatencyReport report;
    report.AddSection("Hardware Configuration",
        "- USB interface: (set JYGLOBALVST_LOOPBACK_IFACE)\n"
        "- TRS cable: output 1/2 → input 1/2\n"
        "- Driver version: 0.1.0\n");
    report.AddSection("Scenario 4 — Single Plugin",
        "Status: NOT RUN (placeholder)\n");
    report.AddSection("Scenario 4 — Heavy Chain",
        "Status: NOT RUN (placeholder)\n");
    report.AddSection("Scenario 8 — CPU Profile",
        "Status: NOT RUN (placeholder)\n");
    report.AddSection("Scenario 14 — 12-Hour Soak",
        "Status: NOT RUN (placeholder)\n");
    report.AddSection("Scenario 7 — Sample-Rate Transitions",
        "Status: NOT RUN (placeholder)\n");

    std::filesystem::path outDir = "tests/latency/reports";
    std::filesystem::create_directories(outDir);

    auto now = std::time(nullptr);
    std::tm tm {};
    localtime_s(&tm, &now);
    std::ostringstream fname;
    fname << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_hardware_loopback.md";

    auto path = outDir / fname.str();
    report.WriteToFile(path);
    std::cout << "Report written to: " << path.string() << "\n";
}

}  // namespace jyglobalvst::testing

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    // Always emit a report stub, even if all tests were skipped.
    jyglobalvst::testing::EmitReport();
    return result;
}
