#pragma once

#include "builtin_ids.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

namespace jyglobalvst::engine {

class CompressorProcessor : public juce::AudioPluginInstance
{
public:
    CompressorProcessor();
    ~CompressorProcessor() override;

    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    const juce::String getName() const override { return "Compressor"; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void fillInPluginDescription(juce::PluginDescription& description) const override;

    int getLatencySamples() const { return 0; }

    static constexpr int kNumParams = builtin::compressor::NUM_PARAMETERS;

    float getThresholdDb() const { return threshold_db_.load(std::memory_order_relaxed); }
    void setThresholdDb(float db)
    {
        threshold_db_.store(std::max(builtin::compressor::THRESHOLD_MIN_DB,
                                     std::min(builtin::compressor::THRESHOLD_MAX_DB, db)),
                            std::memory_order_relaxed);
    }

    float getRatio() const { return ratio_.load(std::memory_order_relaxed); }
    void setRatio(float r)
    {
        ratio_.store(std::max(builtin::compressor::RATIO_MIN,
                              std::min(builtin::compressor::RATIO_MAX, r)),
                     std::memory_order_relaxed);
    }

    float getAttackMs() const { return attack_ms_.load(std::memory_order_relaxed); }
    void setAttackMs(float ms)
    {
        attack_ms_.store(std::max(builtin::compressor::ATTACK_MIN_MS,
                                  std::min(builtin::compressor::ATTACK_MAX_MS, ms)),
                         std::memory_order_relaxed);
    }

    float getReleaseMs() const { return release_ms_.load(std::memory_order_relaxed); }
    void setReleaseMs(float ms)
    {
        release_ms_.store(std::max(builtin::compressor::RELEASE_MIN_MS,
                                   std::min(builtin::compressor::RELEASE_MAX_MS, ms)),
                          std::memory_order_relaxed);
    }

    float getMakeupDb() const { return makeup_db_.load(std::memory_order_relaxed); }
    void setMakeupDb(float db)
    {
        makeup_db_.store(std::max(builtin::compressor::MAKEUP_MIN_DB,
                                  std::min(builtin::compressor::MAKEUP_MAX_DB, db)),
                         std::memory_order_relaxed);
    }

private:
    double sample_rate_ = 0.0;

    std::atomic<float> threshold_db_ {-20.0f};
    std::atomic<float> ratio_ {4.0f};
    std::atomic<float> attack_ms_ {10.0f};
    std::atomic<float> release_ms_ {100.0f};
    std::atomic<float> makeup_db_ {0.0f};

    float envelope_ = 0.0f;
    float attack_coeff_ = 0.0f;
    float release_coeff_ = 0.0f;

    void updateCoefficients();
    void applyCompression(float* left, float* right, int num_samples) noexcept;
};

}  // namespace jyglobalvst::engine
