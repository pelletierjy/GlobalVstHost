#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

namespace jyglobalvst::engine {

class CompressorProcessor;

class CompressorEditor : public juce::AudioProcessorEditor,
                            public juce::Slider::Listener
{
public:
    explicit CompressorEditor(CompressorProcessor& processor);
    ~CompressorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;

private:
    CompressorProcessor& processor_;

    struct ParamControl
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> value_label;
    };

    ParamControl threshold_;
    ParamControl ratio_;
    ParamControl attack_;
    ParamControl release_;
    ParamControl makeup_;

    void setupParamControl(ParamControl& ctrl,
                           const juce::String& name,
                           double min,
                           double max,
                           double default_val,
                           const juce::String& suffix);
    void updateValueLabels();
    void applyThemeColors();
};

}  // namespace jyglobalvst::engine
