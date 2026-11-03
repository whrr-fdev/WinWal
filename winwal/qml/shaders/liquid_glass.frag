#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_refraction;
    float u_radius;
    float u_time;
    vec4 u_base_color;
};

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    vec2 p = qt_TexCoord0 * res;
    vec2 halfRes = 0.5 * res;
    vec2 centerP = p - halfRes;
    float r = clamp(u_radius, 0.0, min(halfRes.x, halfRes.y));
    float d = sdRoundedBox(centerP, halfRes, r);
    float mask = 1.0 - smoothstep(-0.8, 0.8, d);
    if (mask <= 0.001) { fragColor = vec4(0.0); return; }

    float edgeDist = max(0.0, -d);
    float eps = 1.5;
    vec2 sdfGrad = normalize(vec2(
        sdRoundedBox(centerP + vec2(eps, 0.0), halfRes, r) - sdRoundedBox(centerP - vec2(eps, 0.0), halfRes, r),
        sdRoundedBox(centerP + vec2(0.0, eps), halfRes, r) - sdRoundedBox(centerP - vec2(0.0, eps), halfRes, r)
    ) + vec2(1e-5));

    float bevelW = clamp(min(res.x, res.y) * 0.32, 5.0, 14.0);
    float meniscus = pow(1.0 - clamp(edgeDist / bevelW, 0.0, 1.0), 2.1);

    vec3 surfN = normalize(vec3(-sdfGrad * meniscus * 1.4, 1.0));
    float spec = pow(max(0.0, dot(surfN, normalize(vec3(-0.45, -0.6, 0.65)))), 9.0);
    float rim = exp(-edgeDist * 1.0) * (0.16 + 0.22 * max(0.0, dot(sdfGrad, vec2(-0.6, -0.8))));

    vec3 col = u_base_color.rgb + vec3(0.95, 0.98, 1.0) * (spec * 0.16 * u_refraction + meniscus * 0.06 * u_refraction + rim);
    float alpha = clamp(u_base_color.a + rim * 0.5 + meniscus * 0.08 * u_refraction, 0.04, 0.95) * mask * qt_Opacity;
    fragColor = vec4(col * alpha, alpha);
}
