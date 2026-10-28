#include "PluginEditor.h"

ADPCMDecayEditor::ADPCMDecayEditor(ADPCMDecayProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    sectionLabel0.setText("Input", juce::dontSendNotification);
    sectionLabel0.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel0.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel0);
    sectionLabel1.setText("Encode", juce::dontSendNotification);
    sectionLabel1.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel1.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel1);
    sectionLabel2.setText("Decode", juce::dontSendNotification);
    sectionLabel2.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel2.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel2);
    sectionLabel3.setText("Saturation", juce::dontSendNotification);
    sectionLabel3.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel3.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel3);
    sectionLabel4.setText("Filter", juce::dontSendNotification);
    sectionLabel4.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel4.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel4);
    sectionLabel5.setText("Quantization", juce::dontSendNotification);
    sectionLabel5.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel5.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel5);
    sectionLabel6.setText("Corruption", juce::dontSendNotification);
    sectionLabel6.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel6.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel6);
    sectionLabel7.setText("Leakage", juce::dontSendNotification);
    sectionLabel7.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel7.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel7);
    sectionLabel8.setText("Output", juce::dontSendNotification);
    sectionLabel8.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel8.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel8);
    sectionLabel9.setText("Mix", juce::dontSendNotification);
    sectionLabel9.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    sectionLabel9.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0c0));
    addAndMakeVisible(sectionLabel9);
    input_gain_dbSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    input_gain_dbSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    input_gain_dbSlider.setName("input_gain_db");
    addAndMakeVisible(input_gain_dbSlider);
    encode_bit_depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    encode_bit_depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    encode_bit_depthSlider.setName("encode_bit_depth");
    addAndMakeVisible(encode_bit_depthSlider);
    encode_sample_rate_reductionSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    encode_sample_rate_reductionSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    encode_sample_rate_reductionSlider.setName("encode_sample_rate_reduction");
    addAndMakeVisible(encode_sample_rate_reductionSlider);
    encode_mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    encode_mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    encode_mixSlider.setName("encode_mix");
    addAndMakeVisible(encode_mixSlider);
    decode_bit_depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    decode_bit_depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    decode_bit_depthSlider.setName("decode_bit_depth");
    addAndMakeVisible(decode_bit_depthSlider);
    decode_sample_rate_reductionSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    decode_sample_rate_reductionSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    decode_sample_rate_reductionSlider.setName("decode_sample_rate_reduction");
    addAndMakeVisible(decode_sample_rate_reductionSlider);
    decode_mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    decode_mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    decode_mixSlider.setName("decode_mix");
    addAndMakeVisible(decode_mixSlider);
    saturation_drive_dbSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    saturation_drive_dbSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    saturation_drive_dbSlider.setName("saturation_drive_db");
    addAndMakeVisible(saturation_drive_dbSlider);
    saturation_output_dbSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    saturation_output_dbSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    saturation_output_dbSlider.setName("saturation_output_db");
    addAndMakeVisible(saturation_output_dbSlider);
    filter_cutoffSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    filter_cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    filter_cutoffSlider.setName("filter_cutoff");
    addAndMakeVisible(filter_cutoffSlider);
    quantization_levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    quantization_levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    quantization_levelSlider.setName("quantization_level");
    addAndMakeVisible(quantization_levelSlider);
    quantization_colorBox.setName("quantization_color");
    addAndMakeVisible(quantization_colorBox);
    corruption_densitySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    corruption_densitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    corruption_densitySlider.setName("corruption_density");
    addAndMakeVisible(corruption_densitySlider);
    leakage_rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    leakage_rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    leakage_rateSlider.setName("leakage_rate");
    addAndMakeVisible(leakage_rateSlider);
    leakage_depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    leakage_depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    leakage_depthSlider.setName("leakage_depth");
    addAndMakeVisible(leakage_depthSlider);
    output_gain_dbSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    output_gain_dbSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    output_gain_dbSlider.setName("output_gain_db");
    addAndMakeVisible(output_gain_dbSlider);
    mix_mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mix_mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    mix_mixSlider.setName("mix_mix");
    addAndMakeVisible(mix_mixSlider);
    saturation_asymmetrySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    saturation_asymmetrySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    saturation_asymmetrySlider.setName("saturation_asymmetry");
    addAndMakeVisible(saturation_asymmetrySlider);
    filter_orderBox.setName("filter_order");
    addAndMakeVisible(filter_orderBox);
    corruption_amp_jitterSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    corruption_amp_jitterSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    corruption_amp_jitterSlider.setName("corruption_amp_jitter");
    addAndMakeVisible(corruption_amp_jitterSlider);
    corruption_polarityBox.setName("corruption_polarity");
    addAndMakeVisible(corruption_polarityBox);
    corruption_lowpassSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    corruption_lowpassSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    corruption_lowpassSlider.setName("corruption_lowpass");
    addAndMakeVisible(corruption_lowpassSlider);
    leakage_smoothnessSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    leakage_smoothnessSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    leakage_smoothnessSlider.setName("leakage_smoothness");
    addAndMakeVisible(leakage_smoothnessSlider);


    input_gain_dbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "input_gain_db", input_gain_dbSlider);
    encode_bit_depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "encode_bit_depth", encode_bit_depthSlider);
    encode_sample_rate_reductionAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "encode_sample_rate_reduction", encode_sample_rate_reductionSlider);
    encode_mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "encode_mix", encode_mixSlider);
    decode_bit_depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "decode_bit_depth", decode_bit_depthSlider);
    decode_sample_rate_reductionAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "decode_sample_rate_reduction", decode_sample_rate_reductionSlider);
    decode_mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "decode_mix", decode_mixSlider);
    saturation_drive_dbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "saturation_drive_db", saturation_drive_dbSlider);
    saturation_output_dbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "saturation_output_db", saturation_output_dbSlider);
    filter_cutoffAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "filter_cutoff", filter_cutoffSlider);
    quantization_levelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "quantization_level", quantization_levelSlider);
    quantization_colorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "quantization_color", quantization_colorBox);
    corruption_densityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "corruption_density", corruption_densitySlider);
    leakage_rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "leakage_rate", leakage_rateSlider);
    leakage_depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "leakage_depth", leakage_depthSlider);
    output_gain_dbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "output_gain_db", output_gain_dbSlider);
    mix_mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "mix_mix", mix_mixSlider);
    saturation_asymmetryAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "saturation_asymmetry", saturation_asymmetrySlider);
    filter_orderAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "filter_order", filter_orderBox);
    corruption_amp_jitterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "corruption_amp_jitter", corruption_amp_jitterSlider);
    corruption_polarityAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "corruption_polarity", corruption_polarityBox);
    corruption_lowpassAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "corruption_lowpass", corruption_lowpassSlider);
    leakage_smoothnessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "leakage_smoothness", leakage_smoothnessSlider);


    setSize(820, 1328);
}

