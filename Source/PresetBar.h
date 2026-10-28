/*
    PresetBar.h — the patch-management UI for a VybeCode-generated plugin.

    Emitted verbatim into every generated project alongside PresetManager.h. Static: everything
    plugin-specific arrives through setColours() and the PresetManager reference.

    Two components:
      PresetBar     — the always-visible strip: ◂ ▸, the patch name (click to browse), A/B, SAVE.
      PresetBrowser — a full overlay panel: search, category list, patch list, save-as / rename /
                      delete / reveal-in-folder.

    DESIGN CONSTRAINTS, each learned the hard way in this repo:

    * NO MODAL LOOPS, NO AlertWindow. A plugin must never run a modal loop inside a host, and
      juce::AlertWindow has no async input box — a generated editor hallucinated
      `showInputBoxAsync` reaching for exactly this preset-save affordance, and it is now on
      CodeValidator's GEN-011 denylist. Naming is done with an inline TextEditor instead, which
      is also better UX.

    * FULLY THEMED. It draws itself from the colours it is given, so it sits inside the plugin's
      own look rather than announcing itself as a bolted-on JUCE default. A preset bar that looks
      generic is the fastest way for a producer to dismiss a plugin.

    * NO OWNERSHIP OF LAYOUT. The bar draws inside whatever bounds it is given. The editor decides
      where it goes, so the Native UI canvas can show it exactly where it will compile.

    Copyright (c) 2026 VybeCode Software. All rights reserved.
*/

#pragma once
#include <JuceHeader.h>
#include <functional>

#include "PresetManager.h"
#include "ModMatrix.h"   // the MOD launcher opens a vybecode::ModMatrixPanel (Pillar 2)

