// src/audio-engine/builtin-effects/builtin_theme.h
//
// Theme colours for the built-in effect editors (EQ + Bass Boost, Auto volume leveller / Compressor).
//
// The built-in editors live in the audio-engine layer and cannot depend on the
// tray-app's CustomLookAndFeel. Instead the tray app pushes the currently
// selected theme's colours into this holder whenever the theme changes, and the
// built-in editors read them when they are constructed / painted. Access is
// confined to the message thread (theme changes and editor creation both happen
// there), so no synchronisation is required.

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace jyglobalvst::engine {

// A minimal subset of the tray-app theme palette, expressed in engine-layer
// terms so the built-in editors can colour themselves to match.
struct BuiltinThemeColors
{
    juce::Colour background {0xFF0A0E1A};
    juce::Colour panel {0xFF12182B};
    juce::Colour panelBorder {0xFF1E2A4A};
    juce::Colour accent {0xFF00E5FF};
    juce::Colour accentSecondary {0xFF2979FF};
    juce::Colour textPrimary {0xFFE0E6F1};
    juce::Colour textDim {0xFF8A96B8};
    juce::Colour controlBg {0xFF1A2340};
    juce::Colour controlHover {0xFF243560};
    juce::Colour trackBg {0xFF0F1525};
};

// Returns the shared, mutable theme-colour holder. The tray app writes to it on
// theme change; the built-in editors read from it. Defaults to the NeonBlue
// palette so the editors are still sensibly coloured if the theme was never set.
inline BuiltinThemeColors& builtinThemeColors()
{
    static BuiltinThemeColors colors;
    return colors;
}

}  // namespace jyglobalvst::engine
