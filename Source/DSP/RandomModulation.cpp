/*
    RandomModulation.cpp — Perlin-noise-like smooth random modulation
*/

#include "RandomModulation.h"

namespace vybecode {

void RandomModulation::prepare(const ModuleSpec& spec) {
    sampleRate = spec.sampleRate;
    currentValue = 0.5f;
    lp1 = 0.5f;
    lp2 = 0.5f;
}

void RandomModulation::process(juce::AudioBuffer<float>& buffer) {
    const float rate = rateParam ? rateParam->load() : 1.0f;
    const float depth = depthParam ? depthParam->load() : 0.3f;
    const float smoothness = smoothnessParam ? smoothnessParam->load() : 0.7f;

    // LP cutoff frequency derived from rate parameter
    const float cutoffHz = rate;

    // Two cascaded one-pole LP filters for smooth random
    // coeff = exp(-2*pi*fc/fs), higher smoothness = more filtering
    const float baseCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoffHz
                                     / static_cast<float>(sampleRate));
    // Smoothness increases the effective filtering (raises coefficient)
    const float coeff1 = baseCoeff + (1.0f - baseCoeff) * smoothness * 0.95f;
    const float coeff2 = baseCoeff + (1.0f - baseCoeff) * smoothness * 0.98f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample) {
        // White noise source (0..1 range)
        float noise = rng.nextFloat();

        // Two-pole LP filter
        lp1 = lp1 * coeff1 + noise * (1.0f - coeff1);
        lp2 = lp2 * coeff2 + lp1 * (1.0f - coeff2);

        currentValue = lp2;

        // Apply as amplitude modulation
        float modGain = (1.0f - depth) + depth * currentValue;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[sample] *= modGain;
    }
}

void RandomModulation::reset() {
    currentValue = 0.5f;
    lp1 = 0.5f;
    lp2 = 0.5f;
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
RandomModulation::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "rate", 1},
        prefix + "Rate",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.4f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "depth", 1},
        prefix + "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "smoothness", 1},
        prefix + "Smoothness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f));

    return params;
}

void RandomModulation::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                          const juce::String& prefix) {
    rateParam = getParam(apvts, prefix, "rate");
    depthParam = getParam(apvts, prefix, "depth");
    smoothnessParam = getParam(apvts, prefix, "smoothness");
}

} // namespace vybecode
