// src/tray-app/ipc-client/engine_proxy.h
//
// T117 — IPC client in tray app: IAudioEngine implementation over named pipe.

#pragma once

#include "jyglobalvst/audio_engine.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <windows.h>

namespace jyglobalvst::tray {

class IpcEngineProxy : public IAudioEngine
{
public:
    IpcEngineProxy();
    ~IpcEngineProxy() override;

    // Probe whether the service pipe is available.
    static bool isServiceModeAvailable();

    // Open the pipe and negotiate protocol version.
    bool connect();
    void disconnect();

    // IAudioEngine ----------------------------------------------------------
    void start() override;
    void stop() override;
    bool isRunning() const override;
    void setListener(IAudioEngineListener* listener) override;
    void setMasterVolume(float gain_linear) override;
    void reset() override;

    std::vector<HardwareOutputInfo> listOutputs() const override;
    void selectOutput(const EndpointId& id) override;
    EndpointId currentOutput() const override;
    DeviceResolutionSource currentResolutionSource() const override;

    void setBufferSize(int samples) override;
    int bufferSize() const override;
    void setSampleRate(double rate) override;
    double sampleRate() const override;
    int negotiatedSampleRate() const override;
    int outputDeviceSampleRate() const override;
    int inputDeviceSampleRate() const override;

    void setAsioOutputPair(int channel_offset) override;
    int asioOutputPair() const override;
    void openAsioControlPanel() override;

    void setWasapiExclusive(bool exclusive) override;
    bool wasapiExclusive() const override;

    std::vector<HardwareOutputInfo> listInputs() const override;
    void selectInput(const EndpointId& id) override;
    EndpointId currentInput() const override;

    void rescanPlugins(IScanProgressListener* progress) override;
    void cancelScan() override;
    std::vector<PluginCatalogEntry> catalog() const override;

    ChainSnapshot snapshotChain() const override;
    InstanceId addPlugin(const PluginRef& ref, int position) override;
    InstanceId addPluginFromPath(const std::filesystem::path& vst3_path,
                                  int position) override;
    void removeSlot(int position) override;
    void moveSlot(int from, int to) override;
    void setBypass(int position, bool bypassed) override;
    void setParameter(int position, ParamId param, float value) override;
    void openEditor(int position) override;
    void closeEditor(int position) override;
    void repointPlaceholder(int position, const PluginRef& ref) override;

    void loadPreset(const std::filesystem::path& path) override;
    void savePreset(const std::filesystem::path& path,
                    const std::string& name) override;
    void restoreChain(const std::filesystem::path& path) override;

    LatencyProfile latencyProfile() const override;
    CpuStats cpuStats() const override;
    MeterFrame latestMeterFrame() const override;

private:
    void readLoop();
    nlohmann::json sendCommand(const std::string& cmd,
                               const nlohmann::json& payload);

    std::wstring pipe_name_;
    HANDLE hPipe_ {nullptr};
    std::atomic<bool> stop_reader_ {false};
    std::thread reader_thread_;

    mutable std::mutex listener_mutex_;
    IAudioEngineListener* listener_ {nullptr};
};

}  // namespace jyglobalvst::tray
