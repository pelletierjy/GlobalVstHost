// tests/unit/i18n_translation_test.cpp
//
// FR/ES localization (supersedes the v1 English-only constraint FR-019c):
// language preference round-trip, table installation, key coverage and the
// invariants the generated tables must keep (non-empty values, printf
// specifiers preserved by every translated format string).

#include <gtest/gtest.h>

#include "i18n/translation.h"

#include <juce_core/juce_core.h>

#include <string>

namespace i18n = jyglobalvst::tray::i18n;

namespace
{

int countPercent(const juce::String& text)
{
    int n = 0;
    const char* p = text.toRawUTF8();
    for (; *p != '\0'; ++p)
        if (*p == '%')
            ++n;
    return n;
}

std::string tr(const char* key)
{
    return i18n::tr(key).toStdString();
}

}  // namespace

TEST(I18nTranslation, LanguageCodesRoundTrip)
{
    EXPECT_EQ(std::string(i18n::languageCode(i18n::Language::System)), "system");
    EXPECT_EQ(std::string(i18n::languageCode(i18n::Language::English)), "en");
    EXPECT_EQ(std::string(i18n::languageCode(i18n::Language::French)), "fr");
    EXPECT_EQ(std::string(i18n::languageCode(i18n::Language::Spanish)), "es");

    EXPECT_EQ(i18n::parseLanguage("system"), i18n::Language::System);
    EXPECT_EQ(i18n::parseLanguage("en"), i18n::Language::English);
    EXPECT_EQ(i18n::parseLanguage("fr"), i18n::Language::French);
    EXPECT_EQ(i18n::parseLanguage("es"), i18n::Language::Spanish);

    // Unknown or missing codes fall back to "follow the system language".
    EXPECT_EQ(i18n::parseLanguage("de"), i18n::Language::System);
    EXPECT_EQ(i18n::parseLanguage(""), i18n::Language::System);
}

TEST(I18nTranslation, EnglishInstallsNoMappings)
{
    i18n::install(i18n::Language::English);

    EXPECT_EQ(i18n::installedLanguage(), i18n::Language::English);
    EXPECT_EQ(tr("Ready"), "Ready");
    EXPECT_EQ(tr("Save Preset..."), "Save Preset...");
    EXPECT_EQ(i18n::trW(L"Exit"), std::wstring(L"Exit"));
}

TEST(I18nTranslation, FrenchTableResolves)
{
    i18n::install(i18n::Language::French);

    EXPECT_EQ(i18n::installedLanguage(), i18n::Language::French);
    EXPECT_EQ(tr("Ready"), "Prêt");
    EXPECT_EQ(tr("Settings"), "Paramètres");
    EXPECT_EQ(tr("Cancel"), "Annuler");
    EXPECT_EQ(tr("Save Preset..."), "Enregistrer le préréglage...");
    EXPECT_EQ(tr("Captured audio device:"), "Périphérique de capture :");
    EXPECT_EQ(i18n::trW(L"Exit"), std::wstring(L"Quitter"));
    // Accented wide string: guards against an ASCII/Latin-1 conversion sneaking in.
    EXPECT_EQ(i18n::trW(L"Start Audio"), std::wstring(L"Démarrer l'audio"));
    EXPECT_EQ(i18n::languageDisplayName(i18n::Language::System).toStdString(),
              std::string("Langue du système"));

    // Keys with no entry stay English instead of coming back empty.
    EXPECT_EQ(tr("A key nobody translated"), "A key nobody translated");

    i18n::install(i18n::Language::English);
}

TEST(I18nTranslation, SpanishTableResolves)
{
    i18n::install(i18n::Language::Spanish);

    EXPECT_EQ(i18n::installedLanguage(), i18n::Language::Spanish);
    EXPECT_EQ(tr("Ready"), "Listo");
    EXPECT_EQ(tr("Settings"), "Configuración");
    EXPECT_EQ(tr("Cancel"), "Cancelar");
    EXPECT_EQ(tr("Save Preset..."), "Guardar preconfiguración...");
    EXPECT_EQ(tr("Captured audio device:"), "Dispositivo de captura:");
    EXPECT_EQ(i18n::trW(L"Exit"), std::wstring(L"Salir"));

    i18n::install(i18n::Language::English);
}

TEST(I18nTranslation, SystemPreferenceResolvesThroughWindowsLanguage)
{
    // Language::System must land on a concrete language (fr/es/en depending on
    // the machine's Windows UI language) and must never leave the app with a
    // half-installed table.
    i18n::install(i18n::Language::System);
    const auto active = i18n::installedLanguage();
    EXPECT_TRUE(active == i18n::Language::English || active == i18n::Language::French ||
                active == i18n::Language::Spanish);
    EXPECT_FALSE(tr("Ready").empty());
    i18n::install(i18n::Language::English);
}

TEST(I18nTranslation, FrenchTableMeetsStructureInvariants)
{
    i18n::install(i18n::Language::French);
    // i18n-OK: the FR/ES tables are read back for structure checks (FR-019c amended).
    const auto* mappings = juce::LocalisedStrings::getCurrentMappings();  // i18n-OK: FR/ES localization
    ASSERT_NE(mappings, nullptr);

    const auto& keys = mappings->getMappings().getAllKeys();
    const auto& values = mappings->getMappings().getAllValues();
    ASSERT_EQ(keys.size(), values.size());
    EXPECT_GE(keys.size(), 130) << "French table lost entries";

    for (int i = 0; i < keys.size(); ++i)
    {
        const auto& key = keys.getReference(i);
        const auto& value = values.getReference(i);
        EXPECT_FALSE(value.isEmpty()) << "empty French value for key: " << key;
        if (key.contains("%"))
        {
            EXPECT_EQ(countPercent(key), countPercent(value))
                << "printf specifiers changed in French translation of: " << key;
        }
    }

    i18n::install(i18n::Language::English);
}

TEST(I18nTranslation, SpanishTableMeetsStructureInvariants)
{
    i18n::install(i18n::Language::Spanish);
    // i18n-OK: the FR/ES tables are read back for structure checks (FR-019c amended).
    const auto* mappings = juce::LocalisedStrings::getCurrentMappings();  // i18n-OK: FR/ES localization
    ASSERT_NE(mappings, nullptr);

    const auto& keys = mappings->getMappings().getAllKeys();
    const auto& values = mappings->getMappings().getAllValues();
    ASSERT_EQ(keys.size(), values.size());
    EXPECT_GE(keys.size(), 130) << "Spanish table lost entries";

    for (int i = 0; i < keys.size(); ++i)
    {
        const auto& key = keys.getReference(i);
        const auto& value = values.getReference(i);
        EXPECT_FALSE(value.isEmpty()) << "empty Spanish value for key: " << key;
        if (key.contains("%"))
        {
            EXPECT_EQ(countPercent(key), countPercent(value))
                << "printf specifiers changed in Spanish translation of: " << key;
        }
    }

    i18n::install(i18n::Language::English);
}
