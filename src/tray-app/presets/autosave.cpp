// src/tray-app/presets/autosave.cpp
//
// T081 / T082 / T083 — Auto-save implementation.

#include "autosave.h"

#include "platform/known_folders.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace jyglobalvst::tray {

namespace {

std::string iso8601Now()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

}  // namespace

AutoSaveStore::AutoSaveStore()
    : path_(jyglobalvst::shared::localStateDir() / "autosave.json")
{
}

std::filesystem::path AutoSaveStore::autosavePath() const
{
    return path_;
}

void AutoSaveStore::write(IAudioEngine* engine, bool suppress_slots_due_to_preset_override,
                          int theme_id)
{
    // FR-022e: an explicit preset load overrides the auto-save entirely — the next
    // launch must restore that preset, not a stale auto-save file — so the write is
    // skipped altogether rather than merely omitting the slot list.
    if (suppress_slots_due_to_preset_override)
    {
        return;
    }

    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["saved_at"] = iso8601Now();
    doc["buffer_size"] = engine->bufferSize();
    doc["sample_rate"] = static_cast<int>(engine->sampleRate());

    const auto input = engine->currentInput();
    const auto output = engine->currentOutput();
    doc["input_endpoint_id"] = input;
    doc["output_endpoint_id"] = output;
    doc["audio_running"] = engine->isRunning();
    doc["theme_id"] = theme_id;
    doc["wasapi_exclusive"] = engine->wasapiExclusive();

    const auto chain = engine->snapshotChain();
    nlohmann::json slots = nlohmann::json::array();
    for (const auto& slot : chain.slots)
    {
        nlohmann::json s;
        s["position"] = slot.position;
        s["plugin_uid"] = PluginUidToHexString(slot.ref.plugin_uid);
        s["plugin_vendor"] = slot.ref.vendor;
        s["plugin_name"] = slot.ref.name;
        s["is_bypassed"] = slot.is_bypassed;
        s["kind"] = (slot.kind == PluginSlotKind::Placeholder) ? "placeholder" : "plugin";
        s["tag"] = slot.tag;
        s["shortcut"] = slot.shortcut;
        slots.push_back(std::move(s));
    }
    doc["slots"] = std::move(slots);

    std::filesystem::create_directories(path_.parent_path());
    std::ofstream ofs(path_, std::ios::binary);
    if (ofs)
    {
        ofs << doc.dump(2);
    }

    // Save a companion chain file (preset format with state chunks) so that
    // plugins are restored with their full VST3 state on next launch.
    auto chain_path = path_.parent_path() / "autosave-chain.jvst";
    engine->savePreset(chain_path, "autosave");
}

bool AutoSaveStore::restore(IAudioEngine* engine, bool* out_audio_running, int* out_theme_id) const
{
    std::ifstream ifs(path_);
    if (!ifs)
    {
        return false;
    }

    nlohmann::json doc;
    try
    {
        ifs >> doc;
    }
    catch (const std::exception&)
    {
        // FR-022d: silently discard on corruption.
        return false;
    }

    if (!doc.is_object())
    {
        return false;
    }

    // Restore endpoints first — output must be selected before buffer size so the
    // engine knows whether it is in ASIO mode (which allows buffer size 64).
    if (doc.contains("input_endpoint_id") && doc["input_endpoint_id"].is_string())
    {
        std::string id = doc["input_endpoint_id"].get<std::string>();
        if (!id.empty())
            engine->selectInput(id);
    }
    if (doc.contains("output_endpoint_id") && doc["output_endpoint_id"].is_string())
    {
        std::string id = doc["output_endpoint_id"].get<std::string>();
        if (!id.empty())
            engine->selectOutput(id);
    }
    if (doc.contains("wasapi_exclusive") && doc["wasapi_exclusive"].is_boolean())
        engine->setWasapiExclusive(doc["wasapi_exclusive"].get<bool>());

    // Restore buffer size after output selection (ASIO allows 64; WASAPI does not).
    // Guard with try/catch: the saved size may be invalid for the restored transport
    // mode (e.g. 64 was saved while in ASIO mode but the output is now WASAPI).
    if (doc.contains("buffer_size") && doc["buffer_size"].is_number_integer())
    {
        int bs = doc["buffer_size"].get<int>();
        if (bs == 32 || bs == 64 || bs == 128 || bs == 256 || bs == 512 || bs == 1024)
        {
            try { engine->setBufferSize(bs); }
            catch (const std::exception&) { /* size invalid for current transport; keep default */ }
        }
    }

    // Restore sample rate.
    if (doc.contains("sample_rate") && doc["sample_rate"].is_number_integer())
    {
        int sr = doc["sample_rate"].get<int>();
        if (sr == 44100 || sr == 48000 || sr == 88200 || sr == 96000 || sr == 176400 || sr == 192000)
            engine->setSampleRate(static_cast<double>(sr));
    }

    // Restore plugin chain slots sorted by position.
    if (doc.contains("slots") && doc["slots"].is_array())
    {
        struct SlotEntry
        {
            int position;
            PluginRef ref;
            bool is_bypassed;
            std::string tag;
            bool shortcut;
        };
        std::vector<SlotEntry> to_restore;

        for (const auto& s : doc["slots"])
        {
            if (!s.is_object())
                continue;
            if (s.value("kind", std::string("plugin")) == "placeholder")
                continue;

            SlotEntry entry;
            entry.position    = s.value("position", 0);
            entry.is_bypassed = s.value("is_bypassed", false);
            entry.tag         = s.value("tag", std::string {});
            entry.shortcut    = s.value("shortcut", false);

            if (s.contains("plugin_uid") && s["plugin_uid"].is_string())
                entry.ref.plugin_uid = HexStringToPluginUid(s["plugin_uid"].get<std::string>());
            if (s.contains("plugin_vendor") && s["plugin_vendor"].is_string())
                entry.ref.vendor = s["plugin_vendor"].get<std::string>();
            if (s.contains("plugin_name") && s["plugin_name"].is_string())
                entry.ref.name = s["plugin_name"].get<std::string>();

            to_restore.push_back(std::move(entry));
        }

        std::sort(to_restore.begin(), to_restore.end(),
                  [](const SlotEntry& a, const SlotEntry& b) { return a.position < b.position; });

        // Prefer the chain file (contains VST3 state chunks) over individual addPlugin
        // calls (which load plugins with default/blank state). Without saved state,
        // plugins like Guitar Rig 6 can crash when their editor is opened.
        auto chain_path = path_.parent_path() / "autosave-chain.jvst";
        if (std::filesystem::exists(chain_path))
        {
            engine->restoreChain(chain_path);
        }
        else
        {
            for (const auto& slot : to_restore)
            {
                engine->addPlugin(slot.ref, slot.position);
                if (slot.is_bypassed)
                    engine->setBypass(slot.position, true);
                if (!slot.tag.empty())
                    engine->setSlotTag(slot.position, slot.tag);
                if (slot.shortcut)
                    engine->setSlotShortcut(slot.position, true);
            }
        }
    }

    // Restore audio running state.
    if (out_audio_running)
    {
        *out_audio_running = false;
        if (doc.contains("audio_running") && doc["audio_running"].is_boolean())
            *out_audio_running = doc["audio_running"].get<bool>();
    }

    // Restore theme.
    if (out_theme_id)
    {
        *out_theme_id = 1;
        if (doc.contains("theme_id") && doc["theme_id"].is_number_integer())
        {
            int tid = doc["theme_id"].get<int>();
            if (tid >= 1 && tid <= 7)
                *out_theme_id = tid;
        }
    }

    return true;
}

}  // namespace jyglobalvst::tray
