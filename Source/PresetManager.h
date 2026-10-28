/*
    PresetManager.h — patch management for a VybeCode-generated plugin.

    Emitted verbatim into every generated project (Source/PresetManager.h). It is STATIC: nothing
    here is templated or token-replaced. Everything plugin-specific arrives at runtime through
    initialise() — the company/plugin name and the generated factory table — so this file is
    identical in every plugin and can be reasoned about, tested and fixed once.

    TWO KINDS OF PATCH, deliberately modelled differently:

      FACTORY — authored by the plugin's CREATOR, in the .psf's presets[]. A SPARSE map of
                <prefix><param> -> real engineering units (PSF_V1 D5). It is applied by setting
                only the params it names, through each param's own NormalisableRange
                (convertTo0to1), so skew and choice/bool index are handled without duplicating any
                range. It must NOT be applied by replacing the APVTS state: a sparse preset would
                blow away every parameter it does not mention.

      USER    — saved by the plugin's END USER (the musician) at runtime. A FULL snapshot of the
                APVTS state, written as XML under Documents/<Company>/<Plugin>/Presets. Applied by
                replacing the state wholesale, which is correct precisely because it is complete.

    The two share one unified, index-addressed list: [0, numFactory) are factory, the rest are
    user. That is the list the plugin's own browser shows.

    The DAW's program menu (getNumPrograms) deliberately exposes the FACTORY presets only, so the
    host-visible program count is stable for the plugin's lifetime. Hosts cache it; growing it when
    a musician saves a patch invites real host bugs. The user bank lives in the plugin's own UI,
    which is where every commercial plugin puts it.

    Copyright (c) 2026 VybeCode Software. All rights reserved.
*/

#pragma once
#include <JuceHeader.h>

#include <algorithm>
#include <vector>

namespace vybecode {

/** One authored value in a factory preset: a full <prefix><param> id and a REAL-unit value. */
struct PresetParam
{
    const char* id;
    float       value;
};

/** A factory preset, as generated from the .psf's presets[]. Sparse by design. */
struct FactoryPreset
{
    const char*        name;
    const char*        category;
    const PresetParam* params;
    int                numParams;
};

//==============================================================================
class PresetManager
{
public:
    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        /** The selected preset changed. */
        virtual void presetChanged (int newIndex, const juce::String& newName) = 0;
        /** The user bank gained or lost entries — any cached name list is stale. */
        virtual void presetListChanged() {}
    };

    // Explicit: the processor holds this as a bare member, and JUCE_DECLARE_NON_COPYABLE below
    // user-declares a (deleted) copy constructor, which suppresses the IMPLICIT default one —
    // C2512 at the member declaration. That is GEN-017 in this repo's own bug registry; it is a
    // shipped defect class here, not a hypothetical.
    PresetManager() = default;

    //==========================================================================
    /** Call once from the AudioProcessor constructor. `factory` may be nullptr / `numFactory` 0
        for a plugin whose .psf authored no presets — the user bank still works. */
    void initialise (juce::AudioProcessorValueTreeState& apvtsRef,
                     const juce::String& companyName,
                     const juce::String& pluginNameIn,
                     const FactoryPreset* factory,
                     int numFactory)
    {
        apvts             = &apvtsRef;
        pluginName        = pluginNameIn;
        factoryPresets    = factory;
        numFactoryPresets = (factory != nullptr) ? juce::jmax (0, numFactory) : 0;

        // Documents/<Company>/<Plugin>/Presets — the plugin author's OWN branding, visible and
        // portable, so a musician can back up, hand-edit and share their patches.
        userPresetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile (sanitise (companyName, "VybeCode"))
                            .getChildFile (sanitise (pluginNameIn, "Plugin"))
                            .getChildFile ("Presets");
        userPresetDir.createDirectory();

        scanUserPresets();
        markClean();
    }

    //==========================================================================
    // Querying — the unified list: [0, numFactory) factory, then user.
    //==========================================================================

    int  getNumPresets()        const { return numFactoryPresets + (int) userPresets.size(); }
    int  getNumFactoryPresets() const { return numFactoryPresets; }
    int  getNumUserPresets()    const { return (int) userPresets.size(); }
    int  getCurrentIndex()      const { return currentIndex; }

