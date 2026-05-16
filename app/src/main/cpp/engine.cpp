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
}

void Engine::SetSurface(ANativeWindow* window) {
    (void)window;
}

void Engine::OnResize(int width, int height) {
    mWidth = width;
    mHeight = height;
    glViewport(0, 0, width, height);
}

void Engine::InitGLES() {
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
        uniform float u_mid;
        uniform float u_high;
        uniform vec2 u_touch;

        mat2 rot(float a) { 
            float s = sin(a); float c = cos(a);
            return mat2(c, -s, s, c);
        }

        // --- PURE BLUE NEON FRACTAL ---
        vec3 fractal(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            
            // Warp center and scale based on touch
            p *= 0.5 + touch.x * 2.0;
            
            float d = 1e10;
            for(int i=0; i<8; i++) {
                // Iterative folding
                p = abs(p) / dot(p,p) - (0.5 + mid * 0.2);
                // Twisting based on time and touch vertical
                p *= rot(time * 0.2 + touch.y * 5.0);
                // Geometric distance estimation
                d = min(d, length(p.x) * length(p.y));
            }
            
            // Neon blue glow logic
            vec3 neonColor = vec3(0.1, 0.5, 1.0);
            float intensity = 1.0 / (d * 15.0 + 0.08);
            
            // Audio-reactive pulsing
            intensity *= (1.2 + bass * 8.0);
            
            return neonColor * intensity;
        }

        void main() {
            vec3 col = fractal(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            fragColor = vec4(col, 1.0);
        })";

    mProgram = LinkProgram(CompileShader(GL_VERTEX_SHADER, vSrc), CompileShader(GL_FRAGMENT_SHADER, fSrc));
    mResLoc = glGetUniformLocation(mProgram, "u_resolution");
    mTimeLoc = glGetUniformLocation(mProgram, "u_time");
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
    if (!mProgram) return;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto audio = mAudio.GetLatestFeatures();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float currentTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f - mStartTime;

    glUseProgram(mProgram);
    glUniform2f(mResLoc, (float)mWidth, (float)mHeight);
    glUniform1f(mTimeLoc, currentTime);
    glUniform1f(glGetUniformLocation(mProgram, "u_bass"), audio.bass);
    glUniform1f(glGetUniformLocation(mProgram, "u_mid"), audio.mid);
    glUniform1f(glGetUniformLocation(mProgram, "u_high"), audio.treble);
    glUniform2f(glGetUniformLocation(mProgram, "u_touch"), mTouchX, mTouchY);
    
    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Engine::UpdateTouch(float x, float y) {
    mTouchX = x;
    mTouchY = y;
}

void Engine::TerminateGLES() {}
GLuint Engine::CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s); return s;
}
GLuint Engine::LinkProgram(GLuint v, GLuint f) {
    GLuint p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p); return p;
}
void Engine::UpdateControls(float z, float w, float d) { (void)z; (void)w; (void)d; }
void Engine::PushAudioData(const float* data, int len) { mAudio.PushData(data, len); }

extern "C" {
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_init(JNIEnv* env, jobject obj, jobject surface) {
        (void)env; (void)obj; (void)surface; Engine::GetInstance()->InitGLES();
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_onResize(JNIEnv* env, jobject obj, jint w, jint h) {
        (void)env; (void)obj; Engine::GetInstance()->OnResize(w, h);
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_renderFrame(JNIEnv* env, jobject obj) {
        (void)env; (void)obj; Engine::GetInstance()->Render();
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_updateTouch(JNIEnv* env, jobject obj, jfloat x, jfloat y) {
        (void)env; (void)obj; Engine::GetInstance()->UpdateTouch(x, y);
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_pushAudioData(JNIEnv* env, jobject obj, jfloatArray data) {
        jfloat* b = env->GetFloatArrayElements(data, nullptr);
        Engine::GetInstance()->PushAudioData(b, (int)env->GetArrayLength(data));
        env->ReleaseFloatArrayElements(data, b, JNI_ABORT);
    }
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_updateControls(JNIEnv* env, jobject obj, jfloat z, jfloat w, jfloat d) {
        (void)env; (void)obj; Engine::GetInstance()->UpdateControls(z, w, d);
    }
}
