// src/tray-app/ui/horizontal_meter_panel.h
//
// T098b — Horizontal level meter with peak-hold indicator.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

class HorizontalMeterPanel : public juce::Component
{
public:
    HorizontalMeterPanel();

    void setLevels(float peakDb, float rmsDb);
    void reset();

    void paint(juce::Graphics& g) override;

    static constexpr float kMinDb = -60.0f;

private:
    float peak_db_ { -120.0f };
    float rms_db_ { -120.0f };
    float hold_db_ { -120.0f };

    static constexpr float kHoldDecayDbPerSec = 20.0f;
    juce::Time last_update_;
};

}  // namespace jyglobalvst::tray
