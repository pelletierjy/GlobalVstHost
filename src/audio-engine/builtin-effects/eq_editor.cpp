#include "eq_editor.h"
#include "eq_processor.h"
#include "builtin_ids.h"
#include "builtin_theme.h"
#include <cmath>

namespace jyglobalvst::engine {

namespace {

// Normalizes a linear peak amplitude (0..~1) to a 0..1 meter position, using
// the same -60 dB floor convention as the tray app's level meters.
constexpr float kBandMeterMinDb = -60.0f;

float bandLevelToNormalized(float linear_peak)
{
    if (linear_peak <= 0.0f)
        return 0.0f;
    float db = 20.0f * std::log10(linear_peak);
    if (db <= kBandMeterMinDb)
        return 0.0f;
    if (db >= 0.0f)
        return 1.0f;
    return (db - kBandMeterMinDb) / (-kBandMeterMinDb);
}

// Green -> yellow -> orange -> red ramp; red signals the band is hot enough
// to be pushing the mix toward clipping/distortion.
juce::Colour bandMeterColour(float norm)
{
    static const juce::Colour green {0xFF00E676};
    static const juce::Colour yellow{0xFFFFD400};
    static const juce::Colour orange{0xFFFF9100};
    static const juce::Colour red   {0xFFFF1744};

    if (norm <= 0.60f)
        return green;
    if (norm <= 0.80f)
        return green.interpolatedWith(yellow, (norm - 0.60f) / 0.20f);
    if (norm <= 0.92f)
        return yellow.interpolatedWith(orange, (norm - 0.80f) / 0.12f);
    return orange.interpolatedWith(red, juce::jlimit(0.0f, 1.0f, (norm - 0.92f) / 0.08f));
}

}  // namespace

EqEditor::EqEditor(EqProcessor& processor) : juce::AudioProcessorEditor(&processor), processor_(processor)
{
    // Input volume trim, positioned before the band sliders.
    input_volume_slider_ = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                            juce::Slider::TextBoxBelow);
    input_volume_slider_->setRange(builtin::eq::INPUT_VOLUME_MIN_DB, builtin::eq::INPUT_VOLUME_MAX_DB, 0.1);
    input_volume_slider_->setValue(0.0);
    input_volume_slider_->addListener(this);
    addAndMakeVisible(*input_volume_slider_);

    input_volume_label_ = std::make_unique<juce::Label>("InputVolumeLabel", "Volume");
    input_volume_label_->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*input_volume_label_);

    // Create band sliders (0-9)
    for (int i = 0; i < 10; ++i)
    {
        band_sliders_[i] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                           juce::Slider::TextBoxBelow);
        band_sliders_[i]->setRange(-12.0, 12.0, 0.1);
        band_sliders_[i]->setValue(0.0);
        band_sliders_[i]->addListener(this);
        addAndMakeVisible(*band_sliders_[i]);

        band_labels_[i] = std::make_unique<juce::Label>(
            "BandLabel" + juce::String(i),
            juce::String(builtin::eq::BAND_CENTERS_HZ[i], 0) + " Hz");
        band_labels_[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*band_labels_[i]);
    }

    // Bass boost slider
    bass_boost_slider_ = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                         juce::Slider::TextBoxRight);
    bass_boost_slider_->setRange(0.0, 12.0, 0.1);
    bass_boost_slider_->setValue(0.0);
    bass_boost_slider_->setTextValueSuffix(" dB");
    bass_boost_slider_->addListener(this);
    addAndMakeVisible(*bass_boost_slider_);

    bass_boost_label_ = std::make_unique<juce::Label>("BassBoostLabel", "Bass Boost:");
    bass_boost_label_->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*bass_boost_label_);

    // Reset button
    reset_button_ = std::make_unique<juce::TextButton>("Flat/Reset");
    reset_button_->addListener(this);
    addAndMakeVisible(*reset_button_);

    updateBandSliders();
    updateBassBoostSlider();
    updateInputVolumeSlider();

    applyThemeColors();

    setSize(660, 350);

    startTimerHz(30);
}

EqEditor::~EqEditor()
{
    stopTimer();
}

void EqEditor::timerCallback()
{
    repaint();
}

void EqEditor::applyThemeColors()
{
    const auto& c = builtinThemeColors();

    input_volume_slider_->setColour(juce::Slider::thumbColourId, c.accent);
    input_volume_slider_->setColour(juce::Slider::trackColourId, c.trackBg);
    input_volume_slider_->setColour(juce::Slider::backgroundColourId, c.trackBg);
    input_volume_slider_->setColour(juce::Slider::textBoxTextColourId, c.textPrimary);
    input_volume_slider_->setColour(juce::Slider::textBoxBackgroundColourId, c.controlBg);
    input_volume_slider_->setColour(juce::Slider::textBoxOutlineColourId, c.panelBorder);
    input_volume_label_->setColour(juce::Label::textColourId, c.textPrimary);

    for (int i = 0; i < 10; ++i)
    {
        band_sliders_[i]->setColour(juce::Slider::thumbColourId, c.accent);
        band_sliders_[i]->setColour(juce::Slider::trackColourId, c.trackBg);
        band_sliders_[i]->setColour(juce::Slider::backgroundColourId, c.trackBg);
        band_sliders_[i]->setColour(juce::Slider::textBoxTextColourId, c.textPrimary);
        band_sliders_[i]->setColour(juce::Slider::textBoxBackgroundColourId, c.controlBg);
        band_sliders_[i]->setColour(juce::Slider::textBoxOutlineColourId, c.panelBorder);
        band_labels_[i]->setColour(juce::Label::textColourId, c.textDim);
    }

    bass_boost_slider_->setColour(juce::Slider::thumbColourId, c.accent);
    bass_boost_slider_->setColour(juce::Slider::trackColourId, c.accentSecondary);
    bass_boost_slider_->setColour(juce::Slider::backgroundColourId, c.trackBg);
    bass_boost_slider_->setColour(juce::Slider::textBoxTextColourId, c.textPrimary);
    bass_boost_slider_->setColour(juce::Slider::textBoxBackgroundColourId, c.controlBg);
    bass_boost_slider_->setColour(juce::Slider::textBoxOutlineColourId, c.panelBorder);
    bass_boost_label_->setColour(juce::Label::textColourId, c.textPrimary);

    reset_button_->setColour(juce::TextButton::buttonColourId, c.controlBg);
    reset_button_->setColour(juce::TextButton::textColourOffId, c.textPrimary);
    reset_button_->setColour(juce::TextButton::textColourOnId, c.textPrimary);
}

