// tests/unit/assistant_url_test.cpp
//
// The in-app AI Assistant loads the deployed ConversiaCore tutor page. These
// tests pin the exact URL so a typo can never silently 404 the assistant.
//
// subject=Others + gradeLevel=20 together (a) bypass both pickers — the SPA
// only shows them when either param is missing — and (b) key the knowledge
// base to a subject no entry is ever filed under, so the assistant runs
// purely on the host system prompt with no RAG grounding.

#include "assistant_url.h"

#include <gtest/gtest.h>

using jyglobalvst::tray::assistantUrl;

TEST(AssistantUrlTest, DarkThemeUsesExactDeployedConversiaCoreUrl)
{
    EXPECT_EQ(assistantUrl(false).toStdString(),
              "https://pelletierjy.github.io/ConversiaCore/?context=GlobalVstHost"
              "&subject=Others&gradeLevel=20&theme=dark");
}

TEST(AssistantUrlTest, LightThemeUsesLightPalette)
{
    EXPECT_EQ(assistantUrl(true).toStdString(),
              "https://pelletierjy.github.io/ConversiaCore/?context=GlobalVstHost"
              "&subject=Others&gradeLevel=20&theme=light");
}