namespace vybecode {

/** The colours both components draw from — handed down from the plugin's theme. */
struct PresetTheme
{
    juce::Colour background { 0xff1a1a2e };
    juce::Colour accent     { 0xff6366f1 };
    juce::Colour text       { 0xffe2e2e8 };
    juce::Colour dim        { 0xff6b6b80 };
};

//==============================================================================
/** Flat styling for the bar's buttons.

    Without this they are stock LookAndFeel_V4: silver, bevelled, gradient-filled, with a dark
    outline — which is exactly what a default JUCE build looks like, and a producer reads that as
    "developer placeholder" in about a second. Setting buttonColourId alone does NOT fix it;
    LookAndFeel_V4::drawButtonBackground layers its own gradient and bevel over whatever colour it
    is given, so the only way to a flat control is to draw it ourselves.

    Verified by looking at the compiled plugin, not by reading the code.
*/
class PresetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PresetTheme theme;

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        // Lifted off the background rather than merged into it. At brighter(0.10) over a
        // near-black theme these read as barely-there smudges — a control a producer isn't sure
        // is a control. Confidence costs nothing here.
        auto fill = theme.background.brighter (0.22f);
        if (down)      fill = theme.accent.withAlpha (0.60f);
        else if (over) fill = theme.accent.withAlpha (0.28f);
        else if (on)   fill = theme.accent.withAlpha (0.34f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (over || down ? theme.accent : theme.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        g.setColour (over || down ? juce::Colours::white : theme.text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }
};

//==============================================================================
/** The overlay browser: search + categories + the unified factory/user list. */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private PresetManager::Listener
{
public:
    explicit PresetBrowser (PresetManager& pm) : manager (pm)
    {
        manager.addListener (this);

        addAndMakeVisible (title);
        title.setText ("PRESETS", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (search);
        search.setTextToShowWhenEmpty ("Search...", theme.dim);
        search.onTextChange = [this] { rebuild(); };

        addAndMakeVisible (categories);
        categories.setModel (&categoryModel);
        categoryModel.owner = this;

        addAndMakeVisible (list);
        list.setModel (this);

        addAndMakeVisible (closeButton);
        closeButton.setButtonText ("X");
        closeButton.onClick = [this] { setVisible (false); };

        for (auto* b : { &saveAsButton, &renameButton, &deleteButton, &revealButton })
            addAndMakeVisible (b);
        saveAsButton.setButtonText ("Save As");
        renameButton.setButtonText ("Rename");
        deleteButton.setButtonText ("Delete");
        revealButton.setButtonText ("Folder");

        saveAsButton.onClick = [this] { beginNaming (NameMode::saveAs); };
        renameButton.onClick = [this] { beginNaming (NameMode::rename); };
        deleteButton.onClick = [this] { deleteSelected(); };
        revealButton.onClick = [this] { manager.getUserPresetDirectory().revealToUser(); };

        // Inline naming — no modal, no AlertWindow (see the header comment).
        addChildComponent (nameEditor);
        nameEditor.onReturnKey = [this] { commitNaming(); };
        nameEditor.onEscapeKey = [this] { nameEditor.setVisible (false); };
        nameEditor.onFocusLost = [this] { nameEditor.setVisible (false); };

        rebuild();
    }

    ~PresetBrowser() override
    {
        manager.removeListener (this);
        // JUCE requires every child to drop the LookAndFeel before it dies.
        for (auto* c : getChildren()) c->setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    void setTheme (const PresetTheme& t)
    {
        theme = t;
        laf.theme = t;
        setLookAndFeel (&laf);
        for (auto* b : { &closeButton, &saveAsButton, &renameButton, &deleteButton, &revealButton })
            b->setLookAndFeel (&laf);
        title.setColour (juce::Label::textColourId, theme.accent);
        search.setColour (juce::TextEditor::backgroundColourId, theme.background.brighter (0.12f));
        search.setColour (juce::TextEditor::textColourId, theme.text);
        search.setColour (juce::TextEditor::outlineColourId, theme.background.brighter (0.25f));
        nameEditor.setColour (juce::TextEditor::backgroundColourId, theme.background.brighter (0.18f));
        nameEditor.setColour (juce::TextEditor::textColourId, theme.text);
        for (auto* lb : { &categories, &list })
        {
            lb->setColour (juce::ListBox::backgroundColourId, theme.background.darker (0.3f));
            lb->setColour (juce::ListBox::outlineColourId, theme.accent.withAlpha (0.2f));
            lb->setOutlineThickness (1);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // OPAQUE. At 0.97 the plugin's own title bled through the panel and it read as
        // unfinished — visible only by looking at the built plugin.
        g.fillAll (theme.background);
        g.setColour (theme.accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);

        auto top = r.removeFromTop (22);
        closeButton.setBounds (top.removeFromRight (24));
        title.setBounds (top);

        r.removeFromTop (6);
        search.setBounds (r.removeFromTop (24));
        r.removeFromTop (6);

        auto bottom = r.removeFromBottom (26);
        r.removeFromBottom (6);
        const int bw = juce::jmax (48, bottom.getWidth() / 4 - 4);
        saveAsButton.setBounds (bottom.removeFromLeft (bw)); bottom.removeFromLeft (4);
        renameButton.setBounds (bottom.removeFromLeft (bw)); bottom.removeFromLeft (4);
        deleteButton.setBounds (bottom.removeFromLeft (bw)); bottom.removeFromLeft (4);
        revealButton.setBounds (bottom.removeFromLeft (bw));

        // 92px floor, not 70: "Character" only just fitted, and a user's own category name is
        // very likely to be longer than that.
        categories.setBounds (r.removeFromLeft (juce::jmax (92, r.getWidth() / 4)));
        r.removeFromLeft (6);
        list.setBounds (r);

        nameEditor.setBounds (list.getBounds().withHeight (26));
    }

    /** Re-read the manager and refresh both lists. */
    void rebuild()
    {
        cats = manager.getCategories();
        cats.insert (0, "All");
        if (! cats.contains (activeCategory)) activeCategory = "All";

        visible.clearQuick();
        for (int idx : manager.search (search.getText()))
            if (activeCategory == "All" || manager.getPresetCategory (idx) == activeCategory)
                visible.add (idx);

        categories.updateContent();
        list.updateContent();
        categories.repaint();
        list.repaint();
    }

private:
    enum class NameMode { saveAs, rename };

    //== the patch list ==
    int getNumRows() override { return visible.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, visible.size())) return;
        const int idx = visible[row];
        const bool current = idx == manager.getCurrentIndex();

        // A soft tint plus a left accent bar, rather than a saturated full-width band. The band
        // read as cheap next to the rest of the plugin; this is what a considered browser does,
        // and it lets SELECTED (what you are pointing at) and CURRENT (what you are hearing) be
        // told apart — the band collapsed them into one.
        if (selected)
        {
            g.setColour (theme.accent.withAlpha (0.12f));
            g.fillRect (0, 0, w, h);
        }
        if (current)
        {
            g.setColour (theme.accent);
            g.fillRect (0, 0, 3, h);
        }

        g.setColour (current ? theme.accent : theme.text);
        g.setFont (juce::FontOptions (13.0f, current ? juce::Font::bold : juce::Font::plain));
        g.drawText (manager.getPresetName (idx), 10, 0, w - 50, h,
                    juce::Justification::centredLeft, true);

        // Factory patches are read-only; say so rather than letting a user discover it by
        // pressing Delete and watching nothing happen.
        if (manager.isFactory (idx))
        {
            g.setColour (theme.dim);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("FACTORY", w - 44, 0, 40, h, juce::Justification::centredRight, false);
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, visible.size()))
            manager.loadPreset (visible[row]);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, visible.size()))
        {
            manager.loadPreset (visible[row]);
            setVisible (false);          // double-click = "this one" -> get out of the way
        }
    }

