// src/tray-app/ui/assistant_url.cpp

#include "assistant_url.h"

namespace jyglobalvst::tray {

namespace {

// Operator-issued embedding credential (ConversiaCore feature 002-secure-embed-telemetry).
// NEVER committed to source: the value is provided at build time —
//   cmake -B build -A x64 -DJYGLOBALVST_CONVERSIA_CLIENT_KEY=cvk_globalvsthost_...
// (or the JYGLOBALVST_CONVERSIA_CLIENT_KEY environment variable; release CI reads it from
// the JYGLOBALVST_CONVERSIA_CLIENT_KEY repo secret — same pattern as JYGLOBALVST_ASIO_SDK_PATH).
// An empty value means "not provided": no injection happens and the assistant shows the
// widget's friendly "not configured / not authorized" message.
constexpr const char* kConversiaRemoteId = "globalvsthost";

}  // namespace

#ifndef JYGLOBALVST_CONVERSIA_CLIENT_KEY
#define JYGLOBALVST_CONVERSIA_CLIENT_KEY ""
#endif

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
    return clientInjectionScript(kConversiaRemoteId, JYGLOBALVST_CONVERSIA_CLIENT_KEY);
}

}  // namespace jyglobalvst::tray
