#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace progsynth {

struct Theme {
    // yellow phosphor on dark grey
    static juce::Colour bg()        { return juce::Colour::fromRGB(20, 20, 20); }
    static juce::Colour bgPanel()   { return juce::Colour::fromRGB(28, 28, 28); }
    static juce::Colour fg()        { return juce::Colour::fromRGB(232, 196,  64); }
    static juce::Colour fgDim()     { return juce::Colour::fromRGB(170, 142,  40); }
    static juce::Colour accent()    { return juce::Colour::fromRGB(255, 235, 100); }
    static juce::Colour error()     { return juce::Colour::fromRGB(220,  90,  60); }
    static juce::Colour gridLine()  { return juce::Colour::fromRGB(60,  60,  60); }

    static juce::Font monoFont(float size = 14.0f) {
        return juce::Font(juce::FontOptions("Consolas", size, juce::Font::plain));
    }
};

} // namespace progsynth
