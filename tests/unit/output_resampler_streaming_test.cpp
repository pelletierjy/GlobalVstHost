// tests/unit/output_resampler_streaming_test.cpp
//
// Regression tests for the chain → device output resampling bridge used by the
// ASIO / JUCE-callback path (audio_engine_impl.cpp, step 4 of the audio
// callback).
//
// The bug these pin down: the callback used to size the chain block as
// ceil(num_samples * ratio) + 2, hand all of it to the resampler, and then throw
// away whatever the resampler did not consume. WindowedSincResampler keeps its
// filter state across calls but NOT unconsumed input, so those surplus frames
// were removed from the signal on every single callback — a periodic
// discontinuity that is plainly audible on any mismatched rate pair.
//
// The fix routes chain output through a FIFO and advances it only by the frames
// the resampler reports consuming. These tests model that loop exactly.

#include <gtest/gtest.h>

#include <concurrency/lockfree_ring_buffer.h>
#include <routing/resampler.h>

#include <cmath>
#include <vector>

using jyglobalvst::engine::WindowedSincResampler;
using jyglobalvst::shared::LockFreeAudioRingBuffer;

namespace {

struct BridgeConfig
{
    double chain_rate;
    double device_rate;
    int device_block;
};

// Mirrors audioDeviceAboutToStart() + the output leg of the audio callback.
class OutputBridge
{
public:
    explicit OutputBridge(const BridgeConfig& cfg)
        : ratio_(cfg.chain_rate / cfg.device_rate)
        , device_block_(cfg.device_block)
        , chain_block_(static_cast<int>(std::ceil(cfg.device_block * ratio_)) + 8)
        , fifo_needed_(static_cast<int>(std::ceil(cfg.device_block * ratio_)) + 4)
        , fifo_(static_cast<std::size_t>(chain_block_) * 4, 2)
        , peek_buffer_(static_cast<std::size_t>(chain_block_) * 4)
    {
        resampler_.prepare(ratio_, static_cast<std::size_t>(chain_block_), 2);
    }

    int chainBlock() const { return chain_block_; }

    // One device callback. Pulls chain blocks from `source` (advancing
    // source_pos_) until the FIFO can satisfy this block, then resamples exactly
    // device_block_ frames out. Appends them to `out`.
    void runCallback(const std::vector<float>& source, std::vector<float>& out)
    {
        while (static_cast<int>(fifo_.available()) < fifo_needed_)
        {
            std::vector<float> block(static_cast<std::size_t>(chain_block_), 0.0f);
            for (int i = 0; i < chain_block_; ++i)
            {
                const std::size_t idx = source_pos_ + static_cast<std::size_t>(i);
                block[static_cast<std::size_t>(i)] = idx < source.size() ? source[idx] : 0.0f;
            }
            source_pos_ += static_cast<std::size_t>(chain_block_);

            const float* src[2] = {block.data(), block.data()};
            const std::size_t written = fifo_.tryWrite(src, static_cast<std::size_t>(chain_block_));
            frames_pushed_ += written;
            ASSERT_EQ(written, static_cast<std::size_t>(chain_block_))
                << "FIFO overflowed — capacity is too small for this ratio";
        }

        float* peek_dst[2] = {peek_buffer_.data(), peek_buffer_.data()};
        const std::size_t peek_n =
            std::min(static_cast<std::size_t>(fifo_needed_), fifo_.available());
        const std::size_t peeked = fifo_.peek(peek_dst, peek_n);

        std::vector<float> device_block(static_cast<std::size_t>(device_block_), 0.0f);
        const float* chain_src[2] = {peek_buffer_.data(), peek_buffer_.data()};
        float* dst[2] = {device_block.data(), device_block.data()};

        std::size_t consumed = 0;
        resampler_.process(chain_src, dst, static_cast<std::size_t>(device_block_), peeked, &consumed);
        fifo_.advanceRead(consumed);
        frames_consumed_ += consumed;

        out.insert(out.end(), device_block.begin(), device_block.end());
    }

    std::size_t framesPushed() const { return frames_pushed_; }
    std::size_t framesConsumed() const { return frames_consumed_; }
    std::size_t fifoRemaining() const { return fifo_.available(); }

private:
    double ratio_;
    int device_block_;
    int chain_block_;
    int fifo_needed_;
    LockFreeAudioRingBuffer fifo_;
    std::vector<float> peek_buffer_;
    WindowedSincResampler resampler_;
    std::size_t source_pos_ = 0;
    std::size_t frames_pushed_ = 0;
    std::size_t frames_consumed_ = 0;
};

}  // namespace

