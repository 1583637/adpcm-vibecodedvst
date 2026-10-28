/*
    ModMatrix.h — the runtime modulation matrix engine (VybeCode DSP, Pillar 2).

    Header-only, shipped into every generated project (like PresetManager.h). It owns a
    SOURCE BANK (4 LFOs, 2 ADSR envelopes, 4 macros, a step sequencer, and the performance
    sources velocity / mod-wheel / aftertouch / pitch-bend) and 16 routing SLOTS. Each slot
    is three automatable APVTS params — mmN_src (a Choice over the source bank), mmN_dest (a
    Choice over the plugin's registered params), mmN_depth (a bipolar Float) — so the plugin's
    END USER can route anything to anything, live, and presets/automation capture it.

    Each block: process() advances the sources, reads the 16 slots, and accumulates
    `depth · source` into a per-destination accumulator. The processor then reads
    getDestModulation(i) per destination and calls `<module>.setModulation(param, reach · v)`
    (the assembler bakes the per-parameter `reach`, unit-derived). The module's base-class
    shadow (DSPModule.h) folds that offset in through the param's own NormalisableRange — so
    a matrix route to any param of any module Just Works, with no per-module code.

    Real-time safe: all state is fixed-size, no allocation after prepare().

    Copyright (c) 2026 PromptMonster Media Ltd / VybeCode. All rights reserved.
*/

#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

// Number of routing slots — author-defined (the Modulation Designer). The assembler emits
// `#define VYBE_MM_SLOTS <n>` in PluginProcessor.h BEFORE this header is included when the author
// asks for a non-default count; absent that, it defaults to 16, so a plugin built without a
// modulation definition is identical to before.
#ifndef VYBE_MM_SLOTS
 #define VYBE_MM_SLOTS 16
#endif

// Presence flag (UI_DSP_GUI_V1 §4/§14). A designer-placed strip/grid guards its engine binding on
// VYBE_HAS_MODMATRIX so the SAME component code compiles in a DSP build (this header present) and in
// a standalone export (VybeModMatrix.h present instead). This header being included at all means the
// real engine is here, so define it 1; the editor's own __has_include probe sets it for the
// standalone case where neither this nor the engine header has yet defined it.
#ifndef VYBE_HAS_MODMATRIX
 #define VYBE_HAS_MODMATRIX 1
#endif

// The designer-placeable affordances (VybeModDestBox / VybeModStrip / VybeModGrid) live in a shared
// header so the SAME code binds the mmN_* params in a DSP build (here) and a standalone export
// (VybeModMatrix.h includes the identical file). Single source of truth — no drift.
#include "VybeModControls.h"

namespace vybecode {

class ModMatrix {
public:
    static constexpr int kNumSlots   = VYBE_MM_SLOTS;
    static constexpr int kNumLfos    = 4;
    static constexpr int kNumEnvs    = 2;
    static constexpr int kNumMacros  = 4;
    static constexpr int kNumSteps   = 8;
    static constexpr int kMaxDests   = 256;   // registered-param headroom
    // Source-bank layout in the mmN_src Choice (index 0 = "None"):
    //   1..4  LFO 1..4   5..6  Env 1..2   7..10 Macro 1..4   11 Step Seq
    //   12 Velocity   13 Mod Wheel   14 Aftertouch   15 Pitch Bend
    static constexpr int kNumSources = 16;   // including index 0 = None

    ModMatrix() = default;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrix)

    void prepare(double sampleRate) {
        sr_ = (sampleRate > 0.0 ? sampleRate : 44100.0);
        for (auto& p : lfoPhase_) p = 0.0f;
        for (auto& r : lfoRand_)  r = 0.0f;
        for (auto& l : lfoLast_)  l = 0.0f;
        for (auto& e : envLevel_) e = 0.0f;
        for (auto& s : envStage_) s = 0;   // 0 idle,1 atk,2 dec,3 sus,4 rel
        seqPhase_ = 0.0f; seqIndex_ = 0;
        for (auto& v : source_)  v = 0.0f;
        for (auto& d : destMod_) d = 0.0f;
    }

