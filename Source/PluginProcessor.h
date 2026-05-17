#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "dsp/SynthEngine.h"
#include "dsp/SpectrumSink.h"
#include "lang/CompiledPatch.h"
#include "lang/Compiler.h"
#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "Presets.h"

class ProgSynthAudioProcessor : public juce::AudioProcessor
{
public:
    ProgSynthAudioProcessor();
    ~ProgSynthAudioProcessor() override;

    void prepareToPlay (double sr, int block) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ProgSynth"; }

    bool acceptsMidi() const override   { return true; }
    bool producesMidi() const override  { return false; }
    bool isMidiEffect() const override  { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                 { return 1; }
    int getCurrentProgram() override              { return 0; }
    void setCurrentProgram (int) override         {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- editor-facing API --------------------------------------------------

    // Returns true on success, populates errorLines on failure.
    // The editor passes its current text in.
    bool compileScript(const juce::String& source,
                       progsynth::CompiledPatch& outPatch,
                       juce::StringArray& outErrorLines);

    // Atomically install a compiled patch into the live audio engine.
    void installPatch(progsynth::CompiledPatch patch);

    juce::String getScriptText() const   { return scriptText; }
    void         setScriptText(const juce::String& s) { scriptText = s; }

    static const char* getDefaultPatchScript();

    // ---- preset API ---------------------------------------------------------
    progsynth::PresetManager&       getPresetManager()       { return presets; }
    const progsynth::PresetManager& getPresetManager() const { return presets; }

    // Replace the live script with a preset's script and (if it compiles)
    // install the resulting patch into the engine. Returns false when the
    // preset isn't found or fails to compile.
    bool loadPreset(const juce::String& name);

    // Persist the current script text under `name`. Returns false on name
    // collision with a factory preset, empty name, or write failure.
    bool saveCurrentAsPreset(const juce::String& name);

    // UI-facing accessors -----------------------------------------------------
    juce::MidiKeyboardState&        getKeyboardState()       { return keyboardState; }
    const progsynth::SpectrumSink&  getSpectrumSink()  const { return spectrum; }
    double                          getCurrentSampleRate() const { return getSampleRate(); }
    int                             getActiveVoiceCount() const { return engine.activeVoices(); }
    float                           getLastBlockPeak()    const { return engine.lastBlockPeak(); }
    int                             getLastMidiEventCount() const { return lastMidiEvents.load(std::memory_order_relaxed); }
    int                             getLastMidiNoteOnCount() const { return lastMidiNoteOns.load(std::memory_order_relaxed); }
    bool                            hasPatchInstalled() const { return engine.hasPatchInstalled(); }
    float                           getCurrentMasterGain() const { return engine.currentMasterGain(); }

private:
    progsynth::SynthEngine engine;
    progsynth::SpectrumSink spectrum;
    progsynth::PresetManager presets;
    juce::MidiKeyboardState keyboardState;
    juce::String           scriptText;
    std::atomic<int>       lastMidiEvents{0};
    std::atomic<int>       lastMidiNoteOns{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgSynthAudioProcessor)
};
