#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

#include "../dsp/SpectrumSink.h"

namespace progsynth {

class SpectrumPane : public juce::Component, private juce::Timer {
public:
    explicit SpectrumPane(const SpectrumSink& sink, double sampleRate = 44100.0);
    ~SpectrumPane() override;

    void setSampleRate(double sr) { sampleRate = sr; }

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    const SpectrumSink& sink;
    double sampleRate = 44100.0;

    static constexpr int fftOrder = 10;                  // 2^10 = 1024
    static constexpr int fftSize  = 1 << fftOrder;
    static_assert(fftSize == SpectrumSink::fftSize, "fft size mismatch");

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann
    };

    std::array<float, fftSize * 2> fftData{};      // real input + complex out
    std::array<float, fftSize / 2> mags{};         // smoothed magnitudes (dB)
};

} // namespace progsynth
