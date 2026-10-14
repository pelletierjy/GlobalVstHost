// src/tray-app/i18n/translation.cpp
//
// Language resolution + translation-table installation for FR/ES localization.

#include "translation.h"

#include <juce_core/juce_core.h>

#include <string>
#include <windows.h>

namespace jyglobalvst::tray::i18n
{

namespace
{

Language g_installed = Language::English;

// i18n-OK: JUCE's LocalisedStrings is the sanctioned localization mechanism for
// this release; FR-019c (English-only v1) is amended in
// specs/001-jyglobalvst-system-host/spec.md.
juce::LocalisedStrings* makeTable(const char* text)  // i18n-OK: FR/ES localization
{
    // fromUTF8(): juce::String(const char*) is the ASCII/Latin-1 constructor and
    // would double-encode the accented text in the tables.
    return new juce::LocalisedStrings(juce::String::fromUTF8(text), false);  // i18n-OK: FR/ES localization
}

Language resolve(Language preference)
{
    if (preference != Language::System)
        return preference;

    return parseLanguage(systemLanguageCode());
}

}  // namespace

// --- Preference round-trip -------------------------------------------------

Language parseLanguage(const std::string& code)
{
    if (code == "en")
        return Language::English;
    if (code == "fr")
        return Language::French;
    if (code == "es")
        return Language::Spanish;
    return Language::System;
}

const char* languageCode(Language language)
{
    switch (language)
    {
    case Language::English: return "en";
    case Language::French:  return "fr";
    case Language::Spanish: return "es";
    case Language::System:  break;
    }
    return "system";
}

juce::String languageDisplayName(Language language)
{
    switch (language)
    {
    case Language::System:  return tr("System default");
    case Language::English: return "English";
    case Language::French:  return juce::String::fromUTF8("Français");
    case Language::Spanish: return juce::String::fromUTF8("Español");
    }
    return {};
}

const char* systemLanguageCode()
{
    const LANGID uiLang = ::GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(uiLang))
    {
    case LANG_FRENCH:  return "fr";
    case LANG_SPANISH: return "es";
    default:           break;
    }
    return "en";
}

// --- Activation ------------------------------------------------------------

void install(Language preference)
{
    const Language resolved = resolve(preference);

    // Build the table for the resolved language; nullptr below clears it.
    juce::LocalisedStrings* next = nullptr;  // i18n-OK: FR/ES localization
    if (resolved == Language::French)
        next = makeTable(tables::french());
    else if (resolved == Language::Spanish)
        next = makeTable(tables::spanish());

    // i18n-OK: installs (or, with nullptr, releases) the active FR/ES table.
    juce::LocalisedStrings::setCurrentMappings(next);

    g_installed = resolved;
}

Language installedLanguage()
{
    return g_installed;
}

// --- Lookup ----------------------------------------------------------------

juce::String tr(const char* text)
{
    // fromUTF8(): same reason as makeTable() — the ASCII constructor would
    // mangle the handful of keys that contain accented characters or arrows.
    return juce::translate(juce::String::fromUTF8(text));
}

juce::String tr(const juce::String& text)
{
    return juce::translate(text);
}

std::wstring trW(const wchar_t* text)
{
    const juce::String translated = tr(juce::String(text));
    return std::wstring(translated.toWideCharPointer());
}

}  // namespace jyglobalvst::tray::i18n
