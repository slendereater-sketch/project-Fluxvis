#include "engine.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <filesystem>

#define TAG "Engine"

Engine* Engine::sInstance = nullptr;

Engine* Engine::GetInstance() {
    if (!sInstance) sInstance = new Engine();
    return sInstance;
}

Engine::Engine() {
    mAudio.Start();
}

Engine::~Engine() {
}

void Engine::SetSurface(ANativeWindow* window) {
    if (window) {
        if (mRenderer.Init(window)) {
            mInitialized = true;
            mVideoDecoder.SetOutputWindow(window); // In a real Vulkan app, we'd decode to a texture
            
            // Try to load a video from the specified directory
            std::string videoDir = "/storage/emulated/0/Pictures/psychedelicshaders/";
            try {
                for (const auto& entry : std::filesystem::directory_iterator(videoDir)) {
                    if (entry.path().extension() == ".mp4") {
                        mCurrentVideoPath = entry.path().string();
                        __android_log_print(ANDROID_LOG_INFO, TAG, "Loading video: %s", mCurrentVideoPath.c_str());
                        if (mVideoDecoder.LoadVideo(mCurrentVideoPath)) {
                            mVideoDecoder.Start();
                        }
                        break;
                    }
                }
            } catch (const std::exception& e) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Error listing directory: %s", e.what());
            }
        }
    } else {
        mRenderer.Terminate();
        mVideoDecoder.Stop();
        mInitialized = false;
    }
}

void Engine::OnResize(int width, int height) {
    mWidth = width;
    mHeight = height;
    mRenderer.OnResize(width, height);
}

void Engine::Render() {
    if (!mInitialized) return;

    AudioFeatures features = mAudio.GetLatestFeatures();
    
    // Update video decoding
    mVideoDecoder.Update();
    if (mVideoDecoder.IsFinished()) {
        // Loop video
        mVideoDecoder.LoadVideo(mCurrentVideoPath);
        mVideoDecoder.Start();
    }

    // Render with Vulkan
    mRenderer.Render(features);
}

void Engine::PushAudioData(const float* data, int length) {
    mAudio.PushData(data, length);
}

// JNI methods
extern "C" JNIEXPORT void JNICALL
Java_com_visualizer_engine_NativeInterface_nSetSurface(JNIEnv* env, jobject, jobject surface) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    Engine::GetInstance()->SetSurface(window);
}

extern "C" JNIEXPORT void JNICALL
Java_com_visualizer_engine_NativeInterface_nOnResize(JNIEnv*, jobject, jint w, jint h) {
    Engine::GetInstance()->OnResize(w, h);
}

extern "C" JNIEXPORT void JNICALL
Java_com_visualizer_engine_NativeInterface_nRender(JNIEnv*, jobject) {
    Engine::GetInstance()->Render();
}

extern "C" JNIEXPORT void JNICALL
Java_com_visualizer_engine_NativeInterface_nPushAudioData(JNIEnv* env, jobject, jfloatArray data) {
    jsize len = env->GetArrayLength(data);
    jfloat* body = env->GetFloatArrayElements(data, nullptr);
    Engine::GetInstance()->PushAudioData(body, len);
    env->ReleaseFloatArrayElements(data, body, JNI_ABORT);
}
