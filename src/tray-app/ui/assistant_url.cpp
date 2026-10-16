// src/tray-app/ui/assistant_url.cpp

#include "assistant_url.h"

namespace jyglobalvst::tray {

namespace {

// Operator-issued embedding credential (ConversiaCore feature 002-secure-embed-telemetry).
// Before releasing an app build, fill kConversiaClientKey with the one-time key printed by:
//   node tools/conversia-admin/index.js client add --remote-id globalvsthost
// (ConversiaCore repo). Deploy order: register client → ship this injection → deploy the
// gated worker — see ConversiaCore specs/002-secure-embed-telemetry/release-notes.md.
// An empty key means "not issued yet": no injection happens and the assistant shows the
// widget's friendly "not configured / not authorized" message.
constexpr const char* kConversiaRemoteId = "globalvsthost";
constexpr const char* kConversiaClientKey = "cvk_globalvsthost_CfjXpP9bSlB1eqGQ_oy73o2nmkTKPFi7o0gQmwhnP0o";

}  // namespace

juce::String assistantUrl(bool light_theme)
{
    // subject=Others + gradeLevel=20: forces the session straight past both
    // pickers, and "Others" is a subject no knowledge-base entry is filed
    // under, so the assistant runs with no RAG grounding — just the host
    // system prompt (see appConfig/GlobalVstHost in the ConversiaCore admin).
    return juce::String{"https://pelletierjy.github.io/ConversiaCore/"} +
           "?context=GlobalVstHost&subject=Others&gradeLevel=20&theme=" + (light_theme ? "light" : "dark");
}

juce::String clientInjectionScript(const juce::String& remote_id, const juce::String& key)
{
    if (remote_id.isEmpty() || key.isEmpty())
        return {};
    // remote_id matches ^[a-z0-9][a-z0-9-]{2,63}$ and the key is base64url, so neither can
    // contain quotes or backslashes — plain concatenation yields valid, injection-safe JS.
    return "window.conversiaClient = { version: 1, remoteId: \"" + remote_id + "\", key: \"" + key +
           "\" };";
}

juce::String assistantClientInjectionScript()
{
    return clientInjectionScript(kConversiaRemoteId, kConversiaClientKey);
}

}  // namespace jyglobalvst::tray
