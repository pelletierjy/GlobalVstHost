// src/tray-app/ui/engine_settings_guard.h
//
// RAII guard for audio-settings mutations (device selection, buffer size,
// VST rate, WASAPI exclusive mode, ASIO control panel).
//
// The engine's mutation methods self-restart when running_, so mutating a
// live transport tears it down mid-switch (the WASAPI -> ASIO crash). The
// guard stops the engine BEFORE the mutation so the engine method only
// records state, then restarts it afterwards only if it was running when
// the guard was constructed — a stopped engine stays stopped, and a
// mutation that throws still gets its restart from the destructor during
// stack unwinding.
//
// Uses only IAudioEngine: works unchanged against both the in-process
// engine and the service-mode IPC proxy, with no build-mode branches.

#pragma once

#include "jyglobalvst/audio_engine.h"

namespace jyglobalvst {

class EngineSettingsGuard
{
public:
    explicit EngineSettingsGuard(IAudioEngine* engine) : engine_(engine)
    {
        was_running_ = engine_->isRunning();
        if (was_running_)
        {
            engine_->stop();
        }
    }

    ~EngineSettingsGuard()
    {
        if (was_running_ && !engine_->isRunning())
        {
            engine_->start();
        }
    }

    EngineSettingsGuard(const EngineSettingsGuard&) = delete;
    EngineSettingsGuard& operator=(const EngineSettingsGuard&) = delete;

private:
    IAudioEngine* engine_;
    bool was_running_{false};
};

}  // namespace jyglobalvst