ADPCMDecayEditor::~ADPCMDecayEditor() {}

void ADPCMDecayEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));


    g.setColour(juce::Colour(0xffc0c0c0));
    g.setFont(juce::FontOptions(20.0f));
    g.drawFittedText("ADPCMDecay", getLocalBounds().removeFromTop(40),
                     juce::Justification::centred, 1);
}

void ADPCMDecayEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(44); // Title space

    sectionLabel0.setBounds(14, 58, 792, 18);
    sectionLabel1.setBounds(14, 166, 792, 18);
    sectionLabel2.setBounds(14, 274, 792, 18);
    sectionLabel3.setBounds(14, 382, 792, 18);
    sectionLabel4.setBounds(14, 490, 792, 18);
    sectionLabel5.setBounds(14, 598, 792, 18);
    sectionLabel6.setBounds(14, 706, 792, 18);
    sectionLabel7.setBounds(14, 814, 792, 18);
    sectionLabel8.setBounds(14, 922, 792, 18);
    sectionLabel9.setBounds(14, 1030, 792, 18);
    input_gain_dbSlider.setBounds(8, 76, 72, 84);
    encode_bit_depthSlider.setBounds(8, 184, 72, 84);
    encode_sample_rate_reductionSlider.setBounds(76, 184, 72, 84);
    encode_mixSlider.setBounds(140, 184, 72, 84);
    decode_bit_depthSlider.setBounds(8, 292, 72, 84);
    decode_sample_rate_reductionSlider.setBounds(76, 292, 72, 84);
    decode_mixSlider.setBounds(140, 292, 72, 84);
    saturation_drive_dbSlider.setBounds(8, 400, 72, 84);
    saturation_output_dbSlider.setBounds(76, 400, 72, 84);
    filter_cutoffSlider.setBounds(8, 508, 72, 84);
    quantization_levelSlider.setBounds(8, 616, 72, 84);
    quantization_colorBox.setBounds(92, 620, 124, 72);
    corruption_densitySlider.setBounds(8, 724, 72, 84);
    leakage_rateSlider.setBounds(8, 832, 72, 84);
    leakage_depthSlider.setBounds(76, 832, 72, 84);
    output_gain_dbSlider.setBounds(8, 940, 72, 84);
    mix_mixSlider.setBounds(8, 1048, 72, 84);
    saturation_asymmetrySlider.setBounds(8, 1144, 189, 78);
    filter_orderBox.setBounds(213, 1144, 189, 78);
    corruption_amp_jitterSlider.setBounds(418, 1144, 189, 78);
    corruption_polarityBox.setBounds(623, 1144, 189, 78);
    corruption_lowpassSlider.setBounds(8, 1234, 189, 78);
    leakage_smoothnessSlider.setBounds(213, 1234, 189, 78);

}
