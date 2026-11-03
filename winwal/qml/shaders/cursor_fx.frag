#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_size;
    vec2 u_origin;
    vec2 u_mouse;
    vec2 u_trail1;
    vec2 u_trail2;
    vec2 u_trail3;
    float u_cursor_strength;
    float u_liquid_enabled;
    float u_refraction;
    vec3 u_custom_color;
};

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
    return length(pa - ba * h);
}

void main() {
    vec2 p = u_origin + qt_TexCoord0 * u_size;
    float cStr = clamp(u_cursor_strength, 0.0, 1.0);
    float mDist = length(p - u_mouse);

    float aura = exp(-(mDist * mDist) / (78.0 * 78.0)) * 0.13 * cStr;
    float core = exp(-(mDist * mDist) / (24.0 * 24.0)) * 0.09 * cStr;
    float val = aura + core;

    if (u_liquid_enabled > 0.5) {
        float speed = clamp((length(u_mouse - u_trail1) + length(u_trail1 - u_trail2) + length(u_trail2 - u_trail3)) / 22.0, 0.0, 1.3);
        if (speed > 0.01) {
            float d1 = sdSegment(p, u_mouse, u_trail1);
            float d2 = sdSegment(p, u_trail1, u_trail2);
            float d3 = sdSegment(p, u_trail2, u_trail3);
            float ridge = exp(-pow((d1 - 9.0) / 6.0, 2.0)) * 0.22
                        + exp(-pow((d2 - 14.0) / 8.5, 2.0)) * 0.15
                        + exp(-pow((d3 - 19.0) / 11.0, 2.0)) * 0.09;
            val += ridge * speed * cStr * (0.65 + 0.55 * u_refraction);
        }
    }

    float edgeFade = smoothstep(0.5, 0.35, length(qt_TexCoord0 - vec2(0.5)));
    float a = clamp(val * edgeFade, 0.0, 0.85) * qt_Opacity;
    fragColor = vec4(u_custom_color * a, a);
}
