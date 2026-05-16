#include "engine.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#define TAG "Engine"

Engine* Engine::sInstance = nullptr;

Engine* Engine::GetInstance() {
    if (!sInstance) sInstance = new Engine();
    return sInstance;
}

Engine::Engine() {
    mAudio.Start();
    // mTPU.Initialize("/data/data/com.visualizer.engine/files/tpu_mixer.tflite");
    for(int i=0; i<100; ++i) mWeights[i] = 0.1f;
}

Engine::~Engine() {
    TerminateGLES();
    mAudio.Stop();
}

void Engine::SetSurface(ANativeWindow* window) {
    mWindow = window;
}

void Engine::InitGLES() {
    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(mDisplay, nullptr, nullptr);

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(mDisplay, configAttribs, &config, 1, &numConfigs);

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    mContext = eglCreateContext(mDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    mSurface = eglCreateWindowSurface(mDisplay, config, mWindow, nullptr);

    eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);

    mWidth = ANativeWindow_getWidth(mWindow);
    mHeight = ANativeWindow_getHeight(mWindow);

    CreateFBOs(mWidth, mHeight);
    SetupUBO();
    
    // Shader compilation omitted for brevity, assuming loaded from assets
}

void Engine::CreateFBOs(int width, int height) {
    glGenFramebuffers(2, mFBO);
    glGenTextures(2, mTextures);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, mTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[i], 0);
    }
}

void Engine::SetupUBO() {
    glGenBuffers(1, &mUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
    glBufferData(GL_UNIFORM_BUFFER, 100 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mUBO);
}

void Engine::Render() {
    auto audio = mAudio.GetLatestFeatures();
    // float weights[100];
    
    {
        std::lock_guard<std::mutex> lock(mControlMutex);
        // mTPU.ProcessNeuralWeights(audio, mUserControls, weights);
        mWeights[0] = audio.bass;
        mWeights[1] = audio.mid;
        mWeights[2] = audio.treble;
        mWeights[26] = mUserControls[1]; // Warp intensity
    }

    // Update UBO
    glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 100 * sizeof(float), mWeights);

    // Ping-pong render
    int nextIdx = 1 - mPingPongIdx;
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO[nextIdx]);
    glViewport(0, 0, mWidth, mHeight);
    
    glUseProgram(mProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[mPingPongIdx]);
    // Draw full-screen quad...
    
    // Final composite to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Draw nextIdx texture to screen...

    eglSwapBuffers(mDisplay, mSurface);
    mPingPongIdx = nextIdx;
}

void Engine::UpdateControls(float zoom, float warp, float dampening) {
    std::lock_guard<std::mutex> lock(mControlMutex);
    mUserControls[0] = zoom;
    mUserControls[1] = warp;
    mUserControls[2] = dampening;
}

// JNI Bindings
extern "C" {
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_init(JNIEnv* env, jobject obj, jobject surface) {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        Engine::GetInstance()->SetSurface(window);
        Engine::GetInstance()->InitGLES();
    }

    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_renderFrame(JNIEnv* env, jobject obj) {
        Engine::GetInstance()->Render();
    }

    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_updateControls(JNIEnv* env, jobject obj, jfloat zoom, jfloat warp, jfloat dampening) {
        Engine::GetInstance()->UpdateControls(zoom, warp, dampening);
    }
}
