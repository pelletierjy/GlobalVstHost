// src/tray-app/ui/main_window.cpp
//
// T045 / T062 — Main window with neon/glow theme and FlexBox layout.

#include "main_window.h"
#include "about_diagnostics.h"

#include "builtin-effects/builtin_theme.h"
#include "builtin-effects/builtin_ids.h"

#include "BinaryData.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>

namespace jyglobalvst::tray {

namespace {

int findPluginByUid(const ChainSnapshot& chain, const PluginUid& uid)
{
    for (size_t i = 0; i < chain.slots.size(); ++i)
    {
        if (chain.slots[i].ref.plugin_uid == uid)
            return static_cast<int>(i);
    }
    return -1;
}

void StartupLog(const char* msg)
{
    try
    {
        auto path = std::filesystem::path(std::getenv("LOCALAPPDATA")) / "JyGlobalVST" / "startup.log";
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::app);
        if (ofs)
        {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            ofs << std::put_time(std::gmtime(&t), "%H:%M:%S") << " [MainWindow] " << msg << "\n";
        }
    }
    catch (...)
    {
    }
}



constexpr int kTimerHz = 10;  // 10 Hz UI refresh for meters / CPU.
constexpr UINT kTrayIconMsg = WM_APP + 1;
constexpr UINT kTrayIconId  = 1;

// Tiny "in" / "out" marker sat under a meter pair in the tray popup. Drawn as a
// vector glyph through the LookAndFeel, so it matches the mute / power icons on
// the same popup rather than reading as a text caption.
class SignalDirectionIcon : public juce::Component
{
public:
    explicit SignalDirectionIcon(bool is_input) : is_input_(is_input)
    {
        setInterceptsMouseClicks(false, false);
        setTitle(is_input ? "Input" : "Output");
    }

    void paint(juce::Graphics& g) override
    {
        auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
        const juce::Colour colour = (laf != nullptr) ? laf->colors().textDim : kTextDim;
        CustomLookAndFeel::drawSignalDirectionIcon(g, getLocalBounds().toFloat(),
                                                   is_input_, colour);
    }

private:
    bool is_input_;
};

// Small content component shown in a CallOutBox when the tray icon is left-clicked.
// Holds input/output meters on sides, a vertical master-volume slider in center,
// and control buttons below, all reporting changes through callbacks.
class TrayVolumeContent : public juce::Component
{
public:
    TrayVolumeContent(float initial_gain, bool audio_running,
                      std::function<void(float)> on_change,
                      std::function<bool()> on_toggle_audio,
                      std::function<void()> on_mute_toggle,
                      std::function<void()> on_toggle_eq_bypass,
                      std::function<void()> on_toggle_nt_bypass,
                      bool has_eq, bool has_nt)
        : on_change_(std::move(on_change)), on_toggle_audio_(std::move(on_toggle_audio)),
          on_mute_toggle_(std::move(on_mute_toggle)),
          on_toggle_eq_bypass_(std::move(on_toggle_eq_bypass)),
          on_toggle_nt_bypass_(std::move(on_toggle_nt_bypass)),
          pre_mute_volume_(initial_gain), has_eq_(has_eq), has_nt_(has_nt)
    {
        title_.setText("Master Volume", juce::dontSendNotification);
        title_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title_);

        slider_.setSliderStyle(juce::Slider::LinearVertical);
        slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 18);
        slider_.setRange(0.0, 1.0, 0.0);
        slider_.setNumDecimalPlacesToDisplay(2);
        slider_.setValue(initial_gain, juce::dontSendNotification);
        slider_.onValueChange = [this]()
        {
            if (on_change_)
            {
                float new_val = static_cast<float>(slider_.getValue());
                on_change_(new_val);
                if (new_val > 0.0f && is_muted_)
                {
                    is_muted_ = false;
                    updateMuteButtonVisual();
                    pre_mute_volume_ = new_val;
                }
                else if (new_val == 0.0f)
                {
                    is_muted_ = true;
                    updateMuteButtonVisual();
                }
                else
                {
                    pre_mute_volume_ = new_val;
                }
            }
        };
        addAndMakeVisible(slider_);

        meter_input_l_ = std::make_unique<MeterPanel>();
        meter_input_r_ = std::make_unique<MeterPanel>();
        meter_output_l_ = std::make_unique<MeterPanel>();
        meter_output_r_ = std::make_unique<MeterPanel>();
        addAndMakeVisible(meter_input_l_.get());
        addAndMakeVisible(meter_input_r_.get());
        addAndMakeVisible(meter_output_l_.get());
        addAndMakeVisible(meter_output_r_.get());

        addAndMakeVisible(input_icon_);
        addAndMakeVisible(output_icon_);

        is_muted_ = (initial_gain <= 0.0f);
        updateMuteButtonVisual();
        mute_button_.setClickingTogglesState(true);
        mute_button_.onClick = [this]()
        {
            if (on_mute_toggle_)
                on_mute_toggle_();
        };
        addAndMakeVisible(mute_button_);

        if (has_eq_)
        {
            eq_button_.setButtonText("EQ");
            eq_button_.setClickingTogglesState(true);
            eq_button_.onClick = [this]()
            {
                if (on_toggle_eq_bypass_)
                    on_toggle_eq_bypass_();
            };
            addAndMakeVisible(eq_button_);
        }

        if (has_nt_)
        {
            nt_button_.setButtonText("VL");
            nt_button_.setClickingTogglesState(true);
            nt_button_.onClick = [this]()
            {
                if (on_toggle_nt_bypass_)
                    on_toggle_nt_bypass_();
            };
            addAndMakeVisible(nt_button_);
        }

        audio_button_.setButtonText(audio_running ? "ON" : "OFF");
        audio_button_.setToggleState(audio_running, juce::dontSendNotification);
        audio_button_.onClick = [this]()
        {
            if (on_toggle_audio_)
            {
                const bool now_running = on_toggle_audio_();
                audio_button_.setToggleState(now_running, juce::dontSendNotification);
                audio_button_.setButtonText(now_running ? "ON" : "OFF");
            }
        };
        addAndMakeVisible(audio_button_);

        // Height allows for the in/out glyph strip without shortening the meters.
        setSize(160, 300);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(6);
        title_.setBounds(b.removeFromTop(20));

        // Layout buttons at bottom based on what's available
        int button_count = 1;  // audio_button is always present
        if (has_eq_)
            button_count++;
        if (has_nt_)
            button_count++;

        // Reserve space for buttons
        auto button_area = b.removeFromBottom(26);
        b.removeFromBottom(6);

        // Layout buttons in button_area
        juce::FlexBox button_fb;
        button_fb.flexDirection = juce::FlexBox::Direction::row;
        button_fb.items.add(juce::FlexItem(audio_button_).withFlex(1.0f));
        if (has_nt_)
        {
            button_fb.items.add(juce::FlexItem().withWidth(4));
            button_fb.items.add(juce::FlexItem(nt_button_).withFlex(1.0f));
        }
        if (has_eq_)
        {
            button_fb.items.add(juce::FlexItem().withWidth(4));
            button_fb.items.add(juce::FlexItem(eq_button_).withFlex(1.0f));
        }
        button_fb.items.add(juce::FlexItem().withWidth(4));
        button_fb.items.add(juce::FlexItem(mute_button_).withFlex(1.0f));
        button_fb.performLayout(button_area);

        // Meter area + volume slider: layout horizontally
        // Meters (14px each) on left/right, volume in middle
        const int meter_width = 14;
        const int spacing = 3;
        const int icon_height = 16;

        auto in_area = b.removeFromLeft(meter_width * 2 + spacing);
        b.removeFromLeft(spacing);
        auto out_area = b.removeFromRight(meter_width * 2 + spacing);
        b.removeFromRight(spacing);

        // The in/out glyph sits directly under each meter pair, spanning both channels.
        input_icon_.setBounds(in_area.removeFromBottom(icon_height));
        output_icon_.setBounds(out_area.removeFromBottom(icon_height));
        in_area.removeFromBottom(3);
        out_area.removeFromBottom(3);

        auto in_l_area = in_area.removeFromLeft(meter_width);
        in_area.removeFromLeft(spacing);
        auto in_r_area = in_area.removeFromLeft(meter_width);
        auto out_l_area = out_area.removeFromRight(meter_width);
        out_area.removeFromRight(spacing);
        auto out_r_area = out_area.removeFromRight(meter_width);

        meter_input_l_->setBounds(in_l_area);
        meter_input_r_->setBounds(in_r_area);
        meter_output_r_->setBounds(out_r_area);
        meter_output_l_->setBounds(out_l_area);

        // Volume slider takes remaining space in center
        slider_.setBounds(b);
    }

    void setMeterLevels(float in_l_peak, float in_l_rms,
                       float in_r_peak, float in_r_rms,
                       float out_l_peak, float out_l_rms,
                       float out_r_peak, float out_r_rms)
    {
        meter_input_l_->setLevels(in_l_peak, in_l_rms);
        meter_input_r_->setLevels(in_r_peak, in_r_rms);
        meter_output_l_->setLevels(out_l_peak, out_l_rms);
        meter_output_r_->setLevels(out_r_peak, out_r_rms);
    }

    void setMuted(bool muted)
    {
        is_muted_ = muted;
        updateMuteButtonVisual();
    }

    void setEqBypassed(bool bypassed)
    {
        if (has_eq_)
            eq_button_.setToggleState(!bypassed, juce::dontSendNotification);
    }

    void setNtBypassed(bool bypassed)
    {
        if (has_nt_)
            nt_button_.setToggleState(!bypassed, juce::dontSendNotification);
    }

    void setAudioRunning(bool running)
    {
        audio_button_.setToggleState(running, juce::dontSendNotification);
    }

    bool isMuted() const { return is_muted_; }
    void setPreMuteVolume(float vol) { pre_mute_volume_ = vol; }
    float getPreMuteVolume() const { return pre_mute_volume_; }

private:
    void updateMuteButtonVisual()
    {
        mute_button_.setButtonText(is_muted_ ? "SPEAKER_OFF" : "SPEAKER_ON");
        // Toggle state tracks "active" (unmuted) so the button highlights when
        // audio is audible, not when it's muted.
        mute_button_.setToggleState(!is_muted_, juce::dontSendNotification);
    }

    juce::Label title_;
    juce::Slider slider_;
    juce::ToggleButton mute_button_;
    juce::ToggleButton eq_button_;
    juce::ToggleButton nt_button_;
    juce::ToggleButton audio_button_;
    std::unique_ptr<MeterPanel> meter_input_l_;
    std::unique_ptr<MeterPanel> meter_input_r_;
    std::unique_ptr<MeterPanel> meter_output_l_;
    std::unique_ptr<MeterPanel> meter_output_r_;
    SignalDirectionIcon input_icon_ {true};
    SignalDirectionIcon output_icon_ {false};
    std::function<void(float)> on_change_;
    std::function<bool()> on_toggle_audio_;
    std::function<void()> on_mute_toggle_;
    std::function<void()> on_toggle_eq_bypass_;
    std::function<void()> on_toggle_nt_bypass_;
    bool is_muted_ {false};
    float pre_mute_volume_;
    bool has_eq_;
    bool has_nt_;
};

LRESULT CALLBACK mainWindowSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    auto* self = reinterpret_cast<MainWindow*>(dwRefData);
    if (self != nullptr)
    {
        if (msg == WM_POWERBROADCAST)
        {
            if (wParam == PBT_APMSUSPEND)
            {
                self->onSystemSuspend();
            }
            else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND)
            {
                self->onSystemResume();
            }
        }
        else if (msg == kTrayIconMsg)
        {
            if (self->handleTrayMessage(static_cast<UINT>(lParam), wParam))
            {
                return 0;
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

juce::String endpointDisplayName(const HardwareOutputInfo& info, bool for_input)
{
    juce::String s;
    if (for_input)
    {
        if (info.is_loopback)
            s += "System: ";
        else
            s += "Input: ";
    }
    s += juce::String(info.friendly_name);
    if (info.transport_kind == TransportKind::Asio)
    {
        s += " (ASIO)";
    }
    if (info.is_default)
    {
        s += " (Default)";
    }
    if (!info.is_present)
    {
        s += " [Disconnected]";
    }
    return s;
}

// Create an HICON from a JUCE Image at the requested size. When `grayscale` is
// true each pixel is desaturated to its luminance (alpha preserved), used for the
// "engine paused" tray icon.
HICON createHICONFromJuceImage(const juce::Image& sourceImage, int width, int height,
                               bool grayscale = false)
{
    auto image = sourceImage.rescaled(width, height);

    // Create image with alpha channel for proper transparency.
    juce::Image rgba(juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g(rgba);
        g.drawImageAt(image, 0, 0);
    }

    HDC screenDC = GetDC(nullptr);
    if (!screenDC)
        return nullptr;

    // Create color DIB with alpha channel.
    BITMAPV5HEADER bmi = {};
    bmi.bV5Size = sizeof(BITMAPV5HEADER);
    bmi.bV5Width = width;
    bmi.bV5Height = -height; // top-down
    bmi.bV5Planes = 1;
    bmi.bV5BitCount = 32;
    bmi.bV5Compression = BI_BITFIELDS;
    bmi.bV5RedMask = 0x00FF0000;
    bmi.bV5GreenMask = 0x0000FF00;
    bmi.bV5BlueMask = 0x000000FF;
    bmi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HBITMAP hColor = CreateDIBSection(screenDC, reinterpret_cast<BITMAPINFO*>(&bmi),
                                      DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDC);

    if (!hColor || !bits)
    {
        DeleteObject(hColor);
        return nullptr;
    }

    // Copy image data to DIB in BGRA format.
    juce::Image::BitmapData srcData(rgba, juce::Image::BitmapData::readOnly);
    auto* dstRow = static_cast<uint8_t*>(bits);
    for (int y = 0; y < height; ++y)
    {
        auto* src = srcData.getLinePointer(y);
        auto* dst = dstRow;
        for (int x = 0; x < width; ++x)
        {
            // Convert from ARGB to BGRA, keeping straight alpha (not premultiplied)
            if (grayscale)
            {
                // Rec. 601 luma; alpha untouched so the icon keeps its shape.
                const auto lum = static_cast<uint8_t>(
                    (src[0] * 299 + src[1] * 587 + src[2] * 114) / 1000);
                dst[0] = lum;  // B
                dst[1] = lum;  // G
                dst[2] = lum;  // R
            }
            else
            {
                dst[0] = src[2];  // B
                dst[1] = src[1];  // G
                dst[2] = src[0];  // R
            }
            dst[3] = src[3];  // A
            dst += 4;
            src += 4;
        }
        dstRow += (width * 4);
    }

    // Create icon with alpha channel support.
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = nullptr;  // Use DIB's alpha channel instead of mask
    ii.hbmColor = hColor;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColor);
    return hIcon;
}

}  // namespace

