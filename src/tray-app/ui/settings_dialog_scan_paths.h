// src/tray-app/ui/settings_dialog_scan_paths.h
//
// T064 — User-managed scan paths settings panel.
//
// Allows adding/removing/disabling additional VST3 directories beyond defaults.
// Persists to roaming settings.json (US3 T087).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace jyglobalvst::tray {

struct ScanPathItem
{
    juce::String path;
    bool enabled {true};
};

class SettingsDialogScanPaths : public juce::Component
                              , public juce::Button::Listener
                              , public juce::ListBoxModel
{
public:
    SettingsDialogScanPaths();

    void buttonClicked(juce::Button* button) override;

    // juce::ListBoxModel -----------------------------------------------------
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;

    void addPath(const juce::String& path);
    void removeSelectedPath();
    std::vector<ScanPathItem> items() const;

    void resized() override;

private:
    std::unique_ptr<juce::ListBox> list_box_;
    std::unique_ptr<juce::TextButton> add_button_;
    std::unique_ptr<juce::TextButton> remove_button_;
    std::vector<ScanPathItem> items_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialogScanPaths)
};

}  // namespace jyglobalvst::tray