    /** True once the user has moved anything since the patch loaded — the UI shows "Warm Glue*".
        Computed by COMPARING against the snapshot taken at load, deliberately, rather than by
        listening for parameter changes: APVTS syncs its state tree to the message thread
        asynchronously, so a listener fires AFTER loadPreset() has already cleared the flag and
        every freshly-loaded patch would show as edited. Comparison is self-synchronising and has
        no such race. Message thread only; cheap enough for a UI poll. */
    bool isDirty() const
    {
        if (apvts == nullptr || ! cleanSnapshot.isValid()) return false;
        return ! apvts->copyState().isEquivalentTo (cleanSnapshot);
    }

    bool isValidIndex (int index) const { return index >= 0 && index < getNumPresets(); }
    bool isFactory    (int index) const { return index >= 0 && index < numFactoryPresets; }

    juce::String getPresetName (int index) const
    {
        if (! isValidIndex (index)) return {};
        return isFactory (index) ? juce::String (factoryPresets[index].name)
                                 : userPresets[(size_t) (index - numFactoryPresets)].name;
    }

    juce::String getPresetCategory (int index) const
    {
        if (! isValidIndex (index)) return {};
        return isFactory (index) ? juce::String (factoryPresets[index].category)
                                 : userPresets[(size_t) (index - numFactoryPresets)].category;
    }

