#pragma once
/*  VybeModControls.h — the designer-placeable modulation affordances (VybeCode DSP, Pillar 2).

    Shared, single-source-of-truth UI over a plugin's `mmN_{src,dest,depth}` APVTS params:
      • VybeModDestBox — a whitelist-restricted destination ComboBox,
      • VybeModStrip   — a single-slot dest+depth strip (source pinned, or a full router),
      • VybeModGrid    — a rows×cols lattice, one live depth per cell.

    Included by BOTH engine headers so the affordance code is literally identical across builds:
    ModMatrix.h (a DSP-assembled plugin) and VybeModMatrix.h (a standalone export). These
    components touch only juce + the mmN_* params — they know nothing about which engine is present,
    so the SAME editor binds them whether the real DSP engine or the standalone engine backs the
    params. Requires JUCE (juce_gui_basics + juce_audio_processors) to be available at include time,
    exactly like the engine headers that include it. */

namespace vybecode {

//==============================================================================
/** A destination ComboBox restricted to an author's WHITELIST subset of a slot's `mmN_dest`
    Choice. A plain ComboBoxAttachment lists EVERY registered destination, defeating the whitelist,
    so this maps the visible subset <-> the underlying Choice index by hand and reflects host
    automation. "None" (index 0) is always offered so a user can clear the slot.

    If automation parks the destination OUTSIDE the whitelist, the real value is shown transiently
    (appended, marked "[ext]") rather than the box lying about where the slot points. Self-contained:
    constructed from the APVTS + the whitelist alone, so the SAME code binds in a DSP build and a
    standalone export. */
class VybeModDestBox : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    VybeModDestBox (juce::AudioProcessorValueTreeState& apvtsIn, juce::String paramIdIn,
                    const juce::Array<int>& allowedIndices)
        : apvts (apvtsIn), paramId (std::move (paramIdIn))
    {
        param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId));
        addAndMakeVisible (box);

        allowed.addIfNotAlreadyThere (0);                       // "None" — always clearable
        if (allowedIndices.isEmpty() && param != nullptr)
            // Empty whitelist = expose EVERY destination (an unrestricted dest box). A
            // designer-placed strip whose whitelist macro DSP hasn't resolved falls back to this.
            for (int i = 1; i < param->choices.size(); ++i) allowed.addIfNotAlreadyThere (i);
        else
            for (int idx : allowedIndices) allowed.addIfNotAlreadyThere (idx);

        rebuildItems();
        box.onChange = [this] { pushToParam(); };

        if (param != nullptr) apvts.addParameterListener (paramId, this);
        syncFromParam();
    }

    ~VybeModDestBox() override
    {
        if (param != nullptr) apvts.removeParameterListener (paramId, this);
    }

    void resized() override { box.setBounds (getLocalBounds()); }

    juce::ComboBox& comboBox() noexcept { return box; }         // for the strip to theme it

private:
    void rebuildItems()
    {
        box.clear (juce::dontSendNotification);
        indexForId.clear();
        idForIndex.clear();
        if (param == nullptr) return;
        const auto& choices = param->choices;
        int itemId = 1;
        for (int idx : allowed)
        {
            if (idx < 0 || idx >= choices.size()) continue;
            box.addItem (choices[idx], itemId);
            indexForId[itemId] = idx;
            idForIndex[idx] = itemId;
            ++itemId;
        }
    }

    void pushToParam()
    {
        if (param == nullptr) return;
        const auto it = indexForId.find (box.getSelectedId());
        if (it == indexForId.end()) return;                     // the transient "[ext]" item, ignore
        const int idx = it->second;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
        param->endChangeGesture();
    }

    void syncFromParam()
    {
        if (param == nullptr) return;
        const int idx = param->getIndex();
        const auto it = idForIndex.find (idx);
        if (it != idForIndex.end())
        {
            rebuildItems();                                     // drop any stale transient item
            box.setSelectedId (it->second, juce::dontSendNotification);
            return;
        }
        // Outside the whitelist (host automation) — surface the real value rather than lie.
        rebuildItems();
        if (idx >= 0 && idx < param->choices.size())
        {
            box.addItem (param->choices[idx] + " [ext]", kExtId);   // ASCII marker (no escape hazards)
            box.setSelectedId (kExtId, juce::dontSendNotification);
        }
    }

    void parameterChanged (const juce::String&, float) override
    {
        // May arrive off the message thread — marshal, guarding against our own destruction.
        juce::Component::SafePointer<VybeModDestBox> safe (this);
        juce::MessageManager::callAsync ([safe]() mutable { if (safe != nullptr) safe->syncFromParam(); });
    }

    static constexpr int kExtId = 9000;                          // id for the transient out-of-set item
    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramId;
    juce::AudioParameterChoice* param { nullptr };
    juce::ComboBox box;
    juce::Array<int> allowed;                                    // dest indices, "None" first
    std::map<int, int> indexForId, idForIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VybeModDestBox)
};