// ============================================================================
// Panel components (defined in anonymous namespace).
// ============================================================================

namespace {

class GlassPanel : public juce::Component
{
public:
    void setSectionTitle(const juce::String& title) { section_title_ = title; }

    void paint(juce::Graphics& g) override
    {
        auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
        if (laf != nullptr)
            laf->drawGlassPanel(g, getLocalBounds().toFloat().reduced(0.5f));
        else
            g.fillAll(kBgPanel);

        if (section_title_.isNotEmpty())
        {
            const juce::Colour titleCol = (laf != nullptr)
                ? laf->colors().accentCyan
                : kAccentCyan;

            juce::FontOptions fo;
            juce::Font f{fo};
            f.setHeight(11.0f);
            g.setFont(f);
            g.setColour(titleCol.withAlpha(0.75f));
            g.drawText(section_title_,
                       getLocalBounds().reduced(10, 0).withHeight(kTitleH),
                       juce::Justification::centredLeft, false);

            // Thin separator line below title.
            g.setColour(titleCol.withAlpha(0.15f));
            g.fillRect(juce::Rectangle<float>(10.0f, static_cast<float>(kTitleH) - 1.0f,
                                              static_cast<float>(getWidth()) - 20.0f, 1.0f));
        }
    }

    // Content area below the title strip.
    juce::Rectangle<int> contentBounds() const
    {
        return section_title_.isNotEmpty()
            ? getLocalBounds().reduced(8).withTrimmedTop(kTitleH - 8)
            : getLocalBounds().reduced(8);
    }

    static constexpr int kTitleH = 18;

private:
    juce::String section_title_;
};

class DevicePanel : public GlassPanel
{
public:
    DevicePanel(juce::Label* transport_label, juce::ComboBox* transport_mode,
                juce::Label* input_label, juce::ComboBox* input,
                juce::Label* input_rate_caption, juce::Label* input_rate_value,
                juce::Label* output_label, juce::ComboBox* output,
                juce::Label* buffer_label, juce::ComboBox* buffer,
                juce::Label* output_rate_caption, juce::Label* output_rate_value,
                juce::Label* vst_rate_caption, juce::ComboBox* vst_rate_selector,
                juce::Label* asio_device_label, juce::ComboBox* asio_device,
                juce::Label* asio_pair_label, juce::ComboBox* asio_pair,
                juce::TextButton* asio_settings)
        : transport_label_(transport_label), transport_mode_(transport_mode),
          input_label_(input_label), input_(input),
          input_rate_caption_(input_rate_caption), input_rate_value_(input_rate_value),
          output_label_(output_label), output_(output),
          buffer_label_(buffer_label), buffer_(buffer),
          output_rate_caption_(output_rate_caption), output_rate_value_(output_rate_value),
          vst_rate_caption_(vst_rate_caption), vst_rate_selector_(vst_rate_selector),
          asio_device_label_(asio_device_label), asio_device_(asio_device),
          asio_pair_label_(asio_pair_label), asio_pair_(asio_pair),
          asio_settings_(asio_settings)
    {
        setSectionTitle("AUDIO DEVICE");
        addAndMakeVisible(transport_label_);
        addAndMakeVisible(transport_mode_);
        addAndMakeVisible(input_label_);
        addAndMakeVisible(input_);
        addAndMakeVisible(input_rate_caption_);
        addAndMakeVisible(input_rate_value_);
        addAndMakeVisible(output_label_);
        addAndMakeVisible(output_);
        addAndMakeVisible(buffer_label_);
        addAndMakeVisible(buffer_);
        addAndMakeVisible(output_rate_caption_);
        addAndMakeVisible(output_rate_value_);
        addAndMakeVisible(vst_rate_caption_);
        addAndMakeVisible(vst_rate_selector_);
        addAndMakeVisible(asio_device_label_);
        addAndMakeVisible(asio_device_);
        addAndMakeVisible(asio_pair_label_);
        addAndMakeVisible(asio_pair_);
        addAndMakeVisible(asio_settings_);
    }

    void paint(juce::Graphics& g) override
    {
        GlassPanel::paint(g);

        auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
        const auto col = (laf != nullptr) ? laf->colors().accentCyan.withAlpha(0.65f)
                                           : kAccentCyan.withAlpha(0.65f);

        juce::FontOptions fo;
        juce::Font f{fo};
        f.setHeight(10.0f);
        g.setFont(f);

        auto b = contentBounds();

        auto input_hdr = b.removeFromTop(kSubHeaderH);
        g.setColour(col);
        g.drawText("INPUT", input_hdr, juce::Justification::centredLeft, false);
        g.setColour(col.withAlpha(0.2f));
        g.fillRect(juce::Rectangle<float>(static_cast<float>(input_hdr.getX()),
                                          static_cast<float>(input_hdr.getBottom()) - 1.0f,
                                          static_cast<float>(input_hdr.getWidth()), 1.0f));

        // Must match the INPUT-section carving in resized() exactly.
        b.removeFromTop(kHdrGap + kRowH + kRowGap + kRowH + kSectionGap);

        auto output_hdr = b.removeFromTop(kSubHeaderH);
        g.setColour(col);
        g.drawText("OUTPUT", output_hdr, juce::Justification::centredLeft, false);
        g.setColour(col.withAlpha(0.2f));
        g.fillRect(juce::Rectangle<float>(static_cast<float>(output_hdr.getX()),
                                          static_cast<float>(output_hdr.getBottom()) - 1.0f,
                                          static_cast<float>(output_hdr.getWidth()), 1.0f));
    }

