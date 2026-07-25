#include "nighttime_editor.h"
#include "nighttime_processor.h"
#include "builtin_theme.h"

#include <cmath>

namespace jyglobalvst::engine {

namespace {

// A synthetic amplitude envelope (0..1) with a wide dynamic range: loud, then
// very quiet, medium, quiet, and loud again. Plateaus are joined with smooth
// ramps so the example reads like real programme material.
float exampleEnvelope(float t)
{
    // Control points: {position, level}.
    static const float kPoints[][2] = {
        {0.00f, 0.98f}, {0.16f, 0.98f},  // loud
        {0.22f, 0.10f}, {0.36f, 0.10f},  // very quiet
        {0.42f, 0.55f}, {0.56f, 0.55f},  // medium
        {0.62f, 0.18f}, {0.74f, 0.18f},  // quiet
        {0.80f, 0.92f}, {1.00f, 0.92f},  // loud
    };
    constexpr int n = static_cast<int>(sizeof(kPoints) / sizeof(kPoints[0]));

    if (t <= kPoints[0][0])
        return kPoints[0][1];
    if (t >= kPoints[n - 1][0])
        return kPoints[n - 1][1];

    for (int i = 1; i < n; ++i)
    {
        if (t <= kPoints[i][0])
        {
            float t0 = kPoints[i - 1][0];
            float t1 = kPoints[i][0];
            float span = juce::jmax(1e-4f, t1 - t0);
            float u = (t - t0) / span;
            float s = u * u * (3.f - 2.f * u);  // smoothstep
            return kPoints[i - 1][1] + s * (kPoints[i][1] - kPoints[i - 1][1]);
        }
    }
    return kPoints[n - 1][1];
}

}  // namespace

NightTimeEditor::NightTimeEditor(NightTimeProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor)
{
    // Preset selector
    preset_combo_ = std::make_unique<juce::ComboBox>();
    preset_combo_->addItem("Light", 1);
    preset_combo_->addItem("Medium", 2);
    preset_combo_->addItem("Strong", 3);
    preset_combo_->addItem("Extreme", 4);
    preset_combo_->addListener(this);
    addAndMakeVisible(*preset_combo_);

    preset_label_ = std::make_unique<juce::Label>("PresetLabel", "Preset:");
    preset_label_->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*preset_label_);

    // Look-ahead is fixed at the maximum (10 ms) so the limiter always has full
    // headroom to catch transients; it is no longer user-adjustable.
    processor_.setLookaheadMs(10.0f);

    // Before/after illustration graph.
    graph_ = std::make_unique<DynamicsGraph>(processor_);
    addAndMakeVisible(*graph_);

    // fromUTF8 is required for the em dash: juce::String's const char* constructor does not
    // treat high bytes as UTF-8, so passing them directly renders as mojibake ("â??").
    // Same pattern as the rate labels in main_window.cpp.
    graph_caption_ = std::make_unique<juce::Label>(
        "GraphCaption",
        juce::String::fromUTF8("High-dynamic example \xe2\x80\x94 before (yellow) vs after (green)"));
    graph_caption_->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*graph_caption_);

    updatePresetCombo();

    applyThemeColors();

    setSize(460, 340);
}

NightTimeEditor::~NightTimeEditor() = default;

void NightTimeEditor::applyThemeColors()
{
    const auto& c = builtinThemeColors();

    preset_combo_->setColour(juce::ComboBox::backgroundColourId, c.controlBg);
    preset_combo_->setColour(juce::ComboBox::textColourId, c.textPrimary);
    preset_combo_->setColour(juce::ComboBox::outlineColourId, c.panelBorder);
    preset_combo_->setColour(juce::ComboBox::arrowColourId, c.accent);
    preset_label_->setColour(juce::Label::textColourId, c.textPrimary);

    graph_caption_->setColour(juce::Label::textColourId, c.textDim);
}

void NightTimeEditor::paint(juce::Graphics& g)
{
    g.fillAll(builtinThemeColors().background);
}

void NightTimeEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    auto row1 = bounds.removeFromTop(40);
    preset_label_->setBounds(row1.removeFromLeft(100));
    preset_combo_->setBounds(row1.removeFromLeft(150));

    bounds.removeFromTop(16);

    graph_caption_->setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(4);
    graph_->setBounds(bounds);
}

void NightTimeEditor::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == preset_combo_.get())
    {
        int selectedId = combo->getSelectedId();
        int presetIndex = selectedId - 1;  // IDs are 1-based, presets are 0-based
        processor_.setPresetIndex(presetIndex);
        if (graph_ != nullptr)
            graph_->repaint();  // Reflect the new preset's dynamics.
    }
}

