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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    mStartTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}

Engine::~Engine() {
    TerminateGLES();
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
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
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
    SetupQuad();

    const char* vSrc = R"(#version 320 es
        layout(location = 0) in vec2 a_pos;
        out vec2 v_uv;
        void main() {
            v_uv = a_pos * 0.5 + 0.5;
            gl_Position = vec4(a_pos, 0.0, 1.0);
        })";

    const char* fSrc = R"(#version 320 es
        precision highp float;
        in vec2 v_uv;
        layout(location = 0) out vec4 fragColor;
        uniform sampler2D u_prevFrame;
        uniform vec2 u_resolution;
        uniform float u_time;
        uniform float u_bass;

        mat2 rot(float a) { 
            float s = sin(a); float c = cos(a);
            return mat2(c, -s, s, c);
        }

        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }

        void main() {
            vec2 uv = v_uv - 0.5;
            uv.x *= u_resolution.x / u_resolution.y;
            
            float segments = 8.0; 
            float r = length(uv);
            float a = atan(uv.y, uv.x);
            float ma = mod(a, 2.0 * 3.14159 / segments);
            vec2 p = vec2(cos(ma), sin(ma)) * r;
            
            float d = 1e10;
            for(int i = 0; i < 6; i++) {
                p = abs(p) - 0.2;
                p *= rot(u_time * 0.1);
                p *= 1.1;
                d = min(d, length(p) - 0.05);
            }
            
            float glow = 0.005 / (0.005 + d);
            vec3 col = hsv2rgb(vec3(r * 0.5 + u_time * 0.05, 0.7, 1.0));
            col *= glow * (2.0 + u_bass * 10.0);
            
            vec3 prev = texture(u_prevFrame, (v_uv - 0.5) * 0.98 + 0.5).rgb;
            col += prev * 0.94;
            
            fragColor = vec4(col, 1.0);
        })";

    const char* blitFSrc = R"(#version 320 es
        precision highp float;
        in vec2 v_uv;
        layout(location = 0) out vec4 fragColor;
        uniform sampler2D u_tex;
        void main() {
            fragColor = texture(u_tex, v_uv);
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
    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);
    glGenBuffers(1, &mQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
}

void Engine::CreateFBOs(int width, int height) {
    if (width <= 0 || height <= 0) return;
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

void Engine::Render() {
    if (!mProgram || !mBlitProgram) return;

    auto audio = mAudio.GetLatestFeatures();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float currentTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f - mStartTime;

    int nextIdx = 1 - mPingPongIdx;
    
    glBindVertexArray(mVAO);

    // Pass 1: Effect
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO[nextIdx]);
    glViewport(0, 0, mWidth, mHeight);
    glUseProgram(mProgram);
    glUniform2f(mResLoc, (float)mWidth, (float)mHeight);
    glUniform1f(mTimeLoc, currentTime);
    glUniform1f(glGetUniformLocation(mProgram, "u_bass"), audio.bass);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[mPingPongIdx]);
    glUniform1i(mPrevLoc, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Pass 2: Blit
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mWidth, mHeight);
    glUseProgram(mBlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[nextIdx]);
    glUniform1i(mBlitTexLoc, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindVertexArray(0);
    eglSwapBuffers(mDisplay, mSurface);
    mPingPongIdx = nextIdx;
}

void Engine::UpdateControls(float zoom, float warp, float dampening) {
    std::lock_guard<std::mutex> lock(mControlMutex);
    mUserControls[0] = zoom; mUserControls[1] = warp; mUserControls[2] = dampening;
}

void Engine::PushAudioData(const float* data, int length) {
    mAudio.PushData(data, length);
}

void Engine::TerminateGLES() {
    if (mDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(mDisplay);
    }
}

GLuint Engine::CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint Engine::LinkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert); glAttachShader(prog, frag);
    glLinkProgram(prog);
    return prog;
}

void Engine::SetupUBO() {}

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
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_pushAudioData(JNIEnv* env, jobject obj, jfloatArray data) {
        jfloat* buffer = env->GetFloatArrayElements(data, nullptr);
        jsize length = env->GetArrayLength(data);
        Engine::GetInstance()->PushAudioData(buffer, (int)length);
        env->ReleaseFloatArrayElements(data, buffer, JNI_ABORT);
    }
}
