// src/tray-app/soak/soak_runner.cpp
//
// See soak_runner.h. Not real-time code: plain sleeps and file I/O are fine
// here because no audio callback runs on this thread.

#include "soak_runner.h"

#include "jyglobalvst/audio_engine.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <thread>

namespace jyglobalvst::tray {

bool parseSoakOptions(const juce::String& commandLine, SoakOptions& opts)
{
    juce::StringArray args;
    args.addTokens(commandLine, true);
    args.trim();

    if (!args.contains("--soak-test"))
    {
        return false;
    }

    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--duration-minutes" && i + 1 < args.size())
        {
            opts.duration_minutes = std::max(0.0, args[i + 1].getDoubleValue());
        }
        else if (args[i] == "--report" && i + 1 < args.size())
        {
            opts.report_path = args[i + 1].unquoted();
        }
    }
    return true;
}

int runSoakTest(const SoakOptions& opts)
{
    auto engine = createAudioEngine();
    engine->start();
    // Give the device graph a moment to spin up before the first sample.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const bool started = engine->isRunning();

    const std::uint64_t xruns_start = engine->cpuStats().xrun_count_session;

    double rolling_sum = 0.0;
    int samples = 0;
    float rolling_peak = 0.0f;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::duration<double>(opts.duration_minutes * 60.0);

    do
    {
        const auto stats = engine->cpuStats();
        rolling_sum += stats.rolling_1s_pct;
        rolling_peak = std::max(rolling_peak, stats.rolling_1s_pct);
        ++samples;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (std::chrono::steady_clock::now() < deadline);

    const auto cpu = engine->cpuStats();
    const auto lat = engine->latencyProfile();
    engine->stop();
    engine.reset();

    const std::uint64_t xruns = cpu.xrun_count_session - xruns_start;
    const double rolling_avg = samples > 0 ? rolling_sum / static_cast<double>(samples) : 0.0;
    const int exit_code = !started ? 2 : (xruns == 0 ? 0 : 1);

    std::ofstream out(opts.report_path.toStdString(), std::ios::trunc);
    if (out)
    {
        out.precision(4);
        out << std::fixed;
        out << "{\n"
            << "  \"tool\": \"jyglobalvst_tray --soak-test\",\n"
            << "  \"duration_minutes_requested\": " << opts.duration_minutes << ",\n"
            << "  \"samples\": " << samples << ",\n"
            << "  \"engine_started\": " << (started ? "true" : "false") << ",\n"
            << "  \"cpu_rolling_1s_pct_avg\": " << rolling_avg << ",\n"
            << "  \"cpu_rolling_1s_pct_peak\": " << rolling_peak << ",\n"
            << "  \"cpu_instantaneous_pct_final\": " << cpu.instantaneous_pct << ",\n"
            << "  \"xruns_during_soak\": " << xruns << ",\n"
            << "  \"xrun_count_session_final\": " << cpu.xrun_count_session << ",\n"
            << "  \"latency_ms\": {\n"
            << "    \"capture\": " << lat.capture_ms << ",\n"
            << "    \"resample\": " << lat.resample_ms << ",\n"
            << "    \"plugin_chain\": " << lat.plugin_chain_ms << ",\n"
            << "    \"output\": " << lat.output_ms << ",\n"
            << "    \"total_round_trip\": " << lat.total_round_trip_ms << "\n"
            << "  },\n"
            << "  \"passed\": " << ((exit_code == 0) ? "true" : "false") << "\n"
            << "}\n";
    }
    else if (exit_code == 0)
    {
        // Report write failure is itself a soak failure: CI uploads this file.
        return 3;
    }

    return exit_code;
}

}  // namespace jyglobalvst::tray
