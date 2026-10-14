// src/tray-app/soak/soak_runner.h
//
// Headless soak-test mode for jyglobalvst_tray (--soak-test). Runs the audio
// engine for a fixed duration with an empty chain, samples CPU/xrun stats, and
// writes a JSON report consumed by the CI soak job (audio-pr-gate.yml).

#pragma once

#include <juce_core/juce_core.h>

namespace jyglobalvst::tray {

struct SoakOptions
{
    double duration_minutes{30.0};
    juce::String report_path{"soak_report.json"};
};

// Returns true if commandLine requests soak mode (--soak-test) and fills opts.
bool parseSoakOptions(const juce::String& commandLine, SoakOptions& opts);

// Runs the engine for opts.duration_minutes and writes the JSON report to
// opts.report_path. Return value becomes the process exit code:
//   0 = clean soak (engine ran, no xruns observed)
//   1 = xruns observed during the soak window
//   2 = engine failed to start (no usable audio endpoint)
int runSoakTest(const SoakOptions& opts);

}  // namespace jyglobalvst::tray
