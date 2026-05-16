#include "engine.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <time.h>

#define TAG "Engine"

Engine* Engine::sInstance = nullptr;

Engine* Engine::GetInstance() {
    if (!sInstance) sInstance = new Engine();
    return sInstance;
}

Engine::Engine() {
    mAudio.Start();
    for(int i=0; i<100; ++i) mWeights[i] = 0.1f;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    mStartTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
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
    SetupQuad();

    const char* vSrc = R"(#version 300 es
        layout(location = 0) in vec2 a_pos;
        void main() {
            gl_Position = vec4(a_pos, 0.0, 1.0);
        })";

    const char* fSrc = R"(#version 300 es
        precision highp float;
        layout(location = 0) out vec4 fragColor;
        layout(std140, binding = 0) uniform NeuralWeights { float weights[100]; } u_tpu;
        uniform sampler2D u_prevFrame;
        uniform vec2 u_resolution;
        uniform float u_time;
        void main() {
            vec2 uv = gl_FragCoord.xy / u_resolution;
            float bass = u_tpu.weights[0];
            float mid = u_tpu.weights[1];
            vec3 prev = texture(u_prevFrame, (uv - 0.5) * 0.98 + 0.5).rgb;
            float dist = length(uv - 0.5);
            float ring = smoothstep(0.05 + bass * 0.2, 0.0, abs(dist - 0.2 - mid * 0.3));
            vec3 color = vec3(ring) * vec3(0.0, 0.8, 1.0);
            color += prev * 0.97;
            fragColor = vec4(color, 1.0);
        })";

    const char* blitFSrc = R"(#version 300 es
        precision highp float;
        layout(location = 0) out vec4 fragColor;
        uniform sampler2D u_tex;
        void main() {
            vec2 uv = gl_FragCoord.xy / vec2(textureSize(u_tex, 0));
            fragColor = texture(u_tex, uv);
        })";

    mProgram = LinkProgram(CompileShader(GL_VERTEX_SHADER, vSrc), CompileShader(GL_FRAGMENT_SHADER, fSrc));
    mBlitProgram = LinkProgram(CompileShader(GL_VERTEX_SHADER, vSrc), CompileShader(GL_FRAGMENT_SHADER, blitFSrc));

    mResLoc = glGetUniformLocation(mProgram, "u_resolution");
    mTimeLoc = glGetUniformLocation(mProgram, "u_time");
    mPrevLoc = glGetUniformLocation(mProgram, "u_prevFrame");
    mBlitTexLoc = glGetUniformLocation(mBlitProgram, "u_tex");
}

void Engine::SetupQuad() {
    float vertices[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &mQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
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
    if (!mProgram || !mBlitProgram) return;

    auto audio = mAudio.GetLatestFeatures();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float currentTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f - mStartTime;

    {
        std::lock_guard<std::mutex> lock(mControlMutex);
        mWeights[0] = audio.bass;
        mWeights[1] = audio.mid;
        mWeights[2] = audio.treble;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 100 * sizeof(float), mWeights);

    int nextIdx = 1 - mPingPongIdx;
    
    // Pass 1: Effect
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO[nextIdx]);
    glViewport(0, 0, mWidth, mHeight);
    glUseProgram(mProgram);
    glUniform2f(mResLoc, (float)mWidth, (float)mHeight);
    glUniform1f(mTimeLoc, currentTime);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[mPingPongIdx]);
    glUniform1i(mPrevLoc, 0);
    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Pass 2: Blit
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mWidth, mHeight);
    glUseProgram(mBlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[nextIdx]);
    glUniform1i(mBlitTexLoc, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(mDisplay, mSurface);
    mPingPongIdx = nextIdx;
}

void Engine::UpdateControls(float zoom, float warp, float dampening) {
    std::lock_guard<std::mutex> lock(mControlMutex);
    mUserControls[0] = zoom; mUserControls[1] = warp; mUserControls[2] = dampening;
}

void Engine::PushAudioData(const float* data, int length) { mAudio.PushData(data, length); }

void Engine::TerminateGLES() {
    if (mDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (mContext != EGL_NO_CONTEXT) eglDestroyContext(mDisplay, mContext);
        if (mSurface != EGL_NO_SURFACE) eglDestroySurface(mDisplay, mSurface);
        eglTerminate(mDisplay);
    }
}

GLuint Engine::CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512]; glGetShaderInfoLog(shader, 512, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Shader Error: %s", log);
        return 0;
    }
    return shader;
}

GLuint Engine::LinkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert); glAttachShader(prog, frag);
    glLinkProgram(prog);
    return prog;
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_init(JNIEnv* env, jobject obj, jobject surface) {
        (void)env; (void)obj;
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        Engine::GetInstance()->SetSurface(window);
        Engine::GetInstance()->InitGLES();
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_renderFrame(JNIEnv* env, jobject obj) {
        (void)env; (void)obj; Engine::GetInstance()->Render();
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_updateControls(JNIEnv* env, jobject obj, jfloat zoom, jfloat warp, jfloat dampening) {
        (void)env; (void)obj; Engine::GetInstance()->UpdateControls(zoom, warp, dampening);
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_pushAudioData(JNIEnv* env, jobject obj, jfloatArray data) {
        (void)obj;
        jfloat* buffer = env->GetFloatArrayElements(data, nullptr);
        jsize length = env->GetArrayLength(data);
        Engine::GetInstance()->PushAudioData(buffer, (int)length);
        env->ReleaseFloatArrayElements(data, buffer, JNI_ABORT);
    }
}
