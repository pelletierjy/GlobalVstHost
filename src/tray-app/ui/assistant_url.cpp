// src/tray-app/ui/assistant_url.cpp

#include "assistant_url.h"

namespace jyglobalvst::tray {

juce::String assistantUrl(bool light_theme)
{
    return juce::String{"https://pelletierjy.github.io/ConversiaCore/"} +
           "?context=GlobalVstHost&theme=" + (light_theme ? "light" : "dark");
}

}  // namespace jyglobalvst::tray
