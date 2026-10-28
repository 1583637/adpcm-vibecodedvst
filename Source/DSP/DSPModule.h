/*
    DSPModule.h — Universal Module Interface for VybeCode DSP

    Every pre-built module implements this interface. The contract:
    - prepare() is called once when sample rate or block size changes
    - process() is called every audio block (real-time safe — no allocation)
    - reset() clears internal state (delay lines, filters, etc.)
    - createParameters() returns APVTS-compatible parameters with a prefix
    - updateFromAPVTS() reads parameter values before each process() call

    The prefix system allows multiple instances of the same module:
    - Module "filter" with prefix "filter1_" creates "filter1_cutoff", "filter1_resonance"
    - Module "filter" with prefix "filter2_" creates "filter2_cutoff", "filter2_resonance"

    ── Modulation (the modulation matrix, 2026-07) ──────────────────────────────
    EVERY parameter of EVERY module is a modulation destination, with NO per-module
    code. The mechanism lives entirely in this base class:
      • getParam() returns a per-parameter SHADOW the base owns (not the raw APVTS
        atomic). A module's ordinary `param->load()` therefore transparently reads
        the MODULATED value — no module needs to know about modulation.
      • setModulation(paramID, amount) accumulates a normalised offset for one of
        this module's parameters (the processor calls it once per block per active
        routing).
      • refreshShadows() (the processor calls it once per block, after
        updateFromAPVTS caches the shadows and after setModulation has accumulated,
        before process()) folds the accumulated offset into each shadow in
        NORMALISED parameter space through the parameter's own NormalisableRange —
        so a skewed cutoff moves ~exponentially and a linear gain linearly, each
        respecting its own skew — then snaps to a legal value (a Choice steps, a
        Bool gates at 0.5) and clears the accumulator.
    A summed routing of +1.0 pushes a parameter one full normalised unit; the
    matrix engine scales `depth·source` per destination BEFORE calling
    setModulation, so the musical reach is tuned per parameter (seeded from the
    old per-module curves). This universal mechanism REPLACES the old hand-rolled
    per-module setModulation overrides.

    Copyright (c) 2026 PromptMonster Media Ltd / VybeCode. All rights reserved.
*/

#pragma once
#include <JuceHeader.h>
#include <array>

namespace vybecode {

struct ModuleSpec {
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int numChannels = 2;
};

class DSPModule {
public:
    virtual ~DSPModule() = default;

    // ── Lifecycle ────────────────────────────────────────
    virtual void prepare(const ModuleSpec& spec) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;

    // ── Parameter Contract ───────────────────────────────
    // Returns parameters for APVTS. The prefix is prepended to all IDs
    // so multiple instances don't collide (e.g., "filter1_cutoff").
    virtual std::vector<std::unique_ptr<juce::RangedAudioParameter>>
        createParameters(const juce::String& prefix) = 0;

    // Called before process() — cache atomic parameter values.
    virtual void updateFromAPVTS(juce::AudioProcessorValueTreeState& apvts,
                                  const juce::String& prefix) = 0;

    // ── Note / Gate (instruments) ─────────────────────────
    // Default no-ops so effects need no changes. A synth processBlock calls
    // these (before process()) as MIDI notes arrive: generators override
    // noteOn() to set pitch from the note; envelopes override noteOn()/noteOff()
    // to trigger/release the gate. Effects simply inherit the no-ops.
    virtual void noteOn(int midiNoteNumber, float velocity) { juce::ignoreUnused(midiNoteNumber, velocity); }
    virtual void noteOff(int midiNoteNumber) { juce::ignoreUnused(midiNoteNumber); }

    // ── Modulation routing (universal — the modulation matrix) ────────────
    // Accumulate a normalised modulation offset for one of this module's params.
    // The base implementation makes EVERY param a destination with zero per-module
    // code (no override needed — the old per-module overrides are retired). Kept
    // virtual only so a rare module could special-case a param if ever needed.
    virtual void setModulation(const juce::String& paramID, float amount) {
        for (int i = 0; i < numShadows_; ++i)
            if (shadows_[i].id == paramID) { shadows_[i].modAccum += amount; return; }
    }

    // Recompute every shadow from the live APVTS value + this block's accumulated
    // modulation (in normalised space, respecting the param's own skew), snap to a
    // legal value (Choice steps / Bool gates), then clear the accumulator so a
    // re-routed matrix slot leaves its old destination unmodulated next block.
    // The processor calls this once per block per module — after updateFromAPVTS
    // and setModulation, before process(). Allocation-free (real-time safe).
    void refreshShadows() {
        for (int i = 0; i < numShadows_; ++i) {
            auto& s = shadows_[i];
            const float raw = s.raw->load();
            if (s.modAccum != 0.0f && s.rap != nullptr) {
                const float norm = juce::jlimit(0.0f, 1.0f, s.rap->convertTo0to1(raw) + s.modAccum);
                s.shadow.store(s.rap->getNormalisableRange().snapToLegalValue(s.rap->convertFrom0to1(norm)));
            } else {
                s.shadow.store(raw);
            }
            s.modAccum = 0.0f;
        }
    }

    // ── Metadata ─────────────────────────────────────────
    virtual juce::String getModuleID() const = 0;
    virtual juce::String getModuleName() const = 0;
    virtual int getLatencySamples() const { return 0; }

protected:
    // Convenience: cache a parameter's per-block SHADOW from APVTS.
    // Returns a pointer to a base-owned std::atomic<float> whose value carries the
    // parameter modulated (in normalised space) by the modulation matrix; a module
    // reads it exactly as before (`param->load()`), so no module code changed.
    // Returns nullptr when the parameter is not registered, preserving every
    // module's existing `if (param != nullptr)` guard. The shadow's address is
    // stable for the module's lifetime (fixed inline array — never reallocated).
    std::atomic<float>* getParam(juce::AudioProcessorValueTreeState& apvts,
                                  const juce::String& prefix,
                                  const juce::String& paramID) {
        auto* raw = apvts.getRawParameterValue(prefix + paramID);
        if (raw == nullptr)
            return nullptr;
        for (int i = 0; i < numShadows_; ++i)
            if (shadows_[i].id == paramID)
                return &shadows_[i].shadow;
        if (numShadows_ >= kMaxShadows)
            return raw;   // store full: fall back to the raw value (that param stays unmodulatable)
        auto& s = shadows_[static_cast<size_t>(numShadows_++)];
        s.id  = paramID;
        s.raw = raw;
        s.rap = apvts.getParameter(prefix + paramID);
        s.shadow.store(raw->load());   // warm the shadow so block 1 reads a valid value
        s.modAccum = 0.0f;
        return &s.shadow;
    }

private:
    struct ShadowParam {
        juce::String id;
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* rap = nullptr;
        std::atomic<float> shadow { 0.0f };
        float modAccum = 0.0f;
    };
    static constexpr int kMaxShadows = 16;   // max module param count is 8; 2x headroom
    std::array<ShadowParam, kMaxShadows> shadows_;
    int numShadows_ = 0;
};

} // namespace vybecode
