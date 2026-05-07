#include "PluginEditor.h"
#include "ui/Theme.h"

using namespace progsynth;

ProgSynthAudioProcessorEditor::ProgSynthAudioProcessorEditor(ProgSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      spectrum(processorRef.getSpectrumSink(), processorRef.getCurrentSampleRate()),
      keyboard(processorRef.getKeyboardState(),
               juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(720, 520, 1800, 1300);
    setSize(960, 820);

    addAndMakeVisible(editor);
    addAndMakeVisible(status);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(repl);
    addAndMakeVisible(keyboard);

    editor.setText(processorRef.getScriptText());
    editor.onCompileRequested = [this]() { compileFromEditor(); };

    repl.onSubmit = [this](const juce::String& line) { handleReplCommand(line); };

    // keyboard styling: tighter octave range that still fits comfortably
    keyboard.setLowestVisibleKey(36);   // C2
    keyboard.setKeyWidth(18.0f);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                       juce::Colour::fromRGB(60, 50, 20));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                       juce::Colour::fromRGB(15, 15, 15));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                       Theme::gridLine());
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                       Theme::accent().withAlpha(0.55f));
    keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, Theme::fg());
    keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId,
                       juce::Colours::black.withAlpha(0.35f));
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId,
                       Theme::bgPanel());
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId,
                       Theme::fg());

    compileFromEditor();
    startTimerHz(15);
}

ProgSynthAudioProcessorEditor::~ProgSynthAudioProcessorEditor() { stopTimer(); }

void ProgSynthAudioProcessorEditor::timerCallback() {
    // Repaint only the top bar so the live diagnostics update without flicker.
    repaint(0, 0, getWidth(), 28);
}

void ProgSynthAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg());

    auto top = getLocalBounds().removeFromTop(28);
    g.setColour(Theme::bgPanel());
    g.fillRect(top);

    g.setColour(Theme::fg());
    g.setFont(Theme::monoFont(14.0f));
    g.drawText("ProgSynth   [Ctrl+Enter to compile]", top.reduced(8, 0),
               juce::Justification::centredLeft);

    // live diagnostics on the right side of the top bar
    int   voices = processorRef.getActiveVoiceCount();
    float peak   = processorRef.getLastBlockPeak();
    int   midiEvents = processorRef.getLastMidiEventCount();
    int   noteOns = processorRef.getLastMidiNoteOnCount();
    bool  patchOk = processorRef.hasPatchInstalled();

    float master = processorRef.getCurrentMasterGain();

    juce::String diag;
    diag << "patch: " << (patchOk ? "ok" : "NULL")
         << "    master: " << juce::String(master, 3)
         << "    midi: " << midiEvents << "/" << noteOns
         << "    voices: " << voices
         << "    peak: ";
    if (peak <= 1.0e-5f) diag << "  -inf dB";
    else                 diag << juce::String(20.0 * std::log10((double) peak), 1) << " dB";

    g.setColour(peak > 1.0e-5f ? Theme::accent() : Theme::fgDim());
    g.drawText(diag, top.reduced(8, 0), juce::Justification::centredRight);
}

void ProgSynthAudioProcessorEditor::resized() {
    auto r = getLocalBounds();
    r.removeFromTop(28);                           // top bar

    auto kbArea     = r.removeFromBottom(80);      // on-screen keyboard
    auto replArea   = r.removeFromBottom(110);     // single-line REPL + history

    int total       = r.getHeight();
    int midH        = juce::jmax(140, total / 4);  // status + spectrum row
    auto editorArea = r.removeFromTop(total - midH);

    auto midRow     = r;                           // remainder = mid row
    auto statusArea = midRow.removeFromLeft(midRow.getWidth() / 2);
    auto specArea   = midRow;

    editor.setBounds(editorArea);
    status.setBounds(statusArea);
    spectrum.setBounds(specArea);
    repl.setBounds(replArea);
    keyboard.setBounds(kbArea);
}

void ProgSynthAudioProcessorEditor::compileFromEditor() {
    auto src = editor.getText();
    processorRef.setScriptText(src);

    juce::StringArray errs;
    progsynth::CompiledPatch patch;
    if (processorRef.compileScript(src, patch, errs)) {
        status.showCompileSuccess(patch);
        processorRef.installPatch(std::move(patch));
    } else {
        status.showCompileErrors(errs);
    }

    spectrum.setSampleRate(processorRef.getCurrentSampleRate());
}

void ProgSynthAudioProcessorEditor::handleReplCommand(const juce::String& line) {
    auto cmd = line.trim();

    if (cmd.equalsIgnoreCase("help")) {
        repl.echo("commands:");
        repl.echo("  compile  - recompile editor buffer");
        repl.echo("  routes   - list active modulation routings");
        repl.echo("  voices   - print voice activity");
        repl.echo("  reset    - revert live patch to editor buffer");
        repl.echo("  help     - this list");
        repl.echo("  (set/get/save/load: not yet implemented in v0.1)");
        return;
    }
    if (cmd.equalsIgnoreCase("compile") || cmd.equalsIgnoreCase("reset")) {
        compileFromEditor();
        repl.echo("ok");
        return;
    }
    if (cmd.equalsIgnoreCase("routes") || cmd.equalsIgnoreCase("voices")) {
        repl.echo("(see status pane)");
        return;
    }
    if (cmd.startsWithIgnoreCase("set ") ||
        cmd.startsWithIgnoreCase("get ") ||
        cmd.startsWithIgnoreCase("save ") ||
        cmd.startsWithIgnoreCase("load "))
    {
        repl.echo("note: '" + cmd.upToFirstOccurrenceOf(" ", false, true)
                  + "' not implemented in v0.1; edit the script and Ctrl+Enter");
        return;
    }
    repl.echo("unknown command: " + cmd + "  (try 'help')");
}
