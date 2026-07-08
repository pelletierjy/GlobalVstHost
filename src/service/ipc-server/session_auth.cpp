// src/service/ipc-server/session_auth.cpp
//
// T116 — Per-session pipe authentication.

#include "session_auth.h"

#include <windows.h>

namespace jyglobalvst::service {

bool authenticatePipeClient(HANDLE hPipe)
{
    if (hPipe == INVALID_HANDLE_VALUE || hPipe == nullptr)
        return false;

    ULONG clientPid = 0;
    if (!GetNamedPipeClientProcessId(hPipe, &clientPid))
        return false;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, clientPid);
    if (!hProcess)
        return false;

    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
    {
        CloseHandle(hProcess);
        return false;
    }

    DWORD sessionId = 0;
    DWORD retLen = 0;
    BOOL ok = GetTokenInformation(
        hToken, TokenSessionId, &sessionId, sizeof(sessionId), &retLen);

    CloseHandle(hToken);
    CloseHandle(hProcess);

    if (!ok)
        return false;

    DWORD mySessionId = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mySessionId))
        return false;

    return sessionId == mySessionId;
}

}  // namespace jyglobalvst::service
