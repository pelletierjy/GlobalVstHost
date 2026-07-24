// tests/integration/loopback_fixture.h
//
// T027 — Virtual-loopback test fixture for CI.
//
// Provides a reusable test environment that sets up the AudioEngine with a
// virtual loopback device (in testable-dev: WASAPI loopback on the default
// render endpoint; in release: the JyGlobalVST virtual driver). Allows tests
// to inject test audio and measure engine output without external hardware.

#pragma once

#include <gtest/gtest.h>
#include <jyglobalvst/audio_engine.h>

#include "../../src/audio-engine/routing/audio_engine_impl.h"
#include "../../src/shared/platform/known_folders.h"

#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstdlib>

namespace jyglobalvst::testing {

// Test listener that records events for assertion.
class TestAudioEngineListener : public IAudioEngineListener
{
public:
    void onChainRevision(int new_revision) override { chain_revision_ = new_revision; }
    void onPluginFailed(const InstanceId& id, const std::string& reason) override
    {
        plugin_failures_.push_back({id, reason});
    }
    void onDeviceLost(const EndpointId& lost, const EndpointId& fallback_to) override
    {
        device_lost_events_.push_back({lost, fallback_to});
    }
    void onDeviceRestored(const EndpointId& restored) override { last_restored_device_ = restored; }
    void onCpuWarning(float rolling_1s_pct) override { last_cpu_warning_ = rolling_1s_pct; }
    void onMeterFrame(const MeterFrame& frame) override
    {
        latest_meter_frame_ = frame;
        meter_frame_count_++;
    }
    void onPresetPartialLoad(const std::vector<MissingPluginInfo>& missing) override
    {
        missing_plugins_ = missing;
    }
    void onSameDeviceConflict(const EndpointId& device) override { last_conflict_device_ = device; }
    void onCaptureMuteFallbackRequired(const EndpointId& endpoint) override { last_mute_fallback_endpoint_ = endpoint; }

    int chain_revision() const { return chain_revision_; }
    const auto& plugin_failures() const { return plugin_failures_; }
    const auto& device_lost_events() const { return device_lost_events_; }
    const auto& last_restored_device() const { return last_restored_device_; }
    float last_cpu_warning() const { return last_cpu_warning_; }
    const auto& latest_meter_frame() const { return latest_meter_frame_; }
    int meter_frame_count() const { return meter_frame_count_; }
    const auto& last_conflict_device() const { return last_conflict_device_; }
    const auto& last_mute_fallback_endpoint() const { return last_mute_fallback_endpoint_; }

private:
    int chain_revision_ {0};
    std::vector<std::pair<InstanceId, std::string>> plugin_failures_;
    std::vector<std::pair<EndpointId, EndpointId>> device_lost_events_;
    EndpointId last_restored_device_;
    float last_cpu_warning_ {0.0f};
    MeterFrame latest_meter_frame_;
    int meter_frame_count_ {0};
    std::vector<MissingPluginInfo> missing_plugins_;
    EndpointId last_conflict_device_;
    EndpointId last_mute_fallback_endpoint_;
};

// Fixture for integration tests using the audio engine.
class LoopbackFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Ensure test isolation: remove persisted scan cache so that
        // ScanReturnsEmptyCatalogWhenNoPathsSet (T050) sees a clean state.
        {
            std::error_code ec;
            std::filesystem::remove(shared::localStateDir() / "scan-cache.json", ec);
        }

        // Keep rescanPlugins() from touching real, machine-specific VST3 directories
        // (see default_scan_paths.cpp): tests must not depend on — or hang because
        // of — whatever plugins happen to be installed on the machine running them.
        _putenv_s("JYGLOBALVST_TEST_NO_DEFAULT_SCAN_PATHS", "1");

        engine_impl_ = std::make_unique<jyglobalvst::engine::AudioEngineImpl>();
        listener_ = std::make_unique<TestAudioEngineListener>();
        engine_impl_->setListener(listener_.get());
    }

    void TearDown() override
    {
        if (engine_impl_)
        {
            engine_impl_->stop();
            engine_impl_.reset();
        }
        listener_.reset();
    }

    void StartEngine()
    {
        if (engine_impl_)
        {
            engine_impl_->start();
            // Brief pause to allow device initialization.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void StopEngine()
    {
        if (engine_impl_)
        {
            engine_impl_->stop();
        }
    }

    void InjectTestPlugin(const std::string& vendor, const std::string& name)
    {
        PluginCatalogEntry entry;
        entry.ref.plugin_uid = {};
        entry.ref.vendor = vendor;
        entry.ref.name = name;
        entry.version = "1.0.0";
        entry.file_path = "C:\\TestPlugin.vst3";
        entry.category = "Fx";
        entry.has_editor = true;
        entry.scan_timestamp = std::chrono::system_clock::now();
        engine_impl_->injectTestCatalogEntry(entry);
    }

    InstanceId AddPlaceholderSlot(int position)
    {
        return engine_impl_->addPlaceholderSlot(position);
    }

    IAudioEngine* engine() { return engine_impl_.get(); }
    jyglobalvst::engine::AudioEngineImpl* engine_impl() { return engine_impl_.get(); }
    TestAudioEngineListener* listener() { return listener_.get(); }

private:
    std::unique_ptr<jyglobalvst::engine::AudioEngineImpl> engine_impl_;
    std::unique_ptr<TestAudioEngineListener> listener_;
};

}  // namespace jyglobalvst::testing
