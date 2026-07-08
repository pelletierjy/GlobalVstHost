// src/tray-app/ui/catalog_dialog.h
//
// Plugin catalog selection dialog. Shows scanned VST3 plugins and allows
// the user to select one to add to the chain.

#pragma once

#include "jyglobalvst/audio_engine.h"
#include "jyglobalvst/types.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace jyglobalvst::tray {

class CatalogDialog : public juce::DialogWindow
                    , public juce::Button::Listener
                    , public juce::ListBoxModel
{
public:
    enum class Action
    {
        None,       // User cancelled
        Selected,   // User selected a plugin (will trigger plugin loading)
        Browse,     // User clicked "Browse..."
    };

    using OnAction = std::function<void(Action action)>;

    explicit CatalogDialog(const std::vector<PluginCatalogEntry>& catalog, OnAction on_action);
    ~CatalogDialog() override;

    // juce::Button::Listener
    void buttonClicked(juce::Button* button) override;

    // juce::ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

    void resized() override;
    void closeButtonPressed() override;

    // Returns the currently selected catalog entry, or nullptr if none selected.
    // Only meaningful to call after Action::Selected is triggered.
    const PluginCatalogEntry* getSelectedEntry() const;
    int getSelectedRowIndex() const;

private:
    void buildUI();
    void onAddClicked();
    void onBrowseClicked();

    std::vector<PluginCatalogEntry> catalog_;
    OnAction on_action_;

    std::unique_ptr<juce::ListBox> list_box_;
    std::unique_ptr<juce::TextButton> add_button_;
    std::unique_ptr<juce::TextButton> browse_button_;
    std::unique_ptr<juce::TextButton> cancel_button_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CatalogDialog)
};

}  // namespace jyglobalvst::tray
