#pragma once

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <android/native_window.h>
#include <string>
#include <vector>

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool LoadVideo(const std::string& path);
    void SetOutputWindow(ANativeWindow* window);
    bool Start();
    void Stop();
    
    // Decodes the next frame. Should be called in a loop.
    void Update();

    bool IsFinished() const { return mFinished; }

private:
    AMediaExtractor* mExtractor = nullptr;
    AMediaCodec* mCodec = nullptr;
    ANativeWindow* mWindow = nullptr;
    
    bool mStarted = false;
    bool mFinished = false;
    bool mSawInputEOS = false;
    bool mSawOutputEOS = false;

    void Reset();
};