void EqEditor::paint(juce::Graphics& g)
{
    const auto& c = builtinThemeColors();

    g.fillAll(c.background);

    // Zero-dB reference line, aligned with the band sliders' 0.0 gain position.
    const int zeroY = band_sliders_[0]->getY()
                       + static_cast<int>(band_sliders_[0]->getPositionOfValue(0.0));
    g.setColour(c.accent);
    g.fillRect(0, zeroY, getWidth(), 1);

    // Per-band level meter: a bar centred on the zero-gain line that bumps
    // symmetrically up/down with that band's signal intensity, coloured
    // green -> yellow -> orange -> red as it approaches distortion.
    constexpr int kMeterBarWidth = 10;
    for (int i = 0; i < 10; ++i)
    {
        const auto bounds = band_sliders_[i]->getBounds();
        const int centerX = bounds.getCentreX();
        const int top = bounds.getY();
        const int bottom = bounds.getBottom();
        const int bandZeroY = top + static_cast<int>(band_sliders_[i]->getPositionOfValue(0.0));

        // Faint resting track, visible even at silence.
        g.setColour(c.trackBg);
        g.fillRect(centerX - 1, top, 2, bottom - top);

        const float norm = bandLevelToNormalized(processor_.getBandLevel(i));
        if (norm <= 0.0f)
            continue;

        const int extentUp = static_cast<int>((bandZeroY - top) * norm);
        const int extentDown = static_cast<int>((bottom - bandZeroY) * norm);

        g.setColour(bandMeterColour(norm));
        g.fillRect(centerX - kMeterBarWidth / 2, bandZeroY - extentUp,
                   kMeterBarWidth, extentUp + extentDown);
    }
}

void EqEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Bottom control area
    auto bottomArea = bounds.removeFromBottom(80);

    auto bassRow = bottomArea.removeFromTop(40);
    bass_boost_label_->setBounds(bassRow.removeFromLeft(100));
    bass_boost_slider_->setBounds(bassRow.removeFromLeft(200));
    reset_button_->setBounds(bassRow.removeFromLeft(100));

    // Band sliders area (input volume + 10 bands)
    int bandWidth = bounds.getWidth() / 11;

    auto volumeArea = bounds.removeFromLeft(bandWidth);
    auto volumeLabelArea = volumeArea.removeFromBottom(30);
    input_volume_slider_->setBounds(volumeArea);
    input_volume_label_->setBounds(volumeLabelArea);

    for (int i = 0; i < 10; ++i)
    {
        auto bandArea = bounds.removeFromLeft(bandWidth);
        auto labelArea = bandArea.removeFromBottom(30);

        band_sliders_[i]->setBounds(bandArea);
        band_labels_[i]->setBounds(labelArea);
    }
}

void EqEditor::sliderValueChanged(juce::Slider* slider)
{
    // Check if it's the input volume slider
    if (slider == input_volume_slider_.get())
    {
        processor_.setInputVolume(static_cast<float>(slider->getValue()));
        return;
    }

    // Check if it's a band slider
    for (int i = 0; i < 10; ++i)
    {
        if (slider == band_sliders_[i].get())
        {
            processor_.setBandGain(i, static_cast<float>(slider->getValue()));
            return;
        }
    }

    // Check if it's the bass boost slider
    if (slider == bass_boost_slider_.get())
    {
        processor_.setBassBoost(static_cast<float>(slider->getValue()));
    }
}

void EqEditor::buttonClicked(juce::Button* button)
{
    if (button == reset_button_.get())
    {
        // Reset all bands to 0
        for (int i = 0; i < 10; ++i)
        {
            processor_.setBandGain(i, 0.0f);
            band_sliders_[i]->setValue(0.0, juce::NotificationType::dontSendNotification);
        }

        processor_.setBassBoost(0.0f);
        bass_boost_slider_->setValue(0.0, juce::NotificationType::dontSendNotification);

        processor_.setInputVolume(0.0f);
        input_volume_slider_->setValue(0.0, juce::NotificationType::dontSendNotification);
    }
}

void EqEditor::updateBandSliders()
{
    for (int i = 0; i < 10; ++i)
    {
        band_sliders_[i]->setValue(processor_.getBandGain(i),
                                    juce::NotificationType::dontSendNotification);
    }
}

void EqEditor::updateBassBoostSlider()
{
    bass_boost_slider_->setValue(processor_.getBassBoost(),
                                 juce::NotificationType::dontSendNotification);
}

void EqEditor::updateInputVolumeSlider()
{
    input_volume_slider_->setValue(processor_.getInputVolume(),
                                    juce::NotificationType::dontSendNotification);
}

}  // namespace jyglobalvst::engine
