// src/tray-app/i18n/translation.h
//
// FR/ES localization (supersedes the v1 English-only constraint FR-019c).
//
// A thin wrapper over JUCE's LocalisedStrings (juce_core) so every user-visible
// string in the tray app goes through one call site:
//
//     label->setText(i18n::tr("Ready"), juce::dontSendNotification);
//
// Untranslated keys fall back to the English source text, so a missing table
// entry degrades to English rather than breaking the UI. Wide-string helpers
// cover the Win32 APIs (menu bar, folder browser, file-dialog filters) that
// cannot take a juce::String.
//
// The active language comes from RoamingSettings::language
// ("system" | "en" | "fr" | "es", default "system" -> Windows user UI language)
// and is installed once at startup, before the first window is built.

#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace jyglobalvst::tray::i18n
{

enum class Language
{
    System = 0,  // Follow the Windows user interface language.
    English,
    French,
    Spanish,
};

// --- Preference round-trip -------------------------------------------------
// parseLanguage() accepts the settings.json codes; languageCode() is its inverse.
// Unknown/absent codes resolve to Language::System.
Language parseLanguage(const std::string& code);
const char* languageCode(Language language);

// Name shown in the language picker. Language names are never translated
// (English/Français/Español always read in their own language); only
// "System default" goes through the active translation table.
juce::String languageDisplayName(Language language);

// ISO-ish code of the Windows user interface language: "fr", "es" or "en".
const char* systemLanguageCode();

// --- Activation ------------------------------------------------------------
// Installs the translation table for `preference` (System resolves through
// systemLanguageCode()). English installs no mappings. Safe to call again to
// switch language; the previous table is released.
void install(Language preference);

// The resolved language currently active (Language::English when no table is
// installed). Mainly for tests and diagnostics.
Language installedLanguage();

// --- Lookup ----------------------------------------------------------------
// Returns the translated text, or `text` itself when no table is active or the
// key is missing.
juce::String tr(const char* text);
juce::String tr(const juce::String& text);

// For Win32 callers (AppendMenu, BROWSEINFOW, OPENFILENAME filters).
// Keep the returned std::wstring alive for as long as the API needs it.
std::wstring trW(const wchar_t* text);

// --- Tables (src/tray-app/i18n/translations_*.cpp) -------------------------
// JUCE LocalisedStrings text format: one "source" = "translation" pair per line.
namespace tables
{
const char* french();
const char* spanish();
}  // namespace tables

}  // namespace jyglobalvst::tray::i18n
