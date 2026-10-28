/*
    DryWetMixer.cpp — Simple dry/wet mix control
*/

#include "DryWetMixer.h"

namespace vybecode {

void DryWetMixer::prepare(const ModuleSpec& spec) {
    mixSmoother.reset(spec.sampleRate, 0.02);
    mixSmoother.setCurrentAndTargetValue(0.5f);

    dryBuffer.setSize(spec.numChannels, spec.maxBlockSize);
    dryBuffer.clear();
    dryCaptured = false;
}

void DryWetMixer::captureDryInput(const juce::AudioBuffer<float>& buffer) {
    const int numChannels = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
    const int numSamples = juce::jmin(buffer.getNumSamples(), dryBuffer.getNumSamples());

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    dryCaptured = true;
}

void DryWetMixer::process(juce::AudioBuffer<float>& buffer) {
    if (mixParam != nullptr)
        mixSmoother.setTargetValue(mixParam->load());

    if (!dryCaptured)
        return; // No dry signal captured, pass through wet unchanged

    const int numChannels = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
    const int numSamples = juce::jmin(buffer.getNumSamples(), dryBuffer.getNumSamples());

    for (int sample = 0; sample < numSamples; ++sample) {
        float mix = mixSmoother.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch) {
            float dry = dryBuffer.getReadPointer(ch)[sample];
            float wet = buffer.getReadPointer(ch)[sample];
            buffer.getWritePointer(ch)[sample] = dry * (1.0f - mix) + wet * mix;
        }
    }

    dryCaptured = false;
}

void DryWetMixer::reset() {
    mixSmoother.setCurrentAndTargetValue(0.5f);
    dryBuffer.clear();
    dryCaptured = false;
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
DryWetMixer::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "mix", 1},
        prefix + "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));

    return params;
}

void DryWetMixer::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& prefix) {
    mixParam = getParam(apvts, prefix, "mix");
}

} // namespace vybecode
