// src/tray-app/ui/assistant_url.h
//
// URL builder for the in-app AI Assistant (ConversiaCore <conversia-app> tutor).

#pragma once

#include <juce_core/juce_core.h>

namespace jyglobalvst::tray {

// Returns the deployed ConversiaCore tutor page URL used by AssistantDialog.
// light_theme maps the app's CustomLookAndFeel theme onto the widget palette.
juce::String assistantUrl(bool light_theme);

}  // namespace jyglobalvst::tray
