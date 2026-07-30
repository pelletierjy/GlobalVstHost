#include "volume_leveler_editor.h"
#include "volume_leveler_processor.h"
#include "builtin_theme.h"

namespace jyglobalvst::engine {

VolumeLevelerEditor::VolumeLevelerEditor(VolumeLevelerProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor)
{
    setupParamControl(threshold_, "Threshold", -60.0, 0.0, -20.0, " dB");
    setupParamControl(ratio_, "Ratio", 1.0, 20.0, 4.0, ":1");
    setupParamControl(attack_, "Attack", 0.1, 100.0, 10.0, " ms");
    setupParamControl(release_, "Release", 1.0, 1000.0, 100.0, " ms");
    setupParamControl(makeup_, "Makeup", 0.0, 24.0, 0.0, " dB");

    threshold_.slider->setValue(processor_.getThresholdDb(), juce::dontSendNotification);
    ratio_.slider->setValue(processor_.getRatio(), juce::dontSendNotification);
    attack_.slider->setValue(processor_.getAttackMs(), juce::dontSendNotification);
    release_.slider->setValue(processor_.getReleaseMs(), juce::dontSendNotification);
    makeup_.slider->setValue(processor_.getMakeupDb(), juce::dontSendNotification);

    updateValueLabels();
    applyThemeColors();

    setSize(320, 240);
}

VolumeLevelerEditor::~VolumeLevelerEditor() = default;

void VolumeLevelerEditor::setupParamControl(ParamControl& ctrl,
                                            const juce::String& name,
                                            double min,
                                            double max,
                                            double default_val,
                                            const juce::String& suffix)
{
    ctrl.label = std::make_unique<juce::Label>(juce::String(), name);
    ctrl.label->setJustificationType(juce::Justification::centredLeft);

    ctrl.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                  juce::Slider::NoTextBox);
    ctrl.slider->setRange(min, max, 0.0);
    ctrl.slider->setValue(default_val, juce::dontSendNotification);
    ctrl.slider->addListener(this);

    ctrl.value_label = std::make_unique<juce::Label>(juce::String(), juce::String(default_val, 1) + suffix);
    ctrl.value_label->setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(ctrl.label.get());
    addAndMakeVisible(ctrl.slider.get());
    addAndMakeVisible(ctrl.value_label.get());
}

void VolumeLevelerEditor::paint(juce::Graphics& g)
{
    g.fillAll(engine::builtinThemeColors().background);
}

void VolumeLevelerEditor::resized()
{
    auto b = getLocalBounds().reduced(16);
    b.removeFromTop(8);  // title gap

    auto row = [&b](ParamControl& ctrl) {
        auto line = b.removeFromTop(28);
        ctrl.label->setBounds(line.removeFromLeft(70));
        ctrl.value_label->setBounds(line.removeFromRight(60));
        ctrl.slider->setBounds(line.reduced(4, 0));
        b.removeFromTop(8);
    };

    row(threshold_);
    row(ratio_);
    row(attack_);
    row(release_);
    row(makeup_);
}

void VolumeLevelerEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == threshold_.slider.get())
        processor_.setThresholdDb(static_cast<float>(slider->getValue()));
    else if (slider == ratio_.slider.get())
        processor_.setRatio(static_cast<float>(slider->getValue()));
    else if (slider == attack_.slider.get())
        processor_.setAttackMs(static_cast<float>(slider->getValue()));
    else if (slider == release_.slider.get())
        processor_.setReleaseMs(static_cast<float>(slider->getValue()));
    else if (slider == makeup_.slider.get())
        processor_.setMakeupDb(static_cast<float>(slider->getValue()));

    updateValueLabels();
}

void VolumeLevelerEditor::updateValueLabels()
{
    threshold_.value_label->setText(juce::String(threshold_.slider->getValue(), 1) + " dB",
                                    juce::dontSendNotification);
    ratio_.value_label->setText(juce::String(ratio_.slider->getValue(), 1) + ":1",
                                juce::dontSendNotification);
    attack_.value_label->setText(juce::String(attack_.slider->getValue(), 1) + " ms",
                                juce::dontSendNotification);
    release_.value_label->setText(juce::String(release_.slider->getValue(), 1) + " ms",
                                 juce::dontSendNotification);
    makeup_.value_label->setText(juce::String(makeup_.slider->getValue(), 1) + " dB",
                                juce::dontSendNotification);
}

void VolumeLevelerEditor::applyThemeColors()
{
    const auto& c = engine::builtinThemeColors();
    for (ParamControl* ctrl : { &threshold_, &ratio_, &attack_, &release_, &makeup_ })
    {
        ctrl->label->setColour(juce::Label::textColourId, c.textPrimary);
        ctrl->value_label->setColour(juce::Label::textColourId, c.textDim);
    }
}

}  // namespace jyglobalvst::engine
