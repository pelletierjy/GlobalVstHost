// src/shared/concurrency/lockfree_ring_buffer.h
//
// Lock-free single-producer / single-consumer stereo audio ring buffer.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • The consumer (ASIO callback) MUST NOT block. tryRead is lock-free.
//   • The producer (WASAPI capture thread) MUST NOT block. tryWrite is
//     lock-free.
//   • Buffer is pre-allocated at construction; no allocation on write/read.
//   • Capacity is rounded up to a power of two so the index mask is a
//     single AND.
// =====================================================================

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace jyglobalvst::shared {

namespace {

constexpr std::size_t nextPowerOfTwo(std::size_t v) noexcept
{
    std::size_t p = 1;
    while (p < v)
    {
        p <<= 1;
    }
    return p;
}

}  // unnamed namespace

class LockFreeAudioRingBuffer
{
public:
    LockFreeAudioRingBuffer(std::size_t max_frames, std::size_t channels)
        : max_frames_(nextPowerOfTwo(max_frames))
        , channels_(channels)
        , mask_(max_frames_ - 1)
        , buffer_(max_frames_ * channels_)
    {
    }

    LockFreeAudioRingBuffer(const LockFreeAudioRingBuffer&) = delete;
    LockFreeAudioRingBuffer& operator=(const LockFreeAudioRingBuffer&) = delete;
    LockFreeAudioRingBuffer(LockFreeAudioRingBuffer&&) = delete;
    LockFreeAudioRingBuffer& operator=(LockFreeAudioRingBuffer&&) = delete;

    // Producer-only (WASAPI capture thread).
    // Returns number of frames actually written (may be less than requested).
    [[nodiscard]] std::size_t tryWrite(const float* const* src,
                                       std::size_t frames) noexcept
    {
        const auto w = write_idx_.load(std::memory_order_relaxed);
        const auto r = read_idx_.load(std::memory_order_acquire);
        const auto avail = (r - w - 1) & mask_;
        const auto n = (avail < frames) ? avail : frames;
        for (std::size_t c = 0; c < channels_; ++c)
        {
            const float* src_ch = src[c];
            for (std::size_t i = 0; i < n; ++i)
            {
                buffer_[c * max_frames_ + ((w + i) & mask_)] = src_ch[i];
            }
        }
        write_idx_.store((w + n) & mask_, std::memory_order_release);
        return n;
    }

    // Consumer-only (ASIO callback).
    // Returns number of frames actually read (may be less than requested).
    [[nodiscard]] std::size_t tryRead(float* const* dst,
                                      std::size_t frames) noexcept
    {
        const auto r = read_idx_.load(std::memory_order_relaxed);
        const auto w = write_idx_.load(std::memory_order_acquire);
        const auto avail = (w - r) & mask_;
        const auto n = (avail < frames) ? avail : frames;
        for (std::size_t c = 0; c < channels_; ++c)
        {
            float* dst_ch = dst[c];
            for (std::size_t i = 0; i < n; ++i)
            {
                dst_ch[i] = buffer_[c * max_frames_ + ((r + i) & mask_)];
            }
        }
        read_idx_.store((r + n) & mask_, std::memory_order_release);
        return n;
    }

    // Consumer-only (ASIO callback). Copies up to `frames` frames from the
    // current read position WITHOUT advancing the read pointer. Returns the
    // number of frames actually copied. Pair with advanceRead() so the read
    // pointer only moves by the amount a downstream resampler truly consumed —
    // this prevents silently dropping the frames the resampler didn't use.
    [[nodiscard]] std::size_t peek(float* const* dst, std::size_t frames) const noexcept
    {
        const auto r = read_idx_.load(std::memory_order_relaxed);
        const auto w = write_idx_.load(std::memory_order_acquire);
        const auto avail = (w - r) & mask_;
        const auto n = (avail < frames) ? avail : frames;
        for (std::size_t c = 0; c < channels_; ++c)
        {
            float* dst_ch = dst[c];
            for (std::size_t i = 0; i < n; ++i)
            {
                dst_ch[i] = buffer_[c * max_frames_ + ((r + i) & mask_)];
            }
        }
        return n;
    }

    // Consumer-only (ASIO callback). Advances the read pointer by up to
    // `frames` (clamped to what is available). Use after peek().
    void advanceRead(std::size_t frames) noexcept
    {
        const auto r = read_idx_.load(std::memory_order_relaxed);
        const auto w = write_idx_.load(std::memory_order_acquire);
        const auto avail = (w - r) & mask_;
        const auto n = (avail < frames) ? avail : frames;
        read_idx_.store((r + n) & mask_, std::memory_order_release);
    }

    // Consumer-only. Returns frames available to read.
    [[nodiscard]] std::size_t available() const noexcept
    {
        const auto w = write_idx_.load(std::memory_order_acquire);
        const auto r = read_idx_.load(std::memory_order_acquire);
        return (w - r) & mask_;
    }

    // Both sides. Resets buffer to empty.
    void clear() noexcept
    {
        write_idx_.store(0, std::memory_order_release);
        read_idx_.store(0, std::memory_order_release);
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return mask_; }
    [[nodiscard]] std::size_t channels() const noexcept { return channels_; }

private:
    std::size_t max_frames_;
    std::size_t channels_;
    std::size_t mask_;
    std::vector<float> buffer_;
    alignas(64) std::atomic<std::size_t> write_idx_ {0};
    alignas(64) std::atomic<std::size_t> read_idx_ {0};
};

}  // namespace jyglobalvst::shared
