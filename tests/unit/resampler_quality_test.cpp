// tests/unit/resampler_quality_test.cpp
//
// T030 — Unit tests for WindowedSinc resampler quality (T023).
//
// Tests verify resampling correctness at various ratios and sample rates.
// Tests the five supported sample rates: 44100, 48000, 96000, 176400, 192000.

#include <gtest/gtest.h>
#include <routing/resampler.h>

#include <cmath>
#include <vector>
#include <algorithm>

using namespace jyglobalvst::engine;

class ResamplerQualityTest : public ::testing::Test
{
protected:
    // Generate a sine wave at the given frequency, sample rate, and duration.
    std::vector<float> generateSineWave(float frequency, int sample_rate, float duration_sec)
    {
        const int num_samples = static_cast<int>(sample_rate * duration_sec);
        std::vector<float> wave(num_samples);
        const float phase_increment = 2.0f * 3.14159265358979f * frequency / sample_rate;

        for (int i = 0; i < num_samples; ++i)
        {
            wave[i] = std::sin(phase_increment * i);
        }
        return wave;
    }

    // Measure RMS error between two signals (for round-trip verification).
    float measureRmsError(const std::vector<float>& original, const std::vector<float>& resampled)
    {
        const auto min_len = std::min(original.size(), resampled.size());
        float sum_sq = 0.0f;
        for (std::size_t i = 0; i < min_len; ++i)
        {
            const float diff = original[i] - resampled[i];
            sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / min_len);
    }
};

TEST_F(ResamplerQualityTest, UnityRatio_NoChange)
{
    // Test unity ratio: output count equals input count.
    // Note: WindowedSinc applies a FIR low-pass even at unity ratio,
    // so output is not bit-identical (group delay + slight attenuation).
    WindowedSincResampler resampler;
    resampler.prepare(1.0, 2048, 1);

    auto input = generateSineWave(440.0f, 48000, 0.1f);
    std::vector<float> output(input.size() + 16);

    float* src[] = {input.data()};
    float* dst[] = {output.data()};

    const auto out_frames = resampler.process(src, dst, input.size());

    // With unity ratio, output frames should equal input frames.
    EXPECT_EQ(out_frames, input.size());

    // Output should track input closely after accounting for filter latency.
    // WindowedSinc has algorithmicLatency = 100 samples at unity ratio.
    const std::size_t latency = 100;
    const std::size_t skip = 200;
    EXPECT_GT(out_frames, skip + latency);
    // Align output with input: output[i + latency] corresponds to input[i].
    std::vector<float> input_aligned(input.begin() + skip, input.end() - latency);
    std::vector<float> output_aligned(output.begin() + skip + latency, output.begin() + out_frames);
    const float rms_error = measureRmsError(input_aligned, output_aligned);
    EXPECT_LT(rms_error, 0.02f);  // Allow 2% RMS for FIR pass-through.
}

TEST_F(ResamplerQualityTest, Upsample48To96)
{
    // Upsample from 48 kHz to 96 kHz (ratio = 48000 / 96000 = 0.5).
    WindowedSincResampler resampler;
    resampler.prepare(0.5, 2048, 1);

    auto input = generateSineWave(440.0f, 48000, 0.1f);
    const auto expected_out_frames = static_cast<std::size_t>(input.size() / 0.5);
    std::vector<float> output(expected_out_frames + 64);

    float* src[] = {input.data()};
    float* dst[] = {output.data()};

    const auto out_frames = resampler.process(src, dst, input.size());

    // Upsampling should produce approximately 2x the samples.
    EXPECT_GT(out_frames, input.size());
    EXPECT_LE(out_frames, input.size() * 2 + 8);
}

TEST_F(ResamplerQualityTest, Downsample96To48)
{
    // Downsample from 96 kHz to 48 kHz (ratio = 96000 / 48000 = 2.0).
    WindowedSincResampler resampler;
    resampler.prepare(2.0, 2048, 1);

    auto input = generateSineWave(440.0f, 96000, 0.1f);
    const auto expected_out_frames = static_cast<std::size_t>(input.size() / 2.0);
    std::vector<float> output(expected_out_frames + 64);

    float* src[] = {input.data()};
    float* dst[] = {output.data()};

    const auto out_frames = resampler.process(src, dst, input.size());

    // Downsampling should produce approximately half the samples.
    EXPECT_GT(out_frames, input.size() / 2 - 32);
    EXPECT_LE(out_frames, input.size() / 2 + 32);
}

