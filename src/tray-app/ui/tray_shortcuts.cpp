// src/tray-app/ui/tray_shortcuts.cpp

#include "tray_shortcuts.h"

#include "builtin-effects/builtin_ids.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace jyglobalvst::tray {

namespace {

// Caption kept verbatim rather than run through the generated-caption rule, so
// that an upgrade does not rename a button users already know. Only the Volume
// Leveler qualifies: it is a single built-in with no third-party counterpart to
// be confused with. The built-in equalizer deliberately does not, because a
// chain can hold several equalizers and they all need telling apart - it takes
// an E1 / E2 caption from the counter like any other plugin.
const char* builtinLabel(const PluginUid& uid)
{
    if (uid == engine::builtin::NIGHTTIME_UID)
        return "VL";
    return nullptr;
}

// First letter of the plugin name, uppercased. Leading punctuation and digits
// are skipped so "(Waves) SSL" reads as W rather than as a bracket; a name with
// no letter at all falls back to '#'.
char captionLetter(const std::string& name)
{
    for (const unsigned char ch : name)
    {
        if (std::isalpha(ch) != 0)
            return static_cast<char>(std::toupper(ch));
    }
    return '#';
}

// How many slots ahead of `position` carry the same plugin. Two instances of one
// equalizer are occurrence 0 and 1, which is what an assignment is persisted
// against now that InstanceId cannot be.
int occurrenceOf(const ChainSnapshot& chain, int position)
{
    int occurrence = 0;
    for (int i = 0; i < position; ++i)
    {
        if (chain.slots[static_cast<std::size_t>(i)].ref.plugin_uid
            == chain.slots[static_cast<std::size_t>(position)].ref.plugin_uid)
        {
            ++occurrence;
        }
    }
    return occurrence;
}

}  // namespace

void TrayShortcutSet::resolveAgainst(const ChainSnapshot& chain)
{
    struct SlotKey
    {
        PluginUid uid;
        int occurrence;
        InstanceId instance;
    };

    std::vector<SlotKey> keys;
    keys.reserve(chain.slots.size());
    {
        std::map<PluginUid, int> seen;
        for (const auto& slot : chain.slots)
        {
            keys.push_back(SlotKey{slot.ref.plugin_uid, seen[slot.ref.plugin_uid]++,
                                   slot.instance_id});
        }
    }

    // A slot can back at most one assignment, so each pass claims the slots it
    // binds and later entries skip them.
    std::vector<bool> claimed(keys.size(), false);

    // Pass 1: an assignment whose slot is still in the chain keeps it, whatever
    // has happened around it. Its occurrence is restamped because removing an
    // earlier duplicate shifts it.
    for (auto& e : entries_)
    {
        bool bound = false;
        if (!e.instance.isNull())
        {
            for (std::size_t i = 0; i < keys.size(); ++i)
            {
                if (!claimed[i] && keys[i].instance == e.instance)
                {
                    claimed[i] = true;
                    e.uid = keys[i].uid;
                    e.occurrence = keys[i].occurrence;
                    bound = true;
                    break;
                }
            }
        }
        if (!bound)
        {
            e.instance = InstanceId{};
        }
    }

    // Pass 2: anything still unbound falls back to plugin + occurrence. This is
    // how an assignment restored from settings.json finds its slot on launch,
    // and how one whose plugin was removed re-adopts it when it comes back.
    for (auto& e : entries_)
    {
        if (!e.instance.isNull())
            continue;

        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            if (!claimed[i] && keys[i].uid == e.uid && keys[i].occurrence == e.occurrence)
            {
                claimed[i] = true;
                e.instance = keys[i].instance;
                break;
            }
        }
    }
}

bool TrayShortcutSet::contains(const InstanceId& instance) const
{
    if (instance.isNull())
        return false;

    return std::any_of(entries_.begin(), entries_.end(),
                       [&instance](const Entry& e) { return e.instance == instance; });
}

bool TrayShortcutSet::toggle(const ChainSnapshot& chain, int position)
{
    if (position < 0 || position >= static_cast<int>(chain.slots.size()))
        return false;

    const auto& slot = chain.slots[static_cast<std::size_t>(position)];

    if (!slot.instance_id.isNull())
    {
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [&slot](const Entry& e)
                                     { return e.instance == slot.instance_id; });
        if (it != entries_.end())
        {
            entries_.erase(it);
            return false;
        }
    }

    if (isFull())
        return false;

    entries_.push_back(Entry{slot.ref.plugin_uid, slot.ref.name,
                             occurrenceOf(chain, position), slot.instance_id});
    return true;
}

std::vector<TrayShortcut> TrayShortcutSet::entries() const
{
    std::vector<TrayShortcut> out;
    out.reserve(entries_.size());

    // Counters run per first letter, over the generated captions only: the
    // built-in VL button does not consume a "V".
    std::map<char, int> counters;

    for (const auto& e : entries_)
    {
        TrayShortcut s;
        s.instance = e.instance;
        s.uid = e.uid;
        s.plugin_name = e.name;

        if (const char* fixed = builtinLabel(e.uid); fixed != nullptr)
        {
            s.label = fixed;
        }
        else
        {
            const char letter = captionLetter(e.name);
            s.label = std::string(1, letter) + std::to_string(++counters[letter]);
        }

        out.push_back(std::move(s));
    }

    return out;
}

std::vector<TrayShortcutSetting> TrayShortcutSet::toSettings() const
{
    std::vector<TrayShortcutSetting> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_)
        out.push_back(TrayShortcutSetting{PluginUidToHexString(e.uid), e.name, e.occurrence});
    return out;
}

void TrayShortcutSet::loadFromSettings(const std::vector<TrayShortcutSetting>& settings)
{
    entries_.clear();
    for (const auto& s : settings)
    {
        if (s.plugin_uid.size() != 32 || entries_.size() >= kMaxShortcuts)
            continue;

        Entry e;
        e.uid = HexStringToPluginUid(s.plugin_uid);
        e.name = s.plugin_name;
        e.occurrence = std::max(0, s.occurrence);
        entries_.push_back(std::move(e));
    }
}

void TrayShortcutSet::loadDefaults()
{
    entries_.clear();
    entries_.push_back(Entry{engine::builtin::NIGHTTIME_UID, "Volume Leveler"});
    // Name as the engine registers it — the caption is derived from it, so a
    // mismatch here would put the wrong letter on the button.
    entries_.push_back(Entry{engine::builtin::EQ_UID, "Equalizer"});
}

}  // namespace jyglobalvst::tray
