#include "audio_capture.h"
#include <android/log.h>
#include <algorithm>

#define TAG "AudioCapture"

AudioCapture::AudioCapture() {
    mBuffer.resize(FFT_SIZE, 0.0f);
    AudioFeatures initial;
    mLatestFeatures.store(initial);
}

AudioCapture::~AudioCapture() {
    Stop();
}

bool AudioCapture::Start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(&mStream);
    if (result != oboe::Result::OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open stream: %s", oboe::convertToText(result));
        return false;
    }

    result = mStream->requestStart();
    if (result != oboe::Result::OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to start stream: %s", oboe::convertToText(result));
        return false;
    }

    return true;
}

void AudioCapture::Stop() {
    if (mStream) {
        mStream->stop();
        mStream->close();
        mStream = nullptr;
    }
}

oboe::DataCallbackResult AudioCapture::onAudioReady(
    oboe::AudioStream *audioStream,
    void *audioData,
    int32_t numFrames) {
    (void)audioStream;
    float *samples = static_cast<float *>(audioData);
    ProcessFFT(samples, numFrames);
    
    return oboe::DataCallbackResult::Continue;
}

void AudioCapture::ProcessFFT(const float* samples, int numFrames) {
    int framesToCopy = std::min(numFrames, FFT_SIZE);
    std::copy(samples, samples + framesToCopy, mBuffer.begin());

    std::vector<std::complex<float>> fftData(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i) {
        fftData[i] = {mBuffer[i], 0.0f};
    }

    Radix2FFT(fftData);

    AudioFeatures features;
    float totalEnergy = 0.0f;
    
    for (int i = 0; i < 64; ++i) {
        float mag = std::abs(fftData[i]);
        features.frequency_bins[i] = mag;
        totalEnergy += mag;

        if (i < 8) features.bass += mag;
        else if (i < 32) features.mid += mag;
        else features.treble += mag;
    }

    features.bass /= 8.0f;
    features.mid /= 24.0f;
    features.treble /= 32.0f;

    features.is_beat = mPrevEnergy > 0.0f && (totalEnergy > mPrevEnergy * 1.5f);
    mPrevEnergy = totalEnergy;

    mLatestFeatures.store(features);
}

void AudioCapture::Radix2FFT(std::vector<std::complex<float>>& data) {
    int n = static_cast<int>(data.size());
    if (n <= 1) return;

    std::vector<std::complex<float>> even(n / 2);
    std::vector<std::complex<float>> odd(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        even[i] = data[i * 2];
        odd[i] = data[i * 2 + 1];
    }

    Radix2FFT(even);
    Radix2FFT(odd);

    const float pi = 3.14159265358979323846f;
    for (int k = 0; k < n / 2; ++k) {
        float theta = -2.0f * pi * static_cast<float>(k) / static_cast<float>(n);
        std::complex<float> t = std::polar(1.0f, theta) * odd[k];
        data[k] = even[k] + t;
        data[k + n / 2] = even[k] - t;
    }
}

AudioFeatures AudioCapture::GetLatestFeatures() const {
    return mLatestFeatures.load();
}

void AudioCapture::PushData(const float* samples, int numFrames) {
    ProcessFFT(samples, numFrames);
}

void AudioCapture::DetectBeat(float energy) {
    (void)energy;
}
