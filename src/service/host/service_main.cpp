// src/service/host/service_main.cpp
//
// T114 — Windows Service host that owns the audio engine.

#include "service_main.h"

#include "jyglobalvst/audio_engine.h"

#include <windows.h>

#include <memory>

namespace jyglobalvst::service {

namespace {

// Global state accessible from the service control handler.
struct ServiceContext
{
    SERVICE_STATUS_HANDLE status_handle {nullptr};
    SERVICE_STATUS status {};
    std::unique_ptr<IAudioEngine> engine;
    std::atomic<bool> stopping {false};
    HANDLE stop_event {nullptr};
};

ServiceContext* g_ctx = nullptr;

void reportStatus(DWORD current_state, DWORD win32_exit_code, DWORD wait_hint)
{
    if (!g_ctx || !g_ctx->status_handle)
        return;

    static DWORD checkpoint = 1;
    g_ctx->status.dwCurrentState = current_state;
    g_ctx->status.dwWin32ExitCode = win32_exit_code;
    g_ctx->status.dwWaitHint = wait_hint;

    if (current_state == SERVICE_START_PENDING)
        g_ctx->status.dwControlsAccepted = 0;
    else
        g_ctx->status.dwControlsAccepted =
            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (current_state == SERVICE_RUNNING || current_state == SERVICE_STOPPED)
        g_ctx->status.dwCheckPoint = 0;
    else
        g_ctx->status.dwCheckPoint = checkpoint++;

    SetServiceStatus(g_ctx->status_handle, &g_ctx->status);
}

void WINAPI serviceCtrlHandler(DWORD control)
{
    switch (control)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_ctx)
        {
            g_ctx->stopping = true;
            if (g_ctx->stop_event)
                SetEvent(g_ctx->stop_event);
        }
        break;
    default:
        break;
    }
}

void WINAPI serviceMain(DWORD /*argc*/, LPWSTR* /*argv*/)
{
    ServiceContext ctx;
    g_ctx = &ctx;

    ctx.status_handle = RegisterServiceCtrlHandlerW(
        L"JyGlobalVSTEngine", serviceCtrlHandler);
    if (!ctx.status_handle)
        return;

    ctx.status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    reportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    ctx.stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ctx.stop_event)
    {
        reportStatus(SERVICE_STOPPED, GetLastError(), 0);
        g_ctx = nullptr;
        return;
    }

    // Create and start the audio engine.
    ctx.engine = createAudioEngine();
    if (!ctx.engine)
    {
        reportStatus(SERVICE_STOPPED, ERROR_INVALID_DATA, 0);
        CloseHandle(ctx.stop_event);
        g_ctx = nullptr;
        return;
    }

    ctx.engine->start();
    reportStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // Block until stop requested.
    WaitForSingleObject(ctx.stop_event, INFINITE);

    reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
    ctx.engine->stop();
    ctx.engine.reset();

    CloseHandle(ctx.stop_event);
    reportStatus(SERVICE_STOPPED, NO_ERROR, 0);
    g_ctx = nullptr;
}

}  // namespace

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

int runAsService()
{
    SERVICE_TABLE_ENTRYW dispatch_table[] = {
        {const_cast<LPWSTR>(L"JyGlobalVSTEngine"), serviceMain},
        {nullptr, nullptr}
    };

    if (!StartServiceCtrlDispatcherW(dispatch_table))
    {
        return static_cast<int>(GetLastError());
    }
    return 0;
}

bool installService(const std::filesystem::path& exePath)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
        return false;

    std::wstring wpath = exePath.wstring();
    SC_HANDLE svc = CreateServiceW(
        scm,
        L"JyGlobalVSTEngine",
        L"JyGlobalVST Audio Engine",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        wpath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    bool ok = (svc != nullptr);
    if (svc)
        CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

bool uninstallService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return false;

    SC_HANDLE svc = OpenServiceW(scm, L"JyGlobalVSTEngine", DELETE);
    if (!svc)
    {
        CloseServiceHandle(scm);
        return false;
    }

    bool ok = DeleteService(svc) != FALSE;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

bool startService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return false;

    SC_HANDLE svc = OpenServiceW(scm, L"JyGlobalVSTEngine", SERVICE_START);
    if (!svc)
    {
        CloseServiceHandle(scm);
        return false;
    }

    bool ok = StartServiceW(svc, 0, nullptr) != FALSE;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

bool stopService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return false;

    SC_HANDLE svc = OpenServiceW(scm, L"JyGlobalVSTEngine",
                                  SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc)
    {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status {};
    bool ok = ControlService(svc, SERVICE_CONTROL_STOP, &status) != FALSE;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

}  // namespace jyglobalvst::service

// -------------------------------------------------------------------------
// Entry point
// -------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    using namespace jyglobalvst::service;

    if (argc > 1)
    {
        std::string cmd = argv[1];
        if (cmd == "install")
        {
            auto path = std::filesystem::canonical(argv[0]);
            return installService(path) ? 0 : 1;
        }
        if (cmd == "uninstall")
        {
            return uninstallService() ? 0 : 1;
        }
        if (cmd == "start")
        {
            return startService() ? 0 : 1;
        }
        if (cmd == "stop")
        {
            return stopService() ? 0 : 1;
        }
        std::fprintf(stderr, "Usage: %s [install | uninstall | start | stop]\n", argv[0]);
        return 1;
    }

    // No arguments: run as Windows Service.
    return runAsService();
}