//==============================================================================
/** A single-slot modulation "strip": the compact, per-source drag-out affordance. Shows a
    destination (whitelist) box + a bipolar depth slider bound to one `mmN_` slot. The source is
    PINNED (its `mmN_src` default is seeded by the author) and NOT drawn — the strip sits next to a
    control whose meaning implies the source. A "full router" strip (showSource) also draws a source
    box bound to `mmN_src`. Byte-identical across the DSP and standalone builds (both provide the
    `mmN_*` params + this class). */
class VybeModStrip : public juce::Component
{
public:
    VybeModStrip (juce::AudioProcessorValueTreeState& apvts, int slot,
                  const juce::Array<int>& destWhitelist, bool showSource = false)
        : destBox (apvts, "mm" + juce::String (slot) + "_dest", destWhitelist),
          showSrc (showSource)
    {
        const juce::String p = "mm" + juce::String (slot) + "_";

        addAndMakeVisible (destBox);

        depth.setSliderStyle (juce::Slider::LinearHorizontal);
        depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        addAndMakeVisible (depth);
        if (apvts.getParameter (p + "depth"))
            depthAtt = std::make_unique<SliderAtt> (apvts, p + "depth", depth);

        if (showSrc)
        {
            if (auto* s = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (p + "src")))
                srcBox.addItemList (s->choices, 1);
            addAndMakeVisible (srcBox);
            if (apvts.getParameter (p + "src"))
                srcAtt = std::make_unique<ComboAtt> (apvts, p + "src", srcBox);
        }

        setColours (juce::Colour (0xff1a1a2e), juce::Colour (0xff6366f1), juce::Colour (0xffe2e2e8));
    }

    void setColours (juce::Colour bg, juce::Colour accent, juce::Colour text)
    {
        for (auto* cb : { &destBox.comboBox(), &srcBox })
        {
            cb->setColour (juce::ComboBox::backgroundColourId, bg.darker (0.3f));
            cb->setColour (juce::ComboBox::textColourId, text);
            cb->setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.25f));
            cb->setColour (juce::ComboBox::arrowColourId, accent);
        }
        depth.setColour (juce::Slider::backgroundColourId, bg.darker (0.3f));
        depth.setColour (juce::Slider::trackColourId, accent.withAlpha (0.5f));
        depth.setColour (juce::Slider::thumbColourId, accent);
        depth.setColour (juce::Slider::textBoxTextColourId, text);
        depth.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (showSrc)
        {
            const int third = r.getWidth() / 3;
            srcBox.setBounds (r.removeFromLeft (third).reduced (1));
        }
        const int destW = juce::jmax (60, r.getWidth() * 2 / 5);
        destBox.setBounds (r.removeFromLeft (destW).reduced (1));
        depth.setBounds (r.reduced (1));
    }

private:
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    VybeModDestBox destBox;
    juce::ComboBox srcBox;
    juce::Slider   depth;
    std::unique_ptr<ComboAtt>  srcAtt;
    std::unique_ptr<SliderAtt> depthAtt;
    bool showSrc { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VybeModStrip)
};

