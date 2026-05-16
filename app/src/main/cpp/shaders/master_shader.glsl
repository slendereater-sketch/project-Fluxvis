#version 320 es
precision highp float;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform NeuralWeights {
    float weights[100];
} u_tpu;

uniform sampler2D u_prevFrame;
uniform vec2 u_resolution;
uniform float u_time;

// Generative Background: Plasma Field
vec3 plasma(vec2 uv, float w) {
    float v = 0.0;
    vec2 c = uv * 8.0 - 4.0;
    v += sin(c.x + u_time);
    v += sin((c.y + u_time) / 2.0);
    v += sin((c.x + c.y + u_time) / 2.0);
    return vec3(0.5 + 0.5 * sin(v), 0.5 + 0.5 * cos(v), w);
}

// Audio Mesh Deformer: Domain Warping
vec2 warp(vec2 uv, float intensity) {
    float d = length(uv - 0.5);
    float angle = atan(uv.y - 0.5, uv.x - 0.5);
    angle += sin(d * 10.0 - u_time * 5.0) * intensity;
    return vec2(0.5 + cos(angle) * d, 0.5 + sin(angle) * d);
}

// Pixel Warp Feedback: Recursive Scaling
vec3 feedback(vec2 uv, float zoom) {
    vec2 p = (uv - 0.5) * (1.0 - zoom * 0.05) + 0.5;
    return texture(u_prevFrame, p).rgb;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    
    // 1. Initial color from feedback
    vec3 color = feedback(uv, u_tpu.weights[0]);
    
    // 2. Add generative layers based on TPU weights
    // Weights [1..25] for Generative Backgrounds
    color = mix(color, plasma(uv, u_tpu.weights[1]), u_tpu.weights[2] * 0.1);
    
    // 3. Coordinate warping based on Weights [26..50]
    vec2 warpedUV = warp(uv, u_tpu.weights[26]);
    color = mix(color, texture(u_prevFrame, warpedUV).rgb, u_tpu.weights[27]);
    
    // 4. Color Correction / Filters [76..100]
    color.r *= (1.0 + u_tpu.weights[76] * 0.5);
    color.g *= (1.0 + u_tpu.weights[77] * 0.3);
    
    fragColor = vec4(color, 1.0);
}
