// src/tray-app/ui/plugin_editor_window.h
//
// T061 — Plugin editor host window.
//
// Wraps a JUCE AudioProcessorEditor in a standalone DocumentWindow.
// Show/hide is controlled via IAudioEngine::openEditor/closeEditor.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace jyglobalvst::tray {

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(juce::AudioProcessorEditor* editor, const juce::String& plugin_name);
    ~PluginEditorWindow() override;

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};

}  // namespace jyglobalvst::tray
