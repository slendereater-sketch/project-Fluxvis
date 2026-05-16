#pragma once

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <android/native_window.h>
#include "audio_capture.h"
// #include "tpu_inference.h"
#include <mutex>

class Engine {
public:
    static Engine* GetInstance();

    void SetSurface(ANativeWindow* window);
    void InitGLES();
    void TerminateGLES();
    
    void Render();
    void UpdateControls(float zoom, float warp, float dampening);
    void PushAudioData(const float* data, int length);

private:
    Engine();
    ~Engine();

    void CreateFBOs(int width, int height);
    void SetupUBO();
    void SetupQuad();
    GLuint CompileShader(GLenum type, const char* source);
    GLuint LinkProgram(GLuint vert, GLuint frag);

    static Engine* sInstance;

    ANativeWindow* mWindow = nullptr;
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLSurface mSurface = EGL_NO_SURFACE;
    EGLContext mContext = EGL_NO_CONTEXT;

    AudioCapture mAudio;

    // Graphics state
    GLuint mFBO[2];
    GLuint mTextures[2];
    GLuint mUBO;
    GLuint mQuadVBO;
    GLuint mProgram;
    GLuint mBlitProgram;

    // Uniform locations
    GLint mResLoc, mTimeLoc, mPrevLoc;
    GLint mBlitTexLoc;
    int mPingPongIdx = 0;
    int mWidth, mHeight;
    float mStartTime;

    float mUserControls[3] = {1.0f, 0.5f, 0.1f};
    float mWeights[100] = {0.0f}; // Placeholder weights
    std::mutex mControlMutex;
};