    //== the category rail ==
    struct CategoryModel : public juce::ListBoxModel
    {
        PresetBrowser* owner = nullptr;
        int getNumRows() override { return owner != nullptr ? owner->cats.size() : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (owner == nullptr || ! juce::isPositiveAndBelow (row, owner->cats.size())) return;
            const bool active = owner->cats[row] == owner->activeCategory;
            if (active)
            {
                g.setColour (owner->theme.accent.withAlpha (0.12f));
                g.fillRect (0, 0, w, h);
                g.setColour (owner->theme.accent);
                g.fillRect (0, 0, 3, h);
            }
            // An inactive category at theme.dim on near-black is nearly unreadable — these are
            // navigation, not decoration.
            g.setColour (active ? owner->theme.accent : owner->theme.text.withAlpha (0.65f));
            g.setFont (juce::FontOptions (12.0f, active ? juce::Font::bold : juce::Font::plain));
            g.drawText (owner->cats[row], 10, 0, w - 12, h, juce::Justification::centredLeft, true);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (owner != nullptr && juce::isPositiveAndBelow (row, owner->cats.size()))
            {
                owner->activeCategory = owner->cats[row];
                owner->rebuild();
            }
        }
    };

    //== naming ==
    void beginNaming (NameMode mode)
    {
        nameMode = mode;
        if (mode == NameMode::rename)
        {
            const int idx = selectedIndex();
            if (idx < 0 || manager.isFactory (idx)) return;   // factory is read-only
            nameEditor.setText (manager.getPresetName (idx));
        }
        else
        {
            nameEditor.setText (manager.getCurrentPresetName() + " Copy");
        }
        nameEditor.setVisible (true);
        nameEditor.toFront (true);
        nameEditor.selectAll();
        nameEditor.grabKeyboardFocus();
    }

    void commitNaming()
    {
        const auto name = nameEditor.getText().trim();
        nameEditor.setVisible (false);
        if (name.isEmpty()) return;

        if (nameMode == NameMode::saveAs)
            manager.saveUserPreset (name, activeCategory == "All" || activeCategory == "Factory"
                                              ? "User" : activeCategory);
        else if (const int idx = selectedIndex(); idx >= 0)
            manager.renameUserPreset (idx, name);

        rebuild();
    }

