// src/tray-app/ui/tray_shortcuts.h
//
// Tray-popup shortcut assignments: which plugins get a quick bypass button on
// the system-tray volume popup, in which order, and under what caption.

#pragma once

#include "settings/roaming_settings.h"

#include "jyglobalvst/types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace jyglobalvst::tray {

// Serialisation DTO for persisting shortcut assignments in settings.json.
struct TrayShortcutSetting
{
    std::string plugin_uid;   // 32-char hex string (PluginUid)
    std::string plugin_name;
    int occurrence {0};       // Nth slot carrying this plugin in the chain.
};

// One assigned shortcut, resolved for display.
struct TrayShortcut
{
    InstanceId instance {};  // Chain slot the button drives; null when unresolved.
    PluginUid uid {};
    std::string plugin_name;
    std::string label;  // Short caption drawn on the popup button.
};

// Ordered set of chain slots that own a button on the tray volume popup.
//
// Assignments are per slot, not per plugin: two instances of the same equalizer
// are independent, and either one can own a button without the other doing so.
// Within a session a slot is identified by its InstanceId. That id is not
// persisted, so settings.json records the plugin plus its occurrence in the
// chain ("the second Equalizer") and resolveAgainst() re-binds the two.
//
// Assignment order is preserved because it drives both button order and the
// generated captions, so a slot keeps its caption for as long as nothing ahead
// of it in the list is unassigned.
class TrayShortcutSet
{
public:
    // The popup's button strip is four buttons wide: power, two plugin
    // shortcuts, and mute. Past that the strip stops fitting a tray call-out, so
    // further assignment is refused rather than silently dropping a button.
    static constexpr std::size_t kMaxShortcuts = 2;

    // Re-binds every assignment to a live chain slot. Call this whenever the
    // chain may have changed, before consulting contains() or entries().
    void resolveAgainst(const ChainSnapshot& chain);

    // True when the slot with this instance id owns a button. A null id never
    // matches, so unresolved assignments cannot light up an unrelated row.
    bool contains(const InstanceId& instance) const;

    bool isFull() const { return entries_.size() >= kMaxShortcuts; }
    std::size_t size() const { return entries_.size(); }

    // Grants a button to the slot at `position` if it has none and a slot is
    // free, takes its button away otherwise. Returns the resulting state; an add
    // request against a full set changes nothing and returns false.
    bool toggle(const ChainSnapshot& chain, int position);

    // Assignments in button order, each with its resolved caption. Entries that
    // no longer match a chain slot are included with a null instance.
    std::vector<TrayShortcut> entries() const;

    std::vector<TrayShortcutSetting> toSettings() const;
    void loadFromSettings(const std::vector<TrayShortcutSetting>& settings);

    // The two built-in effects, matching the popup's fixed VL + EQ buttons from
    // before shortcuts were configurable. Used when settings.json has never
    // recorded a choice, so an upgrade leaves the popup looking unchanged.
    void loadDefaults();

private:
    struct Entry
    {
        PluginUid uid {};
        std::string name;
        int occurrence {0};      // Nth slot in the chain carrying this plugin.
        InstanceId instance {};  // Live binding, refreshed by resolveAgainst().
    };

    std::vector<Entry> entries_;
};

}  // namespace jyglobalvst::tray
