// src/audio-engine/routing/resampler.h
//
// T023 — JUCE WindowedSinc resampler binding wrapped in a pre-allocated state
// object. Two instances per session: one at the capture boundary (source SR →
// internal 32f), one at the output boundary (internal → hardware-negotiated).
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • prepare() allocates internal JUCE state on the UI thread.
//   • process() runs on the audio thread and MUST NOT allocate. Internal
//     JUCE Interpolators::WindowedSinc honors this guarantee per its
//     documentation; we wrap it to enforce the contract.
//   • Reset is required after any sample-rate change; reset() is also
//     UI-thread only.
// =====================================================================

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cstddef>

namespace jyglobalvst::engine {

class WindowedSincResampler
{
public:
    // ratio = sourceRate / targetRate. Caller-side it's typically the inverse.
    void prepare(double ratio, std::size_t max_block_size, std::size_t channels);

    void reset() noexcept;

    // Returns the number of output frames written. dst capacity MUST be at
    // least ceil(src_frames / ratio) + 8 (margin for sinc kernel windows).
    std::size_t process(const float* const* src_channels, float* const* dst_channels,
                        std::size_t src_frames) noexcept;

    // Streaming variant: returns output frames written and sets *consumed to
    // input frames actually used. Uses JUCE's safe overload so no out-of-bounds
    // reads occur if the interpolator needs slightly more input than available.
    std::size_t process(const float* const* src_channels, float* const* dst_channels,
                        std::size_t src_frames, std::size_t* consumed) noexcept;

    // Fixed-ratio variant that produces exactly `num_out` output frames from
    // at most `src_frames` input frames. Uses JUCE's safe overload so no
    // out-of-bounds reads occur if the interpolator needs slightly more input
    // than available. Sets *consumed to the input frames actually used.
    std::size_t process(const float* const* src_channels, float* const* dst_channels,
                        std::size_t num_out, std::size_t src_frames,
                        std::size_t* consumed) noexcept;

    // Adaptive (asynchronous) variant for clock-drift compensation. Produces
    // exactly `num_out` output frames using the CALLER-SUPPLIED `ratio` for this
    // call (input samples consumed per output sample), reading at most
    // `available_in` input frames. If more input is needed than available, the
    // tail is zero-filled (JUCE's numInputSamplesAvailable / wrapAround=0 path).
    // Sets *consumed to the input frames actually used so the caller can advance
    // its source read pointer by exactly that amount. The interpolator is
    // stateful across calls, so varying `ratio` per call performs continuous
    // asynchronous resampling.
    std::size_t processAdaptive(double ratio, const float* const* src_channels,
                                float* const* dst_channels, std::size_t num_out,
                                std::size_t available_in, std::size_t* consumed) noexcept;

    [[nodiscard]] double ratio() const noexcept { return ratio_; }

private:
    double ratio_ {1.0};
    std::size_t max_block_size_ {0};
    std::size_t channels_ {0};
    // One interpolator per channel — JUCE Interpolators are not channel-aware.
    juce::OwnedArray<juce::Interpolators::WindowedSinc> per_channel_;
};

}  // namespace jyglobalvst::engine
