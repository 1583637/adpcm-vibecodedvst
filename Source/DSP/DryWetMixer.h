/*
    DryWetMixer.h — Simple dry/wet mix control
    Category: Utility
    Parameters: mix (0-1, default 0.5)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class DryWetMixer : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "drywet"; }
    juce::String getModuleName() const override { return "Dry/Wet Mixer"; }

    // Call before processing the wet signal to capture the dry buffer
    void captureDryInput(const juce::AudioBuffer<float>& buffer);

private:
    std::atomic<float>* mixParam = nullptr;
    juce::SmoothedValue<float> mixSmoother;

    juce::AudioBuffer<float> dryBuffer;
    bool dryCaptured = false;
};

} // namespace vybecode
