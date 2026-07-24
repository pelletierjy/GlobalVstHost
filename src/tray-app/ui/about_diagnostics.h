// src/tray-app/ui/about_diagnostics.h
//
// T120 — About → Diagnostics surface.
// Shows engine host mode ("In-Process" vs "Windows Service") per FR-029.

#pragma once

#include "jyglobalvst/types.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

enum class EngineHostMode
{
    InProcess,
    WindowsService,
};

struct DiagnosticSnapshot
{
    juce::String current_output_friendly_name;
    juce::String current_input_friendly_name;
    int buffer_size {0};
    int sample_rate {0};
    int chain_revision {0};
    int plugin_count {0};
    LatencyProfile latency;
    CpuStats cpu;
};

// Settings-tab controls owned by MainWindow. Passed in (not owned) so the dialog
// can reparent the live components into its Settings tab while it is open; they
// are detached — not destroyed — when the dialog closes, and remain valid for the
// next time the dialog is shown.
struct SettingsControls
{
    juce::Label* theme_label {nullptr};
    juce::ComboBox* theme_selector {nullptr};
    juce::Label* start_minimized_label {nullptr};
    juce::ToggleButton* start_minimized_button {nullptr};
};

class AboutDiagnostics
{
public:
    static void show(juce::Component* parent,
                     EngineHostMode mode,
                     const juce::String& version,
                     const DiagnosticSnapshot& snap,
                     const SettingsControls& settings);
};

}  // namespace jyglobalvst::tray
