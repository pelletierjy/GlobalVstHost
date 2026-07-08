// src/tray-app/ui/meter_panel.cpp
//
// US4 T098 — Neon gradient vertical level meter with peak-hold indicator.

#include "meter_panel.h"
#include "custom_look_and_feel.h"

namespace jyglobalvst::tray {

namespace {

float linearToDb(float linear)
{
    if (linear <= 0.0f)
        return -120.0f;
    return 20.0f * std::log10(linear);
}

float dbToNormalized(float db)
{
    if (db <= MeterPanel::kMinDb)
        return 0.0f;
    if (db >= 0.0f)
        return 1.0f;
    return (db - MeterPanel::kMinDb) / (-MeterPanel::kMinDb);
}

// Classic VU-style meter colours: green for headroom, yellow as the level
// climbs, red at the very top (approaching / over 0 dBFS).
inline const juce::Colour kMeterGreen {0xFF00E676};
inline const juce::Colour kMeterYellow{0xFFFFD400};
inline const juce::Colour kMeterRed   {0xFFFF1744};

// Green/yellow/red ramp keyed to the normalized meter position (0 = bottom,
// 1 = 0 dBFS at the top). Yellow kicks in near the top; red is reserved for
// the last few dB approaching / over 0 dB.
juce::Colour meterColour(float norm)
{
    if (norm <= 0.70f)
        return kMeterGreen;
    if (norm <= 0.88f)
    {
        float t = (norm - 0.70f) / (0.88f - 0.70f);
        return kMeterGreen.interpolatedWith(kMeterYellow, t);
    }
    float t = juce::jlimit(0.0f, 1.0f, (norm - 0.88f) / (1.0f - 0.88f));
    return kMeterYellow.interpolatedWith(kMeterRed, t);
}

}  // namespace

MeterPanel::MeterPanel()
{
    last_update_ = juce::Time::getCurrentTime();
}

void MeterPanel::setLevels(float peakDb, float rmsDb)
{
    peak_db_ = peakDb;
    rms_db_ = rmsDb;
    if (peakDb > hold_db_)
    {
        hold_db_ = peakDb;
    }
    last_update_ = juce::Time::getCurrentTime();
    repaint();
}

void MeterPanel::reset()
{
    peak_db_ = -120.0f;
    rms_db_ = -120.0f;
    hold_db_ = -120.0f;
    repaint();
}

void MeterPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background with rounded corners.
    juce::Path bgPath;
    bgPath.addRoundedRectangle(bounds, 3.0f);
    g.setColour(kMeterBg);
    g.fillPath(bgPath);

    // Decay hold line.
    const auto now = juce::Time::getCurrentTime();
    const float elapsed_sec = static_cast<float>(
        (now - last_update_).inMilliseconds()) / 1000.0f;
    if (elapsed_sec > 0.0f)
    {
        hold_db_ -= kHoldDecayDbPerSec * elapsed_sec;
        if (hold_db_ < peak_db_)
            hold_db_ = peak_db_;
        last_update_ = now;
    }

    const float rms_norm = dbToNormalized(rms_db_);
    const float peak_norm = dbToNormalized(peak_db_);
    const float hold_norm = dbToNormalized(hold_db_);

    // Segmented LED meter. The meter height is divided into discrete blocks
    // with a small gap between them, mimicking a hardware LED/PPM bar graph.
    // Each segment is lit in its zone colour (green/yellow/red) once the RMS
    // level rises into it; unlit segments show a dim "off LED" outline.
    constexpr int kNumSegments = 24;
    constexpr float kSegGap = 1.5f;
    const float seg_stride = h / static_cast<float>(kNumSegments);
    const float seg_h = juce::jmax(1.0f, seg_stride - kSegGap);
    const float led_x = bounds.getX() + 0.5f;
    const float led_w = w - 1.0f;

    const int peak_seg = juce::jlimit(-1, kNumSegments - 1,
                                      static_cast<int>(peak_norm * kNumSegments) - 1);
    const int hold_seg = juce::jlimit(-1, kNumSegments - 1,
                                      static_cast<int>(hold_norm * kNumSegments + 0.5f) - 1);

    for (int i = 0; i < kNumSegments; ++i)
    {
        const float seg_bottom = bounds.getBottom() - static_cast<float>(i) * seg_stride;
        const juce::Rectangle<float> seg(led_x, seg_bottom - seg_h, led_w, seg_h);

        const float seg_pos = (static_cast<float>(i) + 0.5f) / static_cast<float>(kNumSegments);
        const juce::Colour zone = meterColour(seg_pos);
        const bool lit = rms_norm * kNumSegments > static_cast<float>(i);

        if (lit)
        {
            g.setColour(zone);
            g.fillRoundedRectangle(seg, 1.5f);
            // Subtle top highlight so each LED reads as raised/glowing.
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(seg.withHeight(seg_h * 0.5f), 1.5f);
        }
        else
        {
            // Dim "off" LED.
            g.setColour(zone.withAlpha(0.12f));
            g.fillRoundedRectangle(seg, 1.5f);
        }

        // Instantaneous peak transient: light the peak segment brightly even
        // when the RMS bar has not reached it yet.
        if (i == peak_seg && peak_seg > 0 && peak_norm > rms_norm)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRoundedRectangle(seg, 1.5f);
        }

        // Peak-hold: keep the highest recent segment lit as a bright cap with
        // a glow, coloured to its zone.
        if (i == hold_seg && hold_seg >= 0)
        {
            g.setColour(zone);
            g.fillRoundedRectangle(seg, 1.5f);
            g.setColour(zone.withAlpha(0.30f));
            g.drawRoundedRectangle(seg.expanded(1.0f), 2.0f, 1.5f);
        }
    }

    // Border.
    g.setColour(kBgPanelBorder);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

}  // namespace jyglobalvst::tray
