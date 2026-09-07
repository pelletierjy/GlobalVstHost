// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/chain/preset_serializer.cpp
//
// T072 / T075 / T076 / T077 — Preset serialization implementation.

#include "preset_serializer.h"

#include "../vst-host/plugin_instance.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace jyglobalvst::engine {

namespace {

// RFC 4648 base64 alphabet.
constexpr char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64EncodeImpl(const std::uint8_t* data, std::size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len)
    {
        std::uint32_t b = (static_cast<std::uint32_t>(data[i]) << 16)
                          | (static_cast<std::uint32_t>(data[i + 1]) << 8)
                          | static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64Chars[(b >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(b >> 12) & 0x3F]);
        out.push_back(kBase64Chars[(b >> 6) & 0x3F]);
        out.push_back(kBase64Chars[b & 0x3F]);
        i += 3;
    }
    if (i + 2 == len)
    {
        std::uint32_t b = (static_cast<std::uint32_t>(data[i]) << 16)
                          | (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64Chars[(b >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(b >> 12) & 0x3F]);
        out.push_back(kBase64Chars[(b >> 6) & 0x3F]);
        out.push_back('=');
    }
    else if (i + 1 == len)
    {
        std::uint32_t b = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64Chars[(b >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(b >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    }
    return out;
}

}  // namespace

std::string base64Encode(const juce::MemoryBlock& data)
{
    return base64EncodeImpl(static_cast<const std::uint8_t*>(data.getData()), data.getSize());
}

std::vector<std::uint8_t> base64Decode(const std::string& b64)
{
    std::vector<std::uint8_t> out;
    if (b64.empty())
        return out;

    // Build reverse lookup table.
    int decodeTable[256];
    std::fill(std::begin(decodeTable), std::end(decodeTable), -1);
    for (int i = 0; i < 64; ++i)
        decodeTable[static_cast<unsigned char>(kBase64Chars[i])] = i;

    std::size_t inLen = b64.size();
    while (inLen > 0 && b64[inLen - 1] == '=')
        --inLen;

    out.reserve((inLen * 3) / 4);
    std::uint32_t buf = 0;
    int bits = 0;
    for (std::size_t i = 0; i < inLen; ++i)
    {
        int val = decodeTable[static_cast<unsigned char>(b64[i])];
        if (val < 0)
            continue; // skip invalid chars (whitespace etc.)
        buf = (buf << 6) | static_cast<std::uint32_t>(val);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::vector<PresetValidationError> validatePresetDocument(const nlohmann::json& doc,
                                                           std::size_t file_size_bytes)
{
    std::vector<PresetValidationError> errors;

    if (file_size_bytes > 52428800)
    {
        errors.push_back({"File size exceeds 50 MB limit"});
        return errors;
    }

    if (!doc.is_object())
    {
        errors.push_back({"Preset document must be a JSON object"});
        return errors;
    }

    // Required top-level fields.
    static const char* kRequired[] = {"schema_version", "preset_name", "created_at",
                                       "updated_at", "target_buffer_size", "slots"};
    for (const char* key : kRequired)
    {
        if (!doc.contains(key))
        {
            errors.push_back({std::string("Missing required field: ") + key});
        }
    }

    // schema_version
    if (doc.contains("schema_version"))
    {
        const auto& sv = doc["schema_version"];
        if (!sv.is_number_integer() || sv.get<int>() != 1)
        {
            errors.push_back({"Unsupported schema_version; expected 1"});
        }
    }

    // target_buffer_size
    if (doc.contains("target_buffer_size"))
    {
        const auto& bs = doc["target_buffer_size"];
        if (!bs.is_number_integer())
        {
            errors.push_back({"target_buffer_size must be an integer"});
        }
        else
        {
            int v = bs.get<int>();
            if (v != 32 && v != 64 && v != 128 && v != 256 && v != 512 && v != 1024)
            {
                errors.push_back({"target_buffer_size must be one of {32, 64, 128, 256, 512, 1024}"});
            }
        }
    }

    // slots
    if (doc.contains("slots") && doc["slots"].is_array())
    {
        const auto& slots = doc["slots"];
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const auto& slot = slots[i];
            if (!slot.is_object())
            {
                errors.push_back({"Slot " + std::to_string(i) + " is not an object"});
                continue;
            }
            static const char* kSlotRequired[] = {"position", "plugin_uid", "plugin_vendor",
                                                   "plugin_name", "is_bypassed", "state_chunk_b64"};
            for (const char* key : kSlotRequired)
            {
                if (!slot.contains(key))
                {
                    errors.push_back({"Slot " + std::to_string(i) + " missing field: " + key});
                }
            }
            if (slot.contains("state_chunk_b64") && slot["state_chunk_b64"].is_string())
            {
                auto decoded = base64Decode(slot["state_chunk_b64"].get<std::string>());
                if (decoded.size() > 16777216)
                {
                    errors.push_back({"Slot " + std::to_string(i) +
                                      " state_chunk_b64 decoded size exceeds 16 MB"});
                }
            }
        }
    }

    // Unknown top-level fields check (strict per FR-022g-1).
    static const char* kKnownTop[] = {"schema_version", "preset_name", "created_at", "updated_at",
                                       "target_sample_rate", "target_buffer_size",
                                       "target_device_friendly_name", "input_endpoint_id",
                                       "output_endpoint_id", "audio_running", "slots", "theme_id"};
    for (auto it = doc.begin(); it != doc.end(); ++it)
    {
        bool known = false;
        for (const char* key : kKnownTop)
        {
            if (it.key() == key)
            {
                known = true;
                break;
            }
        }
        if (!known)
        {
            errors.push_back({"Unknown top-level field: " + it.key()});
        }
    }

    return errors;
}

nlohmann::json serializePreset(const PluginChain& chain,
                                const std::string& preset_name,
                                int target_buffer_size,
                                int target_sample_rate,
                                const std::string& target_device_friendly_name,
                                const std::string& input_endpoint_id,
                                const std::string& output_endpoint_id,
                                bool audio_running)
{
    nlohmann::json doc;
    doc["schema_version"] = 1;
    doc["preset_name"] = preset_name;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    std::string iso = ss.str();
    doc["created_at"] = iso;
    doc["updated_at"] = iso;

    doc["target_sample_rate"] = target_sample_rate > 0 ? nlohmann::json(target_sample_rate) : nlohmann::json(nullptr);
    doc["target_buffer_size"] = target_buffer_size;
    doc["target_device_friendly_name"] = target_device_friendly_name.empty()
                                              ? nlohmann::json(nullptr)
                                              : nlohmann::json(target_device_friendly_name);
    doc["input_endpoint_id"] = input_endpoint_id.empty() ? nlohmann::json(nullptr) : nlohmann::json(input_endpoint_id);
    doc["output_endpoint_id"] = output_endpoint_id.empty() ? nlohmann::json(nullptr) : nlohmann::json(output_endpoint_id);
    doc["audio_running"] = audio_running;

    nlohmann::json slots = nlohmann::json::array();
    const auto snapshot = chain.snapshot();
    for (const auto& snap : snapshot)
    {
        nlohmann::json slot;
        slot["position"] = snap.position;
        slot["plugin_uid"] = PluginUidToHexString(snap.ref.plugin_uid);
        slot["plugin_vendor"] = snap.ref.vendor;
        slot["plugin_name"] = snap.ref.name;
        slot["is_bypassed"] = snap.is_bypassed;

        juce::MemoryBlock state_chunk;
        if (snap.kind == PluginSlotKind::Plugin)
        {
            auto instance = chain.getSlotInstance(snap.position);
            if (instance && instance->processor())
            {
                instance->processor()->getStateInformation(state_chunk);
            }
        }
        // For placeholders, pending_state_chunk is handled externally.

        slot["state_chunk_b64"] = base64Encode(state_chunk);
        slot["plugin_path_hint"] = snap.file_path;
        slot["tag"] = snap.tag;
        slots.push_back(std::move(slot));
    }
    doc["slots"] = std::move(slots);
    return doc;
}

PresetLoadResult deserializePreset(const nlohmann::json& doc,
                                    const std::function<ResolvedPlugin(const PluginRef& ref)>& resolvePlugin,
                                    const std::function<void(std::shared_ptr<PlaceholderInstance> placeholder)>& onPlaceholder)
{
    PresetLoadResult result;

    auto errors = validatePresetDocument(doc, 0); // file size already checked by caller
    if (!errors.empty())
    {
        result.errors = std::move(errors);
        return result;
    }

    const auto& slots = doc["slots"];
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        const auto& slot = slots[i];
        PluginRef ref;
        std::string uid_hex = slot.value("plugin_uid", "");
        ref.plugin_uid = HexStringToPluginUid(uid_hex);
        ref.vendor = slot.value("plugin_vendor", "");
        ref.name = slot.value("plugin_name", "");
        bool is_bypassed = slot.value("is_bypassed", false);
        std::string path_hint = slot.value("plugin_path_hint", "");
        std::string b64 = slot.value("state_chunk_b64", "");
        auto decoded = base64Decode(b64);
        juce::MemoryBlock chunk(decoded.data(), decoded.size());

        auto resolved = resolvePlugin(ref);
        if (resolved.instance)
        {
            if (!chunk.isEmpty() && resolved.instance->processor())
            {
                resolved.instance->processor()->setStateInformation(chunk.getData(),
                                                                     static_cast<int>(chunk.getSize()));
            }
            resolved.instance->setBypass(is_bypassed);
        }
        else
        {
            auto placeholder = std::make_shared<PlaceholderInstance>(ref, std::filesystem::path(path_hint));
            placeholder->setBypass(is_bypassed);
            if (!chunk.isEmpty())
            {
                placeholder->setPendingStateChunk(chunk);
            }
            onPlaceholder(std::move(placeholder));
            result.missing.push_back({ref, static_cast<int>(i)});
        }
    }

    result.ok = true;
    return result;
}

}  // namespace jyglobalvst::engine
