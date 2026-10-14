// src/tray-app/ui/assistant_dialog.h
//
// In-app AI Assistant. Hosts the deployed ConversiaCore <conversia-app> tutor
// (context "GlobalVstHost") in an embedded WebView2 browser. Opened on demand:
// nothing is fetched or contacted until the user opens this dialog.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace jyglobalvst::tray {

class AssistantDialog : public juce::DialogWindow
{
public:
    // light_theme == true when the app's Light theme is active, so the widget
    // renders with a matching palette.
    explicit AssistantDialog(bool light_theme);
    ~AssistantDialog() override;

    void resized() override;
    void closeButtonPressed() override;

private:
    void buildUI(bool light_theme);

    std::unique_ptr<juce::WebBrowserComponent> browser_;
    std::unique_ptr<juce::Label> fallback_label_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssistantDialog)
};

}  // namespace jyglobalvst::tray
