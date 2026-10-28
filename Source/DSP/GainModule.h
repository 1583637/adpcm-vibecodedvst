/*
    GainModule.h — Simple gain/volume control with smoothing
    Category: Utility
    Parameters: gain_db (-60 to +12 dB, default 0)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class GainModule : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "gain"; }
    juce::String getModuleName() const override { return "Gain"; }

private:
    std::atomic<float>* gainDbParam = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmoother;
    double currentSampleRate = 44100.0;
};

} // namespace vybecode
