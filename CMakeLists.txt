cmake_minimum_required(VERSION 3.25)
project(ADPCMDecay VERSION 1.0.0)

# Fetch JUCE 8
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# CLAP output via free-audio/clap-juce-extensions (CLAP is NOT part of JUCE core).
# Default OFF — VST3 is always built; CLAP is opt-in. VybeCode Unite turns it ON only when it
# has the sources cached, configuring with:
#   -DVYBE_ENABLE_CLAP=ON -DFETCHCONTENT_SOURCE_DIR_CLAP-JUCE-EXTENSIONS=<local cache>
# (so CLAP builds OFFLINE from the cache). Defaulting OFF means an OLDER Unite that knows nothing
# of CLAP still builds the VST3 exactly as before — no regression. A standalone build can opt in
# with -DVYBE_ENABLE_CLAP=ON (fetches clap-juce-extensions from the network). Pin GIT_TAG in production.
option(VYBE_ENABLE_CLAP "Build the CLAP plugin format (needs the clap-juce-extensions sources)" OFF)
if(VYBE_ENABLE_CLAP)
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# Plugin target
juce_add_plugin(ADPCMDecay
    COMPANY_NAME "VybeCode"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Vybc
    PLUGIN_CODE ADPC
    FORMATS VST3 Standalone
    PRODUCT_NAME "ADPCMDecay"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(ADPCMDecay)

target_sources(ADPCMDecay
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/SoftClipper.cpp
        Source/DSP/Bitcrusher.cpp
        Source/DSP/ButterworthLP.cpp
        Source/DSP/DustGenerator.cpp
        Source/DSP/NoiseGenerator.cpp
        Source/DSP/RandomModulation.cpp
        Source/DSP/DryWetMixer.cpp
        Source/DSP/GainModule.cpp

)

target_include_directories(ADPCMDecay
    PRIVATE
        Source
        Source/DSP
)

target_compile_features(ADPCMDecay PRIVATE cxx_std_17)

# JUCE_USE_CURL / JUCE_WEB_BROWSER default to 1 inside JUCE's own headers
# (juce_core.h / juce_gui_extra.h) — libcurl (Linux) + webkit2gtk (Linux GUI)
# support that this plugin never uses (no WebBrowserComponent, no HTTP calls).
# Left at their defaults, a Linux build can hit undefined-reference link errors
# for curl/webkit symbols that are never actually needed. Baked in here (not
# just via a CI/build-tool -D flag) so every consumer — Unite, a manual Linux
# build, cloud CI — gets the same safe default with no extra flags required.
# No-op on Windows/macOS (both macros are Linux-only concerns upstream).
target_compile_definitions(ADPCMDecay
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(ADPCMDecay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# CLAP build — produces a .clap alongside the VST3 (free-audio/clap-juce-extensions).
# Called last so the CLAP format target inherits the fully-configured shared-code target
# (sources, link libs, and any binary-data the GUI-import step links in later — CMake
# resolves link dependencies globally, so order vs. that append does not matter).
if(VYBE_ENABLE_CLAP)
    clap_juce_extensions_plugin(TARGET ADPCMDecay
        CLAP_ID "ing.vybecod.adpcmdecay"
        CLAP_FEATURES audio-effect)
endif()
