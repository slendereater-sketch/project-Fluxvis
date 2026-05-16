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
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(mDisplay, configAttribs, &config, 1, &numConfigs);

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    mContext = eglCreateContext(mDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    mSurface = eglCreateWindowSurface(mDisplay, config, mWindow, nullptr);

    if (!eglMakeCurrent(mDisplay, mSurface, mSurface, mContext)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglMakeCurrent failed");
        return;
    }

    mWidth = ANativeWindow_getWidth(mWindow);
    mHeight = ANativeWindow_getHeight(mWindow);
    
    SetupQuad();

    const char* vSrc = R"(#version 300 es
        layout(location = 0) in vec2 a_pos;
        out vec2 v_uv;
        void main() {
            v_uv = a_pos * 0.5 + 0.5;
            gl_Position = vec4(a_pos, 0.0, 1.0);
        })";

    const char* fSrc = R"(#version 300 es
        precision highp float;
        in vec2 v_uv;
        layout(location = 0) out vec4 fragColor;
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
            float bass = clamp(u_bass, 0.0, 1.0);
            
            for(int i = 0; i < 5; i++) {
                p = abs(p) - (0.15 + bass * 0.1);
                p *= rot(u_time * 0.2 + float(i));
                p *= 1.2;
                d = min(d, length(p) - 0.02);
            }
            
            float glow = 0.004 / (0.004 + d);
            vec3 col = hsv2rgb(vec3(r * 0.3 + u_time * 0.1, 0.8, 1.0));
            col *= glow * (2.0 + bass * 20.0);
            
            fragColor = vec4(col, 1.0);
        })";

    mProgram = LinkProgram(CompileShader(GL_VERTEX_SHADER, vSrc), CompileShader(GL_FRAGMENT_SHADER, fSrc));
    mResLoc = glGetUniformLocation(mProgram, "u_resolution");
    mTimeLoc = glGetUniformLocation(mProgram, "u_time");
    
    __android_log_print(ANDROID_LOG_INFO, TAG, "Engine Ready: %dx%d", mWidth, mHeight);
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

void Engine::Render() {
    if (mDisplay == EGL_NO_DISPLAY || mSurface == EGL_NO_SURFACE) return;

    // CLEAR TO RED (If you see RED, the engine is looping but shader failed)
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!mProgram) {
        eglSwapBuffers(mDisplay, mSurface);
        return;
    }

    auto audio = mAudio.GetLatestFeatures();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float currentTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f - mStartTime;

    glViewport(0, 0, mWidth, mHeight);
    glUseProgram(mProgram);
    glUniform2f(mResLoc, (float)mWidth, (float)mHeight);
    glUniform1f(mTimeLoc, currentTime);
    glUniform1f(glGetUniformLocation(mProgram, "u_bass"), audio.bass);
    
    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    eglSwapBuffers(mDisplay, mSurface);
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
    if (!vert || !frag) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert); glAttachShader(prog, frag);
    glLinkProgram(prog);
    return prog;
}

void Engine::CreateFBOs(int w, int h) { (void)w; (void)h; }
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
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_updateControls(JNIEnv* env, jobject obj, jfloat z, jfloat w, jfloat d) {
        Engine::GetInstance()->UpdateControls(z, w, d);
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_pushAudioData(JNIEnv* env, jobject obj, jfloatArray data) {
        jfloat* buffer = env->GetFloatArrayElements(data, nullptr);
        jsize length = env->GetArrayLength(data);
        Engine::GetInstance()->PushAudioData(buffer, (int)length);
        env->ReleaseFloatArrayElements(data, buffer, JNI_ABORT);
    }
}
