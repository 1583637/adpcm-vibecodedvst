/*
    NoiseGenerator.cpp — White and pink noise generator
*/

#include "NoiseGenerator.h"

namespace vybecode {

void NoiseGenerator::prepare(const ModuleSpec& /*spec*/) {
    reset();
}

void NoiseGenerator::process(juce::AudioBuffer<float>& buffer) {
    const float level = (levelParam != nullptr) ? levelParam->load() : 0.5f;
    const int colorIndex = (colorParam != nullptr) ? static_cast<int>(colorParam->load()) : 0;

    const int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    if (colorIndex == 0) {
        // White noise — independent random values per channel
        for (int ch = 0; ch < numChannels; ++ch) {
            float* channelData = buffer.getWritePointer(ch);
            for (int sample = 0; sample < numSamples; ++sample) {
                channelData[sample] = (random.nextFloat() * 2.0f - 1.0f) * level;
            }
        }
    } else {
        // Pink noise — Voss-McCartney algorithm per channel
        for (int ch = 0; ch < numChannels; ++ch) {
            float* channelData = buffer.getWritePointer(ch);
            for (int sample = 0; sample < numSamples; ++sample) {
                int counter = pinkCounter[ch];

                // Determine which row to update based on trailing zeros in counter
                // Row 0 updates every sample, row 1 every 2, row 2 every 4
                for (int row = 0; row < pinkRows; ++row) {
                    if (row == 0 || (counter & ((1 << row) - 1)) == 0) {
                        pinkRunningSum[ch] -= pinkRows_[ch][row];
                        float newRandom = random.nextFloat() * 2.0f - 1.0f;
                        pinkRows_[ch][row] = newRandom;
                        pinkRunningSum[ch] += newRandom;
                    }
                }

                // Normalize: pink noise with 3 rows has amplitude ~sqrt(3)
                constexpr float pinkNormalize = 1.0f / 3.0f;
                channelData[sample] = pinkRunningSum[ch] * pinkNormalize * level;

                pinkCounter[ch] = (counter + 1) & 0x7FFFFFFF;
            }
        }
    }
}

void NoiseGenerator::reset() {
    for (int ch = 0; ch < maxChannels; ++ch) {
        pinkRunningSum[ch] = 0.0f;
        pinkCounter[ch] = 0;
        for (int row = 0; row < pinkRows; ++row)
            pinkRows_[ch][row] = 0.0f;
    }
}

std::vector<std::unique_ptr<juce::RangedAudioParameter>>
NoiseGenerator::createParameters(const juce::String& prefix) {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{prefix + "level", 1},
        prefix + "Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{prefix + "color", 1},
        prefix + "Color",
        juce::StringArray{"White", "Pink"},
        0));

    return params;
}

void NoiseGenerator::updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& prefix) {
    levelParam = getParam(apvts, prefix, "level");
    colorParam = getParam(apvts, prefix, "color");
}

} // namespace vybecode
