#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "lang/Compiler.h"

using namespace progsynth;

const char* ProgSynthAudioProcessor::getDefaultPatchScript() {
    return
        "# patch: \"hello world\"\n"
        "\n"
        "osc1 { wave=saw, freq=note,         level=0.6 }\n"
        "osc2 { wave=saw, freq=note + 7cent, level=0.6 }\n"
        "osc3 { wave=saw, freq=note - 7cent, level=0.6 }\n"
        "\n"
        "filter { type=lp, cutoff=1500 + fltEnv*2500, res=0.4 }\n"
        "\n"
        "ampEnv { a=5ms, d=250ms, s=0.75, r=400ms }\n"
        "fltEnv { a=5ms, d=350ms, s=0.30, r=300ms }\n"
        "\n"
        "master { volume=-6dB }\n";
}

ProgSynthAudioProcessor::ProgSynthAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      scriptText(getDefaultPatchScript())
{
    // compile the default so audio works immediately
    juce::StringArray errs;
    progsynth::CompiledPatch patch;
    if (compileScript(scriptText, patch, errs)) {
        installPatch(std::move(patch));
    }
}

ProgSynthAudioProcessor::~ProgSynthAudioProcessor() = default;

void ProgSynthAudioProcessor::installPatch(progsynth::CompiledPatch patch) {
    engine.setPatch(std::make_shared<const progsynth::CompiledPatch>(std::move(patch)));
}

void ProgSynthAudioProcessor::prepareToPlay(double sr, int block) {
    engine.prepareToPlay(sr, block);
    spectrum.setSampleRate(sr);
}

void ProgSynthAudioProcessor::releaseResources() { engine.releaseResources(); }

bool ProgSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void ProgSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm())
                engine.setBpm(*bpm);
        }
    }

    // Merge events from the on-screen keyboard into the incoming MIDI buffer
    // BEFORE the engine processes.
    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);

    int midiEvents = 0;
    int midiNoteOns = 0;
    for (const auto metadata : midi) {
        ++midiEvents;
        if (metadata.getMessage().isNoteOn())
            ++midiNoteOns;
    }
    lastMidiEvents.store(midiEvents, std::memory_order_relaxed);
    lastMidiNoteOns.store(midiNoteOns, std::memory_order_relaxed);

    // Render synth (no master gain yet) and tap to the spectrum so the
    // analyser shows the synth output regardless of what master.volume is.
    engine.renderSynth(buffer, midi);

    const float* L = buffer.getReadPointer(0);
    const float* R = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;
    spectrum.pushStereoBlock(L, R, buffer.getNumSamples());

    engine.applyEffects(buffer);
    engine.applyMaster(buffer);
}

juce::AudioProcessorEditor* ProgSynthAudioProcessor::createEditor() {
    return new ProgSynthAudioProcessorEditor(*this);
}

void ProgSynthAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    dest.setSize(0);
    auto data = scriptText.toRawUTF8();
    dest.append(data, scriptText.getNumBytesAsUTF8());
}

void ProgSynthAudioProcessor::setStateInformation(const void* data, int size) {
    if (size <= 0 || data == nullptr) return;
    scriptText = juce::String::fromUTF8(static_cast<const char*>(data), size);

    juce::StringArray errs;
    progsynth::CompiledPatch patch;
    if (compileScript(scriptText, patch, errs)) {
        installPatch(std::move(patch));
    }
}

bool ProgSynthAudioProcessor::compileScript(const juce::String& source,
                                            progsynth::CompiledPatch& outPatch,
                                            juce::StringArray& outErrorLines)
{
    std::vector<LexError>     lexErrs;
    std::vector<ParseError>   parseErrs;
    std::vector<CompileError> compileErrs;

    Lexer lexer(source.toStdString());
    auto tokens = lexer.tokenize(lexErrs);

    Parser parser(std::move(tokens), parseErrs);
    auto program = parser.parseProgram();

    auto patch = Compiler::compile(program, compileErrs);

    auto fmt = [](const std::string& msg, int line, int col) {
        return juce::String("line ") + juce::String(line)
             + ":" + juce::String(col) + ": " + juce::String(msg);
    };

    for (auto& e : lexErrs)     outErrorLines.add(fmt(e.message, e.line, e.col));
    for (auto& e : parseErrs)   outErrorLines.add(fmt(e.message, e.line, e.col));
    for (auto& e : compileErrs) outErrorLines.add(fmt(e.message, e.line, e.col));

    bool ok = lexErrs.empty() && parseErrs.empty() && compileErrs.empty();
    if (ok) outPatch = std::move(patch);
    return ok;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ProgSynthAudioProcessor();
}