void NightTimeEditor::updatePresetCombo()
{
    int presetIndex = processor_.getPresetIndex();
    preset_combo_->setSelectedId(presetIndex + 1, juce::NotificationType::dontSendNotification);
}

void NightTimeEditor::DynamicsGraph::paint(juce::Graphics& g)
{
    const auto& c = builtinThemeColors();
    auto area = getLocalBounds().toFloat();

    // Panel background + border.
    g.setColour(c.controlBg);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(c.panelBorder);
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);

    auto plot = area.reduced(8.0f);
    if (plot.getWidth() < 8.0f || plot.getHeight() < 8.0f)
        return;

    const float midY = plot.getCentreY();
    const float halfH = plot.getHeight() * 0.5f * 0.92f;

    // Zero line.
    g.setColour(c.panelBorder.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(midY), plot.getX(), plot.getRight());

    // Preset parameters — mirror the DSP in nighttime_processor.cpp.
    auto s = NightTimeProcessor::getPresetSettings(processor_.getPresetIndex());
    const float target_linear = std::pow(10.f, s.target_loudness_db / 20.f);
    const float max_gain_linear = std::pow(10.f, s.max_upward_gain_db / 20.f);
    const float trim_linear = std::pow(10.f, s.output_trim_db / 20.f);
    const float ceiling_linear = std::pow(10.f, s.ceiling_db / 20.f);

    const int N = juce::jmax(64, static_cast<int>(plot.getWidth()));

    // Approximate the AGC attack/release smoothing, assuming the example spans
    // roughly two seconds of audio across the plot width.
    const float points_per_ms = static_cast<float>(N) / 2000.f;
    auto smoothing = [points_per_ms](float ms) {
        return std::exp(-1.f / juce::jmax(1e-3f, ms * points_per_ms));
    };
    const float atk = smoothing(s.attack_ms);
    const float rel = smoothing(s.release_ms);

    juce::Path before_path;
    juce::Path after_path;
    float gain = 1.f;

    for (int i = 0; i < N; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(N - 1);
        const float x = plot.getX() + t * plot.getWidth();

        const float env = exampleEnvelope(t);
        const float carrier = std::sin(t * juce::MathConstants<float>::twoPi * 26.f);
        const float before = env * carrier;

        // Same AGC law as processBlock: drive measured loudness (~env) to target.
        float desired = (env > 1e-4f) ? target_linear / env : 1.f;
        desired = juce::jlimit(0.1f, max_gain_linear, desired);
        const float coeff = (desired > gain) ? atk : rel;
        gain = coeff * gain + (1.f - coeff) * desired;

        float after = before * gain * trim_linear;
        after = juce::jlimit(-ceiling_linear, ceiling_linear, after);

        const float y_before = midY - before * halfH;
        const float y_after = midY - after * halfH;

        if (i == 0)
        {
            before_path.startNewSubPath(x, y_before);
            after_path.startNewSubPath(x, y_after);
        }
        else
        {
            before_path.lineTo(x, y_before);
            after_path.lineTo(x, y_after);
        }
    }

    const juce::Colour before_colour(0xFFFFD54F);  // amber/yellow
    const juce::Colour after_colour(0xFF66BB6A);   // green

    g.setColour(before_colour.withAlpha(0.75f));
    g.strokePath(before_path, juce::PathStrokeType(1.0f));
    g.setColour(after_colour);
    g.strokePath(after_path, juce::PathStrokeType(1.4f));

    // Legend, with an opaque chip behind it so the labels stay legible where the
    // waveform peaks reach the top of the plot.
    auto legend = plot.removeFromTop(18.0f).removeFromRight(148.0f).reduced(2.0f);
    g.setColour(c.controlBg);
    g.fillRoundedRectangle(legend.expanded(3.0f, 1.0f), 3.0f);
    const float sw = 12.0f;
    auto drawKey = [&](juce::Rectangle<float>& r, juce::Colour col, const juce::String& text) {
        auto sq = r.removeFromLeft(sw).withSizeKeepingCentre(sw, 3.0f);
        g.setColour(col);
        g.fillRect(sq);
        r.removeFromLeft(4.0f);
        auto tb = r.removeFromLeft(56.0f);
        g.setColour(c.textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(text, tb, juce::Justification::centredLeft);
    };
    drawKey(legend, before_colour, "Before");
    drawKey(legend, after_colour, "After");
}

}  // namespace jyglobalvst::engine
