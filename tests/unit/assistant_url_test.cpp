// tests/unit/assistant_url_test.cpp
//
// The in-app AI Assistant loads the deployed ConversiaCore tutor page. These
// tests pin the exact URL so a typo can never silently 404 the assistant.

#include "assistant_url.h"

#include <gtest/gtest.h>

using jyglobalvst::tray::assistantUrl;

TEST(AssistantUrlTest, DarkThemeUsesExactDeployedConversiaCoreUrl)
{
    EXPECT_EQ(assistantUrl(false).toStdString(),
              "https://pelletierjy.github.io/ConversiaCore/?context=GlobalVstHost&theme=dark");
}

TEST(AssistantUrlTest, LightThemeUsesLightPalette)
{
    EXPECT_EQ(assistantUrl(true).toStdString(),
              "https://pelletierjy.github.io/ConversiaCore/?context=GlobalVstHost&theme=light");
}
