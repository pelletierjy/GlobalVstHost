#pragma once

#include <array>
#include <vector>
#include <cmath>

// REALTIME CONSTRAINTS HEADER
// ============================================================================
// This header implements BS.1770 K-weighting for loudness measurement.
// All state is preallocated; use is RT-safe once prepareToPlay() is called.
// ============================================================================

namespace jyglobalvst::engine {

class LoudnessMeter
{
public:
    LoudnessMeter();
    ~LoudnessMeter() = default;

    // Prepare for playback. Allocates ring buffers and initializes coefficients
    // for the given sample rate. Must be called before process().
    void prepareToPlay(double sample_rate, int max_samples_per_block);

    // Release resources. Safe to call even if not prepared.
    void releaseResources();

    // Process a block of stereo audio and update short-term loudness estimate.
    // Both arrays must be the same length (num_samples).
    // RT-safe: no allocation, locking, or I/O.
    void process(const float* left, const float* right, int num_samples) noexcept;

    // Returns the short-term loudness in LUFS (loudness units relative to full scale).
    // Updated continuously during process(); value is 0 if no audio has been processed.
    float shortTermLoudnessLufs() const noexcept { return short_term_loudness_lufs_; }

private:
    // K-weighting filter state (biquad).
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1_l = 0.f, z2_l = 0.f, z1_r = 0.f, z2_r = 0.f;

        void reset() { z1_l = z2_l = z1_r = z2_r = 0.f; }

        float processL(float in) noexcept;
        float processR(float in) noexcept;
    };

    double sample_rate_ = 0.0;
    int block_size_ = 0;

    // High-shelf and high-pass biquads for K-weighting.
    Biquad high_shelf_;
    Biquad high_pass_;

    // Ring buffer for short-term loudness (400 ms @ 48 kHz = 19200 samples).
    static constexpr int kShortTermWindowMs = 400;
    std::vector<float> short_term_buffer_l_;
    std::vector<float> short_term_buffer_r_;
    int buffer_write_pos_ = 0;
    int buffer_size_ = 0;

    float short_term_loudness_lufs_ = 0.f;

    void updateShortTermLoudness() noexcept;
    void computeKWeightingCoefficients(double sample_rate);
};

}  // namespace jyglobalvst::engine
