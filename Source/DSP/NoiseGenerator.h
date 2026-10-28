/*
    NoiseGenerator.h — White and pink noise generator
    Category: Generators
    Parameters: level (0-1), color (choice: white/pink)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class NoiseGenerator : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "noise_gen"; }
    juce::String getModuleName() const override { return "Noise Generator"; }

private:
    std::atomic<float>* levelParam = nullptr;
    std::atomic<float>* colorParam = nullptr;

    juce::Random random;

    // Voss-McCartney pink noise state (per channel, max 2)
    static constexpr int maxChannels = 2;
    static constexpr int pinkRows = 3;
    float pinkRunningSum[maxChannels] = {};
    float pinkRows_[maxChannels][pinkRows] = {};
    int pinkCounter[maxChannels] = {};
};

} // namespace vybecode
