#pragma once

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <android/native_window.h>
#include "audio_capture.h"
#include <mutex>

class Engine {
public:
    static Engine* GetInstance();

    void SetSurface(ANativeWindow* window);
    void OnResize(int width, int height);
    void InitGLES();
    void TerminateGLES();

    void Render();
    void UpdateControls(float zoom, float warp, float dampening);
    void PushAudioData(const float* data, int length);

    private:
    Engine();
    ~Engine();

    void SetupQuad();
    GLuint CompileShader(GLenum type, const char* source);
    GLuint LinkProgram(GLuint vert, GLuint frag);

    static Engine* sInstance;

    AudioCapture mAudio;

    // Graphics state
    GLuint mVAO;
    GLuint mQuadVBO;
    GLuint mProgram;

    // Uniform locations
    GLint mResLoc, mTimeLoc;

    int mWidth, mHeight;
    float mStartTime;

    float mUserControls[3] = {1.0f, 0.5f, 0.1f};
    std::mutex mControlMutex;
    };