    void resized() override
    {
        auto b = contentBounds();

        b.removeFromTop(kSubHeaderH);  // INPUT header (drawn in paint)
        b.removeFromTop(kHdrGap);
        auto row_input = b.removeFromTop(kRowH);
        b.removeFromTop(kRowGap);
        auto row_input_rate = b.removeFromTop(kRowH);
        b.removeFromTop(kSectionGap);

        b.removeFromTop(kSubHeaderH);  // OUTPUT header (drawn in paint)
        b.removeFromTop(kHdrGap);
        auto row_transport = b.removeFromTop(kRowH);
        b.removeFromTop(kRowGap);
        auto row_output = b.removeFromTop(kRowH);  // WASAPI output or ASIO device (same slot)
        b.removeFromTop(kRowGap);
        auto row_buffer = b.removeFromTop(kRowH);
        b.removeFromTop(kRowGap);
        auto row_output_rate = b.removeFromTop(kRowH);
        b.removeFromTop(kRowGap);
        auto row_vst_rate = b.removeFromTop(kRowH);
        b.removeFromTop(kRowGap);
        auto row_asio_pair = b.removeFromTop(kRowH);

        juce::FlexBox fb_input;
        fb_input.flexDirection = juce::FlexBox::Direction::row;
        fb_input.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_input.items.add(juce::FlexItem(*input_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_input.items.add(juce::FlexItem().withWidth(4));
        fb_input.items.add(juce::FlexItem(*input_).withFlex(1));
        fb_input.performLayout(row_input);

        juce::FlexBox fb_in_rate;
        fb_in_rate.flexDirection = juce::FlexBox::Direction::row;
        fb_in_rate.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_in_rate.items.add(juce::FlexItem(*input_rate_caption_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_in_rate.items.add(juce::FlexItem().withWidth(4));
        fb_in_rate.items.add(juce::FlexItem(*input_rate_value_).withFlex(1));
        fb_in_rate.performLayout(row_input_rate);

        juce::FlexBox fb_transport;
        fb_transport.flexDirection = juce::FlexBox::Direction::row;
        fb_transport.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_transport.items.add(juce::FlexItem(*transport_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_transport.items.add(juce::FlexItem().withWidth(4));
        fb_transport.items.add(juce::FlexItem(*transport_mode_).withFlex(1));
        fb_transport.performLayout(row_transport);

        // WASAPI output OR ASIO device — same slot, mutually exclusive via visibility.
        juce::FlexBox fb_out_w;
        fb_out_w.flexDirection = juce::FlexBox::Direction::row;
        fb_out_w.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_out_w.items.add(juce::FlexItem(*output_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_out_w.items.add(juce::FlexItem().withWidth(4));
        fb_out_w.items.add(juce::FlexItem(*output_).withFlex(1));
        fb_out_w.performLayout(row_output);

        juce::FlexBox fb_out_a;
        fb_out_a.flexDirection = juce::FlexBox::Direction::row;
        fb_out_a.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_out_a.items.add(juce::FlexItem(*asio_device_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_out_a.items.add(juce::FlexItem().withWidth(4));
        fb_out_a.items.add(juce::FlexItem(*asio_device_).withFlex(1));
        fb_out_a.performLayout(row_output);

        juce::FlexBox fb_buffer;
        fb_buffer.flexDirection = juce::FlexBox::Direction::row;
        fb_buffer.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_buffer.items.add(juce::FlexItem(*buffer_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_buffer.items.add(juce::FlexItem().withWidth(4));
        fb_buffer.items.add(juce::FlexItem(*buffer_).withFlex(1));
        fb_buffer.performLayout(row_buffer);

        juce::FlexBox fb_out_rate;
        fb_out_rate.flexDirection = juce::FlexBox::Direction::row;
        fb_out_rate.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_out_rate.items.add(juce::FlexItem(*output_rate_caption_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_out_rate.items.add(juce::FlexItem().withWidth(4));
        fb_out_rate.items.add(juce::FlexItem(*output_rate_value_).withFlex(1));
        fb_out_rate.performLayout(row_output_rate);

        juce::FlexBox fb_vst_rate;
        fb_vst_rate.flexDirection = juce::FlexBox::Direction::row;
        fb_vst_rate.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_vst_rate.items.add(juce::FlexItem(*vst_rate_caption_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_vst_rate.items.add(juce::FlexItem().withWidth(4));
        fb_vst_rate.items.add(juce::FlexItem(*vst_rate_selector_).withFlex(1));
        fb_vst_rate.performLayout(row_vst_rate);

        // ASIO channel pair + settings (hidden in WASAPI mode).
        juce::FlexBox fb_asio_pair;
        fb_asio_pair.flexDirection = juce::FlexBox::Direction::row;
        fb_asio_pair.alignItems = juce::FlexBox::AlignItems::stretch;
        fb_asio_pair.items.add(juce::FlexItem(*asio_pair_label_).withMinWidth(kLabelW).withWidth(kLabelW));
        fb_asio_pair.items.add(juce::FlexItem().withWidth(4));
        fb_asio_pair.items.add(juce::FlexItem(*asio_pair_).withFlex(1));
        fb_asio_pair.items.add(juce::FlexItem().withWidth(4));
        fb_asio_pair.items.add(juce::FlexItem(*asio_settings_).withMinWidth(90).withWidth(90));
        fb_asio_pair.performLayout(row_asio_pair);
    }

    static constexpr int kSubHeaderH = 14;

    // Row metrics. paint() has to skip the INPUT rows to place the OUTPUT
    // sub-header, so it repeats the same arithmetic as resized() — naming these
    // keeps the two from drifting apart.
    static constexpr int kRowH       = 28;
    static constexpr int kRowGap     = 4;
    static constexpr int kHdrGap     = 2;   // sub-header → first row of its section
    static constexpr int kSectionGap = 6;   // last row of a section → next sub-header

    // Every row is "fixed-width caption | 4 px | flex(1) control". One shared
    // caption width keeps the control column aligned and, unlike the previous
    // per-row values, is wide enough for the longest caption in the panel
    // ("Captured sampling rate:"). The VST row used to get 55 px, which clipped
    // "VST sampling rate:" to "VST sa…" once the panel moved into a 2:1 column.
    static constexpr int kLabelW = 140;

private:
    juce::Label* transport_label_;
    juce::ComboBox* transport_mode_;
    juce::Label* input_label_;
    juce::ComboBox* input_;
    juce::Label* input_rate_caption_;
    juce::Label* input_rate_value_;
    juce::Label* output_label_;
    juce::ComboBox* output_;
    juce::Label* buffer_label_;
    juce::ComboBox* buffer_;
    juce::Label* output_rate_caption_;
    juce::Label* output_rate_value_;
    juce::Label* vst_rate_caption_;
    juce::ComboBox* vst_rate_selector_;
    juce::Label* asio_device_label_;
    juce::ComboBox* asio_device_;
    juce::Label* asio_pair_label_;
    juce::ComboBox* asio_pair_;
    juce::TextButton* asio_settings_;
};


class PluginButtonPanel : public GlassPanel
{
public:
    PluginButtonPanel(juce::TextButton* save, juce::TextButton* load_preset)
        : save_(save), load_preset_(load_preset)
    {
        addAndMakeVisible(save_);
        addAndMakeVisible(load_preset_);
    }

    void resized() override
    {
        auto b = contentBounds();
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::stretch;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.items.add(juce::FlexItem(*load_preset_).withMinWidth(90).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*save_).withMinWidth(90).withFlex(1));
        fb.performLayout(b);
    }

private:
    juce::TextButton* save_;
    juce::TextButton* load_preset_;
};

class MeterStatusPanel : public GlassPanel
{
public:
    MeterStatusPanel(juce::Label* in_label, MeterPanel* in_l, MeterPanel* in_r,
                     juce::Label* out_label, MeterPanel* out_l, MeterPanel* out_r,
                     juce::Label* vol_label, juce::Slider* volume, juce::TextButton* mute_button)
        : in_label_(in_label), in_l_(in_l), in_r_(in_r),
          out_label_(out_label), out_l_(out_l), out_r_(out_r),
          vol_label_(vol_label), volume_(volume), mute_button_(mute_button)
    {
        setSectionTitle("LEVELS");
        addAndMakeVisible(in_label_);
        addAndMakeVisible(in_l_);
        addAndMakeVisible(in_r_);
        addAndMakeVisible(out_label_);
        addAndMakeVisible(out_l_);
        addAndMakeVisible(out_r_);
        addAndMakeVisible(vol_label_);
        addAndMakeVisible(volume_);
        addAndMakeVisible(mute_button_);
    }

    void resized() override
    {
        auto b = contentBounds();
        constexpr int kLabelH = 18;
        constexpr int kButtonH = 24;

        auto labelRow = b.removeFromTop(kLabelH);
        b.removeFromTop(4);

        // Reserve space at the bottom for the mute button
        auto mute_area = b.removeFromBottom(kButtonH);
        b.removeFromBottom(4);

        // Four equal-width vertical bars: In L, In R, Out L, Out R, Vol
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::stretch;
        fb.items.add(juce::FlexItem(*in_l_).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*in_r_).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*out_l_).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*out_r_).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*volume_).withFlex(1));
        fb.performLayout(b);

        // Labels: "Cap" spans in_l+in_r, "Out" spans out_l+out_r, "Vol" over volume
        auto makeLabel = [&](juce::Component* a, juce::Component* b_comp) {
            return a->getBounds().getUnion(b_comp->getBounds())
                       .withY(labelRow.getY()).withHeight(kLabelH);
        };
        in_label_->setBounds(makeLabel(in_l_, in_r_));
        out_label_->setBounds(makeLabel(out_l_, out_r_));
        vol_label_->setBounds(volume_->getBounds().withY(labelRow.getY()).withHeight(kLabelH));

        // Mute button aligned under the volume slider
        auto vol_col = volume_->getBounds().withY(mute_area.getY()).withHeight(kButtonH);
        mute_button_->setBounds(vol_col);
    }

private:
    juce::Label* in_label_;
    MeterPanel* in_l_;
    MeterPanel* in_r_;
    juce::Label* out_label_;
    MeterPanel* out_l_;
    MeterPanel* out_r_;
    juce::Label* vol_label_;
    juce::Slider* volume_;
    juce::TextButton* mute_button_;
};

class HeaderComponent : public juce::Component
{
public:
    HeaderComponent(juce::ImageComponent* logo, juce::Label* title,
                    ResetButton* reset, PowerButton* audio_toggle,
                    SettingsButton* about, juce::Button* energy_saver, juce::TextButton* help)
        : logo_(logo), title_(title), reset_(reset), audio_toggle_(audio_toggle),
          about_(about), energy_saver_(energy_saver), help_(help)
    {
        addAndMakeVisible(logo_);
        addAndMakeVisible(title_);
        addAndMakeVisible(reset_);
        addAndMakeVisible(audio_toggle_);
        addAndMakeVisible(about_);
        addAndMakeVisible(energy_saver_);
        addAndMakeVisible(help_);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add(juce::FlexItem(*logo_).withMinWidth(36).withWidth(36).withMinHeight(36).withHeight(36));
        fb.items.add(juce::FlexItem().withWidth(8));
        fb.items.add(juce::FlexItem(*title_).withFlex(1).withMinHeight(28));
        fb.items.add(juce::FlexItem().withWidth(8));
        fb.items.add(juce::FlexItem(*reset_).withMinWidth(40).withWidth(40).withMinHeight(28).withHeight(28));
        fb.items.add(juce::FlexItem().withWidth(6));
        fb.items.add(juce::FlexItem(*audio_toggle_).withMinWidth(40).withWidth(40).withMinHeight(28).withHeight(28));
        fb.items.add(juce::FlexItem().withWidth(6));
        fb.items.add(juce::FlexItem(*about_).withMinWidth(40).withWidth(40).withMinHeight(28).withHeight(28));
        fb.items.add(juce::FlexItem().withWidth(6));
        // Green leaf toggle, sits immediately left of the "?" button.
        fb.items.add(juce::FlexItem(*energy_saver_).withMinWidth(34).withWidth(34).withMinHeight(28).withHeight(28));
        fb.items.add(juce::FlexItem().withWidth(6));
        fb.items.add(juce::FlexItem(*help_).withMinWidth(40).withWidth(40).withMinHeight(28).withHeight(28));
        fb.performLayout(b);
    }

private:
    juce::ImageComponent* logo_;
    juce::Label* title_;
    ResetButton* reset_;
    PowerButton* audio_toggle_;
    SettingsButton* about_;
    juce::Button* energy_saver_;
    juce::TextButton* help_;
};


// Two-column body: the wide left column stacks AUDIO DEVICE over PLUGIN CHAIN,
// the narrow right column stacks LEVELS over SPECTRUM. Panels in a column share
// its width, so the chain editor lines up with the device panel and the analyser
// with the meters.
class BodyComponent : public juce::Component
{
public:
    BodyComponent(juce::Component* device_panel, juce::Component* chain_panel,
                  juce::Component* meter_panel, juce::Component* spectrum_panel)
        : device_panel_(device_panel), chain_panel_(chain_panel),
          meter_panel_(meter_panel), spectrum_panel_(spectrum_panel)
    {
        addAndMakeVisible(device_panel_);
        addAndMakeVisible(chain_panel_);
        addAndMakeVisible(meter_panel_);
        addAndMakeVisible(spectrum_panel_);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (b.getWidth() <= 0 || b.getHeight() <= 0)
            return;

        // 2:1 column split — the same proportion the device/levels row used before
        // the chain editor and analyser joined them.
        const int right_w = (b.getWidth() - kGap) / 3;
        auto left = b.removeFromLeft(b.getWidth() - kGap - right_w);
        b.removeFromLeft(kGap);
        auto right = b;

        // The top row keeps the fixed height it had as a full-width row; the chain
        // editor and analyser below share whatever the window leaves.
        const int top_h = kTopRowH;

        auto device_area = left.removeFromTop(top_h);
        left.removeFromTop(kGap);
        device_panel_->setBounds(device_area);
        chain_panel_->setBounds(left);

        auto meter_area = right.removeFromTop(top_h);
        right.removeFromTop(kGap);
        meter_panel_->setBounds(meter_area);
        spectrum_panel_->setBounds(right);
    }

    static constexpr int kGap = 8;
    static constexpr int kTopRowH = 320;
    static constexpr int kMinBottomH = 100;

private:
    juce::Component* device_panel_;
    juce::Component* chain_panel_;
    juce::Component* meter_panel_;
    juce::Component* spectrum_panel_;
};

class ChainEditorPanel : public GlassPanel
{
public:
    explicit ChainEditorPanel(juce::Component* editor) : editor_(editor)
    {
        setSectionTitle("PLUGIN CHAIN");
        addAndMakeVisible(editor_);
    }

    void resized() override
    {
        editor_->setBounds(contentBounds());
    }

private:
    juce::Component* editor_;
};

class SpectrumPanel : public GlassPanel
{
public:
    explicit SpectrumPanel(juce::Component* analyzer) : analyzer_(analyzer)
    {
        setSectionTitle("SPECTRUM");
        addAndMakeVisible(analyzer_);
    }

    void resized() override
    {
        analyzer_->setBounds(contentBounds());
    }

private:
    juce::Component* analyzer_;
};

// Simple horizontal progress bar for CPU display. Fills with green proportional
// to the current CPU usage percentage (0 .. 100).
class CpuBar : public juce::Component
{
public:
    void setValue(float percent)
    {
        percent_ = juce::jlimit(0.0f, 100.0f, percent);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(0.5f);

        // Background track
        g.setColour(juce::Colour(0xFF1A2340));
        g.fillRoundedRectangle(area, 4.0f);

        // Green fill
        float fill_w = area.getWidth() * (percent_ / 100.0f);
        if (fill_w > 0.0f)
        {
            auto fill = area.withWidth(fill_w);
            g.setColour(juce::Colour(0xFF66BB6A));
            g.fillRoundedRectangle(fill, 4.0f);
        }

        // Border
        g.setColour(juce::Colour(0xFF1E2A4A));
        g.drawRoundedRectangle(area, 4.0f, 1.0f);
    }

private:
    float percent_ = 0.0f;
};

class StatusBarComponent : public juce::Component
{
public:
    StatusBarComponent(juce::Label* status, juce::Label* latency, juce::Label* cpu_label, juce::Component* cpu_bar)
        : status_(status), latency_(latency), cpu_label_(cpu_label), cpu_bar_(cpu_bar)
    {
        addAndMakeVisible(status_);
        addAndMakeVisible(latency_);
        addAndMakeVisible(cpu_label_);
        addAndMakeVisible(cpu_bar_);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::stretch;
        fb.items.add(juce::FlexItem(*status_).withFlex(1));
        fb.items.add(juce::FlexItem().withWidth(8));
        fb.items.add(juce::FlexItem(*latency_).withMinWidth(270).withWidth(270));
        fb.items.add(juce::FlexItem().withWidth(8));
        fb.items.add(juce::FlexItem(*cpu_label_).withMinWidth(30).withWidth(30));
        fb.items.add(juce::FlexItem().withWidth(4));
        fb.items.add(juce::FlexItem(*cpu_bar_).withMinWidth(140).withWidth(140));
        fb.performLayout(b);
    }

private:
    juce::Label* status_;
    juce::Label* latency_;
    juce::Label* cpu_label_;
    juce::Component* cpu_bar_;
};

} // namespace

// ============================================================================
// MainContentComponent
// ============================================================================

class MainContentComponent : public juce::Component
{
public:
    MainContentComponent(juce::Component* header,
                         juce::Component* body,
                         juce::Component* plugin_panel,
                         juce::Component* status_bar)
        : header_(header), body_(body),
          plugin_panel_(plugin_panel), status_bar_(status_bar)
    {
        addAndMakeVisible(header_);
        addAndMakeVisible(body_);
        addAndMakeVisible(plugin_panel_);
        addAndMakeVisible(status_bar_);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(12);

        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::column;
        fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        fb.alignItems = juce::FlexBox::AlignItems::stretch;

        fb.items.add(juce::FlexItem(*header_).withMinHeight(48).withHeight(48));
        fb.items.add(juce::FlexItem().withHeight(8));
        fb.items.add(juce::FlexItem(*body_).withMinHeight(BodyComponent::kTopRowH
                                                          + BodyComponent::kGap
                                                          + BodyComponent::kMinBottomH)
                                            .withFlex(1.0f));
        fb.items.add(juce::FlexItem().withHeight(8));
        fb.items.add(juce::FlexItem(*plugin_panel_).withMinHeight(44).withHeight(44));
        fb.items.add(juce::FlexItem().withHeight(8));
        fb.items.add(juce::FlexItem(*status_bar_).withMinHeight(28).withHeight(28));

        fb.performLayout(b);
    }

private:
    juce::Component* header_;
    juce::Component* body_;
    juce::Component* plugin_panel_;
    juce::Component* status_bar_;
};

// ============================================================================
// MainWindow implementation
// ============================================================================

// =====================================================================
// LeafButton — Energy Saver toggle
// =====================================================================

LeafButton::LeafButton() : juce::Button("EnergySaver")
{
    setTooltip("Energy Saver: off");
}

void LeafButton::setEnergyState(bool enabled, bool sleeping)
{
    enabled_ = enabled;
    sleeping_ = sleeping;
    setTooltip(enabled_ ? (sleeping_ ? "Energy Saver: sleeping (engine idle)"
                                     : "Energy Saver: on (monitoring)")
                        : "Energy Saver: off");
    repaint();
}

void LeafButton::paintButton(juce::Graphics& g, bool shouldDrawHighlighted, bool /*shouldDrawDown*/)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);

    const juce::Colour leaf_on = kIconActiveGreen;   // active green (shared with power icon)
    const juce::Colour leaf_sleep {0xFF69F0AE};      // brighter mint while sleeping
    const juce::Colour leaf_off = kIconInactiveGrey; // muted grey when disabled (shared)

    juce::Colour col = enabled_ ? (sleeping_ ? leaf_sleep : leaf_on) : leaf_off;
    if (shouldDrawHighlighted)
        col = col.brighter(0.25f);

    // A symmetric pointed-oval leaf with a central midrib, drawn vertically.
    const float h = area.getHeight();
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float top = area.getY() + h * 0.10f;
    const float bottom = area.getBottom() - h * 0.10f;
    const float half = area.getWidth() * 0.32f;

    juce::Path leaf;
    leaf.startNewSubPath(cx, top);
    leaf.quadraticTo(cx + half, cy, cx, bottom);
    leaf.quadraticTo(cx - half, cy, cx, top);
    leaf.closeSubPath();

    const float fill_alpha = enabled_ ? (sleeping_ ? 0.55f : 0.28f) : 0.0f;
    if (fill_alpha > 0.0f)
    {
        g.setColour(col.withAlpha(fill_alpha));
        g.fillPath(leaf);
    }
    g.setColour(col);
    g.strokePath(leaf, juce::PathStrokeType(1.8f));

    juce::Path rib;
    rib.startNewSubPath(cx, top + h * 0.04f);
    rib.lineTo(cx, bottom - h * 0.04f);
    g.strokePath(rib, juce::PathStrokeType(1.2f));
}

PowerButton::PowerButton() : juce::TextButton("ON")
{
    setTooltip("Start/Stop audio");
    setClickingTogglesState(true);
    setToggleState(false, juce::dontSendNotification);
}

void PowerButton::setPowerState(bool running)
{
    setToggleState(running, juce::dontSendNotification);
    setButtonText(running ? "ON" : "OFF");
}

ResetButton::ResetButton() : juce::Button("Reset")
{
    setTooltip("Reset engine");
}

void ResetButton::paintButton(juce::Graphics& g, bool shouldDrawHighlighted, bool /*shouldDrawDown*/)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);

    juce::Colour col = kIconInactiveGrey;
    if (shouldDrawHighlighted)
        col = col.brighter(0.25f);

    // Circular arrow (reset symbol) ↻
    const float radius = std::min(area.getWidth(), area.getHeight()) * 0.38f;
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();

    // Draw an arc (3/4 circle)
    juce::Path arc;
    arc.addCentredArc(cx, cy, radius, radius, 0.0f, juce::MathConstants<float>::pi * 0.5f,
                      juce::MathConstants<float>::pi * 2.0f, true);
    g.setColour(col);
    g.strokePath(arc, juce::PathStrokeType(1.8f));

    // Arrow head at the end (pointing up-right)
    const float arrow_size = radius * 0.35f;
    const float arrow_x = cx + radius * 0.707f;  // cos(45°)
    const float arrow_y = cy - radius * 0.707f;  // -sin(45°)

    juce::Path arrow;
    arrow.startNewSubPath(arrow_x, arrow_y);
    arrow.lineTo(arrow_x - arrow_size * 0.5f, arrow_y + arrow_size * 0.35f);
    arrow.lineTo(arrow_x - arrow_size * 0.35f, arrow_y - arrow_size * 0.5f);
    arrow.closeSubPath();

    g.fillPath(arrow);
}

SettingsButton::SettingsButton() : juce::Button("Settings")
{
    setTooltip("Settings");
}

void SettingsButton::paintButton(juce::Graphics& g, bool shouldDrawHighlighted, bool /*shouldDrawDown*/)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);

    juce::Colour col = kIconInactiveGrey;
    if (shouldDrawHighlighted)
        col = col.brighter(0.25f);

    // Gear/cog icon
    const float radius = std::min(area.getWidth(), area.getHeight()) * 0.35f;
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float tooth_h = radius * 0.25f;
    const int num_teeth = 8;

    // Draw main circle
    g.setColour(col);
    g.drawEllipse(juce::Rectangle<float>(cx - radius * 0.6f, cy - radius * 0.6f,
                                         radius * 1.2f, radius * 1.2f), 1.8f);

    // Draw teeth around the circle
    for (int i = 0; i < num_teeth; ++i)
    {
        const float angle = (juce::MathConstants<float>::twoPi / num_teeth) * i;
        const float cos_a = std::cos(angle);
        const float sin_a = std::sin(angle);

        const float inner_x = cx + cos_a * radius * 0.6f;
        const float inner_y = cy + sin_a * radius * 0.6f;
        const float outer_x = cx + cos_a * (radius + tooth_h);
        const float outer_y = cy + sin_a * (radius + tooth_h);

        g.drawLine(inner_x, inner_y, outer_x, outer_y, 1.5f);
    }

    // Draw center hole
    g.drawEllipse(juce::Rectangle<float>(cx - radius * 0.25f, cy - radius * 0.25f,
                                         radius * 0.5f, radius * 0.5f), 1.2f);
}

MainWindow::MainWindow(std::unique_ptr<IAudioEngine> engine)
    // MUST stay identical to kWindowTitle in main.cpp - the single-instance check
    // locates a running instance by exact window caption via FindWindowW.
    : juce::DocumentWindow("Global VST Host", kBgDeep, juce::DocumentWindow::closeButton)
    , engine_(std::move(engine))
{
    StartupLog("constructor start");
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(600, 480, 1920, 1080);
    setSize(700, 580);

    buildUI();
    StartupLog("buildUI done");

    engine_->setListener(this);
    StartupLog("setListener done");
    refreshDeviceLists();
    StartupLog("refreshDeviceLists done");
    chain_editor_->refreshFromEngine();
    StartupLog("chain_editor refreshFromEngine done");

    // Default buffer size and fallback sample rate — overridden by autosave below if
    // present. The VST chain now auto-follows the output device rate; this fallback is
    // only used when no WASAPI output endpoint is available.
    buffer_selector_->setSelectedId(512, juce::dontSendNotification);
    engine_->setBufferSize(512);
    engine_->setSampleRate(48000.0);

    // Restore last session state (device selections, plugin chain, running state).
    restoreFromAutosave();
    StartupLog("restoreFromAutosave done");

    // If no chain was restored, automatically load EQ, Volume Leveler, and Compressor plugins in bypass mode.
    if (engine_->snapshotChain().slots.empty())
    {
        PluginRef nighttime_ref;
        nighttime_ref.vendor = "JyGlobalVST";
        nighttime_ref.name = "Volume Leveler";

        PluginRef eq_ref;
        eq_ref.vendor = "JyGlobalVST";
        eq_ref.name = "Equalizer";

        PluginRef vl_ref;
        vl_ref.vendor = "JyGlobalVST";
        vl_ref.name = "Compressor";

        engine_->addPlugin(nighttime_ref, 0);
        engine_->addPlugin(eq_ref, 1);
        engine_->addPlugin(vl_ref, 2);
        engine_->setBypass(0, true);
        engine_->setBypass(1, true);
        engine_->setBypass(2, true);
        StartupLog("Default plugins loaded (EQ, Volume Leveler, and Compressor in bypass mode)");
    }

    // T034: If autosave did not restore endpoint selections, fall back to roaming settings.
    {
        auto rs = roaming_settings_store_.load();
        bool input_restored = false;
        bool output_restored = false;

        const EndpointId current_input = engine_->currentInput();
        const EndpointId current_output = engine_->currentOutput();

        // Check if input was restored by autosave (non-empty and present in list).
        if (!current_input.empty())
        {
            for (const auto& id : input_endpoint_ids_)
            {
                if (id == current_input) { input_restored = true; break; }
            }
        }
        // Check if output was restored by autosave.
        if (!current_output.empty())
        {
            for (const auto& id : output_endpoint_ids_)
            {
                if (id == current_output) { output_restored = true; break; }
            }
            if (!output_restored)
            {
                for (const auto& name : asio_device_names_)
                {
                    if (name == current_output) { output_restored = true; break; }
                }
            }
        }

        if (!input_restored && rs.capture_endpoint_id.has_value())
        {
            const EndpointId remembered = *rs.capture_endpoint_id;
            for (int i = 0; i < static_cast<int>(input_endpoint_ids_.size()); ++i)
            {
                if (input_endpoint_ids_[i] == remembered)
                {
                    input_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
                    engine_->selectInput(remembered);
                    input_restored = true;
                    break;
                }
            }
            // Graceful degradation: if remembered ID is no longer present, leave unset.
        }

        if (!output_restored && rs.output_endpoint_id.has_value())
        {
            const EndpointId remembered = *rs.output_endpoint_id;
            for (int i = 0; i < static_cast<int>(output_endpoint_ids_.size()); ++i)
            {
                if (output_endpoint_ids_[i] == remembered)
                {
                    output_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
                    engine_->selectOutput(remembered);
                    output_restored = true;
                    break;
                }
            }
            // Graceful degradation: if remembered ID is no longer present, leave unset.
        }
    }

    installPowerHandler();
    StartupLog("installPowerHandler done");

    startTimerHz(kTimerHz);
    StartupLog("startTimerHz done");

    // Load icon from binary data for tray and UI use.
    app_icon_image_ = juce::ImageCache::getFromMemory(
        jyglobalvst::BinaryData::app_icon_png,
        jyglobalvst::BinaryData::app_icon_pngSize);

    // Restore saved window state AFTER making visible (window needs a peer).
    setVisible(true);
    WindowState ws = local_state_store_.loadWindowState();
    setSize(ws.width, ws.height);
    setTopLeftPosition(ws.x, ws.y);
    if (ws.maximized)
    {
        setFullScreen(true);
    }

    initializing_ = false;
    createTrayIcon();
    StartupLog("createTrayIcon done");

    // Apply "start minimized to tray" setting.
    auto rs = roaming_settings_store_.load();
    if (rs.start_minimized_to_tray)
    {
        setVisible(false);
    }

    // Restore the Energy Saver preference and reflect it on the leaf button.
    engine_->setEnergySaverEnabled(rs.energy_saver_enabled);
    updateEnergySaverVisual();

    // Start a background plugin scan
    status_label_->setText("Scanning plugins...", juce::dontSendNotification);
    scan_dialog_ = std::make_unique<ScanDialog>(engine_.get());
    StartupLog("scan_dialog created");
    engine_->rescanPlugins(scan_dialog_.get());
    StartupLog("rescanPlugins started");
}

MainWindow::~MainWindow()
{
    StartupLog("destructor start");
    // Cancel any in-progress scan before destroying the scan dialog.
    // The scanner thread holds a pointer to scan_dialog_, so we must ensure
    // the scanner is fully stopped before scan_dialog_ is destroyed.
    engine_->cancelScan();
    StartupLog("cancelScan done");

    // Always save window state on exit, regardless of initialization flag
    WindowState ws;
    ws.x = getX();
    ws.y = getY();
    ws.width = getWidth();
    ws.height = getHeight();
    ws.maximized = isFullScreen();

    // Debug: verify values are reasonable before saving
    if (ws.width > 0 && ws.height > 0)
    {
        local_state_store_.saveWindowState(ws);
    }

    destroyTrayIcon();
    removePowerHandler();
    stopTimer();
    autosave_store_.write(engine_.get(), preset_override_flag_,
                          static_cast<int>(custom_laf_.currentTheme()));
    engine_->setListener(nullptr);
    if (audio_running_)
    {
        engine_->stop();
    }
    if (window_icon_big_)
        DestroyIcon(window_icon_big_);
    if (window_icon_small_)
        DestroyIcon(window_icon_small_);
    StartupLog("destructor end");
}

void MainWindow::closeButtonPressed()
{
    // Save window state before minimizing to tray
    WindowState ws;
    ws.x = getX();
    ws.y = getY();
    ws.width = getWidth();
    ws.height = getHeight();
    ws.maximized = isFullScreen();
    if (ws.width > 0 && ws.height > 0)
    {
        local_state_store_.saveWindowState(ws);
    }

    // Minimize to tray rather than quitting.
    setVisible(false);
}

void MainWindow::visibilityChanged()
{
    // Load icon from resource (IDI_ICON1) when window becomes visible.
    if (isVisible() && window_icon_big_ == nullptr)
    {
        if (auto* peer = getPeer())
        {
            if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
            {
                // Load icon from resource
                HICON hicon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
                if (hicon)
                {
                    window_icon_big_ = hicon;
                    SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hicon));
                    SetClassLongPtr(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hicon));
                }
            }
        }
    }
}

void MainWindow::moved()
{
    juce::DocumentWindow::moved();
    saveWindowStateIfNeeded();
}

void MainWindow::resized()
{
    juce::DocumentWindow::resized();
    saveWindowStateIfNeeded();
}

void MainWindow::saveWindowStateIfNeeded()
{
    if (initializing_)
        return;

    WindowState ws;
    ws.x = getX();
    ws.y = getY();
    ws.width = getWidth();
    ws.height = getHeight();
    ws.maximized = isFullScreen();

    local_state_store_.saveWindowState(ws);
}

// =========================================================================
// System tray
// =========================================================================

void MainWindow::createTrayIcon()
{
    if (tray_icon_created_)
        return;

    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr)
        return;

