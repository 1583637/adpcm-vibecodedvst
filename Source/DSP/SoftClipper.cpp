/*
    SoftClipper.cpp — Analog-style soft clipping
*/

#include "SoftClipper.h"

namespace vybecode {

void SoftClipperModule::prepare(const ModuleSpec& spec) {
    driveSmoother.reset(spec.sampleRate, 0.02);
    outputSmoother.reset(spec.sampleRate, 0.02);
    asymmetrySmoother.reset(spec.sampleRate, 0.02);

    driveSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(12.0f));
    outputSmoother.setCurrentAndTargetValue(1.0f);
    asymmetrySmoother.setCurrentAndTargetValue(0.0f);
}

void SoftClipperModule::process(juce::AudioBuffer<float>& buffer) {
    const float driveDb  = driveParam     ? driveParam->load()     : 12.0f;
    const float outputDb = outputParam    ? outputParam->load()    : 0.0f;
    const float asym     = asymmetryParam ? asymmetryParam->load() : 0.0f;

    driveSmoother.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    outputSmoother.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
    asymmetrySmoother.setTargetValue(asym);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample) {
        float drive = driveSmoother.getNextValue();
        float output = outputSmoother.getNextValue();
        float asymmetry = asymmetrySmoother.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch) {
            float x = buffer.getReadPointer(ch)[sample] * drive;

            // Add DC offset for asymmetry (generates even harmonics)
            x += asymmetry * 0.2f;

            // Soft clip using tanh
            float y = std::tanh(x);

            // Remove DC offset introduced by asymmetry
            y -= std::tanh(asymmetry * 0.2f);

            buffer.getWritePointer(ch)[sample] = y * output;
        }
    }
}

void SoftClipperModule::reset() {
    driveSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(12.0f));
    outputSmoother.setCurrentAndTargetValue(1.0f);
    asymmetrySmoother.setCurrentAndTargetValue(0.0f);
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
SoftClipperModule::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "drive_db", 1},
        prefix + "Drive",
        juce::NormalisableRange<float>(0.0f, 36.0f, 0.1f),
        12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "output_db", 1},
        prefix + "Output",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "asymmetry", 1},
        prefix + "Asymmetry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f));

    return params;
}

void SoftClipperModule::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                          const juce::String& prefix) {
    driveParam     = getParam(apvts, prefix, "drive_db");
    outputParam    = getParam(apvts, prefix, "output_db");
    asymmetryParam = getParam(apvts, prefix, "asymmetry");
}

} // namespace vybecode
