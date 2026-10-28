/*
    Bitcrusher.h — Sample rate and bit depth reduction for lo-fi effects
    Category: Effects
    Parameters: bit_depth (1-16, default 8), sample_rate_reduction (1-100, default 1),
                mix (0-1, default 1.0)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class Bitcrusher : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "bitcrusher"; }
    juce::String getModuleName() const override { return "Bitcrusher"; }

private:
    std::atomic<float>* bitDepthParam = nullptr;
    std::atomic<float>* sampleRateReductionParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    juce::SmoothedValue<float> mixSmoother;

    // Sample-and-hold state per channel
    float holdSampleL = 0.0f;
    float holdSampleR = 0.0f;
    float holdCounter = 0.0f;
};

} // namespace vybecode