    // Try to load tray icon from resource first
    if (tray_hicon_ == nullptr)
    {
        HMODULE hMod = GetModuleHandleW(nullptr);
        // Load the small (16x16) icon from resource ID 1
        tray_hicon_ = static_cast<HICON>(LoadImageW(hMod, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    }

    // Fallback: Create from PNG image if resource load fails
    if (tray_hicon_ == nullptr && app_icon_image_.isValid())
    {
        tray_hicon_ = createHICONFromJuceImage(app_icon_image_, 16, 16);
    }

    // Grayscale variant shown while the engine is paused (stopped or asleep).
    // Built from the PNG art; if that is unavailable the paused state simply
    // keeps the colour icon.
    if (tray_hicon_gray_ == nullptr && app_icon_image_.isValid())
    {
        tray_hicon_gray_ = createHICONFromJuceImage(app_icon_image_, 16, 16, /*grayscale=*/true);
    }

    // Pick the icon that matches the current engine state up front so it never
    // flashes colour before the first state refresh.
    const bool active = audio_running_ && !engine_->isEnergySaverSleeping();
    HICON initial = active ? tray_hicon_ : (tray_hicon_gray_ != nullptr ? tray_hicon_gray_ : tray_hicon_);

    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayIconMsg;
    nid.hIcon  = initial != nullptr ? initial : LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Global VST Host");

    if (Shell_NotifyIcon(NIM_ADD, &nid))
    {
        tray_icon_created_ = true;
        tray_icon_active_shown_ = active;
    }
}

// Switch the tray icon between colour (actively processing) and grayscale
// (paused: audio stopped or Energy-Saver sleeping). Cheap no-op when the shown
// state already matches, so it is safe to call from any state change.
void MainWindow::updateTrayIconAppearance()
{
    if (!tray_icon_created_)
        return;

    const bool active = audio_running_ && !engine_->isEnergySaverSleeping();
    if (active == tray_icon_active_shown_)
        return;

    HICON icon = active ? tray_hicon_ : (tray_hicon_gray_ != nullptr ? tray_hicon_gray_ : tray_hicon_);
    if (icon == nullptr)
        return;

    auto* peer = getPeer();
    if (peer == nullptr)
        return;
    HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr)
        return;

    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = kTrayIconId;
    nid.uFlags = NIF_ICON;
    nid.hIcon  = icon;
    if (Shell_NotifyIcon(NIM_MODIFY, &nid))
    {
        tray_icon_active_shown_ = active;
    }
}

void MainWindow::destroyTrayIcon()
{
    if (!tray_icon_created_)
        return;

    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr)
        return;

    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = kTrayIconId;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    tray_icon_created_ = false;

    if (tray_menu_ != nullptr)
    {
        DestroyMenu(tray_menu_);
        tray_menu_ = nullptr;
    }

    if (tray_hicon_ != nullptr)
    {
        DestroyIcon(tray_hicon_);
        tray_hicon_ = nullptr;
    }

