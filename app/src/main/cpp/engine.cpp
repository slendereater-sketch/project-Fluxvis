#include "engine.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <time.h>
#include <algorithm>

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
        uniform int u_preset;

        mat2 rot(float a) { 
            float s = sin(a); float c = cos(a);
            return mat2(c, -s, s, c);
        }

        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }

        // --- PRESET 0: BLUE NEON FRACTAL ---
        vec3 blueFractal(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            p *= 0.5 + touch.x * 1.5;
            float d = 1e10;
            for(int i=0; i<8; i++) {
                p = abs(p) / dot(p,p) - (0.5 + mid * 0.15);
                p *= rot(time * 0.15 + touch.y * 3.0);
                d = min(d, length(p.x) * length(p.y));
            }
            vec3 col = vec3(0.1, 0.4, 1.0);
            float intensity = 0.8 / (d * 12.0 + 0.1);
            return col * intensity * (1.1 + bass * 4.0);
        }

        // --- PRESET 1: SOLAR SYSTEM ORBIT ---
        vec3 spaceOrbit(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            p *= 1.0 + touch.x;
            vec3 col = vec3(0.0);
            float d = length(p);
            
            float stars = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
            if(stars > 0.995) col += vec3(0.5);

            for(int i=1; i<5; i++) {
                float r = 0.2 * float(i) + sin(time * 0.5 + float(i)) * 0.05;
                float ring = abs(d - r);
                float thick = 0.002 + mid * 0.02;
                col += hsv2rgb(vec3(float(i)*0.2, 0.6, 1.0)) * (thick / (ring + thick));
            }
            
            float sun = smoothstep(0.1 + bass * 0.1, 0.0, d);
            col += vec3(1.0, 0.6, 0.2) * sun * 2.0;
            return col;
        }

        // --- PRESET 2: MICROBIAL/BACTERIA ---
        vec3 microbial(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = uv * (3.0 + touch.x * 4.0);
            p += vec2(sin(time*0.2), cos(time*0.3)) * touch.y;
            float v = 0.0;
            v += sin(p.x + time);
            v += sin((p.y + time) / 2.0);
            v += sin(sqrt(p.x*p.x + p.y*p.y + 1.0) + time);
            vec3 col = hsv2rgb(vec3(v * 0.1 + 0.3, 0.8, 0.6));
            col *= 0.5 + high * 1.5;
            float cells = sin(p.x*10.0) * sin(p.y*10.0);
            if(cells > 0.8 - bass * 0.2) col *= 1.5;
            return col;
        }

        void main() {
            vec3 col = vec3(0.0);
            if(u_preset == 0) col = blueFractal(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else if(u_preset == 1) col = spaceOrbit(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else col = microbial(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            
            // Auto-tuned brightness
            col = col / (1.0 + col);
            col = pow(col, vec3(0.85)); 
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

    auto rawAudio = mAudio.GetLatestFeatures();
    const float alpha = 0.15f; 
    mSmoothBass = mSmoothBass * (1.0f - alpha) + rawAudio.bass * alpha;
    mSmoothMid = mSmoothMid * (1.0f - alpha) + rawAudio.mid * alpha;
    mSmoothHigh = mSmoothHigh * (1.0f - alpha) + rawAudio.treble * alpha;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float currentTime = (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f - mStartTime;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram);
    glUniform2f(mResLoc, (float)mWidth, (float)mHeight);
    glUniform1f(mTimeLoc, currentTime);
    glUniform1f(glGetUniformLocation(mProgram, "u_bass"), mSmoothBass);
    glUniform1f(glGetUniformLocation(mProgram, "u_mid"), mSmoothMid);
    glUniform1f(glGetUniformLocation(mProgram, "u_high"), mSmoothHigh);
    glUniform2f(glGetUniformLocation(mProgram, "u_touch"), mTouchX, mTouchY);
    glUniform1i(glGetUniformLocation(mProgram, "u_preset"), mPreset);
    
    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Engine::NextPreset() {
    mPreset = (mPreset + 1) % 3;
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
    JNIEXPORT void JNICALL Java_com_visualizer_engine_NativeInterface_nextPreset(JNIEnv* env, jobject obj) {
        (void)env; (void)obj; Engine::GetInstance()->NextPreset();
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
