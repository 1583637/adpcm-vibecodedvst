/*
    RandomModulation.h — Perlin-noise-like smooth random modulation
    Category: Modulation
    Parameters: rate (0.1-20 Hz), depth (0-1), smoothness (0-1)

    Uses LP-filtered white noise for smooth random modulation.
    Rate controls LP cutoff, smoothness controls steepness.
*/

#pragma once
#include "DSPModule.h"
#include <cmath>

namespace vybecode {

class RandomModulation : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "random_mod"; }
    juce::String getModuleName() const override { return "Random Modulation"; }

    float getCurrentValue() const { return currentValue; }

private:
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* smoothnessParam = nullptr;

    double sampleRate = 44100.0;
    float currentValue = 0.5f;

    // Two-pole LP filter state for smooth random
    float lp1 = 0.0f;
    float lp2 = 0.0f;

    juce::Random rng;
};

} // namespace vybecode
