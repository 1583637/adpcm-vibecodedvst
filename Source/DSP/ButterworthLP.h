/*
    ButterworthLP.h — Maximally-flat lowpass with selectable order (6-48 dB/oct)
    Category: Filters
    Parameters: cutoff (20-20000 Hz), order (choice: 6/12/24/36/48 dB/oct)
*/

#pragma once
#include "DSPModule.h"
#include <array>

namespace vybecode {

class ButterworthLP : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "butterworth_lp"; }
    juce::String getModuleName() const override { return "Butterworth Lowpass"; }

private:
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* orderParam = nullptr;

    // Fixed maximum cascade pre-allocated in prepare(): up to 4 biquads
    // (48 dB/oct) plus one first-order section (the 6 dB/oct setting).
    static constexpr int kMaxBiquads = 4;
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
        float processSample(float x) {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            z1 += 1.0e-18f; z1 -= 1.0e-18f; // denormal guard
            z2 += 1.0e-18f; z2 -= 1.0e-18f;
            return y;
        }
        void resetState() { z1 = z2 = 0.0f; }
    };

    std::vector<std::array<Biquad, kMaxBiquads>> biquads;  // [channel][stage]
    std::vector<float> onePoleState;                       // first-order section per channel

    juce::SmoothedValue<float> cutoffSmoother;
    double currentSampleRate = 44100.0;
    int numChannels = 2;
};

} // namespace vybecode
