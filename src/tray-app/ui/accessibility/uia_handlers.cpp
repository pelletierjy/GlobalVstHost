// src/tray-app/ui/accessibility/uia_handlers.cpp
//
// T111 — UIA accessible names/roles/values via JUCE AccessibilityHandler.

#include "uia_handlers.h"

namespace jyglobalvst::tray {

void configureAccessibleComboBox(juce::ComboBox& box, const juce::String& name)
{
    box.setTitle(name);
    box.setDescription(name);
    box.setAccessible(true);
    box.setWantsKeyboardFocus(true);
}

void configureAccessibleButton(juce::TextButton& btn,
                               const juce::String& name,
                               const juce::String& description)
{
    btn.setButtonText(name);
    btn.setDescription(description.isEmpty() ? name : description);
    btn.setAccessible(true);
    btn.setWantsKeyboardFocus(true);
}

void configureAccessibleLabel(juce::Label& lbl, const juce::String& name)
{
    lbl.setDescription(name);
    lbl.setAccessible(true);
}

void configureAccessibleSlider(juce::Slider& slider, const juce::String& name)
{
    slider.setTitle(name);
    slider.setDescription(name);
    slider.setAccessible(true);
    slider.setWantsKeyboardFocus(true);
}

void configureAccessibleToggleButton(juce::ToggleButton& btn, const juce::String& name)
{
    btn.setButtonText(name);
    btn.setDescription(name);
    btn.setAccessible(true);
    btn.setWantsKeyboardFocus(true);
}

}  // namespace jyglobalvst::tray
