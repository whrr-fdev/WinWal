#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_radius;
    float u_time;
    float u_intensity;
    float u_spread;
    float u_color_mode;
    vec2 u_mouse;
};

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    vec2 p = qt_TexCoord0 * res;
    vec2 center = p - 0.5 * res;
    float distInward = max(0.0, -sdRoundedBox(center, 0.5 * res, u_radius));

    vec2 w = qt_TexCoord0 * 4.5;
    float c1 = sin(w.x * 2.2 + u_time * 0.7 + sin(w.y * 1.8 - u_time * 0.5));
    float c2 = cos(w.y * 2.5 - u_time * 0.6 + cos(w.x * 2.1 + u_time * 0.8));
    float fluidWave = pow(0.5 + 0.25 * (c1 + c2), 2.2);

    vec2 gridUV = fract(p / 34.0);
    float microGrid = smoothstep(0.045, 0.0, abs(gridUV.x - 0.5)) + smoothstep(0.045, 0.0, abs(gridUV.y - 0.5));

    float mDist = length(p - u_mouse);
    float cursorGlow = exp(-(mDist * mDist) / (190.0 * 190.0));

    vec2 normC = center / (0.5 * res);
    float angle = atan(normC.y, normC.x);
    float borderWave = 0.5 + 0.5 * sin(angle * 2.0 - u_time * 1.35);
    float spreadPx = mix(30.0, 160.0, clamp(u_spread, 0.1, 1.0));
    float borderField = exp(-distInward / spreadPx) * (0.3 + 0.7 * borderWave) * u_intensity;

    vec3 baseCol = vec3(0.04, 0.045, 0.055);
    vec3 highlight = vec3(0.86, 0.91, 1.0) * (fluidWave * 0.08 + microGrid * 0.055 + cursorGlow * 0.18 + borderField * 0.42);
    vec3 finalCol = baseCol + highlight;
    fragColor = vec4(finalCol * qt_Opacity, qt_Opacity);
}
