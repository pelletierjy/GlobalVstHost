// src/service/ipc-server/session_auth.h
//
// T116 — Per-session pipe authentication.

#pragma once

#include <windows.h>

namespace jyglobalvst::service {

// Verify that the client connected to hPipe is in the same Windows session
// as the calling process (interactive user session). Returns false on any
// error or mismatch.
bool authenticatePipeClient(HANDLE hPipe);

}  // namespace jyglobalvst::service
