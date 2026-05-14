cmake_minimum_required(VERSION 3.22)

project(ProgSynth VERSION 0.1.1)

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
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE         # keep the plugin in the local build output
    PLUGIN_MANUFACTURER_CODE Ycmp         # 4 chars, exactly one uppercase
    PLUGIN_CODE              Msyn         # 4 chars, exactly one uppercase
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "ProgSynth")

# Source files
target_sources(ProgSynth
    PRIVATE
        # plugin glue
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h

        # language
        Source/lang/Units.h
        Source/lang/Token.h
        Source/lang/Ast.h
        Source/lang/ExprTree.h
        Source/lang/CompiledPatch.h
        Source/lang/Lexer.h
        Source/lang/Lexer.cpp
        Source/lang/Parser.h
        Source/lang/Parser.cpp
        Source/lang/Compiler.h
        Source/lang/Compiler.cpp

        # DSP
        Source/dsp/Oscillator.h
        Source/dsp/Oscillator.cpp
        Source/dsp/LFO.h
        Source/dsp/LFO.cpp
        Source/dsp/Envelope.h
        Source/dsp/Envelope.cpp
        Source/dsp/Voice.h
        Source/dsp/Voice.cpp
        Source/dsp/SynthEngine.h
        Source/dsp/SynthEngine.cpp
        Source/dsp/SpectrumSink.h

        # UI
        Source/ui/Theme.h
        Source/ui/EditorPane.h
        Source/ui/EditorPane.cpp
        Source/ui/StatusPane.h
        Source/ui/StatusPane.cpp
        Source/ui/ReplPane.h
        Source/ui/ReplPane.cpp
        Source/ui/SpectrumPane.h
        Source/ui/SpectrumPane.cpp)

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
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
