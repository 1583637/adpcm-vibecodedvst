/*
    DustGenerator.h — Stochastic impulse source: Poisson-distributed clicks
    from sparse geiger ticks to dense crackle (note-agnostic)
    Category: Generators
    Parameters: density (0.1-2000 Hz), amp_jitter (0-1),
                polarity (Unipolar/Bipolar), lowpass (200-20000 Hz)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class DustGenerator : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "dust_generator"; }
    juce::String getModuleName() const override { return "Dust / Geiger Impulses"; }

private:
    float nextRandom();   // xorshift RNG, uniform [0, 1)

    std::atomic<float>* densityParam = nullptr;
    std::atomic<float>* jitterParam = nullptr;
    std::atomic<float>* polarityParam = nullptr;
    std::atomic<float>* lowpassParam = nullptr;

    uint32_t rngState = 0x12345678;
    float lpState = 0.0f;   // one-pole lowpass that rounds clicks into ticks
    double currentSampleRate = 44100.0;
};

} // namespace vybecode
