/*
    GainModule.cpp — Simple gain/volume control with smoothing
*/

#include "GainModule.h"

namespace vybecode {

void GainModule::prepare(const ModuleSpec& spec) {
    currentSampleRate = spec.sampleRate;
    gainSmoother.reset(spec.sampleRate, 0.02); // 20ms smoothing
    // Seed the smoother from the CURRENT param value (not a hardcoded unity),
    // so the first block after prepare does not ramp/swell from 1.0 -> saved gain.
    // gainDbParam may be null at prepare time in some assemblies (updateFromAPVTS
    // runs after) -> fall back to unity in that case.
    const float g = (gainDbParam != nullptr)
        ? juce::jmax(juce::Decibels::decibelsToGain(gainDbParam->load()), 1.0e-4f)
        : 1.0f;
    gainSmoother.setCurrentAndTargetValue(g);
}

void GainModule::process(juce::AudioBuffer<float>& buffer) {
    if (gainDbParam != nullptr) {
        // Clamp the linear target to a strictly-positive floor before handing it to
        // the Multiplicative smoother so it can never reach 0 / produce NaNs at the
        // bottom of the range (-60 dB already maps to ~1e-3, so musical range is untouched).
        float targetGain = juce::jmax(juce::Decibels::decibelsToGain(gainDbParam->load()), 1.0e-4f);
        gainSmoother.setTargetValue(targetGain);
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Single per-sample loop so the soft ceiling ALWAYS runs (getNextValue() returns
    // the held target when not smoothing, so this is correct in both cases and removes
    // the old two-branch desync). Adds a transparent soft ceiling AFTER the gain
    // multiply: effectively unity below ~-3 dBFS, only rounds peaks above +/-0.9 so a
    // glue-bus signal pushed +12 dB can never run away past full scale.
    for (int sample = 0; sample < numSamples; ++sample) {
        const float g = gainSmoother.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch) {
            float x = buffer.getWritePointer(ch)[sample] * g;
            const float a = std::abs(x);
            if (a > 0.9f)
                x = std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f), x);
            buffer.getWritePointer(ch)[sample] = x;
        }
    }
}

void GainModule::reset() {
    // Do NOT snap to unity on reset (the host calls reset() on transport relocate /
    // play-stop-play / preset load). Re-seed the smoother to the CURRENT param value
    // so the user's gain is preserved instead of jumping to 0 dB -> no transport click.
    const float g = (gainDbParam != nullptr)
        ? juce::jmax(juce::Decibels::decibelsToGain(gainDbParam->load()), 1.0e-4f)
        : 1.0f;
    gainSmoother.setCurrentAndTargetValue(g);
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
GainModule::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "gain_db", 1},
        prefix + "Gain",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return params;
}

void GainModule::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& prefix) {
    gainDbParam = getParam(apvts, prefix, "gain_db");
}

} // namespace vybecode
