/*
    SoftClipper.h — Analog-style soft clipping using polynomial/tanh
    waveshaping. Asymmetry adds even harmonics.
    Category: Dynamics
    Parameters: drive_db (0-36, default 12), output_db (-12 to 0, default 0),
                asymmetry (0-1, default 0)
*/

#pragma once
#include "DSPModule.h"

namespace vybecode {

class SoftClipperModule : public DSPModule {
public:
    void prepare(const ModuleSpec& spec) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) override;
    void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& prefix) override;

    juce::String getModuleID() const override { return "soft_clipper"; }
    juce::String getModuleName() const override { return "Soft Clipper"; }

private:
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* asymmetryParam = nullptr;

    juce::SmoothedValue<float> driveSmoother;
    juce::SmoothedValue<float> outputSmoother;
    juce::SmoothedValue<float> asymmetrySmoother;
};

} // namespace vybecode