    if (tray_hicon_gray_ != nullptr)
    {
        DestroyIcon(tray_hicon_gray_);
        tray_hicon_gray_ = nullptr;
    }
}

void MainWindow::updateTrayIconTooltip(const juce::String& text)
{
    if (!tray_icon_created_)
        return;

    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr)
        return;

    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = kTrayIconId;
    nid.uFlags = NIF_TIP;
    wcscpy_s(nid.szTip, text.toWideCharPointer());
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void MainWindow::showTrayContextMenu()
{
    if (tray_menu_ != nullptr)
    {
        DestroyMenu(tray_menu_);
    }

    tray_menu_ = CreatePopupMenu();

    auto rs = roaming_settings_store_.load();
    UINT flags = rs.start_minimized_to_tray ? MF_CHECKED : MF_UNCHECKED;
    AppendMenu(tray_menu_, MF_STRING | flags, 4, L"Start minimized to tray");
    AppendMenu(tray_menu_, MF_SEPARATOR, 0, nullptr);

    AppendMenu(tray_menu_, MF_STRING, 1, L"Show Global VST Host");
    AppendMenu(tray_menu_, MF_SEPARATOR, 0, nullptr);

    if (audio_running_)
    {
        AppendMenu(tray_menu_, MF_STRING, 2, L"Stop Audio");
    }
    else
    {
        AppendMenu(tray_menu_, MF_STRING, 2, L"Start Audio");
    }

    AppendMenu(tray_menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenu(tray_menu_, MF_STRING, 3, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(static_cast<HWND>(getPeer()->getNativeHandle()));
    int cmd = TrackPopupMenu(tray_menu_, TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, 0,
                             static_cast<HWND>(getPeer()->getNativeHandle()), nullptr);

    switch (cmd)
    {
    case 1:
        restoreFromTray();
        break;
    case 2:
        setAudioRunning(!audio_running_);
        break;
    case 3:
        quitFromTray();
        break;
    case 4:
        rs.start_minimized_to_tray = !rs.start_minimized_to_tray;
        roaming_settings_store_.save(rs);
        break;
    }
}

namespace {

float linearToDb(float linear)
{
    if (linear <= 0.0f)
        return -120.0f;
    return 20.0f * std::log10(linear);
}

}  // namespace

void MainWindow::showTrayVolumePopup()
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const float gain = (volume_slider_ != nullptr)
                           ? static_cast<float>(volume_slider_->getValue())
                           : 1.0f;

    auto content = std::make_unique<TrayVolumeContent>(
        gain, audio_running_,
        [this](float g) { applyMasterVolume(g, true); },
        [this]() -> bool
        {
            setAudioRunning(!audio_running_);
            return audio_running_;
        },
        [this]()
        {
            if (volume_slider_ == nullptr)
                return;
            const float current = static_cast<float>(volume_slider_->getValue());
            if (current > 0.0f)
            {
                pre_mute_volume_ = current;
                applyMasterVolume(0.0f, true);
            }
            else
            {
                applyMasterVolume(pre_mute_volume_, true);
            }
        },
        [this]() { toggleEqBypass(); },
        [this]() { toggleNtBypass(); },
        hasEqPlugin(),
        hasNtPlugin());
    content->setLookAndFeel(&custom_laf_);

    // Set initial bypass states for EQ and NT plugins
    const auto chain = engine_->snapshotChain();
    {
        const int eq_pos = findPluginByUid(chain, engine::builtin::EQ_UID);
        if (eq_pos >= 0)
            content->setEqBypassed(chain.slots[eq_pos].is_bypassed);

        const int nt_pos = findPluginByUid(chain, engine::builtin::NIGHTTIME_UID);
        if (nt_pos >= 0)
            content->setNtBypassed(chain.slots[nt_pos].is_bypassed);
    }

    // Update initial meter levels (converted to dB)
    {
        std::lock_guard<std::mutex> lock(meter_frame_mutex_);
        const auto& frame = latest_meter_frame_;
        content->setMeterLevels(
            linearToDb(frame.input_peak_l), linearToDb(frame.input_rms_l),
            linearToDb(frame.input_peak_r), linearToDb(frame.input_rms_r),
            linearToDb(frame.output_peak_l), linearToDb(frame.output_rms_l),
            linearToDb(frame.output_peak_r), linearToDb(frame.output_rms_r));
    }

    // Keep a safe pointer to update meters in timer callback; automatically nulls out
    // when the CallOutBox destroys the content on dismissal, avoiding a dangling pointer.
    tray_volume_popup_ = content.get();

    // Anchor the call-out at the tray icon itself (not just the cursor), converted
    // from physical to JUCE logical pixels so mixed-DPI multi-monitor setups pick the
    // correct display.  Shell_NotifyIconGetRect is Vista+ (fine for Win10 1909+).
    juce::Rectangle<int> anchor;
    {
        NOTIFYICONIDENTIFIER nidId = {};
        nidId.cbSize = sizeof(nidId);
        nidId.hWnd   = static_cast<HWND>(peer->getNativeHandle());
        nidId.uID    = kTrayIconId;
        RECT rc = {};
        if (SUCCEEDED(Shell_NotifyIconGetRect(&nidId, &rc)))
        {
            const juce::Rectangle<int> physical(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
            anchor = juce::Desktop::getInstance().getDisplays().physicalToLogical(physical);
        }
    }

    if (anchor.isEmpty())
    {
        // Fallback: cursor in logical coordinates (getMousePosition already accounts
        // for per-monitor DPI scale, unlike GetCursorPos which is physical).
        const auto mousePos = juce::Desktop::getMousePosition();
        anchor = juce::Rectangle<int>(mousePos.x, mousePos.y, 1, 1);
    }

    SetForegroundWindow(static_cast<HWND>(peer->getNativeHandle()));

    auto& box = juce::CallOutBox::launchAsynchronously(std::move(content), anchor, nullptr);
    box.setDismissalMouseClicksAreAlwaysConsumed(true);
}

void MainWindow::restoreFromTray()
{
    setVisible(true);
    toFront(true);
    refreshDeviceLists();
}

void MainWindow::quitFromTray()
{
    destroyTrayIcon();
    juce::MessageManager::callAsync([this]() {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });
}

bool MainWindow::handleTrayMessage(UINT msg, LPARAM /*wParam*/)
{
    switch (msg)
    {
    case WM_LBUTTONUP:
        showTrayVolumePopup();
        return true;
    case WM_LBUTTONDBLCLK:
        restoreFromTray();
        return true;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        showTrayContextMenu();
        return true;
    }
    return false;
}

// =========================================================================
// Helper methods for built-in effect plugins
// =========================================================================

bool MainWindow::hasEqPlugin() const
{
    const auto chain = engine_->snapshotChain();
    return findPluginByUid(chain, engine::builtin::EQ_UID) >= 0;
}

bool MainWindow::hasNtPlugin() const
{
    const auto chain = engine_->snapshotChain();
    return findPluginByUid(chain, engine::builtin::NIGHTTIME_UID) >= 0;
}

void MainWindow::toggleEqBypass()
{
    const auto chain = engine_->snapshotChain();
    const int pos = findPluginByUid(chain, engine::builtin::EQ_UID);
    if (pos >= 0)
    {
        engine_->setBypass(pos, !chain.slots[pos].is_bypassed);
    }
}

void MainWindow::toggleNtBypass()
{
    const auto chain = engine_->snapshotChain();
    const int pos = findPluginByUid(chain, engine::builtin::NIGHTTIME_UID);
    if (pos >= 0)
    {
        engine_->setBypass(pos, !chain.slots[pos].is_bypassed);
    }
}

// =========================================================================
// UI Construction
// =========================================================================

void MainWindow::buildUI()
{
    // --- Labels --------------------------------------------------------------
    transport_label_ = std::make_unique<juce::Label>(juce::String(), "Mode:");
    input_label_ = std::make_unique<juce::Label>(juce::String(), "Captured audio device:");
    output_label_ = std::make_unique<juce::Label>(juce::String(), "Output audio device:");
    asio_device_label_ = std::make_unique<juce::Label>(juce::String(), "ASIO Device:");
    asio_pair_label_ = std::make_unique<juce::Label>(juce::String(), "Channels:");
    buffer_label_ = std::make_unique<juce::Label>(juce::String(), "Buffer:");
    input_rate_caption_ = std::make_unique<juce::Label>(juce::String(), "Captured sampling rate:");
    output_rate_caption_ = std::make_unique<juce::Label>(juce::String(), "Output sampling rate:");
    vst_rate_caption_ = std::make_unique<juce::Label>(juce::String(), "VST sampling rate:");
    vol_label_ = std::make_unique<juce::Label>(juce::String(), "Vol:");

    // --- Transport mode selector -------------------------------------------
    transport_mode_selector_ = std::make_unique<juce::ComboBox>();
    transport_mode_selector_->addItem("WASAPI", static_cast<int>(TransportMode::Wasapi));
    transport_mode_selector_->addItem("ASIO", static_cast<int>(TransportMode::Asio));
    // NOTE: "WASAPI Exclusive" is intentionally not offered in the UI — the exclusive
    // path can fail to initialise on some devices and take down the app. The mode and
    // its handling code are kept below but deactivated; we always run shared WASAPI.
    // transport_mode_selector_->addItem("WASAPI Exclusive", static_cast<int>(TransportMode::WasapiExclusive));
    transport_mode_selector_->addListener(this);

    // --- Input selector ----------------------------------------------------
    input_selector_ = std::make_unique<juce::ComboBox>();
    input_selector_->addListener(this);

    // --- Output selector (WASAPI) ------------------------------------------
    output_selector_ = std::make_unique<juce::ComboBox>();
    output_selector_->addListener(this);

    // --- ASIO device selector ----------------------------------------------
    asio_device_selector_ = std::make_unique<juce::ComboBox>();
    asio_device_selector_->addListener(this);

    // --- ASIO output pair selector -----------------------------------------
    asio_pair_selector_ = std::make_unique<juce::ComboBox>();
    asio_pair_selector_->addListener(this);

    // --- ASIO settings button ----------------------------------------------
    asio_settings_button_ = std::make_unique<juce::TextButton>("ASIO Settings...");
    asio_settings_button_->addListener(this);

    // --- Buffer size -------------------------------------------------------
    buffer_selector_ = std::make_unique<juce::ComboBox>();
    buffer_selector_->addItem("32", 32);
    buffer_selector_->addItem("64", 64);
    buffer_selector_->addItem("128", 128);
    buffer_selector_->addItem("256", 256);
    buffer_selector_->addItem("512", 512);
    buffer_selector_->addItem("1024", 1024);
    buffer_selector_->addListener(this);

    // --- Output sample-rate readout (hardware-negotiated, read-only) ----------
    output_rate_value_ = std::make_unique<juce::Label>(juce::String(), juce::String::fromUTF8("\xe2\x80\x94"));

    // --- Input / VST rate readouts -------------------------------------------
    input_rate_value_ = std::make_unique<juce::Label>(juce::String(), juce::String::fromUTF8("\xe2\x80\x94"));

    // --- VST sample-rate selector (user-selected chain rate) -------------------
    vst_rate_selector_ = std::make_unique<juce::ComboBox>();
    for (int rate : {44100, 48000, 88200, 96000, 176400, 192000})
    {
        vst_rate_selector_->addItem(juce::String(rate) + " Hz", rate);
    }
    // Reflect the engine's current desired rate (falls back to 48 kHz if unset).
    {
        const int desired = static_cast<int>(engine_->sampleRate());
        vst_rate_selector_->setSelectedId(desired > 0 ? desired : 48000,
                                             juce::dontSendNotification);
    }
    vst_rate_selector_->addListener(this);

    // --- Audio toggle (Power icon) ----------------------------------------
    audio_toggle_ = std::make_unique<PowerButton>();
    audio_toggle_->addListener(this);

    // --- Master volume slider ----------------------------------------------
    volume_slider_ = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                      juce::Slider::NoTextBox);
    volume_slider_->setRange(0.0, 1.0, 0.0);
    // Restore the persisted master volume from the previous session.
    const float persisted_volume = roaming_settings_store_.load().master_volume;
    volume_slider_->setValue(persisted_volume, juce::dontSendNotification);
    pre_mute_volume_ = persisted_volume;
    volume_slider_->setNumDecimalPlacesToDisplay(2);
    volume_slider_->addListener(this);
    engine_->setMasterVolume(persisted_volume);

    // --- Mute button -------------------------------------------------------
    mute_button_ = std::make_unique<juce::TextButton>();
    mute_button_->addListener(this);
    updateMuteButtonVisual(persisted_volume <= 0.0f);

    // --- Reset engine button (Icon) -----------------------------------------------
    reset_engine_button_ = std::make_unique<ResetButton>();
    reset_engine_button_->addListener(this);

    // --- Save / Load preset ------------------------------------------------
    save_preset_button_ = std::make_unique<juce::TextButton>("Save Preset...");
    save_preset_button_->addListener(this);

    load_preset_button_ = std::make_unique<juce::TextButton>("Load Preset...");
    load_preset_button_->addListener(this);

    // --- Chain editor ------------------------------------------------------
    chain_editor_ = std::make_unique<ChainEditor>(engine_.get());
    chain_editor_->onAddPluginRequested = [this]() { handleLoadPlugin(); };

    // --- Spectrum analyser -------------------------------------------------
    spectrum_analyzer_ = std::make_unique<SpectrumAnalyzer>(engine_.get());

    // --- Latency / CPU -----------------------------------------------------
    latency_label_ = std::make_unique<juce::Label>(juce::String{}, "Latency: — ms");
    cpu_label_ = std::make_unique<juce::Label>(juce::String{}, "CPU");
    cpu_bar_ = std::make_unique<CpuBar>();

    // --- Meters ------------------------------------------------------------
    meter_input_label_ = std::make_unique<juce::Label>(juce::String{}, "Cap");
    meter_input_l_ = std::make_unique<MeterPanel>();
    meter_input_r_ = std::make_unique<MeterPanel>();

    meter_output_label_ = std::make_unique<juce::Label>(juce::String{}, "Out");
    meter_output_l_ = std::make_unique<MeterPanel>();
    meter_output_r_ = std::make_unique<MeterPanel>();

    // --- Status label ------------------------------------------------------
    status_label_ = std::make_unique<juce::Label>(juce::String{}, "Ready");
    status_label_->setColour(juce::Label::textColourId, kTextDim);

    // --- Theme selector ----------------------------------------------------
    theme_label_ = std::make_unique<juce::Label>(juce::String(), "Theme:");
    theme_selector_ = std::make_unique<juce::ComboBox>();
    theme_selector_->addItem("Neon Blue",   static_cast<int>(CustomLookAndFeel::ThemeId::NeonBlue));
    theme_selector_->addItem("Neon Purple", static_cast<int>(CustomLookAndFeel::ThemeId::NeonPurple));
    theme_selector_->addItem("Neon Green",  static_cast<int>(CustomLookAndFeel::ThemeId::NeonGreen));
    theme_selector_->addItem("Neon Orange", static_cast<int>(CustomLookAndFeel::ThemeId::NeonOrange));
    theme_selector_->addItem("Neon Red",    static_cast<int>(CustomLookAndFeel::ThemeId::NeonRed));
    theme_selector_->addItem("Monochrome",  static_cast<int>(CustomLookAndFeel::ThemeId::Mono));
    theme_selector_->setSelectedId(static_cast<int>(CustomLookAndFeel::ThemeId::NeonBlue),
                                   juce::dontSendNotification);
    theme_selector_->addListener(this);

    // --- Start minimized to tray toggle ----------------------------------
    start_minimized_label_ = std::make_unique<juce::Label>(juce::String(), "Start minimized:");
    start_minimized_button_ = std::make_unique<juce::ToggleButton>("to tray");
    auto rs = roaming_settings_store_.load();
    start_minimized_button_->setToggleState(rs.start_minimized_to_tray, juce::dontSendNotification);
    start_minimized_button_->addListener(this);

    // --- Tooltips toggle ---------------------------------------------------
    tooltips_label_ = std::make_unique<juce::Label>(juce::String(), "Tooltips:");
    tooltips_button_ = std::make_unique<juce::ToggleButton>("enabled");
    tooltips_button_->setToggleState(rs.tooltips_enabled, juce::dontSendNotification);
    tooltips_button_->addListener(this);

    // --- Tooltip window (global) -------------------------------------------
    tooltip_window_ = std::make_unique<juce::TooltipWindow>(this, 800);

    // --- Settings button (Gear icon) ----------------------------------------
    about_button_ = std::make_unique<SettingsButton>();
    about_button_->addListener(this);

    // --- Energy Saver leaf toggle -----------------------------------------
    energy_saver_button_ = std::make_unique<LeafButton>();
    energy_saver_button_->addListener(this);

    // --- Help button -------------------------------------------------------
    help_button_ = std::make_unique<juce::TextButton>("?");
    help_button_->addListener(this);

    // --- Header logo -------------------------------------------------------
    auto* logo_comp = new juce::ImageComponent();
    if (app_icon_image_.isValid())
    {
        logo_comp->setImage(app_icon_image_);
    }
    logo_comp->setSize(36, 36);

    title_label_ = new juce::Label(juce::String(), "Global VST Host");
    juce::FontOptions fontOpts;
    juce::Font titleFont{fontOpts};
    titleFont.setHeight(20.0f);
    titleFont.setBold(true);
    title_label_->setFont(titleFont);
    title_label_->setColour(juce::Label::textColourId, kAccentCyan);

    auto* header = new HeaderComponent(logo_comp, title_label_,
                                       reset_engine_button_.get(), audio_toggle_.get(),
                                       about_button_.get(), energy_saver_button_.get(),
                                       help_button_.get());

    // --- Panels ------------------------------------------------------------
    auto* device_panel = new DevicePanel(
        transport_label_.get(), transport_mode_selector_.get(),
        input_label_.get(), input_selector_.get(),
        input_rate_caption_.get(), input_rate_value_.get(),
        output_label_.get(), output_selector_.get(),
        buffer_label_.get(), buffer_selector_.get(),
        output_rate_caption_.get(), output_rate_value_.get(),
        vst_rate_caption_.get(), vst_rate_selector_.get(),
        asio_device_label_.get(), asio_device_selector_.get(),
        asio_pair_label_.get(), asio_pair_selector_.get(),
        asio_settings_button_.get());
    device_panel_ = device_panel;

    auto* plugin_panel = new PluginButtonPanel(
        save_preset_button_.get(), load_preset_button_.get());

    auto* meter_panel = new MeterStatusPanel(
        meter_input_label_.get(), meter_input_l_.get(), meter_input_r_.get(),
        meter_output_label_.get(), meter_output_l_.get(), meter_output_r_.get(),
        vol_label_.get(), volume_slider_.get(), mute_button_.get());

    auto* chain_panel = new ChainEditorPanel(chain_editor_.get());
    auto* spectrum_panel = new SpectrumPanel(spectrum_analyzer_.get());

    auto* body = new BodyComponent(device_panel, chain_panel, meter_panel, spectrum_panel);

    auto* status_bar = new StatusBarComponent(
        status_label_.get(), latency_label_.get(), cpu_label_.get(), cpu_bar_.get());

    auto* content = new MainContentComponent(
        header, body, plugin_panel, status_bar);
    content_root_ = content;

    // Apply global LookAndFeel to the content tree.
    content->setLookAndFeel(&custom_laf_);

    setContentOwned(content, true);

    // Default mode.
    transport_mode_selector_->setSelectedId(static_cast<int>(TransportMode::Wasapi),
                                             juce::dontSendNotification);

    updateControlVisibility();
    applyTooltips();
}

void MainWindow::refreshDeviceLists()
{
    // Refresh transport mode from engine.
    const auto current_output = engine_->currentOutput();
    const auto outputs = engine_->listOutputs();

    // Determine current mode based on engine state.
    TransportMode mode = TransportMode::Wasapi;
    for (const auto& out : outputs)
    {
        if (out.endpoint_id == current_output && out.transport_kind == TransportKind::Asio)
        {
            mode = TransportMode::Asio;
            break;
        }
    }
    // WASAPI Exclusive is deactivated in the UI. If a previous session left the engine
    // in exclusive mode, force it back to shared WASAPI rather than surfacing a mode the
    // user can no longer select (and that may fail to initialise).
    if (engine_->wasapiExclusive())
        engine_->setWasapiExclusive(false);
    current_transport_mode_ = mode;
    transport_mode_selector_->setSelectedId(static_cast<int>(mode), juce::dontSendNotification);

    // Input devices (always WASAPI in testable-dev).
    input_endpoint_ids_.clear();
    input_selector_->clear(juce::dontSendNotification);

    // Add "Disconnected" option as the first input
    input_selector_->addItem("Disconnected", 1);
    input_endpoint_ids_.push_back(std::string(kDisconnectedDeviceId));

    const auto inputs = engine_->listInputs();
    int input_idx = 2;
    const auto current_input = engine_->currentInput();
    bool input_selected = false;

    for (const auto& in : inputs)
    {
        input_selector_->addItem(endpointDisplayName(in, true), input_idx);
        input_endpoint_ids_.push_back(in.endpoint_id);
        if (in.endpoint_id == current_input)
        {
            input_selector_->setSelectedItemIndex(input_idx - 1, juce::dontSendNotification);
            input_selected = true;
        }
        ++input_idx;
    }
    if (!input_selected)
    {
        // Select "Disconnected" if no input was selected, or the first available input
        if (current_input.empty())
        {
            input_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
        }
        else if (!inputs.empty())
        {
            input_selector_->setSelectedItemIndex(1, juce::dontSendNotification);
            engine_->selectInput(inputs[0].endpoint_id);
        }
    }

    // WASAPI output devices.
    output_endpoint_ids_.clear();
    output_selector_->clear(juce::dontSendNotification);

    // Add "Disconnected" option as the first output
    output_selector_->addItem("Disconnected", 1);
    output_endpoint_ids_.push_back(std::string(kDisconnectedDeviceId));

    int output_idx = 2;
    bool output_selected = false;

    for (const auto& out : outputs)
    {
        if (out.transport_kind != TransportKind::Wasapi)
            continue;
        output_selector_->addItem(endpointDisplayName(out, false), output_idx);
        output_endpoint_ids_.push_back(out.endpoint_id);
        if (out.endpoint_id == current_output)
        {
            output_selector_->setSelectedItemIndex(output_idx - 1, juce::dontSendNotification);
            output_selected = true;
        }
        else if (!output_selected && out.is_default && current_output.empty())
        {
            output_selector_->setSelectedItemIndex(output_idx - 1, juce::dontSendNotification);
            engine_->selectOutput(out.endpoint_id);
            output_selected = true;
        }
        ++output_idx;
    }
    if (!output_selected && current_output.empty())
    {
        // If no output is selected and current output is empty (disconnected), keep "Disconnected" selected
        output_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
    }

    // ASIO device list.
    asio_device_names_.clear();
    asio_device_selector_->clear(juce::dontSendNotification);
    int asio_idx = 1;
    for (const auto& out : outputs)
    {
        if (out.transport_kind != TransportKind::Asio)
            continue;
        asio_device_selector_->addItem(juce::String(out.friendly_name), asio_idx);
        asio_device_names_.push_back(out.endpoint_id);
        if (out.endpoint_id == current_output)
        {
            asio_device_selector_->setSelectedItemIndex(asio_idx - 1, juce::dontSendNotification);
        }
        ++asio_idx;
    }

    // ASIO output pair selector — populate with a conservative 8-channel max.
    refreshAsioPairSelector(8);
    const int current_pair = engine_->asioOutputPair() / 2;
    if (current_pair >= 0 && current_pair < asio_pair_selector_->getNumItems())
    {
        asio_pair_selector_->setSelectedItemIndex(current_pair, juce::dontSendNotification);
    }

    updateControlVisibility();
}

void MainWindow::buttonClicked(juce::Button* button)
{
    if (button == audio_toggle_.get())
    {
        setAudioRunning(!audio_running_);
    }
    else if (button == mute_button_.get())
    {
        const float current = static_cast<float>(volume_slider_->getValue());
        if (current > 0.0f)
        {
            pre_mute_volume_ = current;
            applyMasterVolume(0.0f, true);
            updateMuteButtonVisual(true);
        }
        else
        {
            applyMasterVolume(pre_mute_volume_, true);
            updateMuteButtonVisual(false);
        }
    }
    else if (button == save_preset_button_.get())
    {
        handleSavePreset();
    }
    else if (button == load_preset_button_.get())
    {
        handleLoadPreset();
    }
    else if (button == reset_engine_button_.get())
    {
        engine_->reset();
        audio_running_ = engine_->isRunning();
        audio_toggle_->setPowerState(audio_running_);
        status_label_->setText("Engine reset", juce::dontSendNotification);
        updateTrayIconAppearance();
    }
    else if (button == asio_settings_button_.get())
    {
        engine_->openAsioControlPanel();
        audio_running_ = engine_->isRunning();
        audio_toggle_->setPowerState(audio_running_);
        updateTrayIconAppearance();
    }
    else if (button == about_button_.get())
    {
        handleAbout();
    }
    else if (button == energy_saver_button_.get())
    {
        toggleEnergySaver();
    }
    else if (button == help_button_.get())
    {
        handleHelp();
    }
    else if (button == start_minimized_button_.get())
    {
        auto rs = roaming_settings_store_.load();
        rs.start_minimized_to_tray = start_minimized_button_->getToggleState();
        roaming_settings_store_.save(rs);
    }
    else if (button == tooltips_button_.get())
    {
        auto rs = roaming_settings_store_.load();
        rs.tooltips_enabled = tooltips_button_->getToggleState();
        roaming_settings_store_.save(rs);
        applyTooltips();
    }
}

void MainWindow::sliderValueChanged(juce::Slider* slider)
{
    if (slider == volume_slider_.get())
    {
        float new_vol = static_cast<float>(slider->getValue());
        applyMasterVolume(new_vol, false);
        if (new_vol > 0.0f)
        {
            pre_mute_volume_ = new_vol;
            if (mute_button_ != nullptr && mute_button_->getToggleState() == false)
                updateMuteButtonVisual(false);
        }
    }
}

void MainWindow::updateMuteButtonVisual(bool muted)
{
    if (mute_button_ == nullptr)
        return;

    mute_button_->setButtonText(muted ? "SPEAKER_OFF" : "SPEAKER_ON");
    // Toggle state tracks "active" (unmuted) so the button highlights when
    // audio is audible, not when it's muted.
    mute_button_->setToggleState(!muted, juce::dontSendNotification);
}

void MainWindow::setAudioRunning(bool run)
{
    if (run)
    {
        engine_->start();
        audio_running_ = true;
        audio_toggle_->setPowerState(true);
        status_label_->setText("Audio running", juce::dontSendNotification);
    }
    else
    {
        engine_->stop();
        audio_running_ = false;
        audio_toggle_->setPowerState(false);
        status_label_->setText("Audio stopped", juce::dontSendNotification);
    }
    updateTrayIconAppearance();
    saveSessionState();
}

void MainWindow::applyMasterVolume(float gain, bool updateMainSlider)
{
    engine_->setMasterVolume(gain);

    // Persist the master volume so it is restored on the next session.
    auto rs = roaming_settings_store_.load();
    rs.master_volume = gain;
    roaming_settings_store_.save(rs);

    // Keep the main-window slider in sync when the change came from elsewhere
    // (e.g. the tray volume popup).
    if (updateMainSlider && volume_slider_ != nullptr)
    {
        volume_slider_->setValue(gain, juce::dontSendNotification);
    }
}

void MainWindow::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == transport_mode_selector_.get())
    {
        const int id = transport_mode_selector_->getSelectedId();
        const auto mode = static_cast<TransportMode>(id);
        if (mode != current_transport_mode_)
        {
            current_transport_mode_ = mode;
            if (mode == TransportMode::Wasapi)
            {
                engine_->setWasapiExclusive(false);
                // WASAPI benefits from larger buffer for stability
                buffer_selector_->setSelectedId(512, juce::dontSendNotification);
                engine_->setBufferSize(512);
                if (!output_endpoint_ids_.empty())
                {
                    output_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
                    engine_->selectOutput(output_endpoint_ids_[0]);
                }
            }
            else if (mode == TransportMode::WasapiExclusive)
            {
                engine_->setWasapiExclusive(true);
                // WASAPI Exclusive can use larger buffer
                buffer_selector_->setSelectedId(512, juce::dontSendNotification);
                engine_->setBufferSize(512);
                if (!output_endpoint_ids_.empty())
                {
                    output_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
                    engine_->selectOutput(output_endpoint_ids_[0]);
                }
            }
            else  // Asio
            {
                engine_->setWasapiExclusive(false);
                refreshDeviceLists();
                if (!asio_device_names_.empty())
                {
                    asio_device_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
                    engine_->selectOutput(asio_device_names_[0]);
                }
                // refreshDeviceLists() re-derives current_transport_mode_ and the combo box
                // selection from the engine's current output, which at this point is still
                // the previous WASAPI device (selectOutput() above hasn't landed yet when
                // refreshDeviceLists() runs) — so both get clobbered back to Wasapi. Re-assert
                // Asio on both after the real switch so updateControlVisibility() below (which
                // reads current_transport_mode_, not the combo box) shows the right controls.
                current_transport_mode_ = TransportMode::Asio;
                transport_mode_selector_->setSelectedId(
                    static_cast<int>(TransportMode::Asio), juce::dontSendNotification);
            }
            updateControlVisibility();
        }
    }
    else if (comboBoxThatHasChanged == input_selector_.get())
    {
        const int idx = input_selector_->getSelectedItemIndex();
        if (idx >= 0 && idx < static_cast<int>(input_endpoint_ids_.size()))
        {
            const EndpointId new_input = input_endpoint_ids_[idx];
            const EndpointId current_output = engine_->currentOutput();

            // T022: Validate device mismatch before selecting input
            // If the new input conflicts with the current output, disconnect the output
            if (!new_input.empty() && !current_output.empty() && new_input == current_output)
            {
                engine_->selectOutput(std::string(kDisconnectedDeviceId));
                for (int i = 0; i < static_cast<int>(output_endpoint_ids_.size()); ++i)
                {
                    if (output_endpoint_ids_[i] == std::string(kDisconnectedDeviceId))
                    {
                        output_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
                        break;
                    }
                }
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Device Conflict Resolved",
                    "WASAPI loopback requires two separate audio interfaces:\n"
                    "one for capture (system audio) and one for playback (processed output).\n\n"
                    "The output device has been disconnected. The input will be muted automatically.\n\n"
                    "Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
                    "Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ",
                    "OK");
            }

            engine_->selectInput(new_input);

            // Default VST rate to the highest of input/output rates.
            const int in_rate = engine_->inputDeviceSampleRate();
            const int out_rate = engine_->outputDeviceSampleRate();
            int default_vst = 48000;
            if (in_rate > 0 && out_rate > 0)
                default_vst = std::max(in_rate, out_rate);
            else if (in_rate > 0)
                default_vst = in_rate;
            else if (out_rate > 0)
                default_vst = out_rate;
            engine_->setSampleRate(static_cast<double>(default_vst));
            vst_rate_selector_->setSelectedId(default_vst, juce::dontSendNotification);

            saveSessionState();

            // T034: Persist capture endpoint selection to roaming settings.
            auto rs = roaming_settings_store_.load();
            rs.capture_endpoint_id = new_input;
            rs.follow_default_capture = false;
            roaming_settings_store_.save(rs);
        }
    }
    else if (comboBoxThatHasChanged == output_selector_.get())
    {
        const int idx = output_selector_->getSelectedItemIndex();
        if (idx >= 0 && idx < static_cast<int>(output_endpoint_ids_.size()))
        {
            const EndpointId new_output = output_endpoint_ids_[idx];
            const EndpointId current_input = engine_->currentInput();

            // T022: Validate device mismatch before selecting output
            // If the new output conflicts with the current input, disconnect the input
            if (!new_output.empty() && !current_input.empty() && new_output == current_input)
            {
                engine_->selectInput(std::string(kDisconnectedDeviceId));
                for (int i = 0; i < static_cast<int>(input_endpoint_ids_.size()); ++i)
                {
                    if (input_endpoint_ids_[i] == std::string(kDisconnectedDeviceId))
                    {
                        input_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
                        break;
                    }
                }
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Device Conflict Resolved",
                    "WASAPI loopback requires two separate audio interfaces:\n"
                    "one for capture (system audio) and one for playback (processed output).\n\n"
                    "The input device has been disconnected. It will be muted automatically.\n\n"
                    "Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
                    "Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ",
                    "OK");
            }

            engine_->selectOutput(new_output);

            // Default VST rate to the highest of input/output rates.
            const int in_rate = engine_->inputDeviceSampleRate();
            const int out_rate = engine_->outputDeviceSampleRate();
            int default_vst = 48000;
            if (in_rate > 0 && out_rate > 0)
                default_vst = std::max(in_rate, out_rate);
            else if (in_rate > 0)
                default_vst = in_rate;
            else if (out_rate > 0)
                default_vst = out_rate;
            engine_->setSampleRate(static_cast<double>(default_vst));
            vst_rate_selector_->setSelectedId(default_vst, juce::dontSendNotification);

            saveSessionState();

            // T034: Persist output endpoint selection to roaming settings.
            auto rs = roaming_settings_store_.load();
            rs.output_endpoint_id = new_output;
            roaming_settings_store_.save(rs);
        }
    }
    else if (comboBoxThatHasChanged == asio_device_selector_.get())
    {
        const int idx = asio_device_selector_->getSelectedItemIndex();
        if (idx >= 0 && idx < static_cast<int>(asio_device_names_.size()))
        {
            engine_->selectOutput(asio_device_names_[idx]);

            // Default VST rate to the highest of input/output rates.
            const int in_rate = engine_->inputDeviceSampleRate();
            const int out_rate = engine_->outputDeviceSampleRate();
            int default_vst = 48000;
            if (in_rate > 0 && out_rate > 0)
                default_vst = std::max(in_rate, out_rate);
            else if (in_rate > 0)
                default_vst = in_rate;
            else if (out_rate > 0)
                default_vst = out_rate;
            engine_->setSampleRate(static_cast<double>(default_vst));
            vst_rate_selector_->setSelectedId(default_vst, juce::dontSendNotification);

            saveSessionState();
        }
    }
    else if (comboBoxThatHasChanged == asio_pair_selector_.get())
    {
        const int pair_index = asio_pair_selector_->getSelectedItemIndex();
        engine_->setAsioOutputPair(pair_index * 2);
        saveSessionState();
    }
    else if (comboBoxThatHasChanged == buffer_selector_.get())
    {
        try
        {
            engine_->setBufferSize(buffer_selector_->getSelectedId());
            saveSessionState();
        }
        catch (const std::exception& e)
        {
            buffer_selector_->setSelectedId(engine_->bufferSize(), juce::dontSendNotification);
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Buffer Size Not Supported",
                juce::String(e.what()));
        }
    }
    else if (comboBoxThatHasChanged == vst_rate_selector_.get())
    {
        const int rate = vst_rate_selector_->getSelectedId();
        if (rate > 0)
        {
            engine_->setSampleRate(static_cast<double>(rate));
            saveSessionState();
        }
    }
    else if (comboBoxThatHasChanged == theme_selector_.get())
    {
        const auto id = static_cast<CustomLookAndFeel::ThemeId>(theme_selector_->getSelectedId());
        applyThemeChange(id);
        saveSessionState();
    }
}

