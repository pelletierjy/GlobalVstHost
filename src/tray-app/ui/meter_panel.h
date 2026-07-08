// src/tray-app/ui/meter_panel.h
//
// US4 T098 — Vertical level meter with peak-hold indicator.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

class MeterPanel : public juce::Component
{
public:
    MeterPanel();

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
