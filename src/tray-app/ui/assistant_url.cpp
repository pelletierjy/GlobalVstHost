// src/tray-app/ui/assistant_url.cpp

#include "assistant_url.h"

namespace jyglobalvst::tray {

juce::String assistantUrl(bool light_theme)
{
    // subject=Others + gradeLevel=20: forces the session straight past both
    // pickers, and "Others" is a subject no knowledge-base entry is filed
    // under, so the assistant runs with no RAG grounding — just the host
    // system prompt (see appConfig/GlobalVstHost in the ConversiaCore admin).
    return juce::String{"https://pelletierjy.github.io/ConversiaCore/"} +
           "?context=GlobalVstHost&subject=Others&gradeLevel=20&theme=" +
           (light_theme ? "light" : "dark");
}

}  // namespace jyglobalvst::tray
