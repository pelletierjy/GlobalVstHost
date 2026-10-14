// src/tray-app/ui/assistant_dialog.cpp

#include "assistant_dialog.h"

#include "assistant_url.h"
#include "custom_look_and_feel.h"

namespace jyglobalvst::tray {

namespace {

juce::WebBrowserComponent::Options makeBrowserOptions()
{
    return juce::WebBrowserComponent::Options{}.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
}

}  // namespace

AssistantDialog::AssistantDialog(bool light_theme) : juce::DialogWindow("AI Assistant", kBgDeep, true, true)
{
    buildUI(light_theme);
    centreWithSize(560, 720);
    setVisible(true);
}

AssistantDialog::~AssistantDialog() = default;

void AssistantDialog::buildUI(bool light_theme)
{
    auto* content = new juce::Component();
    const auto options = makeBrowserOptions();

    if (juce::WebBrowserComponent::areOptionsSupported(options))
    {
        browser_ = std::make_unique<juce::WebBrowserComponent>(options);
        content->addAndMakeVisible(browser_.get());
        browser_->goToURL(assistantUrl(light_theme));
    }
    else
    {
        // No WebView2 runtime on this machine — open the same assistant in the
        // default browser instead of showing an empty pane.
        fallback_label_ =
            std::make_unique<juce::Label>(juce::String(),
                                          "The in-app assistant needs the Microsoft Edge WebView2 runtime.\n"
                                          "Opening the assistant in your default browser instead.");
        fallback_label_->setJustificationType(juce::Justification::centred);
        content->addAndMakeVisible(fallback_label_.get());
        juce::URL{assistantUrl(light_theme)}.launchInDefaultBrowser();
    }

    setContentOwned(content, true);
}

void AssistantDialog::resized()
{
    DialogWindow::resized();

    if (auto* content = getContentComponent())
    {
        auto b = content->getLocalBounds().reduced(16);

        if (browser_ != nullptr)
            browser_->setBounds(b);
        else if (fallback_label_ != nullptr)
            fallback_label_->setBounds(b);
    }
}

void AssistantDialog::closeButtonPressed()
{
    setVisible(false);
}

}  // namespace jyglobalvst::tray
