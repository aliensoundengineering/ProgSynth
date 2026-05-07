#include "StatusPane.h"
#include "Theme.h"

namespace progsynth {

StatusPane::StatusPane() {
    text.setMultiLine(true);
    text.setReadOnly(true);
    text.setScrollbarsShown(true);
    text.setCaretVisible(false);
    text.setFont(Theme::monoFont(13.0f));
    text.setColour(juce::TextEditor::backgroundColourId, Theme::bgPanel());
    text.setColour(juce::TextEditor::textColourId,       Theme::fg());
    text.setColour(juce::TextEditor::outlineColourId,    Theme::gridLine());
    text.setColour(juce::TextEditor::focusedOutlineColourId, Theme::gridLine());
    addAndMakeVisible(text);
}

StatusPane::~StatusPane() = default;

void StatusPane::resized()              { text.setBounds(getLocalBounds().reduced(4)); }
void StatusPane::paint(juce::Graphics& g) { g.fillAll(Theme::bgPanel()); }

void StatusPane::showInfo(const juce::String& line) {
    text.setText(line, false);
}

void StatusPane::showCompileErrors(const juce::StringArray& lines) {
    juce::String s;
    s << "STATUS: compile FAILED\n\n";
    for (const auto& l : lines) s << l << "\n";
    text.setColour(juce::TextEditor::textColourId, Theme::error());
    text.setText(s, false);
}

void StatusPane::showCompileSuccess(const CompiledPatch& patch) {
    text.setColour(juce::TextEditor::textColourId, Theme::fg());

    juce::String s;
    s << "STATUS: compiled OK"
      << " - " << patch.activeRoutings << " active routings, 16 voices\n";

    if (!patch.warnings.empty()) {
        s << "\nwarnings:\n";
        for (auto& w : patch.warnings) s << "  - " << w.c_str() << "\n";
    }
    if (!patch.routings.empty()) {
        s << "\nroutings:\n";
        for (auto& r : patch.routings) s << "  " << r.c_str() << "\n";
    }
    s << "\n" << renderDiagram(patch);
    text.setText(s, false);
}

juce::String StatusPane::renderDiagram(const CompiledPatch& patch) const {
    juce::String d;
    juce::String fType = (patch.filter.type == FilterKind::LP) ? "lp" : "hp";

    auto hasMod = [&](int idx) {
        auto m = [idx](const Expression& e) { return (e.inputMask & (1u<<idx)) != 0; };
        return m(patch.osc1.freq) || m(patch.osc1.level)
            || m(patch.osc2.freq) || m(patch.osc2.level)
            || m(patch.osc3.freq) || m(patch.osc3.level)
            || m(patch.filter.cutoff) || m(patch.filter.res)
            || m(patch.filter.env)    || m(patch.filter.keytrack);
    };
    bool useAmp  = (patch.ampEnv.s.isConstant ? true : true);
    bool useFlt  = hasMod(ExprInputs::FltEnv);
    bool useLfo1 = hasMod(ExprInputs::Lfo1);
    bool useLfo2 = hasMod(ExprInputs::Lfo2);

    d << "OSC1 ---+\n";
    d << "OSC2 ---+--> MIX --> FILTER[" << fType << "] --> VCA --> OUT\n";
    d << "OSC3 ---+              ^                   ^\n";
    d << "                       |                   |\n";
    d << "                     " << (useFlt ? "fltEnv" : "      ");
    d << "             " << (useAmp ? "ampEnv" : "      ") << "\n";
    if (useLfo1 || useLfo2) {
        d << "                     ";
        if (useLfo1) d << "lfo1 ";
        if (useLfo2) d << "lfo2 ";
        d << "\n";
    }
    return d;
}

} // namespace progsynth