    juce::String getCurrentPresetName() const { return getPresetName (currentIndex); }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        for (int i = 0; i < getNumPresets(); ++i) names.add (getPresetName (i));
        return names;
    }

    /** Categories in list order, deduplicated — the browser's folder list. */
    juce::StringArray getCategories() const
    {
        juce::StringArray cats;
        for (int i = 0; i < getNumPresets(); ++i)
        {
            const auto c = getPresetCategory (i);
            if (c.isNotEmpty() && ! cats.contains (c)) cats.add (c);
        }
        return cats;
    }

    /** Indices of every preset in a category — what the browser shows for one folder. */
    juce::Array<int> getIndicesForCategory (const juce::String& category) const
    {
        juce::Array<int> out;
        for (int i = 0; i < getNumPresets(); ++i)
            if (getPresetCategory (i) == category) out.add (i);
        return out;
    }

    /** Case-insensitive substring match over names + categories — the browser's search box. */
    juce::Array<int> search (const juce::String& query) const
    {
        juce::Array<int> out;
        if (query.isEmpty())
        {
            for (int i = 0; i < getNumPresets(); ++i) out.add (i);
            return out;
        }
        for (int i = 0; i < getNumPresets(); ++i)
            if (getPresetName (i).containsIgnoreCase (query)
                || getPresetCategory (i).containsIgnoreCase (query))
                out.add (i);
        return out;
    }

    //==========================================================================
    // Loading
    //==========================================================================

    /** Load a preset by unified index. Message thread only — it touches parameters and, for a
        user preset, the filesystem. */
    bool loadPreset (int index)
    {
        if (apvts == nullptr || ! isValidIndex (index)) return false;

        const bool ok = isFactory (index) ? applyFactory (index)
                                          : applyUserFile (userPresets[(size_t) (index - numFactoryPresets)].file);
        if (! ok) return false;

        currentIndex = index;
        markClean();
        notifyChanged();
        return true;
    }

    void loadNextPreset()     { stepPreset (1); }
    void loadPreviousPreset() { stepPreset (-1); }

    //==========================================================================
    // The user bank
    //==========================================================================

    /** Save the CURRENT state as a named user patch, overwriting a same-named one. Returns the
        new unified index, or -1 on failure. */
    int saveUserPreset (const juce::String& name, const juce::String& category = "User")
    {
        if (apvts == nullptr || name.trim().isEmpty()) return -1;

        const auto trimmed = name.trim();
        const auto file = userPresetDir.getChildFile (
            juce::File::createLegalFileName (trimmed) + presetExtension);

        // Wrap the APVTS state in our own element rather than tagging attributes ONTO it: the
        // state tree is replaced verbatim on load, so anything written onto its root would be
        // imported back as stray parameter-state properties.
        juce::XmlElement wrapper (presetTag);
        wrapper.setAttribute ("name", trimmed);
        wrapper.setAttribute ("category", category);
        wrapper.setAttribute ("plugin", pluginName);

        auto stateXml = apvts->copyState().createXml();
        if (stateXml == nullptr) return -1;
        wrapper.addChildElement (stateXml.release());   // wrapper takes ownership

        if (! file.getParentDirectory().createDirectory()) return -1;
        if (! wrapper.writeTo (file)) return -1;

        scanUserPresets();
        notifyListChanged();

        const int idx = indexOfUserFile (file);
        if (idx >= 0)
        {
            currentIndex = idx;
            markClean();
            notifyChanged();
        }
        return idx;
    }

    /** Delete a user patch. Factory presets are read-only and always refuse. */
    bool deleteUserPreset (int index)
    {
        if (! isValidIndex (index) || isFactory (index)) return false;
        if (! userPresets[(size_t) (index - numFactoryPresets)].file.deleteFile()) return false;

        scanUserPresets();
        notifyListChanged();
        currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), currentIndex);
        notifyChanged();
        return true;
    }

    /** Rename a user patch (rewrites the file so the name survives a rescan). */
    bool renameUserPreset (int index, const juce::String& newName)
    {
        if (! isValidIndex (index) || isFactory (index) || newName.trim().isEmpty()) return false;

        auto& entry = userPresets[(size_t) (index - numFactoryPresets)];
        auto xml = juce::parseXML (entry.file);
        if (xml == nullptr) return false;

        const auto trimmed = newName.trim();
        xml->setAttribute ("name", trimmed);
        const auto newFile = userPresetDir.getChildFile (
            juce::File::createLegalFileName (trimmed) + presetExtension);

        if (! xml->writeTo (newFile)) return false;
        if (newFile != entry.file) entry.file.deleteFile();

        scanUserPresets();
        notifyListChanged();
        const int idx = indexOfUserFile (newFile);
        if (idx >= 0) { currentIndex = idx; notifyChanged(); }
        return true;
    }

    /** Re-read the bank from disk — the user may have added files behind our back. */
    void refresh() { scanUserPresets(); notifyListChanged(); }

    juce::File getUserPresetDirectory() const { return userPresetDir; }

    //==========================================================================
    // A/B compare — two full snapshots the user can flip between.
    //==========================================================================

    /** Copy the current state into the inactive slot, so A and B start equal. */
    void copyToOtherSlot()
    {
        if (apvts == nullptr) return;
        (usingSlotB ? slotA : slotB) = apvts->copyState().createCopy();
    }

    /** Swap to the other slot. The first swap seeds it from the current state, so B is never
        an empty tree that would wipe every parameter. */
    void toggleSlot()
    {
        if (apvts == nullptr) return;
        auto& leaving = usingSlotB ? slotB : slotA;
        auto& entering = usingSlotB ? slotA : slotB;

        leaving = apvts->copyState().createCopy();
        if (! entering.isValid()) entering = leaving.createCopy();

        usingSlotB = ! usingSlotB;
        apvts->replaceState (entering.createCopy());
        notifyChanged();
    }

    bool isUsingSlotB() const { return usingSlotB; }

    //==========================================================================
    // Host state — call from get/setStateInformation.
    //==========================================================================

    /** The manager's own state (which preset is selected). Merge as a CHILD of the plugin
        state; without it the plugin reopens a saved session reporting preset #1 while the
        knobs hold the session's real values — a desync every host displays. */
    juce::ValueTree getState() const
    {
        juce::ValueTree s (getStateTag());
        s.setProperty ("currentIndex", currentIndex, nullptr);
        s.setProperty ("currentName", getCurrentPresetName(), nullptr);
        return s;
    }

    /** Restore the SELECTION only — never the parameters. The host has already restored the
        APVTS state by this point, and re-applying the preset would discard the user's edits. */
    void restoreState (const juce::ValueTree& s)
    {
        if (! s.isValid()) return;

        // Resolve by name first: a plugin update can reorder or insert presets, and a stale index
        // would silently mislabel the user's patch as a different one.
        const juce::String savedName = s.getProperty ("currentName", "").toString();
        if (savedName.isNotEmpty())
        {
            for (int i = 0; i < getNumPresets(); ++i)
                if (getPresetName (i) == savedName) { currentIndex = i; return; }
        }

        const int savedIndex = (int) s.getProperty ("currentIndex", 0);
        currentIndex = isValidIndex (savedIndex) ? savedIndex : 0;
    }

    /** Constructed on first use rather than as a static member: a static juce::Identifier builds a
        juce::String at static-init time, whose ordering against JUCE's own globals is not
        guaranteed. */
    static const juce::Identifier& getStateTag()
    {
        static const juce::Identifier tag { "VybePresetManager" };
        return tag;
    }

    //==========================================================================
    void addListener    (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    //==========================================================================
    struct UserPreset
    {
        juce::String name, category;
        juce::File   file;
    };

    static constexpr const char* presetTag       = "VybePreset";
    static constexpr const char* presetExtension = ".vybepreset";

    static juce::String sanitise (const juce::String& s, const juce::String& fallback)
    {
        const auto clean = juce::File::createLegalFileName (s.trim());
        return clean.isNotEmpty() ? clean : fallback;
    }

    void stepPreset (int delta)
    {
        const int n = getNumPresets();
        if (n <= 0) return;
        loadPreset (((currentIndex + delta) % n + n) % n);   // wraps in both directions
    }

    /** Apply a SPARSE factory preset: only the params it names, each through its own
        NormalisableRange. An id the processor does not register is skipped, never guessed. */
    bool applyFactory (int index)
    {
        const auto& p = factoryPresets[index];
        for (int i = 0; i < p.numParams; ++i)
        {
            if (auto* param = apvts->getParameter (p.params[i].id))
                param->setValueNotifyingHost (param->convertTo0to1 (p.params[i].value));
        }
        return true;
    }

    /** Apply a FULL user snapshot by replacing the state — correct only because it is complete. */
    bool applyUserFile (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr) return false;

        // Accept our wrapper, and also a bare state tree (a hand-authored or older file).
        const juce::XmlElement* stateXml = nullptr;
        if (xml->hasTagName (presetTag))
            stateXml = xml->getChildByName (apvts->state.getType().toString());
        else if (xml->hasTagName (apvts->state.getType()))
            stateXml = xml.get();

        if (stateXml == nullptr) return false;
        apvts->replaceState (juce::ValueTree::fromXml (*stateXml));
        return true;
    }

    int indexOfUserFile (const juce::File& file) const
    {
        for (size_t i = 0; i < userPresets.size(); ++i)
            if (userPresets[i].file == file) return numFactoryPresets + (int) i;
        return -1;
    }

    void scanUserPresets()
    {
        userPresets.clear();
        if (! userPresetDir.isDirectory()) return;

        auto files = userPresetDir.findChildFiles (juce::File::findFiles, false,
                                                   juce::String ("*") + presetExtension);
        files.sort();

        for (const auto& f : files)
        {
            UserPreset u;
            u.file = f;
            u.name = f.getFileNameWithoutExtension();
            u.category = "User";
            if (auto xml = juce::parseXML (f))
            {
                u.name     = xml->getStringAttribute ("name", u.name);
                u.category = xml->getStringAttribute ("category", "User");
            }
            userPresets.push_back (u);
        }
    }

    /** Snapshot the state as the "unedited" reference isDirty() compares against. */
    void markClean()
    {
        if (apvts != nullptr) cleanSnapshot = apvts->copyState().createCopy();
    }

    void notifyChanged()
    {
        const int idx = currentIndex;
        const auto name = getCurrentPresetName();
        listeners.call ([idx, &name] (Listener& l) { l.presetChanged (idx, name); });
    }

    void notifyListChanged()
    {
        listeners.call ([] (Listener& l) { l.presetListChanged(); });
    }

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    const FactoryPreset* factoryPresets = nullptr;
    int numFactoryPresets = 0;

    std::vector<UserPreset> userPresets;
    juce::File userPresetDir;
    juce::String pluginName;
    int  currentIndex = 0;
    juce::ValueTree cleanSnapshot;

    juce::ValueTree slotA, slotB;
    bool usingSlotB = false;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace vybecode
