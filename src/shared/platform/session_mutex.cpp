// src/shared/platform/session_mutex.cpp
// T015 — see header.

#include "session_mutex.h"

#include <string>

#if defined(_WIN32)
#    include <processthreadsapi.h>
#endif

namespace jyglobalvst::shared {

namespace {

#if defined(_WIN32)
DWORD currentSessionId()
{
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session))
    {
        // If we cannot determine the session, fall back to 0. The single-instance
        // guarantee remains valid (just less granular across RDP sessions).
        session = 0;
    }
    return session;
}
#endif

std::string buildName(const std::string& logical)
{
#if defined(_WIN32)
    return "Local\\JyGlobalVST." + logical + "." + std::to_string(currentSessionId());
#else
    return "JyGlobalVST." + logical;
#endif
}

}  // namespace

SessionMutex::SessionMutex(std::string name)
    : full_name_ {buildName(name)}
{
}

SessionMutex::~SessionMutex()
{
    release();
}

SessionMutex::AcquireResult SessionMutex::tryAcquire()
{
#if defined(_WIN32)
    if (acquired_)
    {
        return AcquireResult::Acquired;
    }

    // CreateMutexA returns a handle whose initial owner status we infer from GetLastError.
    handle_ = CreateMutexA(nullptr, TRUE, full_name_.c_str());
    if (handle_ == nullptr)
    {
        return AcquireResult::Error;
    }
    const DWORD last = GetLastError();
    if (last == ERROR_ALREADY_EXISTS)
    {
        // Another process already owns it. We hold an open handle but not ownership.
        return AcquireResult::AlreadyHeld;
    }
    acquired_ = true;
    return AcquireResult::Acquired;
#else
    return AcquireResult::Acquired;
#endif
}

void SessionMutex::release()
{
#if defined(_WIN32)
    if (handle_ != nullptr)
    {
        if (acquired_)
        {
            ReleaseMutex(handle_);
            acquired_ = false;
        }
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#endif
}

}  // namespace jyglobalvst::shared