    // Cache every param pointer once (createParameterLayout has run by construction time).
    // numDests is derived from the mm0_dest Choice option count (minus 1 for "None"), so the
    // assembler's initialise() call is a constant string that survives preset regeneration.
    void initialise(juce::AudioProcessorValueTreeState& apvts) {
        numDests_ = 0;
        if (auto* dch = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("mm0_dest")))
            numDests_ = juce::jlimit(0, kMaxDests, dch->choices.size() - 1);   // -1 = "None"
        for (int s = 0; s < kNumSlots; ++s) {
            const juce::String p = "mm" + juce::String(s) + "_";
            srcP_[s]   = apvts.getRawParameterValue(p + "src");
            destP_[s]  = apvts.getRawParameterValue(p + "dest");
            depthP_[s] = apvts.getRawParameterValue(p + "depth");
        }
        for (int i = 0; i < kNumLfos; ++i) {
            lfoRateP_[i]  = apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "_rate");
            lfoShapeP_[i] = apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "_shape");
        }
        for (int i = 0; i < kNumEnvs; ++i) {
            const juce::String e = "env" + juce::String(i + 1) + "_";
            envAtkP_[i] = apvts.getRawParameterValue(e + "attack");
            envDecP_[i] = apvts.getRawParameterValue(e + "decay");
            envSusP_[i] = apvts.getRawParameterValue(e + "sustain");
            envRelP_[i] = apvts.getRawParameterValue(e + "release");
        }
        for (int i = 0; i < kNumMacros; ++i)
            macroP_[i] = apvts.getRawParameterValue("macro" + juce::String(i + 1));
        seqRateP_ = apvts.getRawParameterValue("seq_rate");
        for (int i = 0; i < kNumSteps; ++i)
            seqStepP_[i] = apvts.getRawParameterValue("seq_step" + juce::String(i));
    }

    // ── Performance sources (fed from the processor's MIDI each block) ──
    void processMidi(const juce::MidiBuffer& midi) {
        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) {
                velocity_ = m.getFloatVelocity();
                for (int i = 0; i < kNumEnvs; ++i) { envStage_[i] = 1; }   // (re)trigger
            } else if (m.isController() && m.getControllerNumber() == 1) {
                modWheel_ = (float) m.getControllerValue() / 127.0f;
            } else if (m.isChannelPressure()) {
                aftertouch_ = (float) m.getChannelPressureValue() / 127.0f;
            } else if (m.isPitchWheel()) {
                pitchBend_ = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
            } else if (m.isNoteOff() || m.isAllNotesOff()) {
                for (int i = 0; i < kNumEnvs; ++i) if (envStage_[i] != 0) envStage_[i] = 4;
            }
        }
    }

    // Advance the sources by one block, then accumulate depth·source into each destination.
    void process(int numSamples) {
        computeSources(numSamples);
        const int nd = numDests_;
        for (int d = 0; d < nd; ++d) destMod_[d] = 0.0f;
        for (int s = 0; s < kNumSlots; ++s) {
            const int src = srcP_[s]  ? (int) (srcP_[s]->load()  + 0.5f) : 0;
            const int dst = destP_[s] ? (int) (destP_[s]->load() + 0.5f) : 0;
            if (src <= 0 || src >= kNumSources || dst <= 0 || dst > nd) continue;  // 0 = None
            destMod_[dst - 1] += (depthP_[s] ? depthP_[s]->load() : 0.0f) * source_[src];
        }
    }

    // Summed depth·source routed to destination index `d` this block ([-N, N] before reach).
    float getDestModulation(int d) const {
        return (d >= 0 && d < numDests_) ? destMod_[d] : 0.0f;
    }

