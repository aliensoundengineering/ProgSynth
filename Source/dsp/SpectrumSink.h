#pragma once

#include <atomic>
#include <array>

namespace progsynth {

// Lock-free single-producer / single-consumer spectrum tap.
// The audio thread writes mono-mixed samples; the UI thread snapshots the
// most recent fftSize samples for analysis.
class SpectrumSink {
public:
    static constexpr int fftSize = 1024;
    static constexpr int bufSize = fftSize * 2;     // headroom for reads
    static_assert((bufSize & (bufSize - 1)) == 0, "bufSize must be power of two");

    SpectrumSink() { buf.fill(0.0f); }

    // Audio / UI thread (set from prepareToPlay; read from UI).
    void   setSampleRate(double sr) noexcept { sampleRate.store(sr, std::memory_order_relaxed); }
    double getSampleRate() const noexcept    { return sampleRate.load(std::memory_order_relaxed); }

    // Audio thread.
    void pushStereoBlock(const float* L, const float* R, int n) noexcept {
        int w = writeIdx.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) {
            float s = 0.5f * (L[i] + (R ? R[i] : L[i]));
            buf[w & (bufSize - 1)] = s;
            ++w;
        }
        writeIdx.store(w, std::memory_order_release);
    }

    // UI thread: copies the last fftSize samples (oldest -> newest) into dest.
    void snapshot(float* dest) const noexcept {
        int w = writeIdx.load(std::memory_order_acquire);
        int start = w - fftSize;
        for (int i = 0; i < fftSize; ++i) {
            dest[i] = buf[(start + i) & (bufSize - 1)];
        }
    }

private:
    std::array<float, bufSize> buf{};
    std::atomic<int>    writeIdx{0};
    std::atomic<double> sampleRate{44100.0};
};

} // namespace progsynth
