// src/tray-app/ui/main_window.h
//
// T045 — Main application window with neon/glow theme and FlexBox layout.

#pragma once

#include "catalog_dialog.h"
#include "chain_editor.h"
#include "custom_look_and_feel.h"
#include "meter_panel.h"
#include "presets/autosave.h"
#include "scan_dialog.h"
#include "settings/local_state.h"
#include "settings/roaming_settings.h"

#include "jyglobalvst/audio_engine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <windows.h>

#include <memory>
#include <vector>

namespace jyglobalvst::tray {

// Forward declaration of the content component defined in main_window.cpp.
class MainContentComponent;

// Green leaf toggle button for Energy Saver mode. Sits in the header next to the
// "?" button. Painted directly (no bitmap) so it stays crisp at any DPI and can
// reflect three states through colour/fill:
//   • feature off      → muted grey outline
//   • enabled, awake    → green outline + faint fill
//   • enabled, sleeping → brighter green, solid fill (engine is saving energy)
class LeafButton : public juce::Button
{
public:
    LeafButton();
    void setEnergyState(bool enabled, bool sleeping);

private:
    void paintButton(juce::Graphics& g, bool shouldDrawHighlighted, bool shouldDrawDown) override;

    bool enabled_ {false};
    bool sleeping_ {false};
};

// Special endpoint ID representing no device connection
constexpr std::string_view kDisconnectedDeviceId = "";

// Main application window. Implements IAudioEngineListener so it can receive
// async callbacks from the audio engine (chain revisions, device events, etc.).
class MainWindow : public juce::DocumentWindow
               , public juce::Button::Listener
               , public juce::ComboBox::Listener
               , public juce::Slider::Listener
               , public juce::Timer
               , public IAudioEngineListener
{
public:
    explicit MainWindow(std::unique_ptr<IAudioEngine> engine);
    ~MainWindow() override;

    // juce::DocumentWindow
    void closeButtonPressed() override;
    void visibilityChanged() override;
    void moved() override;
    void resized() override;

    // juce::Button::Listener
    void buttonClicked(juce::Button* button) override;

    // juce::ComboBox::Listener
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    // juce::Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override;

    // IAudioEngineListener --------------------------------------------------
    void onChainRevision(int new_revision) override;
    void onPluginFailed(const InstanceId& id, const std::string& reason) override;
    void onDeviceLost(const EndpointId& lost, const EndpointId& fallback_to) override;
    void onDeviceRestored(const EndpointId& restored) override;
    void onCpuWarning(float rolling_1s_pct) override;
    void onMeterFrame(const MeterFrame& frame) override;
    void onPresetPartialLoad(const std::vector<MissingPluginInfo>& missing) override;
    void onSameDeviceConflict(const EndpointId& device) override;
    void onCaptureMuteFallbackRequired(const EndpointId& endpoint) override;
    void onEnergySaverStateChanged(bool sleeping) override;

    // System tray -----------------------------------------------------------
    void createTrayIcon();
    void destroyTrayIcon();
    void updateTrayIconTooltip(const juce::String& text);
    void updateTrayIconAppearance();
    void showTrayContextMenu();
    void showTrayVolumePopup();
    void restoreFromTray();
    void quitFromTray();
    bool handleTrayMessage(UINT msg, LPARAM lParam);

    // Applies a master-volume change from any source (main slider or tray popup):
    // updates the engine, persists it, and optionally syncs the main slider display.
    void applyMasterVolume(float gain, bool updateMainSlider);

    // Starts or stops the audio engine from any source (toolbar button, tray menu,
    // tray popup): updates engine state, the toolbar button text, and the status label.
    void setAudioRunning(bool run);

private:
    void buildUI();
    void refreshDeviceLists();
    void refreshAsioPairSelector(int maxChannels);
    void updateControlVisibility();
    void refreshLatencyAndCpu();
    void refreshMeters();
    void refreshSampleRates();
    void restoreFromAutosave();
    void saveSessionState();
    void handleLoadPlugin();
    void openPluginFileBrowser();
    void handleSavePreset();
    void handleLoadPreset();
    void handleDragAndDropPreset(const std::filesystem::path& path);
    void revealPresetInExplorer(const std::filesystem::path& path);
    void showRepointPlaceholderDialog(int position);
    void handleAbout();
    void handleHelp();
    void toggleEnergySaver();
    void updateEnergySaverVisual();
    void handleCheckForUpdates();
    void applyThemeChange(CustomLookAndFeel::ThemeId id);
    void saveWindowStateIfNeeded();
    bool hasEqPlugin() const;
    bool hasNtPlugin() const;
    void toggleEqBypass();
    void toggleNtBypass();
    void updateMuteButtonVisual(bool muted);

    // Timer callback to poll CPU/latency and refresh UI at 10 Hz.
    void timerCallback() override;

public:
    // Sleep / wake (T043).
    void installPowerHandler();
    void removePowerHandler();
    void onSystemSuspend();
    void onSystemResume();

private:
    std::unique_ptr<IAudioEngine> engine_;
    AutoSaveStore autosave_store_;
    LocalStateStore local_state_store_;
    RoamingSettingsStore roaming_settings_store_;
    bool initializing_ {true};

    // UI elements
    std::unique_ptr<juce::ComboBox> transport_mode_selector_;
    std::unique_ptr<juce::ComboBox> input_selector_;
    std::unique_ptr<juce::ComboBox> output_selector_;
    std::unique_ptr<juce::ComboBox> asio_device_selector_;
    std::unique_ptr<juce::ComboBox> asio_pair_selector_;
    std::unique_ptr<juce::ComboBox> buffer_selector_;
    std::unique_ptr<juce::ComboBox> theme_selector_;
    std::unique_ptr<juce::ToggleButton> start_minimized_button_;

    // Parallel storage for endpoint IDs (JUCE ComboBox has no setItemData).
    std::vector<EndpointId> input_endpoint_ids_;
    std::vector<EndpointId> output_endpoint_ids_;
    std::vector<std::string> asio_device_names_;

    enum class TransportMode : int
    {
        Wasapi = 1,
        Asio = 2,
        WasapiExclusive = 3,
    };
    TransportMode current_transport_mode_ {TransportMode::Wasapi};
    std::unique_ptr<juce::TextButton> audio_toggle_;
    std::unique_ptr<juce::TextButton> asio_settings_button_;
    std::unique_ptr<juce::TextButton> save_preset_button_;
    std::unique_ptr<juce::TextButton> load_preset_button_;
    std::unique_ptr<juce::TextButton> about_button_;
    std::unique_ptr<LeafButton> energy_saver_button_;
    std::unique_ptr<juce::TextButton> help_button_;
    std::unique_ptr<juce::Slider> volume_slider_;
    std::unique_ptr<juce::TextButton> mute_button_;
    float pre_mute_volume_ {1.0f};
    std::unique_ptr<juce::TextButton> reset_engine_button_;
    std::unique_ptr<juce::Label> latency_label_;
    std::unique_ptr<juce::Label> cpu_label_;
    std::unique_ptr<juce::Label> cpu_warning_banner_;
    std::unique_ptr<juce::Label> input_meter_label_;
    std::unique_ptr<juce::Label> output_meter_label_;
    std::unique_ptr<juce::Label> plugin_slot_label_;
    std::unique_ptr<juce::Label> status_label_;

    // Labels (now owned as members for visibility toggling).
    std::unique_ptr<juce::Label> transport_label_;
    std::unique_ptr<juce::Label> input_label_;
    std::unique_ptr<juce::Label> output_label_;
    std::unique_ptr<juce::Label> asio_device_label_;
    std::unique_ptr<juce::Label> asio_pair_label_;
    std::unique_ptr<juce::Label> buffer_label_;
    // Read-only input/capture sample-rate readout.
    std::unique_ptr<juce::Label> input_rate_caption_;
    std::unique_ptr<juce::Label> input_rate_value_;
    // Output sample-rate selector; the VST chain auto-follows the negotiated rate,
    // which is shown read-only below.
    std::unique_ptr<juce::Label> output_rate_caption_;
    std::unique_ptr<juce::ComboBox> output_rate_selector_;
    std::unique_ptr<juce::Label> vst_rate_caption_;
    std::unique_ptr<juce::Label> vst_rate_value_;
    std::unique_ptr<juce::Label> vol_label_;
    std::unique_ptr<juce::Label> theme_label_;
    std::unique_ptr<juce::Label> start_minimized_label_;

    // Raw pointer into the content tree — owned by HeaderComponent, valid for app lifetime.
    juce::Label* title_label_ {nullptr};

    // Input / output meters (US4 T098).
    std::unique_ptr<MeterPanel> meter_input_l_;
    std::unique_ptr<MeterPanel> meter_input_r_;
    std::unique_ptr<MeterPanel> meter_output_l_;
    std::unique_ptr<MeterPanel> meter_output_r_;
    std::unique_ptr<juce::Label> meter_input_label_;
    std::unique_ptr<juce::Label> meter_output_label_;

    std::unique_ptr<ChainEditor> chain_editor_;
    std::unique_ptr<ScanDialog> scan_dialog_;
    std::unique_ptr<CatalogDialog> catalog_dialog_;
    CustomLookAndFeel custom_laf_;

    // Icon image loaded from binary data.
    juce::Image app_icon_image_;

    // Layout root (raw pointer — owned by setContentOwned).
    MainContentComponent* content_root_ {nullptr};

    // Panel references for layout recalculation.
    juce::Component* device_panel_ {nullptr};

    bool audio_running_ {false};
    bool audio_was_running_before_suspend_ {false};
    int last_chain_revision_ {-1};
    bool cpu_warning_active_ {false};
    bool preset_override_flag_ {false};
    bool plugin_scan_complete_ {false};

    // Latest meter frame (written by onMeterFrame, read by timerCallback).
    MeterFrame latest_meter_frame_;
    std::mutex meter_frame_mutex_;

    // System tray state.
    bool tray_icon_created_ {false};
    bool tray_icon_active_shown_ {false};  // true when the colour (active) icon is displayed
    HMENU tray_menu_ {nullptr};
    HICON tray_hicon_ {nullptr};
    HICON tray_hicon_gray_ {nullptr};      // desaturated icon shown while paused
    HICON window_icon_big_ {nullptr};
    HICON window_icon_small_ {nullptr};

    // Safe pointer to the tray volume popup content (if open), for meter updates in the
    // timer callback. Automatically becomes null when the CallOutBox (and its owned
    // content component) is dismissed and destroyed, avoiding a dangling-pointer
    // use-after-free from the periodic meter refresh timer.
    juce::Component::SafePointer<juce::Component> tray_volume_popup_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

}  // namespace jyglobalvst::tray
