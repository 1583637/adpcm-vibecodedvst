/*
    Bitcrusher.cpp — Sample rate and bit depth reduction for lo-fi effects
*/

#include "Bitcrusher.h"

namespace vybecode {

void Bitcrusher::prepare(const ModuleSpec& spec) {
    mixSmoother.reset(spec.sampleRate, 0.02);
    mixSmoother.setCurrentAndTargetValue(1.0f);

    holdSampleL = 0.0f;
    holdSampleR = 0.0f;
    holdCounter = 0.0f;
}

void Bitcrusher::process(juce::AudioBuffer<float>& buffer) {
    if (mixParam != nullptr)
        mixSmoother.setTargetValue(mixParam->load());

    float bitDepth = 8.0f;
    if (bitDepthParam != nullptr)
        bitDepth = bitDepthParam->load();

    float srReduction = 1.0f;
    if (sampleRateReductionParam != nullptr)
        srReduction = sampleRateReductionParam->load();

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Quantization levels from bit depth
    float levels = std::pow(2.0f, std::round(bitDepth));

    for (int sample = 0; sample < numSamples; ++sample) {
        float mix = mixSmoother.getNextValue();

        // Sample-and-hold for rate reduction
        holdCounter += 1.0f;
        bool updateHold = holdCounter >= srReduction;
        if (updateHold)
            holdCounter -= srReduction;

        // Left channel
        if (numChannels > 0) {
            float* chData = buffer.getWritePointer(0);
            float dry = chData[sample];

            if (updateHold) {
                // Bit depth reduction: quantize to discrete levels
                float crushed = std::round(dry * levels) / levels;
                holdSampleL = crushed;
            }

            chData[sample] = dry * (1.0f - mix) + holdSampleL * mix;
        }

        // Right channel
        if (numChannels > 1) {
            float* chData = buffer.getWritePointer(1);
            float dry = chData[sample];

            if (updateHold) {
                float crushed = std::round(dry * levels) / levels;
                holdSampleR = crushed;
            }

            chData[sample] = dry * (1.0f - mix) + holdSampleR * mix;
        }
    }
}

void Bitcrusher::reset() {
    holdSampleL = 0.0f;
    holdSampleR = 0.0f;
    holdCounter = 0.0f;
    mixSmoother.setCurrentAndTargetValue(1.0f);
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
Bitcrusher::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "bit_depth", 1},
        prefix + "Bit Depth",
        juce::NormalisableRange<float>(1.0f, 16.0f, 1.0f),
        8.0f,
        juce::AudioParameterFloatAttributes().withLabel("bits")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "sample_rate_reduction", 1},
        prefix + "SR Reduction",
        juce::NormalisableRange<float>(1.0f, 100.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("x")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "mix", 1},
        prefix + "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f));

    return params;
}

void Bitcrusher::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& prefix) {
    bitDepthParam = getParam(apvts, prefix, "bit_depth");
    sampleRateReductionParam = getParam(apvts, prefix, "sample_rate_reduction");
    mixParam = getParam(apvts, prefix, "mix");
}

} // namespace vybecode
