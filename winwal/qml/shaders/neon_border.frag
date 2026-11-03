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
    float u_direction;
    float u_cursor_strength;
    vec2 u_mouse;
    vec3 u_custom_color;
};

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

vec3 paletteColor(float mode, float phase, float depthFactor) {
    if (mode < 0.5) {
        return mix(vec3(1.0), vec3(0.82, 0.86, 0.94), depthFactor * 0.45);
    } else if (mode < 1.5) {
        vec3 c1 = vec3(0.55, 0.88, 1.00);
        vec3 c2 = vec3(0.90, 0.97, 1.00);
        vec3 c3 = vec3(0.32, 0.58, 0.98);
        return mix(mix(c1, c2, 0.5 + 0.5 * sin(phase)), c3, depthFactor * 0.6);
    } else if (mode < 2.5) {
        return vec3(0.65, 0.68, 0.78) + vec3(0.35, 0.32, 0.28) * cos(6.28318 * (vec3(1.0) * (phase * 0.16 + depthFactor * 0.2) + vec3(0.0, 0.33, 0.67)));
    } else if (mode < 3.5) {
        return mix(vec3(1.0, 0.76, 0.45), vec3(1.0, 0.45, 0.62), 0.5 + 0.5 * sin(phase + depthFactor * 1.2));
    } else {
        return mix(u_custom_color, u_custom_color * 0.75 + vec3(0.25), 0.3 * sin(phase));
    }
}

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    vec2 p = qt_TexCoord0 * res;
    vec2 center = p - 0.5 * res;
    float sdf = sdRoundedBox(center, 0.5 * res, u_radius);
    if (sdf > 1.0) { fragColor = vec4(0.0); return; }

    float distInward = max(0.0, -sdf);
    float maxEdgePx = mix(6.0, 42.0, clamp(u_spread, 0.05, 1.0));
    if (distInward > maxEdgePx * 2.2) {
        fragColor = vec4(0.0);
        return;
    }

    vec2 normC = center / (0.5 * res);
    float rawAngle = atan(normC.y, normC.x);
    float angle = (u_direction > 1.5) ? abs(rawAngle) : rawAngle;
    float dirSign = (u_direction > 0.5 && u_direction < 1.5) ? 1.0 : -1.0;
    float phase = angle * 2.0 + dirSign * u_time * 1.5;

    float w1 = 0.5 + 0.5 * sin(phase);
    float w2 = 0.5 + 0.5 * cos(angle * 3.0 + dirSign * u_time * 2.1);
    float waveEnergy = pow(w1 * 0.65 + w2 * 0.35, 1.5);

    float mouseDist = length(p - u_mouse);
    float mouseBoost = exp(-(mouseDist * mouseDist) / (160.0 * 160.0)) * 0.75 * clamp(u_cursor_strength, 0.0, 1.0);
    float totalEnergy = clamp(0.15 + waveEnergy * 0.85 + mouseBoost, 0.0, 1.4);

    float rimLine = exp(-distInward * 1.15) * (0.45 + 0.85 * totalEnergy);
    float edgeHalo = exp(-distInward / max(2.0, maxEdgePx * 0.38))
                   * smoothstep(maxEdgePx * 1.6, 0.0, distInward)
                   * (0.12 + 0.42 * totalEnergy);

    float lightVal = (rimLine + edgeHalo) * clamp(u_intensity, 0.0, 1.5);
    float depthFactor = clamp(distInward / max(1.0, maxEdgePx), 0.0, 1.0);
    vec3 col = paletteColor(u_color_mode, phase, depthFactor);
    float alpha = clamp(lightVal, 0.0, 1.0) * qt_Opacity;
    fragColor = vec4(col * alpha, alpha);
}
