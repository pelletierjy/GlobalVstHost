#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace jyglobalvst::engine {

class NightTimeProcessor;

class NightTimeEditor : public juce::AudioProcessorEditor,
                        public juce::ComboBox::Listener
{
public:
    explicit NightTimeEditor(NightTimeProcessor& processor);
    ~NightTimeEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void comboBoxChanged(juce::ComboBox* combo) override;

private:
    NightTimeProcessor& processor_;

    std::unique_ptr<juce::ComboBox> preset_combo_;
    std::unique_ptr<juce::Label> preset_label_;

    // Illustrative before/after graph. Synthesizes a high-dynamic-range example
    // signal and runs it through the same AGC/limiter math the processor uses for
    // the selected preset, so the user can see how dynamics are evened out. This
    // is a visual illustration, not a live meter of the actual audio.
    class DynamicsGraph : public juce::Component
    {
    public:
        explicit DynamicsGraph(NightTimeProcessor& processor) : processor_(processor) {}
        void paint(juce::Graphics& g) override;

    private:
        NightTimeProcessor& processor_;
    };

    std::unique_ptr<DynamicsGraph> graph_;
    std::unique_ptr<juce::Label> graph_caption_;

    void updatePresetCombo();
    void applyThemeColors();
};

}  // namespace jyglobalvst::engine
