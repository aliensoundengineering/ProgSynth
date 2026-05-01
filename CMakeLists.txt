cmake_minimum_required(VERSION 3.22)

project(ProgSynth VERSION 0.1.0)

# Use C++17 (JUCE 8 requirement)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull in JUCE from the submodule
add_subdirectory(JUCE)

# Declare the plugin target
juce_add_plugin(ProgSynth
    COMPANY_NAME            "aliensoundengineering"
    BUNDLE_ID               "com.aliensoundengineering.ProgSynth"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE          # auto-copy to the system VST3 folder
    PLUGIN_MANUFACTURER_CODE Ycmp         # 4 chars, exactly one uppercase
    PLUGIN_CODE              Msyn         # 4 chars, exactly one uppercase
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "ProgSynth")

# Source files
target_sources(ProgSynth
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

# Compile-time defs that JUCE expects
target_compile_definitions(ProgSynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Link the JUCE modules we need
target_link_libraries(ProgSynth
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
