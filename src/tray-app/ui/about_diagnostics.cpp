// src/tray-app/ui/about_diagnostics.cpp
//
// T120 — Custom about dialog with icon and themed diagnostics.

#include "about_diagnostics.h"

#include "custom_look_and_feel.h"
#include "BinaryData.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

namespace {

juce::String hostModeString(EngineHostMode mode)
{
    switch (mode)
    {
    case EngineHostMode::InProcess:
        return "In-Process";
    case EngineHostMode::WindowsService:
        return "Windows Service";
    default:
        return "Unknown";
    }
}

juce::String buildDiagnosticsText(EngineHostMode mode,
                                  const juce::String& version,
                                  const DiagnosticSnapshot& snap)
{
    juce::String msg;
    msg << "GlobalVSTHost " << version << "\n\n";
    msg << "Engine host mode: " << hostModeString(mode) << "\n";
    msg << "Current output: " << snap.current_output_friendly_name << "\n";
    msg << "Current input: " << snap.current_input_friendly_name << "\n";
    msg << "Buffer size: " << snap.buffer_size << " samples\n";
    msg << "Sample rate: " << snap.sample_rate << " Hz\n";
    msg << "Chain revision: " << snap.chain_revision << "\n";
    msg << "Plugins in chain: " << snap.plugin_count << "\n";
    msg << "\n";
    msg << "Latency (ms):\n";
    msg << "  Capture:  " << juce::String(snap.latency.capture_ms, 2) << "\n";
    msg << "  Resample: " << juce::String(snap.latency.resample_ms, 2) << "\n";
    msg << "  Chain:    " << juce::String(snap.latency.plugin_chain_ms, 2) << "\n";
    msg << "  Output:   " << juce::String(snap.latency.output_ms, 2) << "\n";
    msg << "  Total RT: " << juce::String(snap.latency.total_round_trip_ms, 2) << "\n";
    msg << "\n";
    msg << "CPU: " << juce::String(snap.cpu.rolling_1s_pct, 1) << "% (peak "
        << juce::String(snap.cpu.instantaneous_pct, 1) << "%)\n";
    msg << "Xruns this session: " << (juce::int64)snap.cpu.xrun_count_session << "\n";
    return msg;
}

class AboutDialogContent : public juce::Component
                          , public juce::Button::Listener
{
public:
    AboutDialogContent(const juce::String& diagnostics)
    {
        auto icon = juce::ImageCache::getFromMemory(
            jyglobalvst::BinaryData::app_icon_png,
            jyglobalvst::BinaryData::app_icon_pngSize);
        if (icon.isValid())
        {
            icon_component_ = std::make_unique<juce::ImageComponent>();
            icon_component_->setImage(icon);
            addAndMakeVisible(icon_component_.get());
        }

        title_label_ = std::make_unique<juce::Label>(juce::String(), "GlobalVSTHost");
        juce::FontOptions fontOpts;
        juce::Font titleFont{fontOpts};
        titleFont.setHeight(20.0f);
        titleFont.setBold(true);
        title_label_->setFont(titleFont);
        title_label_->setColour(juce::Label::textColourId, kAccentCyan);
        addAndMakeVisible(title_label_.get());

        text_editor_ = std::make_unique<juce::TextEditor>();
        text_editor_->setMultiLine(true);
        text_editor_->setReadOnly(true);
        text_editor_->setText(diagnostics);
        text_editor_->setColour(juce::TextEditor::backgroundColourId, kBgPanel);
        text_editor_->setColour(juce::TextEditor::textColourId, kTextPrimary);
        text_editor_->setColour(juce::TextEditor::outlineColourId, kBgPanelBorder);
        addAndMakeVisible(text_editor_.get());

        ok_button_ = std::make_unique<juce::TextButton>("OK");
        ok_button_->addListener(this);
        addAndMakeVisible(ok_button_.get());
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == ok_button_.get())
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(1);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(16);

        auto header = b.removeFromTop(64);
        if (icon_component_ != nullptr)
        {
            icon_component_->setBounds(header.removeFromLeft(64));
            header.removeFromLeft(12);
        }
        title_label_->setBounds(header);

        b.removeFromTop(12);
        auto buttonArea = b.removeFromBottom(32);
        ok_button_->setBounds(buttonArea.withSizeKeepingCentre(80, 28));
        b.removeFromBottom(8);

        text_editor_->setBounds(b);
    }

private:
    std::unique_ptr<juce::ImageComponent> icon_component_;
    std::unique_ptr<juce::Label> title_label_;
    std::unique_ptr<juce::TextEditor> text_editor_;
    std::unique_ptr<juce::TextButton> ok_button_;
};

}  // namespace

void AboutDiagnostics::show(juce::Component* parent,
                            EngineHostMode mode,
                            const juce::String& version,
                            const DiagnosticSnapshot& snap)
{
    auto content = std::make_unique<AboutDialogContent>(
        buildDiagnosticsText(mode, version, snap));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "About / Diagnostics";
    options.content.set(content.release(), true);
    options.componentToCentreAround = parent;
    options.dialogBackgroundColour = kBgDeep;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    // launchAsync() enters a (non-blocking) modal state and takes ownership of
    // the window, deleting it when exitModalState() is called. create() + manual
    // setVisible() leaves the window non-modal, so the OK button's
    // exitModalState() was a no-op and the dialog could not be dismissed.
    auto* dw = options.launchAsync();
    if (dw != nullptr)
        dw->centreWithSize(520, 420);
}

}  // namespace jyglobalvst::tray