// Every chain frame produced is either consumed by the resampler or still
// sitting in the FIFO. Nothing may silently vanish — that is precisely what the
// old "+2 then discard" scheme did on every callback.
TEST(OutputResamplerStreaming, ConservesEveryChainFrame)
{
    const BridgeConfig cfgs[] = {
        {96000.0, 48000.0, 256},
        {44100.0, 48000.0, 256},
        {192000.0, 44100.0, 128},
        {48000.0, 96000.0, 512},
    };

    for (const auto& cfg : cfgs)
    {
        OutputBridge bridge(cfg);
        std::vector<float> source(400000, 0.0f);
        std::vector<float> out;

        for (int cb = 0; cb < 300; ++cb)
        {
            bridge.runCallback(source, out);
        }

        EXPECT_EQ(bridge.framesPushed(), bridge.framesConsumed() + bridge.fifoRemaining())
            << "chain " << cfg.chain_rate << " → device " << cfg.device_rate;
    }
}

// The FIFO must not creep upward over time; steady-state occupancy stays bounded
// well under one chain block, otherwise the bridge is accumulating latency.
TEST(OutputResamplerStreaming, FifoOccupancyStaysBounded)
{
    OutputBridge bridge({96000.0, 48000.0, 256});
    std::vector<float> source(600000, 0.0f);
    std::vector<float> out;

    for (int cb = 0; cb < 50; ++cb)
    {
        bridge.runCallback(source, out);
    }
    const std::size_t early = bridge.fifoRemaining();

    for (int cb = 0; cb < 1000; ++cb)
    {
        bridge.runCallback(source, out);
    }
    const std::size_t late = bridge.fifoRemaining();

    EXPECT_LE(late, static_cast<std::size_t>(bridge.chainBlock()) * 2)
        << "FIFO grew unbounded — added latency accumulates";
    // Occupancy oscillates within a block; it must not have drifted by more than
    // that between the two sample points.
    EXPECT_LE(late < early ? early - late : late - early,
              static_cast<std::size_t>(bridge.chainBlock()) * 2);
}

// A linear ramp survives a symmetric windowed-sinc unchanged (up to gain and
// group delay), so its output must have a constant first difference of exactly
// the rate ratio. Any dropped input frame shows up as a step in that difference.
// This is the direct signal-domain regression test for the discarded-frames bug.
TEST(OutputResamplerStreaming, RampStaysContinuousAcrossCallbacks)
{
    const BridgeConfig cfg{96000.0, 48000.0, 256};
    const double expected_delta = cfg.chain_rate / cfg.device_rate;

    OutputBridge bridge(cfg);

    // The ramp spans [0, 1) over the whole source. Keeping the magnitude near
    // unity matters: float epsilon scales with the value, and a first difference
    // taken between two large ramp samples is dominated by that noise rather than
    // by the effect under test. At this scale the noise floor sits around 1% of
    // the expected delta, while a single dropped chain frame shifts it by 100%.
    std::vector<float> source(400000);
    const double slope = 1.0 / static_cast<double>(source.size());
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<float>(static_cast<double>(i) * slope);
    }

    std::vector<float> out;
    for (int cb = 0; cb < 200; ++cb)
    {
        bridge.runCallback(source, out);
    }

    // Skip the filter's start-up transient.
    const std::size_t skip = 1024;
    ASSERT_GT(out.size(), skip + 1000);

    const double expected = expected_delta * slope;
    double worst = 0.0;
    std::size_t worst_at = 0;
    for (std::size_t i = skip + 1; i < out.size(); ++i)
    {
        const double delta = static_cast<double>(out[i]) - static_cast<double>(out[i - 1]);
        const double err = std::abs(delta - expected);
        if (err > worst)
        {
            worst = err;
            worst_at = i;
        }
    }

    // Threshold sits in the wide gap between two very different magnitudes:
    //   • JUCE's WindowedSinc interpolates a sinc lookup table, so its output
    //     carries a small phase-dependent ripple — about 9% of the expected delta
    //     here, roughly -115 dBFS in absolute terms. Harmless and unavoidable.
    //   • A single dropped chain frame shifts the local slope by a FULL expected
    //     delta — 100%, an order of magnitude above that ripple. The pre-fix code
    //     dropped two frames per callback and fails this decisively.
    EXPECT_LT(worst, expected * 0.3)
        << "discontinuity at output frame " << worst_at
        << " — chain frames are being dropped between callbacks";
}