private:
    void computeSources(int numSamples) {
        const float dt = (float) numSamples / (float) sr_;
        source_[0] = 0.0f;  // None

        // LFOs (bipolar [-1,1]) — indices 1..4
        for (int i = 0; i < kNumLfos; ++i) {
            const float rate  = lfoRateP_[i] ? lfoRateP_[i]->load() : 2.0f;
            const int   shape = lfoShapeP_[i] ? (int) (lfoShapeP_[i]->load() + 0.5f) : 0;
            float ph = lfoPhase_[i] + rate * dt;
            bool wrapped = false;
            while (ph >= 1.0f) { ph -= 1.0f; wrapped = true; }
            lfoPhase_[i] = ph;
            float v = 0.0f;
            switch (shape) {
                case 0: v = std::sin(juce::MathConstants<float>::twoPi * ph); break;      // Sine
                case 1: v = 4.0f * std::abs(ph - 0.5f) - 1.0f; break;                       // Triangle
                case 2: v = 2.0f * ph - 1.0f; break;                                        // Saw
                case 3: v = (ph < 0.5f) ? 1.0f : -1.0f; break;                              // Square
                default:                                                                    // Random (S&H)
                    if (wrapped || lfoRand_[i] == 0.0f) lfoRand_[i] = rng_.nextFloat() * 2.0f - 1.0f;
                    v = lfoRand_[i]; break;
            }
            source_[1 + i] = v;
        }

        // ADSR envelopes (unipolar [0,1]) — indices 5..6, advanced at block rate
        for (int i = 0; i < kNumEnvs; ++i) {
            const float atk = juce::jmax(0.001f, envAtkP_[i] ? envAtkP_[i]->load() : 0.01f);
            const float dec = juce::jmax(0.001f, envDecP_[i] ? envDecP_[i]->load() : 0.1f);
            const float sus = envSusP_[i] ? envSusP_[i]->load() : 0.7f;
            const float rel = juce::jmax(0.001f, envRelP_[i] ? envRelP_[i]->load() : 0.2f);
            float lvl = envLevel_[i];
            switch (envStage_[i]) {
                case 1: lvl += dt / atk; if (lvl >= 1.0f) { lvl = 1.0f; envStage_[i] = 2; } break;
                case 2: lvl -= dt / dec * (1.0f - sus); if (lvl <= sus) { lvl = sus; envStage_[i] = 3; } break;
                case 3: lvl = sus; break;
                case 4: lvl -= dt / rel; if (lvl <= 0.0f) { lvl = 0.0f; envStage_[i] = 0; } break;
                default: lvl = 0.0f; break;
            }
            envLevel_[i] = lvl;
            source_[5 + i] = lvl;
        }

        // Macros (unipolar [0,1]) — indices 7..10
        for (int i = 0; i < kNumMacros; ++i)
            source_[7 + i] = macroP_[i] ? macroP_[i]->load() : 0.0f;

        // Step sequencer (bipolar per-step [-1,1]) — index 11
        const float seqRate = seqRateP_ ? seqRateP_->load() : 4.0f;
        seqPhase_ += seqRate * dt;
        while (seqPhase_ >= 1.0f) { seqPhase_ -= 1.0f; seqIndex_ = (seqIndex_ + 1) % kNumSteps; }
        source_[11] = seqStepP_[seqIndex_] ? seqStepP_[seqIndex_]->load() : 0.0f;

        // Performance sources — indices 12..15
        source_[12] = velocity_;
        source_[13] = modWheel_;
        source_[14] = aftertouch_;
        source_[15] = pitchBend_;
    }

    double sr_ = 44100.0;
    int numDests_ = 0;
    juce::Random rng_;

    // slot params
    std::array<std::atomic<float>*, kNumSlots> srcP_ {};
    std::array<std::atomic<float>*, kNumSlots> destP_ {};
    std::array<std::atomic<float>*, kNumSlots> depthP_ {};
    // source-bank params
    std::array<std::atomic<float>*, kNumLfos> lfoRateP_ {};
    std::array<std::atomic<float>*, kNumLfos> lfoShapeP_ {};
    std::array<std::atomic<float>*, kNumEnvs> envAtkP_ {}, envDecP_ {}, envSusP_ {}, envRelP_ {};
    std::array<std::atomic<float>*, kNumMacros> macroP_ {};
    std::atomic<float>* seqRateP_ = nullptr;
    std::array<std::atomic<float>*, kNumSteps> seqStepP_ {};
    // source-bank state
    std::array<float, kNumLfos> lfoPhase_ {}, lfoRand_ {}, lfoLast_ {};
    std::array<float, kNumEnvs> envLevel_ {};
    std::array<int,   kNumEnvs> envStage_ {};
    float seqPhase_ = 0.0f; int seqIndex_ = 0;
    float velocity_ = 0.0f, modWheel_ = 0.0f, aftertouch_ = 0.0f, pitchBend_ = 0.0f;
    // per-block source values + destination accumulators
    std::array<float, kNumSources> source_ {};
    std::array<float, kMaxDests>  destMod_ {};
};