//==============================================================================
/** A modulation GRID: a rows×cols lattice where each cell is one `mmN_` slot with a live bipolar
    depth. Row = a (pinned) source, column = a (pinned) destination, so the cell at (r,c) routes
    source r -> destination c at whatever depth the user dials. The src/dest of each cell's slot are
    seeded as defaults by the author (apply_mod_matrix_to_source) and NOT drawn — only the depth is
    interactive, exactly the matrix-lattice idiom. `slots` is row-major (slot[r*cols + c]); its size
    is rows*cols (deterministically allocated at author time). Byte-identical across DSP/standalone. */
class VybeModGrid : public juce::Component
{
public:
    VybeModGrid (juce::AudioProcessorValueTreeState& apvts, const juce::Array<int>& slots,
                 const juce::StringArray& rowLabelsIn, const juce::StringArray& colLabelsIn)
        : rowLabels (rowLabelsIn), colLabels (colLabelsIn),
          nRows (rowLabelsIn.size()), nCols (colLabelsIn.size())
    {
        for (int i = 0; i < slots.size(); ++i)
        {
            auto cell = std::make_unique<Cell>();
            const juce::String p = "mm" + juce::String (slots[i]) + "_depth";
            cell->depth.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            cell->depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            cell->depth.setRange (-1.0, 1.0, 0.0);
            addAndMakeVisible (cell->depth);
            if (apvts.getParameter (p))
                cell->att = std::make_unique<SliderAtt> (apvts, p, cell->depth);
            cells.push_back (std::move (cell));
        }
        setColours (juce::Colour (0xff1a1a2e), juce::Colour (0xff6366f1), juce::Colour (0xffe2e2e8));
    }

    void setColours (juce::Colour bg, juce::Colour accent, juce::Colour text)
    {
        bg_ = bg; accent_ = accent; text_ = text;
        for (auto& c : cells)
        {
            c->depth.setColour (juce::Slider::rotarySliderFillColourId, accent);
            c->depth.setColour (juce::Slider::rotarySliderOutlineColourId, bg.darker (0.3f));
            c->depth.setColour (juce::Slider::thumbColourId, text);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (text_.withAlpha (0.85f));
        g.setFont (juce::FontOptions (11.0f));
        const float cw = cellW(), ch = cellH();
        for (int c = 0; c < nCols; ++c)
            g.drawText (colLabels[c], (int) (kLabelW + c * cw), 0, (int) cw, kHeaderH,
                        juce::Justification::centred, true);
        for (int r = 0; r < nRows; ++r)
            g.drawText (rowLabels[r], 0, (int) (kHeaderH + r * ch), kLabelW, (int) ch,
                        juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        const float cw = cellW(), ch = cellH();
        for (int r = 0; r < nRows; ++r)
            for (int c = 0; c < nCols; ++c)
            {
                const int i = r * nCols + c;
                if (i < (int) cells.size())
                    cells[(size_t) i]->depth.setBounds ((int) (kLabelW + c * cw),
                                                        (int) (kHeaderH + r * ch), (int) cw, (int) ch);
            }
    }

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    struct Cell { juce::Slider depth; std::unique_ptr<SliderAtt> att; };

    static constexpr int kLabelW = 70, kHeaderH = 18;
    float cellW() const { return nCols > 0 ? (float) (getWidth()  - kLabelW)  / (float) nCols : 0.0f; }
    float cellH() const { return nRows > 0 ? (float) (getHeight() - kHeaderH) / (float) nRows : 0.0f; }

    juce::StringArray rowLabels, colLabels;
    int nRows { 0 }, nCols { 0 };
    std::vector<std::unique_ptr<Cell>> cells;
    juce::Colour bg_ { 0xff1a1a2e }, accent_ { 0xff6366f1 }, text_ { 0xffe2e2e8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VybeModGrid)
};

} // namespace vybecode
