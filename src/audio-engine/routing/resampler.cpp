// src/audio-engine/routing/resampler.cpp
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • process() does NOT allocate. JUCE Interpolators::WindowedSinc is
//     stateful but heap-stable after prepare() — verified by the unit
//     test resampler_quality_test (T030).
// =====================================================================

#include "resampler.h"

#include <algorithm>

namespace jyglobalvst::engine {

void WindowedSincResampler::prepare(double ratio, std::size_t max_block_size, std::size_t channels)
{
    ratio_ = ratio > 0.0 ? ratio : 1.0;
    max_block_size_ = max_block_size;
    channels_ = channels;

    per_channel_.clearQuick(true);
    for (std::size_t c = 0; c < channels; ++c)
    {
        per_channel_.add(new juce::Interpolators::WindowedSinc());
    }
    reset();
}

void WindowedSincResampler::reset() noexcept
{
    for (auto* interp : per_channel_)
    {
        interp->reset();
    }
}

std::size_t WindowedSincResampler::process(const float* const* src_channels, float* const* dst_channels,
                                           std::size_t src_frames) noexcept
{
    if (per_channel_.isEmpty() || src_frames == 0)
    {
        return 0;
    }

    // JUCE interpolators: last argument is numOutputSamplesToProduce.
    // process() returns input samples consumed; output count is the requested numOut.
    const int num_out = static_cast<int>(src_frames / ratio_);
    for (int c = 0; c < per_channel_.size(); ++c)
    {
        (void)per_channel_[c]->process(ratio_, src_channels[c], dst_channels[c], num_out);
    }
    return static_cast<std::size_t>(num_out);
}

std::size_t WindowedSincResampler::process(const float* const* src_channels, float* const* dst_channels,
                                           std::size_t src_frames, std::size_t* consumed) noexcept
{
    *consumed = 0;
    if (per_channel_.isEmpty() || src_frames == 0)
    {
        return 0;
    }

    const int num_out = static_cast<int>(src_frames / ratio_);
    if (num_out <= 0)
    {
        return 0;
    }

    int total_consumed = 0;
    for (int c = 0; c < per_channel_.size(); ++c)
    {
        int used = per_channel_[c]->process(ratio_, src_channels[c], dst_channels[c], num_out,
                                            static_cast<int>(src_frames), 0);
        if (c == 0)
        {
            total_consumed = used;
        }
    }
    *consumed = static_cast<std::size_t>(total_consumed);
    return static_cast<std::size_t>(num_out);
}

std::size_t WindowedSincResampler::process(const float* const* src_channels, float* const* dst_channels,
                                           std::size_t num_out, std::size_t src_frames,
                                           std::size_t* consumed) noexcept
{
    *consumed = 0;
    if (per_channel_.isEmpty() || num_out == 0)
    {
        return 0;
    }

    int total_consumed = 0;
    for (int c = 0; c < per_channel_.size(); ++c)
    {
        if (dst_channels[c] == nullptr || src_channels[c] == nullptr)
        {
            continue;
        }
        const int u = per_channel_[c]->process(ratio_, src_channels[c], dst_channels[c],
                                                static_cast<int>(num_out),
                                                static_cast<int>(src_frames), 0);
        if (c == 0)
        {
            total_consumed = u;
        }
    }
    *consumed = static_cast<std::size_t>(total_consumed);
    return num_out;
}

std::size_t WindowedSincResampler::processAdaptive(double ratio, const float* const* src_channels,
                                                   float* const* dst_channels, std::size_t num_out,
                                                   std::size_t available_in,
                                                   std::size_t* consumed) noexcept
{
    if (consumed != nullptr)
    {
        *consumed = 0;
    }
    if (per_channel_.isEmpty() || num_out == 0)
    {
        return 0;
    }

    const double r = ratio > 0.0 ? ratio : 1.0;
    int used = 0;
    for (int c = 0; c < per_channel_.size(); ++c)
    {
        if (dst_channels[c] == nullptr || src_channels[c] == nullptr)
        {
            continue;
        }
        // JUCE's numInputSamplesAvailable / wrapAround overload: reads at most
        // available_in input samples and feeds zeroes past that (wrapAround=0).
        // Returns the number of input samples actually consumed. All channels
        // advance identically (same ratio, same available_in, same state
        // evolution), so channel 0's count is authoritative.
        const int u = per_channel_[c]->process(r, src_channels[c], dst_channels[c],
                                                static_cast<int>(num_out),
                                                static_cast<int>(available_in), 0);
        if (c == 0)
        {
            used = u;
        }
    }

    if (consumed != nullptr)
    {
        *consumed = static_cast<std::size_t>(used);
    }
    return num_out;
}

}  // namespace jyglobalvst::engine
