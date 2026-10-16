// src/tray-app/ui/assistant_url.h
//
// URL builder for the in-app AI Assistant (ConversiaCore <conversia-app> tutor).

#pragma once

#include <juce_core/juce_core.h>

namespace jyglobalvst::tray {

// Returns the deployed ConversiaCore tutor page URL used by AssistantDialog.
// light_theme maps the app's CustomAndFeel theme onto the widget palette.
juce::String assistantUrl(bool light_theme);

// Builds the runtime client-credential injection snippet for the ConversiaCore widget
// (ConversiaCore feature 002-secure-embed-telemetry). The credential is delivered through
// the browser bridge and must NEVER be placed in the launch URL (FR-020).
// Returns an empty string when either argument is empty — an unissued key means the
// assistant degrades with a friendly "not configured" message instead of failing silently.
juce::String clientInjectionScript(const juce::String& remote_id, const juce::String& key);

// The configured credential for this app (operator-issued, see assistant_url.cpp).
// Empty until the operator fills in the key issued by ConversiaCore's admin CLI.
juce::String assistantClientInjectionScript();

}  // namespace jyglobalvst::tray
