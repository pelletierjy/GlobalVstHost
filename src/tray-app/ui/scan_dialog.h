// src/tray-app/ui/scan_dialog.h
//
// T063 — Scan progress dialog.
//
// Shows current path, plugin count, and a cancel button.
// Receives IScanProgressListener callbacks from the engine.

#pragma once

#include "jyglobalvst/audio_engine.h"
#include "jyglobalvst/types.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <atomic>

namespace jyglobalvst::tray {

class ScanDialog : public juce::DialogWindow
                 , public juce::Button::Listener
                 , public IScanProgressListener
                 , public juce::Timer
{
public:
    explicit ScanDialog(IAudioEngine* engine);
    ~ScanDialog() override;

    void buttonClicked(juce::Button* button) override;

    // IScanProgressListener --------------------------------------------------
    void onScanStarted(int total_paths) override;
    void onPathStarted(const std::filesystem::path& path) override;
    void onPluginDiscovered(const PluginCatalogEntry& entry) override;
    void onScanFinished(int plugins_discovered) override;
    void onScanCancelled() override;

    // juce::Timer ------------------------------------------------------------
    void timerCallback() override;

    void closeButtonPressed() override;
    void resized() override;

    // Returns true if the scan has finished (either completed or cancelled).
    bool isFinished() const { return finished_.load(); }

private:
    void buildUI();

    IAudioEngine* engine_;

    std::unique_ptr<juce::Label> status_label_;
    std::unique_ptr<juce::Label> count_label_;
    std::unique_ptr<juce::TextButton> cancel_button_;

    std::atomic<int> plugins_discovered_ {0};
    std::atomic<bool> finished_ {false};
    juce::String pending_status_;
    juce::CriticalSection status_lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScanDialog)
};

}  // namespace jyglobalvst::tray
