#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_progress;
    float u_mode;
};

layout(binding = 1) uniform sampler2D u_prevTex;
layout(binding = 2) uniform sampler2D u_currTex;

void main() {
    vec2 uv = qt_TexCoord0;
    vec4 cOld = texture(u_prevTex, uv);
    vec4 cNew = texture(u_currTex, uv);
    float p = clamp(u_progress, 0.0, 1.0);
    if (p >= 0.999) {
        fragColor = cNew * qt_Opacity;
        return;
    }

    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    float mask = p;

    if (u_mode < 0.5) {
        // 0: Растущий круг из центра (Grow / Iris) как в swww
        vec2 d = (uv - vec2(0.5)) * vec2(aspect, 1.0);
        float dist = length(d);
        float maxR = length(vec2(0.5 * aspect, 0.5)) + 0.12;
        float r = p * maxR;
        mask = 1.0 - smoothstep(r - 0.08, r + 0.02, dist);
    } else if (u_mode < 1.5) {
        // 1: Волна (Wave) как в swww
        float wave = sin(uv.y * 10.0 + p * 8.0) * 0.045;
        float diag = (uv.x * 0.85 + uv.y * 0.15) + wave;
        float edge = p * 1.25 - 0.12;
        mask = 1.0 - smoothstep(edge - 0.08, edge + 0.08, diag);
    } else {
        // 2: Диагональный сдвиг (Wipe)
        float wipePos = (uv.x * 0.7 + (1.0 - uv.y) * 0.3);
        float edge = p * 1.20 - 0.10;
        mask = 1.0 - smoothstep(edge - 0.06, edge + 0.06, wipePos);
    }

    fragColor = mix(cOld, cNew, clamp(mask, 0.0, 1.0)) * qt_Opacity;
}