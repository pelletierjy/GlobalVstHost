// src/shared/platform/realtime_clock.h
//
// T013 — RealtimeClock: QueryPerformanceCounter wrapper, RT-safe.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • now() is a single QPC call. No allocation, no lock, no syscall
//     (QPC is a vDSO-equivalent on modern Windows).
//   • frequency_ is initialised at static-init time so the audio thread
//     never observes a half-constructed state.
//   • All durations are reported as 64-bit nanosecond counts to avoid
//     floating-point cost on the hot path.
// =====================================================================

#pragma once

#include <cstdint>

#if defined(_WIN32)
#    include <windows.h>
#    include <profileapi.h>
#endif

namespace jyglobalvst::shared {

class RealtimeClock
{
public:
    RealtimeClock() noexcept
    {
#if defined(_WIN32)
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        frequency_ = static_cast<std::uint64_t>(f.QuadPart);
#else
        frequency_ = 1'000'000'000ULL;  // fallback for tooling builds
#endif
    }

    // Raw counter snapshot — call from anywhere including the audio thread.
    [[nodiscard]] std::uint64_t now() const noexcept
    {
#if defined(_WIN32)
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return static_cast<std::uint64_t>(c.QuadPart);
#else
        return 0;
#endif
    }

    [[nodiscard]] std::uint64_t frequency() const noexcept { return frequency_; }

    // Convert a counter delta to nanoseconds. 128-bit-safe multiplication.
    [[nodiscard]] std::uint64_t deltaToNs(std::uint64_t a, std::uint64_t b) const noexcept
    {
        const std::uint64_t delta = (b > a) ? (b - a) : 0;
        // delta * 1e9 / frequency_ — compute as 128-bit to avoid overflow on long runs.
        const std::uint64_t whole = delta / frequency_;
        const std::uint64_t rem = delta % frequency_;
        return whole * 1'000'000'000ULL + (rem * 1'000'000'000ULL) / frequency_;
    }

    [[nodiscard]] double deltaToMs(std::uint64_t a, std::uint64_t b) const noexcept
    {
        return static_cast<double>(deltaToNs(a, b)) * 1e-6;
    }

private:
    std::uint64_t frequency_ {1};
};

}  // namespace jyglobalvst::shared
