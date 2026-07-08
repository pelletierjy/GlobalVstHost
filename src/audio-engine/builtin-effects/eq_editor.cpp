#include "eq_editor.h"
#include "eq_processor.h"
#include "builtin_ids.h"
#include "builtin_theme.h"

namespace jyglobalvst::engine {

EqEditor::EqEditor(EqProcessor& processor) : juce::AudioProcessorEditor(&processor), processor_(processor)
{
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

    applyThemeColors();

    setSize(600, 350);
}

EqEditor::~EqEditor() = default;

void EqEditor::applyThemeColors()
{
    const auto& c = builtinThemeColors();

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
    g.fillAll(builtinThemeColors().background);
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

    // Band sliders area
    int bandWidth = bounds.getWidth() / 10;

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

}  // namespace jyglobalvst::engine
