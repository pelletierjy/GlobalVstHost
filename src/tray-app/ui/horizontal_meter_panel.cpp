// src/tray-app/ui/horizontal_meter_panel.cpp
//
// T098b — Neon gradient horizontal level meter with peak-hold indicator.

#include "horizontal_meter_panel.h"
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
    if (db <= HorizontalMeterPanel::kMinDb)
        return 0.0f;
    if (db >= 0.0f)
        return 1.0f;
    return (db - HorizontalMeterPanel::kMinDb) / (-HorizontalMeterPanel::kMinDb);
}

inline const juce::Colour kMeterGreen  {0xFF00E676};
inline const juce::Colour kMeterYellow {0xFFFFD400};
inline const juce::Colour kMeterRed    {0xFFFF1744};

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

HorizontalMeterPanel::HorizontalMeterPanel()
{
    last_update_ = juce::Time::getCurrentTime();
}

void HorizontalMeterPanel::setLevels(float peakDb, float rmsDb)
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

void HorizontalMeterPanel::reset()
{
    peak_db_ = -120.0f;
    rms_db_ = -120.0f;
    hold_db_ = -120.0f;
    repaint();
}

void HorizontalMeterPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background with rounded corners.
    juce::Path bgPath;
    bgPath.addRoundedRectangle(bounds, 2.0f);
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

    const float rms_norm  = dbToNormalized(rms_db_);
    const float peak_norm = dbToNormalized(peak_db_);
    const float hold_norm = dbToNormalized(hold_db_);

    // Segmented LED meter — horizontal.
    constexpr int kNumSegments = 16;
    constexpr float kSegGap = 1.5f;
    const float seg_stride = w / static_cast<float>(kNumSegments);
    const float seg_w = juce::jmax(1.0f, seg_stride - kSegGap);
    const float led_y = bounds.getY() + 0.5f;
    const float led_h = h - 1.0f;

    const int peak_seg = juce::jlimit(-1, kNumSegments - 1,
                                      static_cast<int>(peak_norm * kNumSegments) - 1);
    const int hold_seg = juce::jlimit(-1, kNumSegments - 1,
                                      static_cast<int>(hold_norm * kNumSegments + 0.5f) - 1);

    for (int i = 0; i < kNumSegments; ++i)
    {
        const float seg_left = bounds.getX() + static_cast<float>(i) * seg_stride;
        const juce::Rectangle<float> seg(seg_left, led_y, seg_w, led_h);

        const float seg_pos = (static_cast<float>(i) + 0.5f) / static_cast<float>(kNumSegments);
        const juce::Colour zone = meterColour(seg_pos);
        const bool lit = rms_norm * kNumSegments > static_cast<float>(i);

        if (lit)
        {
            g.setColour(zone);
            g.fillRoundedRectangle(seg, 1.0f);
            // Subtle left highlight so each LED reads as raised/glowing.
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(seg.withWidth(seg_w * 0.5f), 1.0f);
        }
        else
        {
            g.setColour(zone.withAlpha(0.12f));
            g.fillRoundedRectangle(seg, 1.0f);
        }

        // Instantaneous peak transient.
        if (i == peak_seg && peak_seg >= 0 && peak_norm > rms_norm)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRoundedRectangle(seg, 1.0f);
        }

        // Peak-hold cap.
        if (i == hold_seg && hold_seg >= 0)
        {
            g.setColour(zone);
            g.fillRoundedRectangle(seg, 1.0f);
            g.setColour(zone.withAlpha(0.30f));
            g.drawRoundedRectangle(seg.expanded(0.5f), 1.0f, 1.0f);
        }
    }

    // Border.
    g.setColour(kBgPanelBorder);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
}

}  // namespace jyglobalvst::tray
