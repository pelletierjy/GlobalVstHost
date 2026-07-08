// src/tray-app/ui/accessibility/uia_handlers.h
//
// T111 — UIA accessible names/roles/values via JUCE AccessibilityHandler.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

// Configure a ComboBox with accessible name, role, and value.
void configureAccessibleComboBox(juce::ComboBox& box, const juce::String& name);

// Configure a TextButton with accessible name, role, and description.
void configureAccessibleButton(juce::TextButton& btn,
                               const juce::String& name,
                               const juce::String& description = {});

// Configure a Label with accessible name (static text role).
void configureAccessibleLabel(juce::Label& lbl, const juce::String& name);

// Configure a Slider with accessible name and value.
void configureAccessibleSlider(juce::Slider& slider, const juce::String& name);

// Configure a ToggleButton with accessible name and checked state.
void configureAccessibleToggleButton(juce::ToggleButton& btn,
                                     const juce::String& name);

}  // namespace jyglobalvst::tray