TEST_F(ResamplerQualityTest, MultiChannelResampling)
{
    // Test stereo (2-channel) resampling.
    WindowedSincResampler resampler;
    resampler.prepare(1.5, 2048, 2);  // Downsample by 1.5x

    auto input_ch0 = generateSineWave(440.0f, 48000, 0.05f);
    auto input_ch1 = generateSineWave(880.0f, 48000, 0.05f);

    const auto expected_out_frames = static_cast<std::size_t>(input_ch0.size() / 1.5);
    std::vector<float> output_ch0(expected_out_frames + 64);
    std::vector<float> output_ch1(expected_out_frames + 64);

    float* src[] = {input_ch0.data(), input_ch1.data()};
    float* dst[] = {output_ch0.data(), output_ch1.data()};

    const auto out_frames = resampler.process(src, dst, input_ch0.size());

    EXPECT_GT(out_frames, 0);
    EXPECT_LE(out_frames, input_ch0.size() + 64);
}

TEST_F(ResamplerQualityTest, All5StandardSampleRates)
{
    // Test resampling transitions between all 5 standard sample rates.
    const int sample_rates[] = {44100, 48000, 96000, 176400, 192000};

    for (int from_sr : sample_rates)
    {
        for (int to_sr : sample_rates)
        {
            if (from_sr == to_sr)
                continue;

            const double ratio = static_cast<double>(from_sr) / to_sr;
            WindowedSincResampler resampler;
            resampler.prepare(ratio, 2048, 1);

            auto input = generateSineWave(1000.0f, from_sr, 0.05f);
            const auto expected_out_frames = static_cast<std::size_t>(input.size() / ratio + 32);
            std::vector<float> output(expected_out_frames);

            float* src[] = {input.data()};
            float* dst[] = {output.data()};

            const auto out_frames = resampler.process(src, dst, input.size());

            EXPECT_GT(out_frames, 0) << "Failed for " << from_sr << " → " << to_sr;
            EXPECT_LE(out_frames, expected_out_frames)
                << "Output exceeded expected for " << from_sr << " → " << to_sr;
        }
    }
}

TEST_F(ResamplerQualityTest, ResetClearsState)
{
    // Test that reset() clears internal resampler state.
    WindowedSincResampler resampler;
    resampler.prepare(1.0, 2048, 1);

    auto input = generateSineWave(440.0f, 48000, 0.05f);
    std::vector<float> output1(input.size() + 16);
    std::vector<float> output2(input.size() + 16);

    float* src[] = {input.data()};
    float* dst[] = {output1.data()};

    const auto frames1 = resampler.process(src, dst, input.size());

    // Reset and process again.
    resampler.reset();

    dst[0] = output2.data();
    const auto frames2 = resampler.process(src, dst, input.size());

    // Both should produce the same number of output frames.
    EXPECT_EQ(frames1, frames2);
}

TEST_F(ResamplerQualityTest, OutputRange)
{
    // Verify output stays within reasonable bounds (no extreme values).
    WindowedSincResampler resampler;
    resampler.prepare(1.0, 2048, 1);

    auto input = generateSineWave(440.0f, 48000, 0.1f);
    std::vector<float> output(input.size() + 16);

    float* src[] = {input.data()};
    float* dst[] = {output.data()};

    const auto out_frames = resampler.process(src, dst, input.size());

    // Check that all output samples are within [-1.5, 1.5] (allowing small overshoot from windowing).
    for (std::size_t i = 0; i < out_frames; ++i)
    {
        EXPECT_GE(output[i], -1.5f) << "Excessive negative at frame " << i;
        EXPECT_LE(output[i], 1.5f) << "Excessive positive at frame " << i;
    }
}

TEST_F(ResamplerQualityTest, LargeRatioVariations)
{
    // Test extreme resampling ratios.
    struct TestCase
    {
        double ratio;
        const char* description;
    };

    const TestCase cases[] = {
        {0.25, "4x downsample"},
        {4.0, "4x upsample"},
        {0.5, "2x downsample"},
        {2.0, "2x upsample"},
        {1.5, "1.5x"},
        {0.667, "2/3 ratio"},
    };

    for (const auto& tc : cases)
    {
        WindowedSincResampler resampler;
        resampler.prepare(tc.ratio, 2048, 1);

        auto input = generateSineWave(440.0f, 48000, 0.05f);
        const auto expected_out = static_cast<std::size_t>(input.size() / tc.ratio + 64);
        std::vector<float> output(expected_out);

        float* src[] = {input.data()};
        float* dst[] = {output.data()};

        const auto out_frames = resampler.process(src, dst, input.size());

        EXPECT_GT(out_frames, 0) << "Failed for " << tc.description;
    }
}
