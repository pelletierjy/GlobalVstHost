// src/audio-engine/routing/format_convert.cpp
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • RT-safe: no allocation, no lock, no exception, no syscall.
//   • Compiled with /W4 /WX; integer overflow on clamping is intentional
//     and guarded.
// =====================================================================

#include "format_convert.h"

#include <algorithm>
#include <cstring>

namespace jyglobalvst::engine {

namespace {

constexpr float kInvInt16 = 1.0f / 32768.0f;
constexpr float kInvInt24 = 1.0f / 8388608.0f;

inline float clamp(float v) noexcept
{
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

}  // namespace

void int16ToFloat32(const std::int16_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept
{
    const std::size_t total = frames * channels;
    for (std::size_t i = 0; i < total; ++i)
    {
        dst[i] = static_cast<float>(src[i]) * kInvInt16;
    }
}

void float32ToInt16(const float* src, std::int16_t* dst, std::size_t frames, std::size_t channels) noexcept
{
    const std::size_t total = frames * channels;
    for (std::size_t i = 0; i < total; ++i)
    {
        float v = src[i];
        if (v <= -1.0f)
        {
            dst[i] = -32768;
        }
        else if (v >= 1.0f)
        {
            dst[i] = 32767;
        }
        else
        {
            v *= 32768.0f;
            dst[i] = static_cast<std::int16_t>(v < 0 ? v - 0.5f : v + 0.5f);
        }
    }
}

void int24ToFloat32(const std::uint8_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept
{
    const std::size_t total = frames * channels;
    for (std::size_t i = 0; i < total; ++i)
    {
        // Sign-extend 24-bit LE → 32-bit signed.
        const std::uint8_t b0 = src[i * 3 + 0];
        const std::uint8_t b1 = src[i * 3 + 1];
        const std::uint8_t b2 = src[i * 3 + 2];
        std::int32_t raw = static_cast<std::int32_t>(static_cast<std::uint32_t>(b0)
                                                    | (static_cast<std::uint32_t>(b1) << 8)
                                                    | (static_cast<std::uint32_t>(b2) << 16));
        if (raw & 0x00800000)
        {
            raw |= static_cast<std::int32_t>(0xFF000000);
        }
        dst[i] = static_cast<float>(raw) * kInvInt24;
    }
}

void float32ToInt24(const float* src, std::uint8_t* dst, std::size_t frames, std::size_t channels) noexcept
{
    const std::size_t total = frames * channels;
    for (std::size_t i = 0; i < total; ++i)
    {
        float v = src[i];
        if (v <= -1.0f)
        {
            v = -1.0f;
        }
        else if (v >= 1.0f)
        {
            v = 1.0f;
        }
        v *= 8388608.0f;
        std::int32_t raw = static_cast<std::int32_t>(v < 0 ? v - 0.5f : v + 0.5f);
        // Clamp to 24-bit range to avoid overflow artifacts.
        if (raw > 8388607)
        {
            raw = 8388607;
        }
        if (raw < -8388608)
        {
            raw = -8388608;
        }
        const auto u = static_cast<std::uint32_t>(raw);
        dst[i * 3 + 0] = static_cast<std::uint8_t>(u & 0xFF);
        dst[i * 3 + 1] = static_cast<std::uint8_t>((u >> 8) & 0xFF);
        dst[i * 3 + 2] = static_cast<std::uint8_t>((u >> 16) & 0xFF);
    }
}

void int16ToFloat32Planar(const std::int16_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept
{
    for (std::size_t c = 0; c < channels; ++c)
    {
        float* out = dst + c * frames;
        for (std::size_t i = 0; i < frames; ++i)
        {
            out[i] = static_cast<float>(src[i * channels + c]) * kInvInt16;
        }
    }
}

void int24ToFloat32Planar(const std::uint8_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept
{
    for (std::size_t c = 0; c < channels; ++c)
    {
        float* out = dst + c * frames;
        for (std::size_t i = 0; i < frames; ++i)
        {
            const std::size_t s = (i * channels + c) * 3;
            // Sign-extend 24-bit LE → 32-bit signed.
            std::int32_t raw = static_cast<std::int32_t>(static_cast<std::uint32_t>(src[s + 0])
                                                        | (static_cast<std::uint32_t>(src[s + 1]) << 8)
                                                        | (static_cast<std::uint32_t>(src[s + 2]) << 16));
            if (raw & 0x00800000)
            {
                raw |= static_cast<std::int32_t>(0xFF000000);
            }
            out[i] = static_cast<float>(raw) * kInvInt24;
        }
    }
}

void deinterleaveFloat32(const float* src, float* dst, std::size_t frames, std::size_t channels) noexcept
{
    for (std::size_t c = 0; c < channels; ++c)
    {
        float* out = dst + c * frames;
        for (std::size_t i = 0; i < frames; ++i)
        {
            out[i] = src[i * channels + c];
        }
    }
}

}  // namespace jyglobalvst::engine