void MainWindow::applyThemeChange(CustomLookAndFeel::ThemeId id)
{
    custom_laf_.applyTheme(id);

    // Push the theme palette to the built-in effect editors (EQ, Volume Leveler),
    // which live in the audio-engine layer and cannot see CustomLookAndFeel.
    {
        const auto& src = custom_laf_.colors();
        auto& dst = engine::builtinThemeColors();
        dst.background = src.bgDeep;
        dst.panel = src.bgPanel;
        dst.panelBorder = src.bgPanelBorder;
        dst.accent = src.accentCyan;
        dst.accentSecondary = src.accentBlue;
        dst.textPrimary = src.textPrimary;
        dst.textDim = src.textDim;
        dst.controlBg = src.controlBg;
        dst.controlHover = src.controlHover;
        dst.trackBg = src.trackBg;
    }
    if (title_label_ != nullptr)
        title_label_->setColour(juce::Label::textColourId, custom_laf_.colors().accentCyan);
    status_label_->setColour(juce::Label::textColourId, custom_laf_.colors().textDim);
    if (cpu_label_ != nullptr)
        cpu_label_->setColour(juce::Label::textColourId, custom_laf_.colors().textDim);
    if (content_root_ != nullptr)
    {
        content_root_->sendLookAndFeelChange();
        content_root_->repaint();
    }
    repaint();
}

