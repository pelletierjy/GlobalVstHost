// src/tray-app/ui/spectrum_analyzer.cpp
//
// Log-band spectrum view of the post-chain output. All FFT work happens on the
// UI thread from a lock-free snapshot of the engine's output tap; the audio
// thread is never involved beyond its plain ring writes.

#include "spectrum_analyzer.h"
#include "custom_look_and_feel.h"

#include <algorithm>
#include <cmath>

namespace jyglobalvst::tray {

namespace {

constexpr int kRefreshHz = 30;

// Peak caps fall at a musical-looking rate rather than snapping down.
constexpr float kPeakFallDbPerSec = 26.0f;
// How fast a bar eases down once the content quietens (rises are instant).
constexpr float kBarFallDbPerSec = 90.0f;

// Height of the frequency-label strip along the bottom of the plot.
constexpr int kAxisStripH = 12;

float magnitudeToDb(float magnitude)
{
    // Hann-windowed forward transform: a full-scale sine lands at N/4, so
    // dividing by N/4 puts a 0 dBFS tone at 0 dB on the display.
    const float normalized = magnitude * 4.0f / static_cast<float>(SpectrumAnalyzer::kFftSize);
    if (normalized <= 1.0e-7f)
        return SpectrumAnalyzer::kMinDb;
    return 20.0f * std::log10(normalized);
}

// Bar colour ramp: cyan through the low/mid range, warming to magenta as a band
// approaches full scale, so hot bands stand out the way the level meters do.
// The pair of colours the analyser draws with: `base` for quiet bands, blended
// towards `hot` as a band approaches full scale.
struct SpectrumAccents
{
    juce::Colour base;
    juce::Colour hot;
};

SpectrumAccents spectrumAccents(const ThemeColors& theme, CustomLookAndFeel::ThemeId id)
{
    // Monochrome's accents are both near-white, which leaves the analyser
    // washed out and hard to read against its own grid. That one theme gets a
    // green spectrum instead; every other theme uses its own accents.
    if (id == CustomLookAndFeel::ThemeId::Mono)
        return {juce::Colour(0xFF00E676), juce::Colour(0xFF76FF03)};

    return {theme.accentCyan, theme.magenta};
}

juce::Colour bandColour(const SpectrumAccents& accents, float norm)
{
    if (norm <= 0.75f)
        return accents.base;
    const float t = juce::jlimit(0.0f, 1.0f, (norm - 0.75f) / 0.25f);
    return accents.base.interpolatedWith(accents.hot, t);
}

}  // namespace

SpectrumAnalyzer::SpectrumAnalyzer(IAudioEngine* engine) : engine_(engine)
{
    setTitle("Spectrum");
    band_db_.fill(kMinDb);
    peak_db_.fill(kMinDb);
    startTimerHz(kRefreshHz);
}

void SpectrumAnalyzer::timerCallback()
{
    // Nothing to show while the window is hidden to the tray — skip the FFT
    // entirely rather than burning CPU on a display nobody can see.
    if (!isShowing())
        return;

    if (!analyseLatestBlock())
        decayBands();

    const float peak_step = kPeakFallDbPerSec / static_cast<float>(kRefreshHz);
    for (int i = 0; i < kNumBands; ++i)
    {
        peak_db_[static_cast<size_t>(i)] =
            std::max(band_db_[static_cast<size_t>(i)],
                     peak_db_[static_cast<size_t>(i)] - peak_step);
    }

    repaint();
}

bool SpectrumAnalyzer::analyseLatestBlock()
{
    if (engine_ == nullptr || !engine_->isRunning())
        return false;

    const int captured = engine_->copyRecentOutputSamples(fft_data_.data(), kFftSize);
    if (captured <= 0)
        return false;

    // A short tap (engine just started) is zero-padded rather than skipped.
    std::fill(fft_data_.begin() + captured, fft_data_.end(), 0.0f);

    window_.multiplyWithWindowingTable(fft_data_.data(), static_cast<size_t>(kFftSize));
    fft_.performFrequencyOnlyForwardTransform(fft_data_.data(), true);

    double sample_rate = static_cast<double>(engine_->negotiatedSampleRate());
    if (sample_rate <= 0.0)
        sample_rate = 48000.0;

    const float nyquist = static_cast<float>(sample_rate * 0.5);
    const float top_hz = std::min(kMaxHz, nyquist);
    const float log_lo = std::log10(kMinHz);
    const float log_span = std::log10(top_hz) - log_lo;
    const float bin_hz = static_cast<float>(sample_rate) / static_cast<float>(kFftSize);
    const int max_bin = kFftSize / 2 - 1;

    const float bar_step = kBarFallDbPerSec / static_cast<float>(kRefreshHz);

    for (int i = 0; i < kNumBands; ++i)
    {
        const float t0 = static_cast<float>(i) / static_cast<float>(kNumBands);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kNumBands);
        const float f_lo = std::pow(10.0f, log_lo + log_span * t0);
        const float f_hi = std::pow(10.0f, log_lo + log_span * t1);

        // The lowest display bands are narrower than one FFT bin; clamping bin_hi
        // up to bin_lo makes those bands read a single bin instead of nothing.
        const int bin_lo = juce::jlimit(1, max_bin, static_cast<int>(f_lo / bin_hz));
        const int bin_hi = juce::jlimit(bin_lo, max_bin, static_cast<int>(f_hi / bin_hz));

        float magnitude = 0.0f;
        for (int bin = bin_lo; bin <= bin_hi; ++bin)
            magnitude = std::max(magnitude, fft_data_[static_cast<size_t>(bin)]);

        const float db = juce::jlimit(kMinDb, kMaxDb, magnitudeToDb(magnitude));
        float& current = band_db_[static_cast<size_t>(i)];
        current = (db > current) ? db : std::max(db, current - bar_step);
    }