    void deleteSelected()
    {
        const int idx = selectedIndex();
        if (idx >= 0 && ! manager.isFactory (idx)) { manager.deleteUserPreset (idx); rebuild(); }
    }

    int selectedIndex() const
    {
        const int row = list.getSelectedRow();
        return juce::isPositiveAndBelow (row, visible.size()) ? visible[row] : -1;
    }

    //== PresetManager::Listener ==
    void presetChanged (int, const juce::String&) override { list.repaint(); }
    void presetListChanged() override { rebuild(); }

    PresetManager& manager;
    PresetTheme theme;

    // Declared BEFORE the components it styles: members are destroyed in reverse declaration
    // order, so this outlives every child that points at it. The destructor also clears them,
    // but ordering makes the guarantee structural rather than a thing to remember.
    PresetLookAndFeel laf;

    juce::Label      title;
    juce::TextEditor search, nameEditor;
    juce::ListBox    categories, list;
    juce::TextButton closeButton, saveAsButton, renameButton, deleteButton, revealButton;
    CategoryModel    categoryModel;

    juce::StringArray cats;
    juce::Array<int>  visible;
    juce::String      activeCategory { "All" };
    NameMode          nameMode { NameMode::saveAs };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

//==============================================================================
/** The always-visible strip. Draws inside whatever bounds the editor gives it. */
class PresetBar : public juce::Component,
                  private PresetManager::Listener,
                  private juce::Timer
{
public:
    static constexpr int defaultHeight = 30;

    explicit PresetBar (PresetManager& pm) : manager (pm)
    {
        manager.addListener (this);

        addAndMakeVisible (prevButton);
        addAndMakeVisible (nextButton);
        addAndMakeVisible (abButton);
        addAndMakeVisible (saveButton);

        prevButton.setButtonText ("<");
        nextButton.setButtonText (">");
        abButton.setButtonText ("A");
        saveButton.setButtonText ("SAVE");

        prevButton.onClick = [this] { manager.loadPreviousPreset(); };
        nextButton.onClick = [this] { manager.loadNextPreset(); };
        abButton.onClick   = [this] { manager.toggleSlot(); refresh(); };
        saveButton.onClick = [this] { openBrowser (true); };

        // Poll for the dirty flag: it is computed by comparison (PresetManager::isDirty), which
        // has no change callback by design — the alternative, a parameter listener, fires
        // asynchronously and would show every freshly-loaded patch as edited.
        startTimerHz (8);
    }

    ~PresetBar() override
    {
        manager.removeListener (this);
        for (auto* c : getChildren()) c->setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    void setTheme (const PresetTheme& t)
    {
        theme = t;
        laf.theme = t;
        setLookAndFeel (&laf);
        for (auto* b : { &prevButton, &nextButton, &abButton, &saveButton, &modButton })
            b->setLookAndFeel (&laf);
        if (browser != nullptr) browser->setTheme (theme);
        repaint();
    }

    /** Add a "MOD" launcher to the bar that opens the modulation matrix (Pillar 2). Called
        only when the plugin has a matrix and mod_ui is on; without it the button never
        appears, so a preset-only plugin's bar is byte-identical to before. */
    void setModCallback (std::function<void()> cb)
    {
        onMod = std::move (cb);
        if (! onMod) return;
        modButton.setButtonText ("MOD");
        modButton.setLookAndFeel (&laf);
        modButton.onClick = [this] { if (onMod) onMod(); };
        addAndMakeVisible (modButton);
        resized();
    }

    /** The browser is added to the EDITOR, not to the bar: it needs the whole window, and the
        bar is only 30px tall. Called on the first browse rather than in the constructor, when
        the bar may not have a parent yet. */
    void openBrowser (bool startNaming)
    {
        auto* host = getParentComponent();
        if (host == nullptr) return;

        if (browser == nullptr)
        {
            browser = std::make_unique<PresetBrowser> (manager);
            browser->setTheme (theme);
            host->addChildComponent (browser.get());
        }
        browser->setBounds (host->getLocalBounds().reduced (host->getWidth() / 8,
                                                            host->getHeight() / 8));
        browser->rebuild();
        browser->setVisible (true);
        browser->toFront (true);
        if (startNaming) browser->resized();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (theme.background.brighter (0.08f));
        g.fillRect (r);
        g.setColour (theme.accent.withAlpha (0.35f));
        g.drawLine ((float) r.getX(), (float) r.getBottom() - 0.5f,
                    (float) r.getRight(), (float) r.getBottom() - 0.5f, 1.0f);

        // The name well — a plain clickable region, not a ComboBox: a stock combo reads as a
        // developer's placeholder to a producer.
        // NB: work off a LOCAL copy. nameArea is a member, and Rectangle::removeFromRight MUTATES;
        // carving the chevron out of the member shrank the well by 16px on EVERY repaint — and
        // shrank its hit area with it, since mouseDown tests the same rectangle — until it
        // disappeared. Visible as a widening gap next to the A button; caught by looking.
        const auto well = nameArea;

        g.setColour (nameHighlighted ? theme.background.darker (0.15f)
                                     : theme.background.darker (0.35f));
        g.fillRoundedRectangle (well.toFloat(), 3.0f);
        g.setColour (theme.accent.withAlpha (nameHighlighted ? 0.9f : 0.45f));
        g.drawRoundedRectangle (well.toFloat().reduced (0.5f), 3.0f, 1.0f);

        auto name = manager.getCurrentPresetName();
        if (name.isEmpty()) name = "Init";

        auto textArea = well.reduced (9, 0).withTrimmedRight (16);
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (name, textArea, juce::Justification::centredLeft, true);

        // The edited marker gets the ACCENT, not the same grey as the name — its whole job is to
        // be noticed, and " *" tacked onto the end in body colour is not noticed.
        if (manager.isDirty())
        {
            const int nameW = juce::GlyphArrangement::getStringWidthInt (
                juce::Font (juce::FontOptions (13.0f)), name);
            g.setColour (theme.accent);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("*", textArea.withTrimmedLeft (juce::jmin (nameW + 4, textArea.getWidth() - 8)),
                        juce::Justification::centredLeft, false);
        }

        // A chevron, not the word "BROWSE". Every plugin on the market signals "this name opens
        // a list" with one, and it survives being dim in a way a 9pt word does not.
        auto chevron = well.withTrimmedLeft (well.getWidth() - 20)
                           .withSizeKeepingCentre (8, 5).toFloat();
        drawChevron (g, chevron, nameHighlighted ? theme.accent : theme.dim.brighter (0.2f));
    }

    /** A downward chevron — the standard "opens a list" affordance. */
    static void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getCentreX(), r.getBottom());
        p.lineTo (r.getRight(), r.getY());
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 3);
        prevButton.setBounds (r.removeFromLeft (22)); r.removeFromLeft (2);
        nextButton.setBounds (r.removeFromLeft (22)); r.removeFromLeft (6);
        if (onMod) { modButton.setBounds (r.removeFromLeft (36)); r.removeFromLeft (6); }
        saveButton.setBounds (r.removeFromRight (44)); r.removeFromRight (4);
        abButton.setBounds (r.removeFromRight (24));  r.removeFromRight (6);
        nameArea = r;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (nameArea.contains (e.getPosition())) openBrowser (false);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool over = nameArea.contains (e.getPosition());
        if (over != nameHighlighted) { nameHighlighted = over; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (nameHighlighted) { nameHighlighted = false; repaint(); }
    }

private:
    void refresh()
    {
        abButton.setButtonText (manager.isUsingSlotB() ? "B" : "A");
        repaint();
    }

