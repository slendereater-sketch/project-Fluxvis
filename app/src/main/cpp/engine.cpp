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

        // --- NEW CUSTOM VISUALIZERS ---
        
        // Helper: Rotation
        mat2 rot(float a) { 
            float s = sin(a); float c = cos(a);
            return mat2(c, -s, s, c);
        }

        // Helper: HSV to RGB
        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }

        // PRESET 0: CYBER MANDALA (Symmetry + Geometry)
        vec3 mandala(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            
            // Zoom out: Increased base multiplier
            p *= 2.0 + touch.x * 4.0;
            
            // Fluidity: Domain warp the coordinates with sine waves
            p += vec2(sin(p.y * 0.8 + time * 0.5), cos(p.x * 0.8 + time * 0.5)) * (0.2 + bass * 0.3);
            
            float r = length(p);
            float a = atan(p.y, p.x);
            
            // 12-fold symmetry for more fluid detail
            float sides = 12.0;
            a = mod(a, 2.0 * 3.14159 / sides) - 3.14159 / sides;
            p = vec2(cos(a), sin(a)) * r;
            
            float d = 1e10;
            for(int i=0; i<5; i++) {
                // Liquid fractal transformation using dot(p,p)
                float l = dot(p, p);
                p = abs(p) / (l + 0.15) - (0.45 + mid * 0.1);
                p *= rot(time * 0.15 + float(i) * 0.4 + high * 0.3);
                d = min(d, length(p));
            }
            
            vec3 col = hsv2rgb(vec3(time * 0.08 + r * 0.15, 0.65, 1.0));
            // Smoother glow with bass influence
            float glow = 0.05 / (d + 0.03 + bass * 0.05);
            return col * glow * (1.1 + bass * 2.5);
        }

        // PRESET 1: QUANTUM FLUX (Fluid Domain Warping)
        vec3 quantumFlux(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            
            for(int i=1; i<4; i++) {
                p += vec2(
                    sin(p.y * 2.0 + time * 0.5 + float(i)),
                    cos(p.x * 2.0 - time * 0.4 + float(i))
                ) * (0.2 + touch.y * 0.5);
            }
            
            float d = length(p);
            vec3 col = hsv2rgb(vec3(d * 0.1 + time * 0.05, 0.8, 0.9));
            col *= smoothstep(1.5, 0.0, d);
            return col * (0.5 + high * 2.0);
        }

        // PRESET 2: ELECTRIC STORM (Glitch Lines)
        vec3 electricStorm(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = uv;
            float g = sin(p.y * 100.0 + time * 20.0);
            float glitch = step(0.98, sin(time * 10.0 + p.y * 10.0));
            p.x += glitch * (bass * 0.2);
            
            float f = 0.0;
            for(int i=0; i<5; i++) {
                float line = abs(p.x - 0.5 + sin(time * 2.0 + float(i) * 1.5) * 0.4);
                f += (0.002 + mid * 0.01) / line;
            }
            
            vec3 col = vec3(0.2, 0.6, 1.0) * f;
            if(glitch > 0.5) col = col.gbr;
            return col;
        }

        // PRESET 3: NEON SINGULARITY (Gravity Wells)
        vec3 neonSingularity(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_resolution.x / u_resolution.y;
            
            vec3 col = vec3(0.0);
            for(int i=0; i<6; i++) {
                float a = time * 0.5 + float(i) * 1.04;
                vec2 center = vec2(cos(a), sin(a)) * (0.6 + touch.x);
                float d = length(p - center);
                float pulse = 0.02 + bass * 0.05;
                col += hsv2rgb(vec3(float(i)*0.16, 0.7, 1.0)) * (pulse / (d + 0.02));
            }
            return col;
        }

        // PRESET 4: HEXAGONAL HIVE (Tessellation)
        vec3 hexHive(vec2 uv, float time, float bass, float mid, float high, vec2 touch) {
            vec2 p = (uv - 0.5) * 10.0;
            p.x *= u_resolution.x / u_resolution.y;
            p *= 1.0 + touch.y;
            
            vec2 r = vec2(1.0, 1.73205);
            vec2 h = r * 0.5;
            vec2 a = mod(p, r) - h;
            vec2 b = mod(p - h, r) - h;
            vec2 g = dot(a, a) < dot(b, b) ? a : b;
            
            float d = 0.5 - max(abs(g.x) * 0.866 + g.y * 0.5, -g.y);
            float edge = smoothstep(0.0, 0.1 + mid * 0.2, d);
            float hole = smoothstep(0.1 + bass * 0.3, 0.0, d);
            
            vec3 col = hsv2rgb(vec3(dot(p,p)*0.01 + time*0.1, 0.6, 1.0));
            return col * edge * (1.0 - hole);
        }

        void main() {
            vec3 col = vec3(0.0);
            if(u_preset == 0) col = mandala(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else if(u_preset == 1) col = quantumFlux(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else if(u_preset == 2) col = electricStorm(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else if(u_preset == 3) col = neonSingularity(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            else col = hexHive(v_uv, u_time, u_bass, u_mid, u_high, u_touch);
            
            // Post-processing
            col = col / (1.0 + col);
            col = pow(col, vec3(0.8)); 
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
    mPreset = (mPreset + 1) % 5;
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
