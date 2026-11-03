#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_refraction;
    float u_radius;
    vec4 u_base_color;
};

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    vec2 localP = qt_TexCoord0 * res;
    vec2 halfRes = 0.5 * res - vec2(0.5);
    vec2 centerP = localP - 0.5 * res;

    float r = clamp(u_radius, 0.0, min(halfRes.x, halfRes.y));
    float d = sdRoundedBox(centerP, halfRes, r);
    float mask = 1.0 - smoothstep(-0.5, 0.6, d);
    if (mask <= 0.001) { fragColor = vec4(0.0); return; }

    float edgeDist = max(0.0, -d);
    float eps = 1.5;
    vec2 sdfGrad = normalize(vec2(
        sdRoundedBox(centerP + vec2(eps, 0.0), halfRes, r) - sdRoundedBox(centerP - vec2(eps, 0.0), halfRes, r),
        sdRoundedBox(centerP + vec2(0.0, eps), halfRes, r) - sdRoundedBox(centerP - vec2(0.0, eps), halfRes, r)
    ) + vec2(1e-5));

    float bevelW = clamp(min(res.x, res.y) * 0.28, 8.0, 22.0);
    float meniscus = pow(1.0 - clamp(edgeDist / bevelW, 0.0, 1.0), 2.2);
    float refrPower = clamp(u_refraction, 0.0, 1.0);
    vec2 totalNormal = sdfGrad * meniscus * (0.45 + 1.15 * refrPower);

    vec3 surfN = normalize(vec3(-totalNormal * 1.5, 1.0));
    float spec1 = pow(max(0.0, dot(surfN, normalize(vec3(-0.5, -0.6, 0.65)))), 9.0);
    float spec2 = pow(max(0.0, dot(surfN, normalize(vec3(0.55, 0.45, 0.7)))), 13.0);

    // Чисто белый/серебристый стеклянный блик без фиолетового оттенка
    float glassSpec = (spec1 * (0.15 + 0.24 * refrPower) + spec2 * 0.09 * refrPower);
    float rimHighlight = exp(-edgeDist * 0.90) * (0.14 + 0.24 * max(0.0, dot(sdfGrad, vec2(-0.5, -0.85))));
    float meniscusGlow = meniscus * (0.04 + 0.08 * refrPower);

    vec3 finalRGB = u_base_color.rgb + vec3(1.0) * (glassSpec + rimHighlight + meniscusGlow);
    float finalAlpha = clamp(u_base_color.a + rimHighlight * 0.5 + meniscusGlow * 0.45, 0.02, 0.95) * mask * qt_Opacity;
    fragColor = vec4(finalRGB * finalAlpha, finalAlpha);
}