//==============================================================================
/** Flat styling for the matrix panel's controls.

    Self-contained (NOT PresetLookAndFeel): the panel ships in projects that may not
    include PresetBar.h — a UI-designer-placed `mod_matrix` binds the same engine with no
    preset bar present. Stock LookAndFeel_V4 buttons are silver/bevelled/gradient — a
    producer reads that as "developer placeholder"; draw them flat ourselves instead.
*/
class ModMatrixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour background { 0xff1a1a2e }, accent { 0xff6366f1 }, text { 0xffe2e2e8 };

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = background.brighter (0.22f);
        if (down)      fill = accent.withAlpha (0.60f);
        else if (over) fill = accent.withAlpha (0.28f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (over || down ? accent : accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        g.setColour (over || down ? juce::Colours::white : text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }
};

//==============================================================================
/** The end-user modulation-matrix panel — 16 re-routable slots, each a source Choice, a
    destination Choice, and a bipolar depth. A click-to-open overlay (mirrors PresetBrowser,
    NOT a docked strip → no window-height sync, avoids the preview==output height bug class).
    Every control is bound to its `mmN_*` APVTS param, so edits are DAW-automatable and
    captured by presets.

    Self-contained: constructed from the APVTS alone (so a UI-designer-placed `mod_matrix`
    can `ModMatrixPanel{processorRef.apvts}` it), colours applied via setColours(). The src
    and dest option lists are read once from slot 0's Choice params — identical per slot by
    construction (the assembler emits the same choice list for every mmN_src / mmN_dest).
*/
class ModMatrixPanel : public juce::Component
{
public:
    explicit ModMatrixPanel (juce::AudioProcessorValueTreeState& apvtsIn)
        : apvts (apvtsIn)
    {
        setLookAndFeel (&laf);

        title.setText ("MODULATION MATRIX", juce::dontSendNotification);
        title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        addAndMakeVisible (title);

        closeButton.setButtonText ("X");
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);

        juce::StringArray srcChoices, destChoices;
        if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mm0_src")))
            srcChoices = s->choices;
        if (auto* d = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mm0_dest")))
            destChoices = d->choices;

        for (int i = 0; i < ModMatrix::kNumSlots; ++i)
        {
            auto row = std::make_unique<Row>();
            const juce::String p = "mm" + juce::String (i) + "_";

            row->number.setText (juce::String (i + 1), juce::dontSendNotification);
            row->number.setFont (juce::FontOptions (11.0f));
            row->number.setJustificationType (juce::Justification::centred);

            row->src.addItemList (srcChoices, 1);
            row->dest.addItemList (destChoices, 1);
            row->depth.setSliderStyle (juce::Slider::LinearHorizontal);
            row->depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);

            content.addAndMakeVisible (row->number);
            content.addAndMakeVisible (row->src);
            content.addAndMakeVisible (row->dest);
            content.addAndMakeVisible (row->depth);

            // Guard each attachment: only bind if the param exists (a partial matrix, or the
            // standalone UI-designer path, must not construct an attachment on a null param).
            if (apvts.getParameter (p + "src"))   row->srcAtt   = std::make_unique<ComboAtt>  (apvts, p + "src",   row->src);
            if (apvts.getParameter (p + "dest"))  row->destAtt  = std::make_unique<ComboAtt>  (apvts, p + "dest",  row->dest);
            if (apvts.getParameter (p + "depth")) row->depthAtt = std::make_unique<SliderAtt> (apvts, p + "depth", row->depth);

            rows.push_back (std::move (row));
        }

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        setColours (juce::Colour (0xff1a1a2e), juce::Colour (0xff6366f1), juce::Colour (0xffe2e2e8));
    }

    ~ModMatrixPanel() override { setLookAndFeel (nullptr); }

    /** Recolour to the plugin's theme (called by the preset bar's wrapper). */
    void setColours (juce::Colour bg, juce::Colour accent, juce::Colour text)
    {
        bg_ = bg; accent_ = accent; text_ = text;
        laf.background = bg; laf.accent = accent; laf.text = text;
        title.setColour (juce::Label::textColourId, text);
        for (auto& r : rows)
        {
            r->number.setColour (juce::Label::textColourId, text.withAlpha (0.7f));
            for (auto* cb : { &r->src, &r->dest })
            {
                cb->setColour (juce::ComboBox::backgroundColourId, bg.darker (0.3f));
                cb->setColour (juce::ComboBox::textColourId, text);
                cb->setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.25f));
                cb->setColour (juce::ComboBox::arrowColourId, accent);
            }
            r->depth.setColour (juce::Slider::backgroundColourId, bg.darker (0.3f));
            r->depth.setColour (juce::Slider::trackColourId, accent.withAlpha (0.5f));
            r->depth.setColour (juce::Slider::thumbColourId, accent);
            r->depth.setColour (juce::Slider::textBoxTextColourId, text);
            r->depth.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // OPAQUE — a translucent panel let the plugin's own UI bleed through and read as
        // unfinished (the PresetBrowser lesson).
        g.fillAll (bg_);
        g.setColour (accent_.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);

        auto top = r.removeFromTop (22);
        closeButton.setBounds (top.removeFromRight (24));
        title.setBounds (top);

        r.removeFromTop (6);
        viewport.setBounds (r);

        const int rowH = 26, gap = 4;
        const int w = juce::jmax (240, viewport.getWidth() - viewport.getScrollBarThickness());
        content.setSize (w, ModMatrix::kNumSlots * (rowH + gap) + gap);

        int y = gap;
        for (auto& row : rows)
        {
            auto rr = juce::Rectangle<int> (0, y, content.getWidth(), rowH).reduced (2, 0);
            row->number.setBounds (rr.removeFromLeft (22));
            rr.removeFromLeft (4);
            auto depthArea = rr.removeFromRight (150);
            rr.removeFromRight (6);
            const int half = rr.getWidth() / 2;
            row->src.setBounds (rr.removeFromLeft (half - 3));
            rr.removeFromLeft (6);
            row->dest.setBounds (rr);
            row->depth.setBounds (depthArea);
            y += rowH + gap;
        }
    }

private:
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Row
    {
        juce::Label    number;
        juce::ComboBox src, dest;
        juce::Slider   depth;
        std::unique_ptr<ComboAtt>  srcAtt, destAtt;
        std::unique_ptr<SliderAtt> depthAtt;
    };

    juce::AudioProcessorValueTreeState& apvts;
    ModMatrixLookAndFeel laf;   // before the controls it styles — reverse-destruction safety
    juce::Label      title;
    juce::TextButton closeButton;
    juce::Component  content;    // before viewport: viewport clears its ref to it on destruction
    juce::Viewport   viewport;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Colour bg_ { 0xff1a1a2e }, accent_ { 0xff6366f1 }, text_ { 0xffe2e2e8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrixPanel)
};

} // namespace vybecode
