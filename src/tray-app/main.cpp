// src/tray-app/main.cpp
//
// T045 — JUCE GUI application entry point.
//
// On launch:
//   1. Check single-instance via session-scoped named mutex (T044).
//   2. If another instance exists, focus its window and exit.
//   3. Otherwise, create IAudioEngine, create MainWindow, run message loop.

#include "jyglobalvst/audio_engine.h"

#include "i18n/translation.h"
#include "platform/session_mutex.h"
#include "settings/roaming_settings.h"
#include "soak/soak_runner.h"
#include "ui/main_window.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdlib>

#include <windows.h>

namespace jyglobalvst::tray {

// MUST stay identical to the DocumentWindow title in main_window.cpp: the
// single-instance check below locates the running instance with
// FindWindowW(nullptr, kWindowTitle), which matches on the exact window caption.
constexpr wchar_t kWindowTitle[] = L"Global VST Host";

class TrayApplication : public juce::JUCEApplication
{
public:
    TrayApplication() = default;

    const juce::String getApplicationName() override
    {
        return "Global VST Host";
    }

    const juce::String getApplicationVersion() override
    {
        // Defined from PROJECT_VERSION by src/tray-app/CMakeLists.txt. The literal
        // is only a fallback for builds that bypass that target; keep it in step
        // with the root CMakeLists version.
#if defined(JYGLOBALVST_VERSION_STRING)
        return JYGLOBALVST_VERSION_STRING;
#else
        return "1.3.6.0";
#endif
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    void initialise(const juce::String& commandLine) override
    {
        // Headless soak mode (--soak-test): run the engine without the window
        // or the single-instance mutex, write the JSON report, and exit with
        // the soak verdict as the process exit code.
        SoakOptions soak_opts;
        if (parseSoakOptions(commandLine, soak_opts))
        {
            const int rc = runSoakTest(soak_opts);
            setApplicationReturnValue(rc);
            // Headless CLI mode: terminate decisively. quit() alone is not
            // reliable here — posted during initialise() it can leave the
            // message loop (and the process) running indefinitely.
            std::exit(rc);
        }

        // Single-instance check using session mutex (T015).
        auto mutex = std::make_unique<shared::SessionMutex>("GlobalVSTHost_Instance");
        const auto result = mutex->tryAcquire();
        if (result != shared::SessionMutex::AcquireResult::Acquired)
        {
            // Another instance is running — focus it and exit.
            HWND existing = FindWindowW(nullptr, kWindowTitle);
            if (existing != nullptr)
            {
                if (IsIconic(existing))
                {
                    ShowWindow(existing, SW_RESTORE);
                }
                SetForegroundWindow(existing);
            }
            quit();
            return;
        }

        instance_mutex_ = std::move(mutex);

        // Install the UI language (RoamingSettings::language, default = Windows
        // user interface language) before the first window is built, so every
        // label, tooltip and status message is translated from the first frame.
        i18n::install(i18n::parseLanguage(RoamingSettingsStore{}.load().language));

        // Create audio engine.
        auto engine = createAudioEngine();

        // Create main window.
        main_window_ = std::make_unique<MainWindow>(std::move(engine));

        // Ensure window title is set so subsequent instances can find us.
        if (auto* peer = main_window_->getPeer())
        {
            if (auto* hwnd = static_cast<HWND>(peer->getNativeHandle()))
            {
                SetWindowTextW(hwnd, kWindowTitle);
            }
        }
    }

    void shutdown() override
    {
        main_window_.reset();
        instance_mutex_.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        // Handled by mutex path; this should not fire because
        // moreThanOneInstanceAllowed() returns false.
    }

private:
    std::unique_ptr<shared::SessionMutex> instance_mutex_;
    std::unique_ptr<MainWindow> main_window_;
};

}  // namespace jyglobalvst::tray

// =========================================================================
// Entry point
// =========================================================================

START_JUCE_APPLICATION(jyglobalvst::tray::TrayApplication)
