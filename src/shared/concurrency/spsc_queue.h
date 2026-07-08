// src/shared/concurrency/spsc_queue.h
//
// T012 — Lock-free single-producer / single-consumer command queue.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • Producer (UI thread) and consumer (audio thread) never share a
//     lock. Both indices are std::atomic with release/acquire fences.
//   • The buffer is fixed at construction; no allocation in tryPush /
//     tryPop. Capacity is a compile-time template parameter.
//   • The element type T MUST be trivially copyable OR cheap to
//     move-assign — there is NO allocation on push/pop.
//   • Capacity is rounded up to a power of two so the index mask is a
//     single AND.
// =====================================================================

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace jyglobalvst::shared {

namespace detail {

constexpr std::size_t nextPowerOfTwo(std::size_t v) noexcept
{
    std::size_t p = 1;
    while (p < v)
    {
        p <<= 1;
    }
    return p;
}

}  // namespace detail

template <typename T, std::size_t Capacity>
class SpscCommandQueue
{
public:
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(std::is_nothrow_move_assignable_v<T> || std::is_trivially_copyable_v<T>,
                  "T must be cheap to transfer on the hot path");

    static constexpr std::size_t kBufferSize = detail::nextPowerOfTwo(Capacity);
    static constexpr std::size_t kMask = kBufferSize - 1;

    SpscCommandQueue() = default;

    SpscCommandQueue(const SpscCommandQueue&) = delete;
    SpscCommandQueue& operator=(const SpscCommandQueue&) = delete;
    SpscCommandQueue(SpscCommandQueue&&) = delete;
    SpscCommandQueue& operator=(SpscCommandQueue&&) = delete;

    // Called only from the producer thread (UI).
    [[nodiscard]] bool tryPush(T value) noexcept
    {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
        {
            return false;  // Queue full; caller decides retry strategy off the RT path.
        }
        buffer_[head] = std::move(value);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Called only from the consumer thread (audio). Drains one item.
    [[nodiscard]] bool tryPop(T& out) noexcept
    {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
        {
            return false;
        }
        out = std::move(buffer_[tail]);
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    // O(1) approximate count — for diagnostics only, never for correctness.
    [[nodiscard]] std::size_t approxSize() const noexcept
    {
        const auto head = head_.load(std::memory_order_acquire);
        const auto tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & kMask;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return kBufferSize - 1; }

private:
    alignas(64) std::atomic<std::size_t> head_ {0};  // producer-owned
    alignas(64) std::atomic<std::size_t> tail_ {0};  // consumer-owned
    alignas(64) T buffer_[kBufferSize] {};
};

}  // namespace jyglobalvst::shared
