#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

namespace jyglobalvst::engine {

class EqProcessor;

class EqEditor : public juce::AudioProcessorEditor,
                 public juce::Slider::Listener,
                 public juce::Button::Listener,
                 private juce::Timer
{
public:
    explicit EqEditor(EqProcessor& processor);
    ~EqEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

private:
    void timerCallback() override;

    EqProcessor& processor_;

    std::unique_ptr<juce::Slider> input_volume_slider_;
    std::unique_ptr<juce::Label> input_volume_label_;

    std::array<std::unique_ptr<juce::Slider>, 10> band_sliders_;
    std::array<std::unique_ptr<juce::Label>, 10> band_labels_;

    std::unique_ptr<juce::Slider> bass_boost_slider_;
    std::unique_ptr<juce::Label> bass_boost_label_;

    std::unique_ptr<juce::TextButton> reset_button_;

    void updateBandSliders();
    void updateBassBoostSlider();
    void updateInputVolumeSlider();
    void applyThemeColors();
};

}  // namespace jyglobalvst::engine
