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
    virtual ~AudioCapture();

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
    
    static constexpr int FFT_SIZE = 128;
    std::vector<float> mBuffer;
    float mPrevEnergy = 0.0f;

    void Radix2FFT(std::vector<std::complex<float>>& data);
};
