#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <atomic>
#include <complex>
#include <cmath>

struct AudioFeatures {
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;
    float frequency_bins[64] = {0.0f};
    bool is_beat = false;
};

class AudioCapture : public oboe::AudioStreamDataCallback {
public:
    AudioCapture();
    ~AudioCapture();

    bool Start();
    void Stop();

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) override;

    AudioFeatures GetLatestFeatures() const;
    void PushData(const float* samples, int numFrames);

private:
    void ProcessFFT(const float* samples, int numFrames);
    void DetectBeat(float energy);

    oboe::AudioStream *mStream = nullptr;
    mutable std::atomic<AudioFeatures> mLatestFeatures;
    
    // Internal FFT state
    static constexpr int FFT_SIZE = 128; // To get 64 bins
    std::vector<float> mBuffer;
    float mPrevEnergy = 0.0f;

    // Simple Radix-2 FFT implementation
    void Radix2FFT(std::vector<std::complex<float>>& data);
};
