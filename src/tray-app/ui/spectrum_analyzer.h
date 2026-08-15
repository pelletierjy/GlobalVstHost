// src/tray-app/ui/spectrum_analyzer.h
//
// Real-time spectrum view of the processed (post-chain) output. Pulls raw
// samples from the engine's lock-free tap, windows and FFTs them on the UI
// thread, and paints log-spaced bands with falling peak caps.

#pragma once

#include "jyglobalvst/audio_engine.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace jyglobalvst::tray
{

class SpectrumAnalyzer : public juce::Component
                       , private juce::Timer
{
public:
    explicit SpectrumAnalyzer(IAudioEngine* engine);

    void paint(juce::Graphics& g) override;

    // FFT size trades resolution against how "live" the display feels. 2048 bins
    // at 48 kHz gives ~23 Hz resolution over a ~43 ms window — enough to separate
    // bass content without smearing transients.
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kNumBands = 30;

    static constexpr float kMinDb = -72.0f;
    static constexpr float kMaxDb = 0.0f;
    static constexpr float kMinHz = 25.0f;
    static constexpr float kMaxHz = 20000.0f;

private:
    void timerCallback() override;

    // Recomputes band_db_ from a freshly captured block. Returns false when the
    // engine had nothing to give (not running yet), leaving the bands to decay.
    bool analyseLatestBlock();
    void decayBands();

    IAudioEngine* engine_ {nullptr};

    juce::dsp::FFT fft_ {kFftOrder};
    juce::dsp::WindowingFunction<float> window_ {static_cast<size_t>(kFftSize),
                                                 juce::dsp::WindowingFunction<float>::hann};

    // Scratch — allocated once, reused every frame.
    std::array<float, kFftSize * 2> fft_data_ {};

    std::array<float, kNumBands> band_db_ {};   // smoothed bar levels
    std::array<float, kNumBands> peak_db_ {};   // falling peak caps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

}  // namespace jyglobalvst::tray
