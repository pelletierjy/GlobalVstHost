// src/tray-app/ui/plugin_editor_window.cpp
//
// T061 — Plugin editor host window implementation.

#include "plugin_editor_window.h"

namespace jyglobalvst::tray {

PluginEditorWindow::PluginEditorWindow(juce::AudioProcessorEditor* editor, const juce::String& plugin_name)
    : juce::DocumentWindow(plugin_name + " — Editor",
                           juce::Colours::darkgrey,
                           juce::DocumentWindow::closeButton)
{
    if (!editor)
    {
        throw std::invalid_argument("PluginEditorWindow: editor pointer is null");
    }

    setUsingNativeTitleBar(true);

    try
    {
        const bool is_resizable = editor->isResizable();
        setResizable(is_resizable, false);
        setContentNonOwned(editor, true);

        const int w = editor->getWidth();
        const int h = editor->getHeight();
        if (w > 0 && h > 0)
        {
            centreWithSize(w, h);
        }
        else
        {
            centreWithSize(400, 300);
        }
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string("PluginEditorWindow initialization failed: ") + e.what());
    }
}

PluginEditorWindow::~PluginEditorWindow() = default;

void PluginEditorWindow::closeButtonPressed()
{
    setVisible(false);
}

}  // namespace jyglobalvst::tray
