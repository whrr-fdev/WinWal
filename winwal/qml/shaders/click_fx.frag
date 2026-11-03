#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float u_progress;
    float u_intensity;
    float u_shape;
    vec3 u_custom_color;
};

void main() {
    vec2 uv = qt_TexCoord0 - vec2(0.5);
    float dist = length(uv) * 2.0;
    if (dist >= 1.0) { fragColor = vec4(0.0); return; }

    float fade = pow(1.0 - u_progress, 1.65) * clamp(u_intensity, 0.0, 1.0);
    float r = u_progress * 0.88;
    float val = 0.0;

    if (u_shape < 0.5) {
        float d = dist - r;
        val = exp(-(d * d) / 0.012) * 0.45;
    } else if (u_shape < 1.5) {
        float rad = mix(0.12, 0.85, u_progress);
        val = exp(-(dist * dist) / (rad * rad * 0.45)) * 0.42;
    } else if (u_shape < 2.5) {
        float rays = exp(-abs(uv.x) * 26.0) + exp(-abs(uv.y) * 26.0);
        float env = exp(-(dist * dist) / (max(0.08, r * r * 0.6)));
        float ring = exp(-pow((dist - r * 0.75) / 0.09, 2.0));
        val = (rays * env * 0.35 + ring * 0.25);
    } else {
        float d1 = dist - r;
        float d2 = dist - max(0.0, r - 0.22);
        val = (exp(-(d1 * d1) / 0.008) * 0.42 + exp(-(d2 * d2) / 0.010) * 0.28);
    }

    float a = clamp(val * fade * (1.0 - smoothstep(0.82, 1.0, dist)), 0.0, 0.90) * qt_Opacity;
    fragColor = vec4(u_custom_color * a, a);
}
