// src/shared/platform/session_mutex.h
//
// T015 — Session-scoped named mutex helper.
// FR-022j: a second launch in the same user session focuses the existing
// window instead of spawning a duplicate. Mutex name pattern:
//   Local\JyGlobalVST.<name>.<SessionId>
//
// UI-thread only. Never invoked from the audio thread.

#pragma once

#include <string>

#if defined(_WIN32)
#    include <windows.h>
#endif

namespace jyglobalvst::shared {

class SessionMutex
{
public:
    enum class AcquireResult
    {
        Acquired,         // We are the first instance in this session.
        AlreadyHeld,      // Another instance owns the mutex.
        Error,            // Acquisition failed for an unexpected reason.
    };

    // name is the logical identifier (e.g., "tray", "service-client"). The
    // SessionId is appended automatically so RDP / Fast-User-Switching
    // sessions get their own slots (FR-022j extended).
    explicit SessionMutex(std::string name);
    ~SessionMutex();

    SessionMutex(const SessionMutex&) = delete;
    SessionMutex& operator=(const SessionMutex&) = delete;

    [[nodiscard]] AcquireResult tryAcquire();

    void release();

    [[nodiscard]] const std::string& fullName() const noexcept { return full_name_; }

private:
    std::string full_name_;
#if defined(_WIN32)
    HANDLE handle_ {nullptr};
    bool   acquired_ {false};
#endif
};

}  // namespace jyglobalvst::shared
