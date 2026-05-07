#include "SpectrumPane.h"
#include "Theme.h"

#include <cmath>
#include <algorithm>

namespace progsynth {

SpectrumPane::SpectrumPane(const SpectrumSink& s, double sr)
    : sink(s), sampleRate(sr)
{
    setOpaque(true);
    mags.fill(-120.0f);
    startTimerHz(30);
}

SpectrumPane::~SpectrumPane() { stopTimer(); }

void SpectrumPane::timerCallback() {
    // Track host sample-rate changes (it can be 0 at editor construction).
    double sr = sink.getSampleRate();
    if (sr > 0.0) sampleRate = sr;

    sink.snapshot(fftData.data());
    // zero the imaginary half (FFT performFrequencyOnlyForwardTransform expects 2*N)
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // Convert to dB and smooth.
    constexpr float minDb = -100.0f;
    constexpr float refScale = 1.0f / (float) (fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i) {
        float mag = fftData[(size_t) i] * refScale;
        float db  = 20.0f * std::log10(std::max(1.0e-9f, mag));
        if (db < minDb) db = minDb;
        // 1-pole smoothing in dB domain
        mags[(size_t) i] = mags[(size_t) i] * 0.6f + db * 0.4f;
    }
    repaint();
}

void SpectrumPane::paint(juce::Graphics& g) {
    g.fillAll(Theme::bgPanel());

    auto bounds = getLocalBounds().reduced(6);
    if (bounds.isEmpty()) return;

    const float w = (float) bounds.getWidth();
    const float h = (float) bounds.getHeight();
    const float x0 = (float) bounds.getX();
    const float y0 = (float) bounds.getY();

    // dB range to display
    constexpr float topDb    = 0.0f;
    constexpr float bottomDb = -90.0f;

    const double nyquist = sampleRate * 0.5;
    const double minHz   = 20.0;
    const double maxHz   = std::max(minHz + 1.0, nyquist);
    const double logMin  = std::log10(minHz);
    const double logMax  = std::log10(maxHz);
    const double logSpan = logMax - logMin;

    // grid: octave verticals
    g.setColour(Theme::gridLine());
    for (double f = 100.0; f <= maxHz; f *= 10.0) {
        for (double m = 1.0; m < 10.0; ++m) {
            double freq = f * m;
            if (freq < minHz || freq > maxHz) continue;
            double tx = (std::log10(freq) - logMin) / logSpan;
            float xx = x0 + (float) tx * w;
            g.setColour(Theme::gridLine().withAlpha(m == 1.0 ? 0.7f : 0.25f));
            g.drawVerticalLine((int) xx, y0, y0 + h);
        }
    }
    // dB grid horizontal lines every 20 dB
    for (float db = topDb; db >= bottomDb; db -= 20.0f) {
        float ty = (db - topDb) / (bottomDb - topDb);
        float yy = y0 + ty * h;
        g.setColour(Theme::gridLine().withAlpha(0.4f));
        g.drawHorizontalLine((int) yy, x0, x0 + w);
    }

    // build path
    juce::Path p;
    bool started = false;
    const int bins = fftSize / 2;
    for (int i = 1; i < bins; ++i) {
        double freq = (double) i * sampleRate / (double) fftSize;
        if (freq < minHz) continue;
        if (freq > maxHz) break;

        double tx = (std::log10(freq) - logMin) / logSpan;
        float xx = x0 + (float) tx * w;

        float db = mags[(size_t) i];
        float ty = (db - topDb) / (bottomDb - topDb);
        ty = std::clamp(ty, 0.0f, 1.0f);
        float yy = y0 + ty * h;

        if (!started) { p.startNewSubPath(xx, yy); started = true; }
        else          { p.lineTo(xx, yy); }
    }
    if (started) {
        // glow
        g.setColour(Theme::fg().withAlpha(0.25f));
        g.strokePath(p, juce::PathStrokeType(2.5f));
        g.setColour(Theme::accent());
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }

    // border + small label
    g.setColour(Theme::gridLine());
    g.drawRect(bounds.toFloat(), 1.0f);
    g.setColour(Theme::fgDim());
    g.setFont(Theme::monoFont(11.0f));
    g.drawText("spectrum  20Hz - " + juce::String((int) (nyquist)) + "Hz",
               bounds.removeFromTop(14).reduced(6, 0),
               juce::Justification::centredLeft);
}

} // namespace progsynth
