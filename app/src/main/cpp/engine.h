#pragma once

#include <android/native_window.h>
#include "audio_capture.h"
#include "vulkan_renderer.h"
#include "video_decoder.h"
#include <mutex>
#include <string>

class Engine {
public:
    static Engine* GetInstance();

    void SetSurface(ANativeWindow* window);
    void OnResize(int width, int height);
    
    void Render();
    void PushAudioData(const float* data, int length);

private:
    Engine();
    ~Engine();

    static Engine* sInstance;

    AudioCapture mAudio;
    VulkanRenderer mRenderer;
    VideoDecoder mVideoDecoder;

    int mWidth, mHeight;
    bool mInitialized = false;
    
    std::string mCurrentVideoPath;
};
