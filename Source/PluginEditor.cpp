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

    presetLabel.setText("preset:", juce::dontSendNotification);
    presetLabel.setFont(Theme::monoFont(13.0f));
    presetLabel.setColour(juce::Label::textColourId, Theme::fgDim());
    presetLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(presetLabel);

    presetBox.setColour(juce::ComboBox::backgroundColourId,     Theme::bg());
    presetBox.setColour(juce::ComboBox::textColourId,           Theme::fg());
    presetBox.setColour(juce::ComboBox::outlineColourId,        Theme::gridLine());
    presetBox.setColour(juce::ComboBox::arrowColourId,          Theme::accent());
    presetBox.setColour(juce::ComboBox::focusedOutlineColourId, Theme::accent());
    presetBox.setColour(juce::ComboBox::buttonColourId,         Theme::bgPanel());
    presetBox.setTextWhenNothingSelected("(select preset)");
    presetBox.onChange = [this]() { onPresetChosen(); };
    addAndMakeVisible(presetBox);

    saveButton.setColour(juce::TextButton::buttonColourId,   Theme::bgPanel());
    saveButton.setColour(juce::TextButton::buttonOnColourId, Theme::accent());
    saveButton.setColour(juce::TextButton::textColourOffId,  Theme::fg());
    saveButton.setColour(juce::TextButton::textColourOnId,   Theme::bg());
    saveButton.onClick = [this]() { onSaveClicked(); };
    addAndMakeVisible(saveButton);

    refreshPresetList();

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

    auto presetRow = getLocalBounds().removeFromBottom(30);
    g.setColour(Theme::bgPanel());
    g.fillRect(presetRow);
    g.setColour(Theme::gridLine());
    g.drawHorizontalLine(presetRow.getY(), 0.0f, (float) getWidth());

    g.setColour(Theme::fg());
    g.setFont(Theme::monoFont(14.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x94\x80  ::\xce\xb1\xcf\x83\xce\xb5~  \xe2\x94\x80   ASE ProgSynth"),
               top.reduced(8, 0),
               juce::Justification::centredLeft);

    // live diagnostics on the right side of the top bar
    int    voices = processorRef.getActiveVoiceCount();
    float  peak   = processorRef.getLastBlockPeak();
    float  master = processorRef.getCurrentMasterGain();

    juce::String diag;
    diag << "master: " << juce::String(master, 3)
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

    auto presetRow  = r.removeFromBottom(30);      // preset bar (bottom)
    presetRow.reduce(8, 4);
    presetLabel.setBounds(presetRow.removeFromLeft(56));
    presetRow.removeFromLeft(4);
    saveButton.setBounds(presetRow.removeFromRight(96));
    presetRow.removeFromRight(6);
    presetBox.setBounds(presetRow);

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
        repl.echo("  compile         - recompile editor buffer");
        repl.echo("  routes / voices - status pane shows active routings & voices");
        repl.echo("  reset           - revert live patch to editor buffer");
        repl.echo("  presets         - list factory and user presets");
        repl.echo("  preset <name>   - load preset by name (case-insensitive)");
        repl.echo("  save <name>     - save current buffer as a user preset");
        repl.echo("  help            - this list");
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
    if (cmd.equalsIgnoreCase("presets")) {
        const auto& list = processorRef.getPresetManager().all();
        repl.echo("presets (" + juce::String((int) list.size()) + "):");
        for (auto& p : list) {
            repl.echo(juce::String("  [") + (p.isFactory ? "F" : "U") + "] " + p.name);
        }
        return;
    }
    if (cmd.startsWithIgnoreCase("preset ")) {
        auto name = cmd.substring(7).trim();
        if (name.isEmpty()) { repl.echo("usage: preset <name>"); return; }
        if (processorRef.loadPreset(name)) {
            editor.setText(processorRef.getScriptText());
            compileFromEditor();
            presetBox.setText(processorRef.getPresetManager().find(name)->name,
                              juce::dontSendNotification);
            repl.echo("loaded: " + name);
        } else {
            repl.echo("preset not found or failed to compile: " + name);
        }
        return;
    }
    if (cmd.startsWithIgnoreCase("save ")) {
        auto name = cmd.substring(5).trim();
        if (name.isEmpty()) { repl.echo("usage: save <name>"); return; }
        processorRef.setScriptText(editor.getText());
        if (processorRef.saveCurrentAsPreset(name)) {
            refreshPresetList(name);
            repl.echo("saved: " + name);
        } else {
            repl.echo("save failed (factory name collision or invalid): " + name);
        }
        return;
    }
    if (cmd.startsWithIgnoreCase("set ") ||
        cmd.startsWithIgnoreCase("get ") ||
        cmd.startsWithIgnoreCase("load "))
    {
        repl.echo("note: '" + cmd.upToFirstOccurrenceOf(" ", false, true)
                  + "' not implemented in v0.1; edit the script and Ctrl+Enter");
        return;
    }
    repl.echo("unknown command: " + cmd + "  (try 'help')");
}

// ---------------------------------------------------------------------------
// Preset bar
// ---------------------------------------------------------------------------

void ProgSynthAudioProcessorEditor::refreshPresetList(const juce::String& selectName) {
    presetBox.clear(juce::dontSendNotification);

    const auto& list = processorRef.getPresetManager().all();
    int id = 1;

    bool anyFactory = false, anyUser = false;
    for (auto& p : list) {
        if (p.isFactory) anyFactory = true; else anyUser = true;
    }

    if (anyFactory) {
        presetBox.addSectionHeading("factory");
        for (auto& p : list)
            if (p.isFactory) presetBox.addItem(p.name, id++);
    }
    if (anyUser) {
        presetBox.addSectionHeading("user");
        for (auto& p : list)
            if (!p.isFactory) presetBox.addItem(p.name, id++);
    }

    if (selectName.isNotEmpty())
        presetBox.setText(selectName, juce::dontSendNotification);
}

void ProgSynthAudioProcessorEditor::onPresetChosen() {
    auto name = presetBox.getText();
    if (name.isEmpty()) return;

    if (processorRef.loadPreset(name)) {
        editor.setText(processorRef.getScriptText());
        compileFromEditor();
        repl.echo("preset loaded: " + name);
    } else {
        repl.echo("preset failed to load: " + name);
    }
}

void ProgSynthAudioProcessorEditor::onSaveClicked() {
    auto* w = new juce::AlertWindow("Save preset",
                                    "Enter a name for the current patch:",
                                    juce::AlertWindow::NoIcon);
    w->addTextEditor("name", "", "");
    w->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    w->enterModalState(true,
        juce::ModalCallbackFunction::create([this, w](int result) {
            if (result == 1) {
                auto name = w->getTextEditorContents("name").trim();
                if (name.isNotEmpty()) {
                    processorRef.setScriptText(editor.getText());
                    if (processorRef.saveCurrentAsPreset(name)) {
                        refreshPresetList(name);
                        repl.echo("preset saved: " + name);
                    } else {
                        repl.echo("preset save failed (factory name collision or invalid): "
                                  + name);
                    }
                }
            }
        }),
        true);
}
