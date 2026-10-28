/*
    DustGenerator.cpp — Stochastic Poisson impulse source
*/

#include "DustGenerator.h"
#include <cmath>

namespace vybecode {

void DustGenerator::prepare(const ModuleSpec& spec) {
    currentSampleRate = spec.sampleRate;
    reset();
}

float DustGenerator::nextRandom() {
    // xorshift32 — fast, deterministic, allocation-free
    uint32_t x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return static_cast<float>(x) * (1.0f / 4294967296.0f);
}

void DustGenerator::process(juce::AudioBuffer<float>& buffer) {
    const float density   = (densityParam  != nullptr) ? densityParam->load()  : 20.0f;
    const float ampJitter = (jitterParam   != nullptr) ? jitterParam->load()   : 0.5f;
    const bool  bipolar   = (polarityParam != nullptr) ? (static_cast<int>(polarityParam->load()) == 1) : true;
    const float cutoff    = (lowpassParam  != nullptr) ? lowpassParam->load()  : 12000.0f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Poisson process: per-sample impulse probability
    const float prob = juce::jmin(1.0f, density / static_cast<float>(currentSampleRate));

    // One-pole lowpass coefficient that rounds clicks into softer ticks
    const float lpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                          * juce::jmin(cutoff, 0.45f * static_cast<float>(currentSampleRate))
                                          / static_cast<float>(currentSampleRate));

    for (int sample = 0; sample < numSamples; ++sample) {
        float impulse = 0.0f;

        if (nextRandom() < prob) {
            float amp = 1.0f - nextRandom() * ampJitter;
            if (bipolar && nextRandom() < 0.5f)
                amp = -amp;
            impulse = amp * 0.5f;   // ~-6 dBFS peak per click
        }

        // Round the click; flush denormal-range residue between impulses
        lpState += lpCoeff * (impulse - lpState);
        if (std::abs(lpState) < 1.0e-20f)
            lpState = 0.0f;

        const float out = lpState;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[sample] = out;
    }
}

void DustGenerator::reset() {
    rngState = 0x12345678;
    lpState = 0.0f;
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
DustGenerator::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "density", 1},
        prefix + "Density",
        juce::NormalisableRange<float>(0.1f, 2000.0f, 0.1f, 0.25f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "amp_jitter", 1},
        prefix + "Amplitude Jitter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{prefix + "polarity", 1},
        prefix + "Polarity",
        juce::StringArray{"Unipolar", "Bipolar"},
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "lowpass", 1},
        prefix + "Click Rounding",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f),
        12000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    return params;
}

void DustGenerator::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                      const juce::String& prefix) {
    densityParam  = getParam(apvts, prefix, "density");
    jitterParam   = getParam(apvts, prefix, "amp_jitter");
    polarityParam = getParam(apvts, prefix, "polarity");
    lowpassParam  = getParam(apvts, prefix, "lowpass");
}

} // namespace vybecode
