// src/audio-engine/routing/capture_drift_controller.h
//
// Pure decision logic for the capture→device clock-drift servo. Kept free of
// atomics, buffers and audio concerns (mirroring EnergySaverController) so it can
// be unit-tested deterministically against synthetic fill levels.
//
// Why it exists: on the ASIO / JUCE-callback transports the capture ring is
// filled by the WASAPI capture thread and drained by the output device's own
// clock. Those are two independent crystals, so the ring's fill level drifts
// even when the two nominal sample rates are identical. Left uncorrected the
// ring eventually starves — the callback then emits zero-filled blocks and the
// user hears the input "stop being detected" until they toggle the engine.
//
// A PI loop on the normalised fill error trims the capture resampling ratio. The
// proportional term reacts to transients; the integral term converges to the TRUE
// rate ratio, so it also corrects a device whose real clock differs from the rate
// it reports, rather than saturating.
//
// State machine:
//   • priming → emit silence, consume nothing, until fill reaches target. Entered
//     at (re)open and whenever the ring runs completely dry. A loopback endpoint
//     delivers no packets at all while nothing is playing, so starting without a
//     cushion means dropping out on the first jitter.
//   • tracking → servo active; trim() is the multiplier to apply to the nominal
//     ratio.

#pragma once

#include <algorithm>
#include <cstddef>

namespace jyglobalvst::engine {

class CaptureDriftController
{
public:
    struct Tuning
    {
        double kp;            // proportional gain on normalised fill error
        double ki;            // integral gain
        double max_trim;      // clamp on the total correction
        double max_integral;  // clamp on the integral term alone
    };

    CaptureDriftController() = default;

    // target_fill of 0 disables the controller entirely: trim() stays 1.0 and
    // shouldEmitSilence() is always false. Used on transports that don't cross
    // two clocks (the pure-WASAPI engine thread paces itself against output-ring
    // backpressure instead).
    void configure(std::size_t target_fill, const Tuning& tuning)
    {
        target_fill_ = target_fill;
        tuning_ = tuning;
        arm();
    }

    // Re-enter priming and drop all accumulated correction. Call whenever the
    // ring is (re)opened or deliberately drained.
    void arm()
    {
        priming_ = target_fill_ > 0;
        trim_ = 1.0;
        integral_ = 0.0;
    }

    [[nodiscard]] bool enabled() const { return target_fill_ > 0; }
    [[nodiscard]] bool priming() const { return priming_; }
    [[nodiscard]] double trim() const { return trim_; }
    [[nodiscard]] std::size_t targetFill() const { return target_fill_; }

    // Advance the servo for one block. `available` is the ring's current fill in
    // frames. Returns true when the caller must emit silence and consume nothing
    // this block (still priming).
    bool update(std::size_t available)
    {
        if (target_fill_ == 0)
        {
            trim_ = 1.0;
            return false;
        }

        // Fully drained. Re-arm rather than grinding out zero-filled blocks, and
        // drop the integrator so a long starvation doesn't leave a lasting ratio
        // offset once audio returns.
        if (available == 0 && !priming_)
        {
            arm();
            underrun_count_++;
        }

        if (priming_)
        {
            if (available < target_fill_)
                return true;
            priming_ = false;
        }

        const double target = static_cast<double>(target_fill_);
        const double error = (static_cast<double>(available) - target) / target;
        integral_ = std::clamp(integral_ + tuning_.ki * error,
                               -tuning_.max_integral, tuning_.max_integral);
        // A fill above target means the ring is filling faster than we drain it,
        // so we must consume MORE input per output frame — hence a larger ratio.
        const double correction = std::clamp(tuning_.kp * error + integral_,
                                             -tuning_.max_trim, tuning_.max_trim);
        trim_ = 1.0 + correction;
        return false;
    }

    [[nodiscard]] unsigned underrunCount() const { return underrun_count_; }

private:
    std::size_t target_fill_ {0};
    Tuning tuning_ {0.0, 0.0, 0.0, 0.0};
    bool priming_ {false};
    double trim_ {1.0};
    double integral_ {0.0};
    unsigned underrun_count_ {0};
};

}  // namespace jyglobalvst::engine
