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
using jyglobalvst::tray::clientInjectionScript;

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

// --- ConversiaCore client-credential injection (feature 002-secure-embed-telemetry) ---

TEST(ClientInjectionScriptTest, EmptyKeyOrRemoteIdYieldsNoInjection)
{
    EXPECT_TRUE(clientInjectionScript("globalvsthost", "").isEmpty());
    EXPECT_TRUE(clientInjectionScript("", "cvk_globalvsthost_abc").isEmpty());
    EXPECT_TRUE(clientInjectionScript("", "").isEmpty());
}

TEST(ClientInjectionScriptTest, ValidCredentialsProduceTheWidgetGlobal)
{
    const auto script = clientInjectionScript("globalvsthost", "cvk_globalvsthost_AbC123");
    EXPECT_EQ(script.toStdString(),
              "window.conversiaClient = { version: 1, remoteId: \"globalvsthost\", "
              "key: \"cvk_globalvsthost_AbC123\" };");
}

TEST(ClientInjectionScriptTest, LaunchUrlNeverCarriesTheCredential)
{
    // FR-020: the credential travels via runtime injection only — never in a URL.
    for (const bool light : { false, true })
    {
        const auto url = assistantUrl(light).toStdString();
        EXPECT_EQ(url.find("cvk_"), std::string::npos);
        EXPECT_EQ(url.find("key="), std::string::npos);
        EXPECT_EQ(url.find("remoteId="), std::string::npos);
    }
}
