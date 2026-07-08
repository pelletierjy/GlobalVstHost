#include "loudness_meter.h"
#include <algorithm>
#include <cmath>
#include <numbers>

constexpr double kPi = 3.14159265358979323846;

namespace jyglobalvst::engine {

float LoudnessMeter::Biquad::processL(float in) noexcept
{
    const float out = b0 * in + z1_l;
    z1_l = b1 * in - a1 * out + z2_l;
    z2_l = b2 * in - a2 * out;
    return out;
}

float LoudnessMeter::Biquad::processR(float in) noexcept
{
    const float out = b0 * in + z1_r;
    z1_r = b1 * in - a1 * out + z2_r;
    z2_r = b2 * in - a2 * out;
    return out;
}

LoudnessMeter::LoudnessMeter() = default;

void LoudnessMeter::prepareToPlay(double sample_rate, int max_samples_per_block)
{
    sample_rate_ = sample_rate;
    block_size_ = max_samples_per_block;

    // Allocate short-term ring buffer (400 ms @ 48 kHz = 19200 samples).
    buffer_size_ = static_cast<int>(sample_rate * kShortTermWindowMs / 1000.0);
    short_term_buffer_l_.assign(buffer_size_, 0.f);
    short_term_buffer_r_.assign(buffer_size_, 0.f);
    buffer_write_pos_ = 0;

    // Compute K-weighting biquad coefficients.
    computeKWeightingCoefficients(sample_rate);
    high_shelf_.reset();
    high_pass_.reset();

    short_term_loudness_lufs_ = 0.f;
}

void LoudnessMeter::releaseResources()
{
    short_term_buffer_l_.clear();
    short_term_buffer_r_.clear();
    buffer_size_ = 0;
    buffer_write_pos_ = 0;
}

void LoudnessMeter::process(const float* left, const float* right, int num_samples) noexcept
{
    if (buffer_size_ == 0 || !left || !right)
        return;

    for (int i = 0; i < num_samples; ++i)
    {
        // K-weighting: high-shelf + high-pass.
        float l = high_shelf_.processL(left[i]);
        float r = high_shelf_.processR(right[i]);
        l = high_pass_.processL(l);
        r = high_pass_.processR(r);

        // Mean square (pre-loudness).
        float ms = (l * l + r * r) * 0.5f;

        // Write to ring buffer.
        short_term_buffer_l_[buffer_write_pos_] = ms;
        short_term_buffer_r_[buffer_write_pos_] = ms;
        buffer_write_pos_ = (buffer_write_pos_ + 1) % buffer_size_;
    }

    updateShortTermLoudness();
}

void LoudnessMeter::updateShortTermLoudness() noexcept
{
    // Compute mean square over the ring buffer.
    double sum = 0.0;
    for (int i = 0; i < buffer_size_; ++i)
    {
        sum += short_term_buffer_l_[i];
    }

    double mean_square = sum / buffer_size_;

    // Gate: if mean-square is below a threshold (~-70 LUFS), don't update.
    // -70 LUFS is roughly 10e-7 linear.
    constexpr double kGateThresholdLinear = 1e-7;
    if (mean_square < kGateThresholdLinear)
    {
        short_term_loudness_lufs_ = -std::numeric_limits<float>::infinity();
        return;
    }

    // Convert mean square to LUFS: LUFS = -0.691 + 10 * log10(mean_square)
    // (The -0.691 is the reference loudness for PPM full scale.)
    const double lufs = -0.691 + 10.0 * std::log10(mean_square);
    short_term_loudness_lufs_ = static_cast<float>(lufs);
}

void LoudnessMeter::computeKWeightingCoefficients(double sample_rate)
{
    // BS.1770 K-weighting: 2nd-order high-shelf + 2nd-order high-pass.
    // High-shelf: +4 dB @ 2 kHz, Q = 0.7.
    // High-pass: 75 Hz, Q = 0.5.

    const double f_hs = 2000.0;  // high-shelf center
    const double q_hs = 0.7;
    const double gain_hs = 4.0;  // dB
    const double a_hs = std::pow(10.0, gain_hs / 40.0);

    // High-shelf biquad coefficients.
    const double w0_hs = 2.0 * kPi * f_hs / sample_rate;
    const double sin_w0 = std::sin(w0_hs);
    const double cos_w0 = std::cos(w0_hs);
    const double alpha = sin_w0 / (2.0 * q_hs);

    const double b0_hs = a_hs * ((a_hs + 1.0) - (a_hs - 1.0) * cos_w0 + 2.0 * std::sqrt(a_hs) * alpha);
    const double b1_hs = 2.0 * a_hs * ((a_hs - 1.0) - (a_hs + 1.0) * cos_w0);
    const double b2_hs = a_hs * ((a_hs + 1.0) - (a_hs - 1.0) * cos_w0 - 2.0 * std::sqrt(a_hs) * alpha);
    const double a0_hs = (a_hs + 1.0) + (a_hs - 1.0) * cos_w0 + 2.0 * std::sqrt(a_hs) * alpha;
    const double a1_hs = -2.0 * ((a_hs - 1.0) + (a_hs + 1.0) * cos_w0);
    const double a2_hs = (a_hs + 1.0) + (a_hs - 1.0) * cos_w0 - 2.0 * std::sqrt(a_hs) * alpha;

    high_shelf_.b0 = static_cast<float>(b0_hs / a0_hs);
    high_shelf_.b1 = static_cast<float>(b1_hs / a0_hs);
    high_shelf_.b2 = static_cast<float>(b2_hs / a0_hs);
    high_shelf_.a1 = static_cast<float>(a1_hs / a0_hs);
    high_shelf_.a2 = static_cast<float>(a2_hs / a0_hs);

    // High-pass biquad: 75 Hz, Q = 0.5.
    const double f_hp = 75.0;
    const double q_hp = 0.5;
    const double w0_hp = 2.0 * kPi * f_hp / sample_rate;
    const double sin_w0_hp = std::sin(w0_hp);
    const double cos_w0_hp = std::cos(w0_hp);
    const double alpha_hp = sin_w0_hp / (2.0 * q_hp);

    const double b0_hp = (1.0 + cos_w0_hp) / 2.0;
    const double b1_hp = -(1.0 + cos_w0_hp);
    const double b2_hp = (1.0 + cos_w0_hp) / 2.0;
    const double a0_hp = 1.0 + alpha_hp;
    const double a1_hp = -2.0 * cos_w0_hp;
    const double a2_hp = 1.0 - alpha_hp;

    high_pass_.b0 = static_cast<float>(b0_hp / a0_hp);
    high_pass_.b1 = static_cast<float>(b1_hp / a0_hp);
    high_pass_.b2 = static_cast<float>(b2_hp / a0_hp);
    high_pass_.a1 = static_cast<float>(a1_hp / a0_hp);
    high_pass_.a2 = static_cast<float>(a2_hp / a0_hp);
}

}  // namespace jyglobalvst::engine
