// src/tray-app/ui/assistant_panel.cpp

#include "assistant_panel.h"

#include "assistant_url.h"
#include "custom_look_and_feel.h"
#include "i18n/translation.h"

namespace jyglobalvst::tray {

namespace {

juce::WebBrowserComponent::Options makeBrowserOptions()
{
    return juce::WebBrowserComponent::Options{}.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
}

// Report channel required by Store policy 11.16 (Live Generative AI Content):
// opens a prefilled GitHub issue in the default browser where the user can
// describe the inappropriate or problematic AI-generated content.
juce::String reportIssueUrl()
{
    return "https://github.com/pelletierjy/GlobalVstHost/issues/new"
           "?title=AI+assistant%3A+report+inappropriate+content";
}

}  // namespace

AssistantPanel::AssistantPanel()
{
    report_button_.setButtonText(i18n::tr("Report an issue"));
    report_button_.setTooltip(
        i18n::tr("Report inappropriate or problematic AI-generated content"));
    report_button_.onClick = [] { juce::URL{reportIssueUrl()}.launchInDefaultBrowser(); };
    addAndMakeVisible(report_button_);

    close_button_.setButtonText("\u00d7");  // multiplication sign = close glyph
    close_button_.setTooltip(i18n::tr("Hide the AI assistant"));
    close_button_.onClick = [this] {
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(close_button_);
}

AssistantPanel::~AssistantPanel() = default;

void AssistantPanel::prepareToShow(bool light_theme)
{
    ensureContent(light_theme);
    resized();
}

void AssistantPanel::ensureContent(bool light_theme)
{
    if (content_built_)
    {
        // Theme changed since the page was loaded: re-navigate so the widget
        // palette follows the app theme (the old dialog recreated itself on
        // every open; the panel lives on, so reload here instead).
        if (light_theme != loaded_light_theme_ && browser_ != nullptr)
        {
            browser_->goToURL(assistantUrl(light_theme));
            if (const auto script = assistantClientInjectionScript(); script.isNotEmpty())
                browser_->evaluateJavascript(script);
        }
        loaded_light_theme_ = light_theme;
        return;
    }
    content_built_ = true;
    loaded_light_theme_ = light_theme;

    const auto options = makeBrowserOptions();

    if (juce::WebBrowserComponent::areOptionsSupported(options))
    {
        browser_ = std::make_unique<juce::WebBrowserComponent>(options);
        addAndMakeVisible(browser_.get());
        browser_->goToURL(assistantUrl(light_theme));
        // Inject the ConversiaCore client credential at runtime through the browser bridge
        // (never in the launch URL — FR-020). goToURL returns while the page is still
        // fetching, and the widget reads window.conversiaClient lazily on its first AI
        // request, so this lands well before any credential is needed.
        if (const auto script = assistantClientInjectionScript(); script.isNotEmpty())
            browser_->evaluateJavascript(script);
        // NOTE: the default-browser fallback below cannot inject — without a credential that
        // path surfaces the widget's friendly "not configured" message rather than an error.
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
        addAndMakeVisible(fallback_label_.get());
        juce::URL{assistantUrl(light_theme)}.launchInDefaultBrowser();
    }
}

void AssistantPanel::paint(juce::Graphics& g)
{
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    if (laf != nullptr)
        laf->drawGlassPanel(g, getLocalBounds().toFloat().reduced(0.5f));
    else
        g.fillAll(kBgPanel);

    // Section title, drawn like GlassPanel::paint — trimmed clear of the
    // header buttons, which are laid out on the right of the same strip.
    const juce::Colour titleCol = (laf != nullptr) ? laf->colors().accentCyan : kAccentCyan;

    juce::FontOptions fo;
    juce::Font f{fo};
    f.setHeight(11.0f);
    g.setFont(f);
    g.setColour(titleCol.withAlpha(0.75f));

    auto titleArea = getLocalBounds().removeFromTop(kHeaderH).reduced(10, 0);
    const int buttonsLeft = juce::jmin(report_button_.getX(), close_button_.getX()) - 6;
    titleArea = titleArea.withTrimmedRight(juce::jmax(0, titleArea.getRight() - buttonsLeft));
    g.drawText(i18n::tr("AI ASSISTANT"), titleArea, juce::Justification::centredLeft, false);

    // Thin separator line below the header strip.
    g.setColour(titleCol.withAlpha(0.15f));
    g.fillRect(juce::Rectangle<float>(10.0f,
                                      static_cast<float>(kHeaderH) - 1.0f,
                                      static_cast<float>(getWidth()) - 20.0f,
                                      1.0f));
}

void AssistantPanel::resized()
{
    auto b = getLocalBounds();

    auto header = b.removeFromTop(kHeaderH).reduced(10, 5);
    close_button_.setBounds(header.removeFromRight(22));
    header.removeFromRight(kGap);
    report_button_.setBounds(header.removeFromRight(120));
    b.removeFromTop(kGap);

    if (browser_ != nullptr)
        browser_->setBounds(b.reduced(4));
    else if (fallback_label_ != nullptr)
        fallback_label_->setBounds(b.reduced(10));
}

}  // namespace jyglobalvst::tray