void MainWindow::applyTooltips()
{
    const bool enabled = roaming_settings_store_.load().tooltips_enabled;
    const juce::String empty;

    transport_mode_selector_->setTooltip(enabled ? "Select audio transport: WASAPI (loopback) or ASIO" : empty);
    input_selector_->setTooltip(enabled ? "Capture source: system audio loopback or a specific capture device" : empty);
    output_selector_->setTooltip(enabled ? "Playback device: where processed audio is sent" : empty);
    asio_device_selector_->setTooltip(enabled ? "ASIO hardware driver to use" : empty);
    asio_pair_selector_->setTooltip(enabled ? "Stereo output channel pair" : empty);
    buffer_selector_->setTooltip(enabled ? "Audio buffer size in samples (larger = more stable, smaller = lower latency)" : empty);
    vst_rate_selector_->setTooltip(enabled ? "Target sampling rate for the VST plugin chain" : empty);
    audio_toggle_->setTooltip(enabled ? "Start or stop audio processing" : empty);
    volume_slider_->setTooltip(enabled ? "Master output volume" : empty);
    mute_button_->setTooltip(enabled ? "Mute / unmute the output" : empty);
    reset_engine_button_->setTooltip(enabled ? "Reset the audio engine and reload the plugin chain" : empty);
    save_preset_button_->setTooltip(enabled ? "Save the current plugin chain and settings to a file" : empty);
    load_preset_button_->setTooltip(enabled ? "Load a previously saved preset file" : empty);
    asio_settings_button_->setTooltip(enabled ? "Open the ASIO driver control panel" : empty);
    theme_selector_->setTooltip(enabled ? "Choose the application colour theme: light, dark, or match the system setting." : empty);
    theme_label_->setTooltip(enabled ? "Choose the application colour theme: light, dark, or match the system setting." : empty);
    start_minimized_button_->setTooltip(enabled ? "When enabled, the app starts directly in the system tray without showing the main window." : empty);
    start_minimized_label_->setTooltip(enabled ? "When enabled, the app starts directly in the system tray without showing the main window." : empty);
    tooltips_button_->setTooltip(enabled ? "Show helpful popup descriptions when hovering over controls. Turn this off for a cleaner interface." : empty);
    tooltips_label_->setTooltip(enabled ? "Show helpful popup descriptions when hovering over controls. Turn this off for a cleaner interface." : empty);
    about_button_->setTooltip(enabled ? "About, diagnostics, and settings" : empty);
    energy_saver_button_->setTooltip(enabled ? "Toggle energy saver: auto-suspend the VST chain when input is silent to save CPU." : empty);
    help_button_->setTooltip(enabled ? "Open help documentation" : empty);

    // Refresh existing button tooltips that were set in their constructors.
    if (enabled)
    {
        energy_saver_button_->setEnergyState(engine_->isEnergySaverEnabled(), engine_->isEnergySaverSleeping());
    }
    else
    {
        energy_saver_button_->setTooltip(empty);
    }
}

void MainWindow::handleLoadPlugin()
{
    const auto catalog = engine_->catalog();

    // If catalog is not empty, show the catalog dialog first.
    if (!catalog.empty())
    {
        auto on_action = [this](CatalogDialog::Action action) {
            if (action == CatalogDialog::Action::Selected)
            {
                if (const auto* entry = catalog_dialog_->getSelectedEntry())
                {
                    const auto id = engine_->addPlugin(entry->ref, 0);
                    juce::String display = juce::String(entry->ref.vendor) + " " + juce::String(entry->ref.name);
                    status_label_->setText("Plugin added: " + display, juce::dontSendNotification);
                    (void)id;
                }
            }
            else if (action == CatalogDialog::Action::Browse)
            {
                openPluginFileBrowser();
            }
        };

        catalog_dialog_ = std::make_unique<CatalogDialog>(catalog, on_action);
        return;
    }

    // Catalog is empty, go straight to file dialog.
    openPluginFileBrowser();
}

