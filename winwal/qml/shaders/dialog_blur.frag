#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_blur;
};

layout(binding = 1) uniform sampler2D source;

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    float b = clamp(u_blur, 0.0, 1.0);
    if (b <= 0.01) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }

    // Непрерывные шаги билинейной фильтрации (без двоения и без пропадания текстуры)
    vec2 s1 = (b * 1.6) / res;
    vec2 s2 = (b * 3.4) / res;
    vec2 s3 = (b * 5.4) / res;

    vec4 sum = texture(source, qt_TexCoord0) * 0.18;
    sum += texture(source, clamp(qt_TexCoord0 + vec2( s1.x,  s1.y), 0.001, 0.999)) * 0.09;
    sum += texture(source, clamp(qt_TexCoord0 + vec2(-s1.x,  s1.y), 0.001, 0.999)) * 0.09;
    sum += texture(source, clamp(qt_TexCoord0 + vec2( s1.x, -s1.y), 0.001, 0.999)) * 0.09;
    sum += texture(source, clamp(qt_TexCoord0 + vec2(-s1.x, -s1.y), 0.001, 0.999)) * 0.09;

    sum += texture(source, clamp(qt_TexCoord0 + vec2( s2.x,  0.0),  0.001, 0.999)) * 0.065;
    sum += texture(source, clamp(qt_TexCoord0 + vec2(-s2.x,  0.0),  0.001, 0.999)) * 0.065;
    sum += texture(source, clamp(qt_TexCoord0 + vec2( 0.0,   s2.y), 0.001, 0.999)) * 0.065;
    sum += texture(source, clamp(qt_TexCoord0 + vec2( 0.0,  -s2.y), 0.001, 0.999)) * 0.065;

    sum += texture(source, clamp(qt_TexCoord0 + vec2( s3.x,  s3.y), 0.001, 0.999)) * 0.05;
    sum += texture(source, clamp(qt_TexCoord0 + vec2(-s3.x,  s3.y), 0.001, 0.999)) * 0.05;
    sum += texture(source, clamp(qt_TexCoord0 + vec2( s3.x, -s3.y), 0.001, 0.999)) * 0.05;
    sum += texture(source, clamp(qt_TexCoord0 + vec2(-s3.x, -s3.y), 0.001, 0.999)) * 0.05;

    fragColor = sum * qt_Opacity;
}
