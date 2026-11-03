#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_progress;
    float u_direction;
    float u_mode; // 0 = Воронка, 1 = 3D Сферический Портал, 2 = Квантовый Телепорт
};

layout(binding = 1) uniform sampler2D source;

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    float t = clamp(u_progress, 0.0, 1.0);
    if (t <= 0.001) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }
    if (t >= 0.995) {
        fragColor = vec4(0.0);
        return;
    }

    vec2 res = max(u_resolution, vec2(1.0));
    vec2 aspect = vec2(res.x / res.y, 1.0);
    vec2 center = vec2(0.5, 0.5);

    // =========================================================================
    // РЕЖИМ 0: ВОРОНКА (Black Hole Swirl)
    // =========================================================================
    if (u_mode < 0.5) {
        vec2 delta = (qt_TexCoord0 - center) * aspect;
        float r = length(delta);

        float shrink = max(0.002, pow(1.0 - t, 1.45));
        float sourceR = r / shrink;

        float swirlAngle = (t * 4.2 + t * t * 15.0) * u_direction / (0.12 + sourceR * 1.1);
        float s = sin(swirlAngle);
        float c = cos(swirlAngle);
        vec2 rotDelta = vec2(delta.x * c - delta.y * s, delta.x * s + delta.y * c);

        vec2 sampleUV = center + (rotDelta / shrink) / aspect;

        vec2 edge = smoothstep(vec2(0.0), vec2(0.03), sampleUV) * smoothstep(vec2(0.0), vec2(0.03), vec2(1.0) - sampleUV);
        float mask = edge.x * edge.y;
        if (mask <= 0.001) {
            fragColor = vec4(0.0);
            return;
        }

        vec4 col = texture(source, clamp(sampleUV, 0.0, 1.0));
        float ringGlow = exp(-pow((r - 0.12 * shrink) / max(0.01, 0.04 * shrink), 2.0)) * t * (1.0 - t) * 0.65;
        float endFade = smoothstep(1.0, 0.86, t);

        vec3 outRgb = col.rgb * mask + vec3(ringGlow * mask);
        float outA = clamp(col.a * mask + ringGlow * mask, 0.0, 1.0) * endFade * qt_Opacity;
        fragColor = vec4(outRgb * endFade * qt_Opacity, outA);
        return;
    }

    // =========================================================================
    // РЕЖИМ 1: СФЕРИЧЕСКИЙ ПОРТАЛ (3D Fisheye Lens + Energy Ring Aperture)
    // =========================================================================
    if (u_mode < 1.5) {
        vec2 delta = (qt_TexCoord0 - center) * aspect;
        float r = length(delta);
        float maxR = length(vec2(0.5, 0.5) * aspect) * 1.05;

        // Радиус открытого портала (при t: 0->1 сжимается в центр, при t: 1->0 расширяется из центра)
        float apertureR = max(0.005, (1.0 - pow(t, 1.25)) * maxR);
        float circleMask = 1.0 - smoothstep(apertureR - 0.035, apertureR + 0.01, r);
        if (circleMask <= 0.001) {
            fragColor = vec4(0.0);
            return;
        }

        // 3D сферическое преломление линзы внутри кольца портала
        float normR = clamp(r / apertureR, 0.0, 1.0);
        float sphereLens = mix(1.0, 0.55 + 0.65 * normR * normR, t * 0.85);
        float wave = sin(normR * 18.0 - t * 14.0) * t * 0.035;

        vec2 warpedDelta = delta * (sphereLens + wave) / max(0.08, 1.0 - t * 0.65);
        vec2 uvBase = center + warpedDelta / aspect;

        vec2 edge = smoothstep(vec2(0.0), vec2(0.02), uvBase) * smoothstep(vec2(0.0), vec2(0.02), vec2(1.0) - uvBase);
        float bounds = edge.x * edge.y * circleMask;
        if (bounds <= 0.001) {
            fragColor = vec4(0.0);
            return;
        }

        // Хроматическая дисперсия на краю кольца портала
        vec2 radialShift = (delta / aspect) * (pow(normR, 2.0) * t * 0.055);
        vec4 cR = texture(source, clamp(uvBase - radialShift, 0.0, 1.0));
        vec4 cG = texture(source, clamp(uvBase, 0.0, 1.0));
        vec4 cB = texture(source, clamp(uvBase + radialShift, 0.0, 1.0));

        // Светящееся кольцо самого портала
        float ringGlow = exp(-pow((r - apertureR + 0.02) / 0.028, 2.0)) * sin(t * 3.14159) * 0.85;
        float endFade = smoothstep(1.0, 0.88, t);

        vec3 rgb = vec3(cR.r, cG.g, cB.b) * bounds + vec3(0.92, 0.97, 1.0) * ringGlow * circleMask;
        float alpha = clamp(cG.a * bounds + ringGlow * circleMask, 0.0, 1.0) * endFade * qt_Opacity;
        fragColor = vec4(rgb * endFade * qt_Opacity, alpha);
        return;
    }

    // =========================================================================
    // РЕЖИМ 2: ТЕЛЕПОРТ / КВАНТОВЫЙ РАСПАД (Quantum Cyber Shatter)
    // =========================================================================
    float numSlices = 26.0;
    float sliceIdx = floor(qt_TexCoord0.y * numSlices);
    float rnd = hash11(sliceIdx + 17.0);
    float dirSign = (mod(sliceIdx, 2.0) < 0.5) ? 1.0 : -1.0;

    float localT = clamp((t - rnd * 0.25) / 0.75, 0.0, 1.0);
    float slideShift = pow(localT, 1.7) * 0.55 * dirSign * u_direction;

    vec2 uv = qt_TexCoord0;
    uv.x += slideShift;

    float bounds = step(0.0, uv.x) * step(uv.x, 1.0);
    if (bounds <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    float chroma = pow(localT, 1.3) * 0.038 * dirSign;
    vec4 colR = texture(source, clamp(uv + vec2(chroma, 0.0), 0.0, 1.0));
    vec4 colG = texture(source, clamp(uv, 0.0, 1.0));
    vec4 colB = texture(source, clamp(uv - vec2(chroma, 0.0), 0.0, 1.0));

    vec3 rgb = vec3(colR.r, colG.g, colB.b);
    float flash = pow(sin(localT * 3.14159), 2.0) * 0.38;
    float fade = 1.0 - pow(localT, 1.8);

    vec3 outCol = (rgb + vec3(0.9, 0.96, 1.0) * flash) * fade;
    float outAlpha = clamp(colG.a + flash * 0.5, 0.0, 1.0) * fade * qt_Opacity;
    fragColor = vec4(outCol * qt_Opacity, outAlpha);
}