    return true;
}

void SpectrumAnalyzer::decayBands()
{
    const float bar_step = kBarFallDbPerSec / static_cast<float>(kRefreshHz);
    for (auto& band : band_db_)
        band = std::max(kMinDb, band - bar_step);
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    const ThemeColors theme = (laf != nullptr) ? laf->colors() : ThemeColors{
        kBgDeep, kBgPanel, kBgPanelBorder, kAccentCyan, kAccentBlue, kAccentGlow,
        kTextPrimary, kTextDim, kControlBg, kControlHover, kTrackBg, kMeterBg,
        kMagenta, kRedNeon};
    const SpectrumAccents accents = spectrumAccents(
        theme, (laf != nullptr) ? laf->currentTheme() : CustomLookAndFeel::ThemeId::NeonBlue);

    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.meterBg);
    g.fillRoundedRectangle(bounds, 3.0f);

    auto axis = bounds.removeFromBottom(static_cast<float>(kAxisStripH));
    auto plot = bounds.reduced(3.0f, 3.0f);
    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f)
        return;

    // Horizontal dB grid.
    g.setColour(theme.bgPanelBorder.withAlpha(0.55f));
    for (float db = -12.0f; db > kMinDb; db -= 12.0f)
    {
        const float y = plot.getY() + plot.getHeight() * (db / kMinDb);
        g.fillRect(juce::Rectangle<float>(plot.getX(), y, plot.getWidth(), 1.0f));
    }

    // Frequency ticks, positioned on the same log scale as the bands.
    const struct { float hz; const char* label; } ticks[] = {
        {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}};
    const float log_lo = std::log10(kMinHz);
    const float log_span = std::log10(kMaxHz) - log_lo;

    juce::FontOptions fo;
    juce::Font tick_font{fo};
    tick_font.setHeight(9.0f);
    g.setFont(tick_font);

    for (const auto& tick : ticks)
    {
        const float t = (std::log10(tick.hz) - log_lo) / log_span;
        const float x = plot.getX() + plot.getWidth() * t;
        g.setColour(theme.bgPanelBorder.withAlpha(0.55f));
        g.fillRect(juce::Rectangle<float>(x, plot.getY(), 1.0f, plot.getHeight()));
        g.setColour(theme.textDim.withAlpha(0.7f));
        g.drawText(tick.label, juce::Rectangle<float>(x - 14.0f, axis.getY(), 28.0f, axis.getHeight()),
                   juce::Justification::centred, false);
    }

    // Bands. Each occupies an equal slot because the band edges are already
    // log-spaced, so the plot reads linearly in octaves.
    const float slot_w = plot.getWidth() / static_cast<float>(kNumBands);
    const float bar_w = std::max(1.0f, slot_w - 2.0f);

    for (int i = 0; i < kNumBands; ++i)
    {
        const float norm = juce::jlimit(0.0f, 1.0f,
                                        (band_db_[static_cast<size_t>(i)] - kMinDb) / (kMaxDb - kMinDb));
        const float peak_norm = juce::jlimit(0.0f, 1.0f,
                                             (peak_db_[static_cast<size_t>(i)] - kMinDb) / (kMaxDb - kMinDb));
        const float x = plot.getX() + static_cast<float>(i) * slot_w + (slot_w - bar_w) * 0.5f;

        // Unlit column, so an idle analyser still shows its shape.
        g.setColour(accents.base.withAlpha(0.07f));
        g.fillRoundedRectangle(juce::Rectangle<float>(x, plot.getY(), bar_w, plot.getHeight()), 1.5f);

        if (norm > 0.001f)
        {
            const float h = plot.getHeight() * norm;
            const juce::Rectangle<float> bar(x, plot.getBottom() - h, bar_w, h);
            const juce::Colour top = bandColour(accents, norm);
            g.setGradientFill(juce::ColourGradient(top, bar.getX(), bar.getY(),
                                                   accents.base.withAlpha(0.45f),
                                                   bar.getX(), plot.getBottom(), false));
            g.fillRoundedRectangle(bar, 1.5f);
        }

        if (peak_norm > 0.001f)
        {
            const float cap_y = plot.getBottom() - plot.getHeight() * peak_norm;
            g.setColour(bandColour(accents, peak_norm).withAlpha(0.9f));
            g.fillRect(juce::Rectangle<float>(x, cap_y, bar_w, 1.5f));
        }
    }

    g.setColour(theme.bgPanelBorder);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.0f, 1.0f);
}

}  // namespace jyglobalvst::tray