void MainWindow::openPluginFileBrowser()
{
    wchar_t file_name[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"VST3 Plugins (*.vst3)\0*.vst3\0All Files\0*.*\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
    {
        auto file = juce::File(juce::String(file_name));
        auto path = std::filesystem::path(file_name);

        // Normalize to the VST3 bundle root. A single-file ".vst3" is itself the
        // bundle; for folder bundles the user may have picked a binary nested
        // inside "<name>.vst3/Contents/...", so resolve to the outermost ".vst3"
        // ancestor. has_relative_path() bounds the walk at the drive root, where
        // parent_path() returns the root unchanged — the previous has_parent_path()
        // loop spun forever there on single-file plugins, hanging the UI.
        std::filesystem::path bundle = path;
        for (auto p = path; p.has_relative_path(); p = p.parent_path())
        {
            if (p.extension() == ".vst3")
                bundle = p;
        }
        path = bundle;

        const auto id = engine_->addPluginFromPath(path, 0);
        auto display_name = std::filesystem::path(path).stem().string();
        status_label_->setText("Plugin added: " + juce::String(display_name),
                               juce::dontSendNotification);
        (void)id;
    }
}

void MainWindow::handleSavePreset()
{
    wchar_t file_name[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Global VST Host Presets (*.jvst)\0*.jvst\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"jvst";

    if (GetSaveFileNameW(&ofn))
    {
        auto path = std::filesystem::path(file_name);
        if (path.extension() != ".jvst")
            path += ".jvst";
        engine_->savePreset(path, path.stem().string());

        // Inject active theme into the preset JSON.
        {
            nlohmann::json preset_doc;
            std::ifstream pifs(path);
            if (pifs) { try { pifs >> preset_doc; } catch (...) {} }
            preset_doc["theme_id"] = static_cast<int>(custom_laf_.currentTheme());
            std::ofstream pofs(path, std::ios::binary);
            if (pofs) pofs << preset_doc.dump(2);
        }

        status_label_->setText("Preset saved: " + juce::String(path.stem().string()),
                               juce::dontSendNotification);
    }
}

void MainWindow::handleLoadPreset()
{
    wchar_t file_name[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Global VST Host Presets (*.jvst)\0*.jvst\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
    {
        auto path = std::filesystem::path(file_name);

        // Extract theme from preset before loading (engine may reset state).
        {
            nlohmann::json preset_doc;
            std::ifstream pifs(path);
            if (pifs)
            {
                try { pifs >> preset_doc; } catch (...) {}
                if (preset_doc.contains("theme_id") && preset_doc["theme_id"].is_number_integer())
                {
                    const int tid = preset_doc["theme_id"].get<int>();
                    if (tid >= 1 && tid <= 6)
                    {
                        applyThemeChange(static_cast<CustomLookAndFeel::ThemeId>(tid));
                        theme_selector_->setSelectedId(tid, juce::dontSendNotification);
                    }
                }
            }
        }

        engine_->loadPreset(path);
        status_label_->setText("Preset loaded: " + juce::String(path.stem().string()),
                               juce::dontSendNotification);
    }
}

void MainWindow::handleAbout()
{
    DiagnosticSnapshot snap;
    snap.current_output_friendly_name = engine_->currentOutput();
    snap.buffer_size = buffer_selector_->getSelectedId();
    snap.sample_rate = engine_->negotiatedSampleRate();
    snap.chain_revision = last_chain_revision_;
    snap.plugin_count = static_cast<int>(engine_->snapshotChain().slots.size());
    snap.latency = engine_->latencyProfile();
    snap.cpu = engine_->cpuStats();

    juce::String version = "1.0.1";
    if (auto* app = juce::JUCEApplication::getInstance())
        version = app->getApplicationVersion();

    SettingsControls settings;
    settings.theme_label = theme_label_.get();
    settings.theme_selector = theme_selector_.get();
    settings.start_minimized_label = start_minimized_label_.get();
    settings.start_minimized_button = start_minimized_button_.get();
    settings.tooltips_label = tooltips_label_.get();
    settings.tooltips_button = tooltips_button_.get();

    AboutDiagnostics::show(this, EngineHostMode::InProcess, version, snap, settings,
                           &custom_laf_);
}

void MainWindow::handleHelp()
{
    // The user guide is embedded as binary data. Materialise it to a stable
    // location under %LOCALAPPDATA% and open it with the default browser.
    try
    {
        const auto dir = std::filesystem::path(std::getenv("LOCALAPPDATA")) / "JyGlobalVST";
        std::filesystem::create_directories(dir);
        const auto html_path = dir / "userguide.html";

        {
            std::ofstream ofs(html_path, std::ios::binary | std::ios::trunc);
            if (ofs)
            {
                ofs.write(jyglobalvst::BinaryData::userguide_html,
                          jyglobalvst::BinaryData::userguide_htmlSize);
            }
        }

        juce::File(juce::String(html_path.string())).startAsProcess();
    }
    catch (const std::exception& e)
    {
        status_label_->setText(juce::String("Could not open user guide: ") + e.what(),
                               juce::dontSendNotification);
    }
}

void MainWindow::toggleEnergySaver()
{
    const bool enable = !engine_->isEnergySaverEnabled();
    engine_->setEnergySaverEnabled(enable);

    // Persist the preference so it is restored next launch.
    auto rs = roaming_settings_store_.load();
    rs.energy_saver_enabled = enable;
    roaming_settings_store_.save(rs);

    status_label_->setText(enable ? "Energy Saver enabled" : "Energy Saver disabled",
                           juce::dontSendNotification);
    updateEnergySaverVisual();
}

void MainWindow::updateEnergySaverVisual()
{
    if (energy_saver_button_ == nullptr)
        return;
    energy_saver_button_->setEnergyState(engine_->isEnergySaverEnabled(),
                                         engine_->isEnergySaverSleeping());
}

void MainWindow::onEnergySaverStateChanged(bool sleeping)
{
    // Fired on the UI thread by the engine when it suspends/resumes processing.
    updateEnergySaverVisual();
    updateTrayIconAppearance();
    if (engine_->isEnergySaverEnabled())
    {
        status_label_->setText(sleeping ? "Energy Saver: engine sleeping (no audio)"
                                        : "Energy Saver: audio resumed",
                               juce::dontSendNotification);
    }
}

void MainWindow::saveSessionState()
{
    autosave_store_.write(engine_.get(), preset_override_flag_,
                          static_cast<int>(custom_laf_.currentTheme()));
}

void MainWindow::restoreFromAutosave()
{
    bool was_running = false;
    int theme_id = 1;
    if (!autosave_store_.restore(engine_.get(), &was_running, &theme_id))
        return;

    // Restore theme before syncing other UI.
    const auto restored_theme = static_cast<CustomLookAndFeel::ThemeId>(theme_id);
    applyThemeChange(restored_theme);
    theme_selector_->setSelectedId(theme_id, juce::dontSendNotification);

    // Sync buffer selector to the restored engine value.
    buffer_selector_->setSelectedId(engine_->bufferSize(), juce::dontSendNotification);

    // Sample rate is auto-followed from the output device; the read-only rate
    // readouts are refreshed live in timerCallback(), nothing to sync here.

    // Sync input selector.
    const EndpointId cur_input = engine_->currentInput();
    for (int i = 0; i < static_cast<int>(input_endpoint_ids_.size()); ++i)
    {
        if (input_endpoint_ids_[i] == cur_input)
        {
            input_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
            break;
        }
    }

    // Sync output selector — try WASAPI first, then ASIO.
    const EndpointId cur_output = engine_->currentOutput();
    bool output_matched = false;
    for (int i = 0; i < static_cast<int>(output_endpoint_ids_.size()); ++i)
    {
        if (output_endpoint_ids_[i] == cur_output)
        {
            output_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
            output_matched = true;
            break;
        }
    }
    if (!output_matched)
    {
        for (int i = 0; i < static_cast<int>(asio_device_names_.size()); ++i)
        {
            if (asio_device_names_[i] == cur_output)
            {
                current_transport_mode_ = TransportMode::Asio;
                transport_mode_selector_->setSelectedId(
                    static_cast<int>(TransportMode::Asio), juce::dontSendNotification);
                asio_device_selector_->setSelectedItemIndex(i, juce::dontSendNotification);
                updateControlVisibility();
                break;
            }
        }
    }

    chain_editor_->refreshFromEngine();

    // Defer engine start until after the window is fully constructed and visible.
    if (was_running)
    {
        const EndpointId out = engine_->currentOutput();
        if (!out.empty())
        {
            status_label_->setText("Session restored - starting audio...", juce::dontSendNotification);
            juce::MessageManager::callAsync([this]() {
                StartupLog("callAsync: starting audio");
                engine_->start();
                if (engine_->isRunning())
                {
                    audio_running_ = true;
                    audio_toggle_->setPowerState(true);
                    status_label_->setText("Session restored - audio running", juce::dontSendNotification);
                }
                else
                {
                    audio_running_ = false;
                    audio_toggle_->setPowerState(false);
                    status_label_->setText("Session restored - failed to start audio", juce::dontSendNotification);
                }
            });
        }
        else
        {
            status_label_->setText("Session restored - output device not found", juce::dontSendNotification);
        }
    }
    else
    {
        status_label_->setText("Session restored", juce::dontSendNotification);
    }
    StartupLog("restoreFromAutosave exit");
}

void MainWindow::timerCallback()
{
    static int timer_call_count = 0;
    if (++timer_call_count <= 20)
    {
        StartupLog(juce::String("timerCallback #" + juce::String(timer_call_count)).toStdString().c_str());
    }

    // Check if initial plugin scan has completed
    if (!plugin_scan_complete_ && scan_dialog_ && scan_dialog_->isFinished())
    {
        StartupLog("timerCallback: scan finished, hiding dialog");
        plugin_scan_complete_ = true;
        status_label_->setText("Ready", juce::dontSendNotification);
        scan_dialog_->setVisible(false);
    }

    // Periodic autosave (every ~2 seconds at 10 Hz = every 20 callbacks)
    static int autosave_counter = 0;
    if (++autosave_counter >= 20)
    {
        autosave_counter = 0;
        saveSessionState();
    }

    refreshLatencyAndCpu();
    refreshMeters();
    refreshSampleRates();
}

void MainWindow::refreshSampleRates()
{
    auto format = [](int hz) -> juce::String {
        return hz > 0 ? juce::String(hz) + " Hz" : juce::String::fromUTF8("\xe2\x80\x94");
    };
    const bool running = engine_->isRunning();

    // Input/capture endpoint rate (read-only).
    input_rate_value_->setText(format(running ? engine_->inputDeviceSampleRate() : 0),
                               juce::dontSendNotification);

    // Output endpoint rate (read-only).
    output_rate_value_->setText(format(running ? engine_->outputDeviceSampleRate() : 0),
                               juce::dontSendNotification);

    // VST chain rate — reflect the user-selected rate in the dropdown without
    // firing a change notification.
    const int desired_rate = static_cast<int>(engine_->sampleRate());
    if (vst_rate_selector_->getSelectedId() != desired_rate && desired_rate > 0)
    {
        vst_rate_selector_->setSelectedId(desired_rate, juce::dontSendNotification);
    }
}

void MainWindow::refreshMeters()
{
    MeterFrame frame;
    {
        std::lock_guard<std::mutex> lk(meter_frame_mutex_);
        frame = latest_meter_frame_;
    }

    meter_input_l_->setLevels(linearToDb(frame.input_peak_l), linearToDb(frame.input_rms_l));
    meter_input_r_->setLevels(linearToDb(frame.input_peak_r), linearToDb(frame.input_rms_r));
    meter_output_l_->setLevels(linearToDb(frame.output_peak_l), linearToDb(frame.output_rms_l));
    meter_output_r_->setLevels(linearToDb(frame.output_peak_r), linearToDb(frame.output_rms_r));

    // Update per-plugin output meters in the chain editor.
    if (chain_editor_ != nullptr && engine_ != nullptr)
    {
        chain_editor_->setPluginMeterLevels(engine_->pluginOutputPeaks(),
                                               engine_->pluginOutputRms());
    }

    // Update tray volume popup meters if still open. tray_volume_popup_ is a
    // Component::SafePointer, so it automatically reads back as null once the
    // CallOutBox has destroyed the popup content — no dangling-pointer risk.
    if (auto* popup_component = tray_volume_popup_.getComponent())
    {
        if (popup_component->isVisible())
        {
            auto* popup = static_cast<TrayVolumeContent*>(popup_component);
            popup->setMeterLevels(
                linearToDb(frame.input_peak_l), linearToDb(frame.input_rms_l),
                linearToDb(frame.input_peak_r), linearToDb(frame.input_rms_r),
                linearToDb(frame.output_peak_l), linearToDb(frame.output_rms_l),
                linearToDb(frame.output_peak_r), linearToDb(frame.output_rms_r));
        }
        else
        {
            // Clear the reference if popup is no longer visible
            tray_volume_popup_ = nullptr;
        }
    }
}

void MainWindow::updateControlVisibility()
{
    const bool is_wasapi_family = (current_transport_mode_ == TransportMode::Wasapi
                                   || current_transport_mode_ == TransportMode::WasapiExclusive);
    const bool is_asio = (current_transport_mode_ == TransportMode::Asio);

    output_selector_->setVisible(is_wasapi_family);
    output_label_->setVisible(is_wasapi_family);
    asio_device_selector_->setVisible(is_asio);
    asio_device_label_->setVisible(is_asio);
    asio_pair_selector_->setVisible(is_asio);
    asio_pair_label_->setVisible(is_asio);
    asio_settings_button_->setVisible(is_asio);

    rebuildBufferSelector();

    if (content_root_ != nullptr)
        content_root_->resized();
}

void MainWindow::rebuildBufferSelector()
{
    const bool is_asio = (current_transport_mode_ == TransportMode::Asio);
    const int current_id = buffer_selector_->getSelectedId();
    buffer_selector_->clear(juce::dontSendNotification);
    if (is_asio)
    {
        buffer_selector_->addItem("32", 32);
        buffer_selector_->addItem("64", 64);
        buffer_selector_->addItem("128", 128);
        buffer_selector_->addItem("256", 256);
    }
    buffer_selector_->addItem("512", 512);
    buffer_selector_->addItem("1024", 1024);

    // Re-select the previous value if it is still valid; otherwise default to 512.
    if (current_id > 0 && buffer_selector_->indexOfItemId(current_id) >= 0)
        buffer_selector_->setSelectedId(current_id, juce::dontSendNotification);
    else
        buffer_selector_->setSelectedId(512, juce::dontSendNotification);
}

void MainWindow::refreshAsioPairSelector(int maxChannels)
{
    asio_pair_selector_->clear(juce::dontSendNotification);
    int pair_id = 1;
    for (int ch = 0; ch + 1 < maxChannels; ch += 2)
    {
        juce::String label = juce::String(ch + 1) + "-" + juce::String(ch + 2);
        asio_pair_selector_->addItem(label, pair_id);
        ++pair_id;
    }
    if (pair_id > 1)
    {
        asio_pair_selector_->setSelectedItemIndex(0, juce::dontSendNotification);
    }
}

void MainWindow::refreshLatencyAndCpu()
{
    const auto latency = engine_->latencyProfile();
    const auto cpu = engine_->cpuStats();

    latency_label_->setText(juce::String::formatted("Latency: %.2f ms (in %.2f + out %.2f)",
                                                     latency.total_round_trip_ms,
                                                     latency.capture_ms,
                                                     latency.output_ms),
                            juce::dontSendNotification);

    static_cast<CpuBar*>(cpu_bar_.get())->setValue(cpu.rolling_1s_pct);
}

// =========================================================================
// IAudioEngineListener
// =========================================================================

void MainWindow::onChainRevision(int new_revision)
{
    juce::MessageManager::callAsync([this, new_revision]() {
        last_chain_revision_ = new_revision;
        status_label_->setText("Chain updated (rev " + juce::String(new_revision) + ")",
                               juce::dontSendNotification);
        if (chain_editor_)
        {
            chain_editor_->refreshFromEngine();
        }
        saveSessionState();
    });
}

void MainWindow::onPluginFailed(const InstanceId& /*id*/, const std::string& reason)
{
    juce::MessageManager::callAsync([this, reason]() {
        status_label_->setText("Plugin failed: " + juce::String(reason),
                               juce::dontSendNotification);
        if (chain_editor_)
        {
            chain_editor_->refreshFromEngine();
        }
    });
}

void MainWindow::onDeviceLost(const EndpointId& lost, const EndpointId& fallback_to)
{
    juce::MessageManager::callAsync([this, lost, fallback_to]() {
        status_label_->setText("Device lost: " + juce::String(lost) + " → fallback: " + juce::String(fallback_to),
                               juce::dontSendNotification);
        refreshDeviceLists();
    });
}

void MainWindow::onDeviceRestored(const EndpointId& restored)
{
    juce::MessageManager::callAsync([this, restored]() {
        status_label_->setText("Device restored: " + juce::String(restored),
                               juce::dontSendNotification);
        refreshDeviceLists();
    });
}

void MainWindow::onDeviceListChanged()
{
    juce::MessageManager::callAsync([this]() {
        refreshDeviceLists();
    });
}

void MainWindow::onCpuWarning(float rolling_1s_pct)
{
    juce::MessageManager::callAsync([this, rolling_1s_pct]() {
        status_label_->setText(juce::String::formatted("CPU warning: %.1f %% — consider larger buffer",
                                                        rolling_1s_pct),
                               juce::dontSendNotification);
    });
}

void MainWindow::onMeterFrame(const MeterFrame& frame)
{
    std::lock_guard<std::mutex> lk(meter_frame_mutex_);
    latest_meter_frame_ = frame;
}

void MainWindow::onPresetPartialLoad(const std::vector<MissingPluginInfo>& missing)
{
    juce::MessageManager::callAsync([this, missing]() {
        juce::String msg = "Preset loaded with missing plugins: " + juce::String((int)missing.size());
        status_label_->setText(msg, juce::dontSendNotification);
    });
}

void MainWindow::onSameDeviceConflict(const EndpointId& device)
{
    (void)device;
    juce::MessageManager::callAsync([this]() {
        status_label_->setText("Cannot use same device for capture and output", juce::dontSendNotification);
    });
}

void MainWindow::onCaptureMuteFallbackRequired(const EndpointId& endpoint)
{
    (void)endpoint;
    juce::MessageManager::callAsync([this]() {
        status_label_->setText("Capture device muting unavailable; select a different output device",
                               juce::dontSendNotification);
    });
}

// =========================================================================
// Sleep / wake (T043)
// =========================================================================

void MainWindow::installPowerHandler()
{
    if (auto* peer = getPeer())
    {
        if (auto* hwnd = static_cast<HWND>(peer->getNativeHandle()))
        {
            SetWindowSubclass(hwnd, mainWindowSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
        }
    }
}

void MainWindow::removePowerHandler()
{
    if (auto* peer = getPeer())
    {
        if (auto* hwnd = static_cast<HWND>(peer->getNativeHandle()))
        {
            RemoveWindowSubclass(hwnd, mainWindowSubclassProc, 0);
        }
    }
}

void MainWindow::onSystemSuspend()
{
    audio_was_running_before_suspend_ = audio_running_;
    if (audio_running_)
    {
        engine_->stop();
        audio_running_ = false;
        juce::MessageManager::callAsync([this]() {
            audio_toggle_->setPowerState(false);
            status_label_->setText("Audio stopped (system sleep)", juce::dontSendNotification);
        });
    }
}

void MainWindow::onSystemResume()
{
    if (audio_was_running_before_suspend_)
    {
        engine_->start();
        audio_running_ = true;
        juce::MessageManager::callAsync([this]() {
            audio_toggle_->setPowerState(true);
            status_label_->setText("Audio running (system resume)", juce::dontSendNotification);
        });
    }
}

}  // namespace jyglobalvst::tray
