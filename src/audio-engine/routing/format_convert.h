// src/audio-engine/routing/format_convert.h
//
// T024 — Format converters between PCM source formats and the internal
// 32-bit float representation.
//
// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • All functions in this header are RT-safe: no allocation, no lock,
//     no branching on data-dependent state beyond the input format.
//   • Caller owns both buffers; lifetimes are the caller's responsibility.
//   • Out-of-range Int24 sign extension is handled in pure C++ — no SIMD
//     intrinsics until release-prep profiling proves the need.
// =====================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace jyglobalvst::engine {

// Int16 LE (interleaved channels) → Float32 (interleaved). Output range [-1.0, 1.0].
// `frames` is per-channel; `channels` is the channel count (typically 2).
void int16ToFloat32(const std::int16_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept;

// Float32 (interleaved) → Int16 LE (interleaved). Clips outside [-1.0, 1.0].
void float32ToInt16(const float* src, std::int16_t* dst, std::size_t frames, std::size_t channels) noexcept;

// Int24 packed little-endian (3 bytes per sample, interleaved) → Float32.
// `src` is a byte pointer; `dst[i]` ∈ [-1.0, 1.0].
void int24ToFloat32(const std::uint8_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept;

// Float32 → Int24 packed LE.
void float32ToInt24(const float* src, std::uint8_t* dst, std::size_t frames, std::size_t channels) noexcept;

// ---------------------------------------------------------------------------
// Planar (deinterleaving) variants — convert + deinterleave in a single pass.
// `dst` is a channel-major planar buffer of `channels * frames` floats:
// channel c occupies dst[c * frames .. c * frames + frames). These fuse the
// int→float conversion with the deinterleave so no interleaved-float temp
// buffer is needed. RT-safe (no allocation, no lock).
// ---------------------------------------------------------------------------

// Int16 LE (interleaved) → Float32 planar.
void int16ToFloat32Planar(const std::int16_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept;

// Int24 packed LE (interleaved) → Float32 planar.
void int24ToFloat32Planar(const std::uint8_t* src, float* dst, std::size_t frames, std::size_t channels) noexcept;

// Float32 (interleaved) → Float32 planar (deinterleave only, no scaling).
void deinterleaveFloat32(const float* src, float* dst, std::size_t frames, std::size_t channels) noexcept;

}  // namespace jyglobalvst::engine
