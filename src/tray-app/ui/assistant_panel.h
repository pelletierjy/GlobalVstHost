// src/tray-app/ui/assistant_panel.h
//
// In-app AI Assistant as a docked panel on the right of the main window
// (replaces the old AssistantDialog popup). Hosts the deployed ConversiaCore
// <conversia-app> tutor (context "GlobalVstHost") in an embedded WebView2
// browser. Created once by MainWindow::buildUI() and shown/hidden by the
// header "AI" toggle button; nothing is fetched or contacted until the panel
// is first shown.
//
// The panel header carries a "Report an issue" button (Store policy 11.16,
// Live Generative AI Content): users can flag inappropriate or problematic
// AI-generated content at any time while the assistant is open.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>

namespace jyglobalvst::tray {

class AssistantPanel : public juce::Component
{
public:
    AssistantPanel();
    ~AssistantPanel() override;

    // Builds the web view and navigates it on the first call. light_theme maps
    // the app's CustomLookAndFeel theme onto the widget palette; a theme change
    // while the panel already exists re-navigates the page (same effect as the
    // old per-open dialog recreation).
    void prepareToShow(bool light_theme);

    // Fired by the panel's close button so the owner can release its toggle
    // button state; the panel does not hide itself.
    std::function<void()> onCloseRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void ensureContent(bool light_theme);

    juce::TextButton report_button_;
    juce::TextButton close_button_;
    std::unique_ptr<juce::WebBrowserComponent> browser_;
    std::unique_ptr<juce::Label> fallback_label_;

    bool content_built_ {false};
    bool loaded_light_theme_ {false};

    static constexpr int kHeaderH = 30;
    static constexpr int kGap = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssistantPanel)
};

}  // namespace jyglobalvst::tray