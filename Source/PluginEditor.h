#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ADPCMDecayEditor : public juce::AudioProcessorEditor
{
public:
    explicit ADPCMDecayEditor(ADPCMDecayProcessor&);
    ~ADPCMDecayEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ADPCMDecayProcessor& processorRef;

    // ── Controls ─────────────────────────────────────────
    juce::Label sectionLabel0;
    juce::Label sectionLabel1;
    juce::Label sectionLabel2;
    juce::Label sectionLabel3;
    juce::Label sectionLabel4;
    juce::Label sectionLabel5;
    juce::Label sectionLabel6;
    juce::Label sectionLabel7;
    juce::Label sectionLabel8;
    juce::Label sectionLabel9;
    juce::Slider input_gain_dbSlider;
    juce::Slider encode_bit_depthSlider;
    juce::Slider encode_sample_rate_reductionSlider;
    juce::Slider encode_mixSlider;
    juce::Slider decode_bit_depthSlider;
    juce::Slider decode_sample_rate_reductionSlider;
    juce::Slider decode_mixSlider;
    juce::Slider saturation_drive_dbSlider;
    juce::Slider saturation_output_dbSlider;
    juce::Slider filter_cutoffSlider;
    juce::Slider quantization_levelSlider;
    juce::ComboBox quantization_colorBox;
    juce::Slider corruption_densitySlider;
    juce::Slider leakage_rateSlider;
    juce::Slider leakage_depthSlider;
    juce::Slider output_gain_dbSlider;
    juce::Slider mix_mixSlider;
    juce::Slider saturation_asymmetrySlider;
    juce::ComboBox filter_orderBox;
    juce::Slider corruption_amp_jitterSlider;
    juce::ComboBox corruption_polarityBox;
    juce::Slider corruption_lowpassSlider;
    juce::Slider leakage_smoothnessSlider;


    // ── Attachments (declared AFTER sliders for correct destruction order) ──
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> input_gain_dbAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> encode_bit_depthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> encode_sample_rate_reductionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> encode_mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decode_bit_depthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decode_sample_rate_reductionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decode_mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturation_drive_dbAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturation_output_dbAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filter_cutoffAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> quantization_levelAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> quantization_colorAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> corruption_densityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leakage_rateAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leakage_depthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> output_gain_dbAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mix_mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturation_asymmetryAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filter_orderAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> corruption_amp_jitterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> corruption_polarityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> corruption_lowpassAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leakage_smoothnessAttach;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADPCMDecayEditor)
};
