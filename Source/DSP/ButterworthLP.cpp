/*
    ButterworthLP.cpp — Maximally-flat lowpass with selectable order (6-48 dB/oct)
*/

#include "ButterworthLP.h"
#include <cmath>

namespace vybecode {

void ButterworthLP::prepare(const ModuleSpec& spec) {
    currentSampleRate = spec.sampleRate;
    numChannels = spec.numChannels;

    biquads.assign(static_cast<size_t>(numChannels), {});
    onePoleState.assign(static_cast<size_t>(numChannels), 0.0f);

    cutoffSmoother.reset(spec.sampleRate, 0.02); // 20ms smoothing
    cutoffSmoother.setCurrentAndTargetValue(8000.0f);
}

void ButterworthLP::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), numChannels);

    float baseCutoff = (cutoffParam != nullptr) ? cutoffParam->load() : 8000.0f;
    const int orderIdx = (orderParam != nullptr) ? static_cast<int>(orderParam->load()) : 2;

    // Choice index → analog order: 6/12/24/36/48 dB/oct = order 1/2/4/6/8
    static constexpr int kOrders[5] = { 1, 2, 4, 6, 8 };
    const int order = kOrders[juce::jlimit(0, 4, orderIdx)];
    const int numBiquads = order / 2;
    const bool hasOnePole = (order % 2) == 1;

    cutoffSmoother.setTargetValue(juce::jlimit(20.0f, 20000.0f, baseCutoff));
    const float freq = juce::jmin(cutoffSmoother.skip(numSamples),
                                  static_cast<float>(currentSampleRate) * 0.45f);

    // Recompute coefficients per block (smoothed cutoff avoids zipper noise).
    // Butterworth pole Qs computed analytically: Q_k = 1 / (2·cos((2k+1)π/2N)).
    const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float>(currentSampleRate);
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);

    float stageB0[kMaxBiquads], stageB1[kMaxBiquads], stageB2[kMaxBiquads];
    float stageA1[kMaxBiquads], stageA2[kMaxBiquads];
    for (int s = 0; s < numBiquads; ++s) {
        const float theta = (2.0f * static_cast<float>(s) + 1.0f)
                            * juce::MathConstants<float>::pi
                            / (2.0f * static_cast<float>(order));
        const float q = 1.0f / (2.0f * std::cos(theta));
        const float alpha = sinw0 / (2.0f * q);
        const float a0 = 1.0f + alpha;
        stageB0[s] = ((1.0f - cosw0) * 0.5f) / a0;
        stageB1[s] = (1.0f - cosw0) / a0;
        stageB2[s] = ((1.0f - cosw0) * 0.5f) / a0;
        stageA1[s] = (-2.0f * cosw0) / a0;
        stageA2[s] = (1.0f - alpha) / a0;
    }

    // First-order TPT coefficient for odd orders (the 6 dB/oct setting)
    const float g = std::tan(juce::MathConstants<float>::pi * freq
                             / static_cast<float>(currentSampleRate));
    const float G = g / (1.0f + g);

    for (int ch = 0; ch < channels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        auto& chain = biquads[static_cast<size_t>(ch)];
        for (int s = 0; s < numBiquads; ++s) {
            chain[static_cast<size_t>(s)].b0 = stageB0[s];
            chain[static_cast<size_t>(s)].b1 = stageB1[s];
            chain[static_cast<size_t>(s)].b2 = stageB2[s];
            chain[static_cast<size_t>(s)].a1 = stageA1[s];
            chain[static_cast<size_t>(s)].a2 = stageA2[s];
        }

        for (int sample = 0; sample < numSamples; ++sample) {
            float x = data[sample];
            if (hasOnePole) {
                float& z = onePoleState[static_cast<size_t>(ch)];
                const float t = (x - z) * G;
                const float lp = t + z;
                z = lp + t;
                z += 1.0e-18f; z -= 1.0e-18f; // denormal guard
                x = lp;
            }
            for (int s = 0; s < numBiquads; ++s)
                x = chain[static_cast<size_t>(s)].processSample(x);
            data[sample] = x;
        }
    }
}

void ButterworthLP::reset() {
    for (auto& chain : biquads)
        for (auto& f : chain)
            f.resetState();
    std::fill(onePoleState.begin(), onePoleState.end(), 0.0f);
    cutoffSmoother.setCurrentAndTargetValue(8000.0f);
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
ButterworthLP::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "cutoff", 1},
        prefix + "Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
        8000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{prefix + "order", 1},
        prefix + "Order",
        juce::StringArray{"6 dB/oct", "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct"},
        2));

    return params;
}

void ButterworthLP::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                      const juce::String& prefix) {
    cutoffParam = getParam(apvts, prefix, "cutoff");
    orderParam  = getParam(apvts, prefix, "order");
}

} // namespace vybecode