    void presetChanged (int, const juce::String&) override { refresh(); }
    void presetListChanged() override { refresh(); }

    void timerCallback() override
    {
        const bool d = manager.isDirty();
        if (d != wasDirty) { wasDirty = d; repaint(); }
    }

    PresetManager& manager;
    PresetTheme theme;

    PresetLookAndFeel laf;   // before the buttons — see the note in PresetBrowser
    juce::TextButton prevButton, nextButton, abButton, saveButton, modButton;
    std::function<void()> onMod;   // set → the MOD button appears and opens the matrix
    juce::Rectangle<int> nameArea;
    std::unique_ptr<PresetBrowser> browser;
    bool nameHighlighted = false;
    bool wasDirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

//==============================================================================
/** Stacks a PresetBar above the plugin's real editor.

    This is what makes patch management universal without touching a single editor generator.
    There are four of them — the deterministic assembler editor, the Native UI canvas
    (build_positioned_editor), Claude's AI GUI, and an imported vybecode-ui bundle (UI_DSP_GUI_V1)
    — and only the first two are ours to edit. Wrapping means all four get the bar, identically,
    with zero changes to any of them and nothing for the AI to get wrong.

    The body keeps the exact size it chose for itself; the window simply grows by the bar's
    height. So whatever the user arranged on the canvas still compiles to exactly those bounds —
    the bar is chrome above it, never a re-layout of their design.

    Nesting an AudioProcessorEditor inside another is legal: the inner one is never the
    processor's active editor (this wrapper is), so its destructor's activeEditor assertion holds
    and its editorBeingDeleted() call is a no-op for a pointer that was never registered.
*/
class PresetEditorWrapper : public juce::AudioProcessorEditor
{
public:
    PresetEditorWrapper (juce::AudioProcessor& p,
                         std::unique_ptr<juce::Component> bodyIn,
                         PresetManager& pm,
                         const PresetTheme& theme,
                         juce::AudioProcessorValueTreeState* apvts = nullptr,
                         bool showMod = false)
        : juce::AudioProcessorEditor (p), body (std::move (bodyIn)), bar (pm)
    {
        jassert (body != nullptr);
        barColour = theme.background;
        bar.setTheme (theme);
        addAndMakeVisible (bar);
        addAndMakeVisible (*body);

        // The MOD launcher + matrix overlay appear only when the plugin actually registers a
        // matrix (mm0_src present) AND mod_ui is on — a preset-only plugin is byte-identical.
        if (showMod && apvts != nullptr && apvts->getParameter ("mm0_src") != nullptr)
        {
            modPanel = std::make_unique<ModMatrixPanel> (*apvts);
            modPanel->setColours (theme.background, theme.accent, theme.text);
            addChildComponent (*modPanel);            // hidden until MOD is pressed
            bar.setModCallback ([this]
            {
                const bool show = ! modPanel->isVisible();
                modPanel->setVisible (show);
                if (show) modPanel->toFront (true);
            });
        }

        // Adopt the body's own chosen size and add the strip. Its layout is not ours to touch.
        setSize (juce::jmax (240, body->getWidth()),
                 body->getHeight() + PresetBar::defaultHeight);
    }

    void paint (juce::Graphics& g) override { g.fillAll (barColour); }

    void resized() override
    {
        auto r = getLocalBounds();
        bar.setBounds (r.removeFromTop (PresetBar::defaultHeight));
        body->setBounds (r);
        if (modPanel != nullptr) modPanel->setBounds (getLocalBounds());   // full-window overlay
    }

private:
    std::unique_ptr<juce::Component> body;
    PresetBar bar;
    std::unique_ptr<ModMatrixPanel> modPanel;
    juce::Colour barColour { 0xff1a1a2e };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetEditorWrapper)
};

} // namespace vybecode
