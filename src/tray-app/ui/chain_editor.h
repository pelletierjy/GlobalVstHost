// src/tray-app/ui/chain_editor.h
//
// T062 — Chain editor UI component.
//
// Scrollable list of plugin slots with per-slot bypass/remove buttons.
// Plugins can be reordered by holding and dragging a row by its title.

#pragma once

#include "jyglobalvst/audio_engine.h"
#include "horizontal_meter_panel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace jyglobalvst::tray {

// One row in the chain editor.
class ChainSlotRow : public juce::Component
                        , public juce::Button::Listener
                        , public juce::DragAndDropTarget
{
public:
    ChainSlotRow(IAudioEngine* engine, int position, const ChainSlotSnapshot& slot);

    void buttonClicked(juce::Button* button) override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    // juce::DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    int position() const noexcept { return position_; }

    // Update the per-plugin output meter (peak / RMS in dB).
    void setMeterLevels(float peakDb, float rmsDb);

    // Invoked when the user drops a dragged row, requesting a reorder.
    // Arguments are (from_position, to_position) for IAudioEngine::moveSlot.
    std::function<void(int from, int to)> onReorderRequest;

private:
    bool isDraggable() const noexcept;

    IAudioEngine* engine_;
    int position_;
    ChainSlotSnapshot slot_;

    bool drag_over_ {false};
    bool insert_above_ {true};

    std::unique_ptr<juce::TextButton> bypass_button_;
    std::unique_ptr<juce::TextButton> remove_button_;
    std::unique_ptr<juce::TextButton> editor_button_;
    std::unique_ptr<juce::Label> name_label_;
    std::unique_ptr<HorizontalMeterPanel> meter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainSlotRow)
};

// Scrollable container for all chain slots.
class ChainEditor : public juce::Viewport
                  , public juce::Button::Listener
                  , public juce::DragAndDropContainer
{
public:
    explicit ChainEditor(IAudioEngine* engine);

    void refreshFromEngine();
    void resized() override;

    // Push per-plugin meter levels into the visible rows.
    void setPluginMeterLevels(const std::vector<float>& peaks,
                              const std::vector<float>& rms);

    void buttonClicked(juce::Button* button) override;

    // Callback invoked when the user clicks the "+" button to add a plugin.
    std::function<void()> onAddPluginRequested;

private:
    static constexpr int kRowHeight = 40;

    IAudioEngine* engine_;
    std::unique_ptr<juce::Component> content_;
    std::vector<std::unique_ptr<ChainSlotRow>> rows_;
    std::unique_ptr<juce::TextButton> add_plugin_button_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainEditor)
};

}  // namespace jyglobalvst::tray
