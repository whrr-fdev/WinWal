#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 u_resolution;
    float u_has_scene;
    float u_refraction_strength;
    float u_time;
    vec2 u_mouse;
    float u_cursor_strength;
    vec3 u_cursor_color;
    float u_border_strength;
    float u_border_spread;
    float u_border_color_mode;
    vec3 u_border_color;
    float u_fog_strength;
    float u_lightning;
    float u_lightning_x;
    float u_drop_tint_strength;
    float u_drop_highlight;
    vec3 u_drop_tint_color;
    float u_wipe_strength;
    float u_idle_fog_boost;
    float u_frost_strength;
};

layout(binding = 1) uniform sampler2D u_waterMap;
layout(binding = 2) uniform sampler2D u_overlayMap;
layout(binding = 3) uniform sampler2D u_scene;

// Органическое микро-искажение координат воды (делает контур капель живым, неидеальным и чуть колышущимся при движении по стеклу)
vec2 computeLiquidWarp(vec2 pixelP, vec2 texel) {
    float wx = sin(pixelP.y * 0.16 + pixelP.x * 0.09 + u_time * 0.45)
             + 0.55 * cos(pixelP.y * 0.34 - pixelP.x * 0.21)
             + 0.30 * sin(pixelP.y * 0.62 + pixelP.x * 0.41);
    float wy = cos(pixelP.x * 0.15 - pixelP.y * 0.11 + u_time * 0.40)
             + 0.55 * sin(pixelP.x * 0.31 + pixelP.y * 0.27)
             + 0.30 * cos(pixelP.x * 0.58 - pixelP.y * 0.39);
    return vec2(wx, wy) * texel * 1.45;
}

float sampleWaterField(vec2 uv, vec2 texel, float rMul) {
    vec2 t1 = texel * (0.85 * rMul);
    vec2 t2 = texel * (1.95 * rMul);
    float c  = texture(u_waterMap, uv).b * 2.6;
    float n1 = texture(u_waterMap, uv + vec2(0.0, -t1.y)).b;
    float s1 = texture(u_waterMap, uv + vec2(0.0,  t1.y)).b;
    float w1 = texture(u_waterMap, uv + vec2(-t1.x, 0.0)).b;
    float e1 = texture(u_waterMap, uv + vec2( t1.x, 0.0)).b;
    float nw = texture(u_waterMap, uv + vec2(-t2.x, -t2.y)).b * 0.6;
    float ne = texture(u_waterMap, uv + vec2( t2.x, -t2.y)).b * 0.6;
    float sw = texture(u_waterMap, uv + vec2(-t2.x,  t2.y)).b * 0.6;
    float se = texture(u_waterMap, uv + vec2( t2.x,  t2.y)).b * 0.6;
    return (c + n1 + s1 + w1 + e1 + nw + ne + sw + se) / 9.0;
}

float sampleFogField(vec2 uv, vec2 texel) {
    vec2 t  = texel * 2.4;
    vec2 t2 = texel * 5.5;
    float c  = texture(u_waterMap, uv).g;
    float w  = texture(u_waterMap, uv - vec2(t.x, 0.0)).g;
    float e  = texture(u_waterMap, uv + vec2(t.x, 0.0)).g;
    float w2 = texture(u_waterMap, uv - vec2(t2.x, 0.0)).g;
    float e2 = texture(u_waterMap, uv + vec2(t2.x, 0.0)).g;

    // Если в радиусе ±5.5px есть протёртая полоса или капля (g > 0.35), берём минимальный фоновый туман снаружи полосы,
    // чтобы сглаженные боковые края полосы (где g проходит через 0.20..0.45) НИКОГДА не создавали светлых боковых границ!
    float maxG = max(c, max(max(w, e), max(w2, e2)));
    if (maxG > 0.35) {
        float outerBg = min(w2, e2);
        float bgFog = (outerBg < 0.34) ? outerBg : 0.0;
        return clamp(bgFog * 2.8, 0.0, 1.0);
    }

    float n = texture(u_waterMap, uv + vec2(0.0, -t.y)).g;
    float s = texture(u_waterMap, uv + vec2(0.0,  t.y)).g;
    float avg = (c * 2.0 + n + s + w + e) / 6.0;
    return clamp(avg * 2.8, 0.0, 1.0);
}

float sampleWinWiped(vec2 uv, vec2 texel) {
    vec2 t1 = texel * 1.6;
    vec2 t2 = texel * 3.4;
    float c  = smoothstep(0.36, 0.88, texture(u_waterMap, uv).g);
    float w1 = smoothstep(0.36, 0.88, texture(u_waterMap, uv - vec2(t1.x, 0.0)).g);
    float e1 = smoothstep(0.36, 0.88, texture(u_waterMap, uv + vec2(t1.x, 0.0)).g);
    float w2 = smoothstep(0.36, 0.88, texture(u_waterMap, uv - vec2(t2.x, 0.0)).g);
    float e2 = smoothstep(0.36, 0.88, texture(u_waterMap, uv + vec2(t2.x, 0.0)).g);
    float n1 = smoothstep(0.36, 0.88, texture(u_waterMap, uv - vec2(0.0, t1.y)).g);
    float s1 = smoothstep(0.36, 0.88, texture(u_waterMap, uv + vec2(0.0, t1.y)).g);
    return clamp((c * 2.2 + (w1 + e1) * 1.2 + (w2 + e2) * 0.5 + (n1 + s1) * 0.8) / 7.2, 0.0, 1.0);
}


float hashFrost(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noiseFrost(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hashFrost(i + vec2(0.0, 0.0)), hashFrost(i + vec2(1.0, 0.0)), u.x),
               mix(hashFrost(i + vec2(0.0, 1.0)), hashFrost(i + vec2(1.0, 1.0)), u.x), u.y);
}

vec2 hash2Frost(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

// Кристаллическая ячейка льда Вороного с 6-лучевой снежной звёздочкой внутри и острыми гранями (0% волн!)
vec4 voronoiIceCluster(vec2 x) {
    vec2 n = floor(x);
    vec2 f = fract(x);
    float d1 = 8.0;
    float d2 = 8.0;
    vec2 bestVec = vec2(0.0);
    float bestId = 0.0;

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash2Frost(n + g);
            vec2 r = g + (0.12 + 0.76 * o) - f;
            float d = dot(r, r);
            if (d < d1) {
                d2 = d1;
                d1 = d;
                bestVec = r;
                bestId = o.x + o.y * 3.7;
            } else if (d < d2) {
                d2 = d;
            }
        }
    }
    float dist = sqrt(d1);
    float edgeDist = sqrt(d2) - dist;

    // 6-лучевая кристаллическая снежинка/дендрит внутри каждой ячейки льда
    float ang = atan(bestVec.y, bestVec.x) + bestId * 6.283185;
    float sector = mod(abs(ang), 1.0471975);
    if (sector > 0.5235988) sector = 1.0471975 - sector;
    float uRay = dist * cos(sector);
    float vRay = dist * sin(sector);
    float mainRay = exp(-vRay * 26.0) * smoothstep(0.68, 0.05, uRay);
    float subRay1 = exp(-abs(vRay - max(0.0, uRay - 0.14) * 0.577) * 34.0) * smoothstep(0.55, 0.14, uRay);
    float subRay2 = exp(-abs(vRay - max(0.0, uRay - 0.28) * 0.577) * 38.0) * smoothstep(0.62, 0.28, uRay);
    float starCrystal = clamp(mainRay + subRay1 * 0.75 + subRay2 * 0.65, 0.0, 1.0);

    // Острые полигональные трещинки/грани между кристаллами льда
    float facetCrack = smoothstep(0.09, 0.0, edgeDist);
    float bevel = (bestVec.x - bestVec.y) * 1.4;
    return vec4(starCrystal, facetCrack, fract(bestId), bevel);
}

float fbmRime(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    mat2 rot = mat2(0.80, -0.60, 0.60, 0.80);
    for (int i = 0; i < 4; ++i) {
        v += a * noiseFrost(p);
        p = rot * p * 2.12 + vec2(13.7, 29.3);
        a *= 0.5;
    }
    return v;
}

// Генератор редких детальных 6-лучевых снежинок в некоторых местах для Бархатного инея
vec2 sparseVelvetSnowflakes(vec2 pixelP) {
    float totalFlake = 0.0;
    float totalGlint = 0.0;
    // 2 разреженных слоя (крупные и средние снежинки местами)
    for (int layer = 0; layer < 2; ++layer) {
        float cellSz = (layer == 0) ? 86.0 : 54.0;
        vec2 gridP = (pixelP + vec2(float(layer) * 41.7, float(layer) * 67.3)) / cellSz;
        vec2 cellId = floor(gridP);
        vec2 f = fract(gridP);
        vec2 h  = hash2Frost(cellId * vec2(1.73, 2.39) + vec2(float(layer) * 31.4, 11.7));
        vec2 h2 = hash2Frost(cellId * vec2(3.11, 1.47) + vec2(73.1, float(layer) * 47.9));
        if (h.x < 0.76) continue;

        vec2 center = vec2(0.18 + 0.64 * h2.x, 0.18 + 0.64 * h2.y);
        vec2 d = (f - center) * cellSz;
        float maxR = (layer == 0) ? (11.0 + 6.0 * fract(h.y * 5.1)) : (6.5 + 4.0 * fract(h.y * 3.7));
        float r = length(d) / maxR;
        if (r > 1.05) continue;

        float rot = (h.x * 6.283185) + float(layer);
        float ang = atan(d.y, d.x) + rot;
        float sector = mod(abs(ang), 1.0471975);
        if (sector > 0.5235988) sector = 1.0471975 - sector;

        float u = r * cos(sector);
        float v = r * sin(sector);

        // Главный луч + 2 пары боковых веточек под углом 60° + центральный гексагон
        float mainArm = smoothstep(0.038, 0.008, v) * smoothstep(1.0, 0.05, u);
        float b1 = smoothstep(0.030, 0.006, abs(v - max(0.0, u - 0.25) * 0.577)) * smoothstep(0.72, 0.25, u) * step(0.24, u);
        float b2 = smoothstep(0.026, 0.005, abs(v - max(0.0, u - 0.52) * 0.577)) * smoothstep(0.88, 0.52, u) * step(0.50, u);
        float hexCore = smoothstep(0.22, 0.16, u + v * 0.577) * (1.0 - 0.55 * smoothstep(0.14, 0.08, u + v * 0.577));

        float flake = clamp(mainArm + b1 * 0.85 + b2 * 0.75 + hexCore * 0.80, 0.0, 1.0);
        totalFlake = max(totalFlake, flake);
        totalGlint = max(totalGlint, flake * (0.6 + 0.4 * cos(ang * 6.0)));
    }
    return vec2(totalFlake, totalGlint);
}

// Проекция одного слоя перекрещивающихся острых ледяных игл (игольчатый кристаллический иней)
vec2 needleAxisLayer(vec2 pixelP, float angleRad, float seed) {
    float c = cos(angleRad), s = sin(angleRad);
    vec2 rp = vec2(pixelP.x * c - pixelP.y * s, pixelP.x * s + pixelP.y * c);
    // Поперечная координата иглы (узкая) и продольная координата вдоль острия иглы (длинная)
    vec2 sc = vec2(rp.x * 0.34 + seed * 11.7, rp.y * 0.024 + seed * 5.3);
    vec2 id = floor(sc);
    vec2 fr = fract(sc);
    vec2 h  = hash2Frost(id + vec2(seed * 17.0));

    // Игла присутствует в ~55% дорожек со случайным смещением и длиной
    float isNeedleActive = smoothstep(0.38, 0.48, h.x);
    float center = 0.20 + 0.60 * h.y;
    float halfW  = 0.14 + 0.16 * fract(h.x * 9.1);
    float profileX = clamp(1.0 - abs(fr.x - center) / halfW, 0.0, 1.0);
    // Острое сужение иглы к обоим концам (ромбовидное ледяное остриё)
    float tipTaper = sin(fr.y * 3.14159265);
    float sharpNeedle = pow(profileX, 1.7) * pow(tipTaper, 0.75) * isNeedleActive;
    float sideSlope = ((fr.x < center) ? 1.0 : -1.0) * sharpNeedle;
    return vec2(sharpNeedle, sideSlope);
}

// 3 формы замерзания:
// style == 0 (или 3 со снежинками): ❄ Бархатный снежный иней
// style == 1: 🌿 Перекрещивающиеся острые ледяные иглы
// style == 2: ✨ Звёздная морозная корка
vec3 computeFrostCrystal(vec2 pixelP, vec2 uv, vec2 res, float fogDensity, int style) {
    float micro1 = noiseFrost(pixelP * 0.38);
    float micro2 = noiseFrost(pixelP * 0.95 + vec2(17.1, 31.4));
    float diamondGlint = pow(hashFrost(floor(pixelP * 0.65)), 14.0);

    // Очень мягкая облачная огибающая для плавного растворения инея к центру экрана
    float softEnv = smoothstep(0.0, 0.82, fogDensity);

    if (style == 0 || style == 3) {
        float cloud1 = fbmRime(pixelP * 0.022);
        float cloud2 = fbmRime(pixelP * 0.068 + vec2(cloud1 * 2.2));
        float fineRime = mix(micro1, micro2, 0.5);
        float rimeBody = clamp(cloud1 * 0.42 + cloud2 * 0.35 + fineRime * 0.35, 0.0, 1.0);
        float edgeMask = smoothstep(0.0, 0.75, fogDensity * (0.55 + 0.65 * cloud2));
        float pattern = clamp(edgeMask * (0.32 + 0.68 * rimeBody), 0.0, 1.0);
        float sparkle = (diamondGlint * 0.75 + pow(micro1 * micro2, 2.6) * 0.55) * edgeMask * softEnv;
        float bevel = (cloud2 - 0.5) * 0.55 * edgeMask;

        if (style == 3 && fogDensity > 0.02) {
            vec2 sf = sparseVelvetSnowflakes(pixelP);
            // Снежинки в инее плавно проявляются вместе с плотностью инея, не образуя резкой границы!
            float sfFade = pow(smoothstep(0.03, 0.65, fogDensity), 1.25);
            float sfVis  = sf.x * sfFade;
            pattern = clamp(pattern + sfVis * 0.78, 0.0, 1.0);
            sparkle = clamp(sparkle + sf.y * sfVis * 0.65, 0.0, 1.2);
            bevel  += sfVis * 0.35;
        }
        return vec3(pattern, sparkle, bevel);
    } else if (style == 1) {
        float domAng = fbmRime(pixelP * 0.008) * 0.55;
        vec2 n1 = needleAxisLayer(pixelP, 0.2618 + domAng, 1.0);
        vec2 n2 = needleAxisLayer(pixelP, 1.3090 + domAng, 2.0);
        vec2 n3 = needleAxisLayer(pixelP, 2.3562 + domAng, 3.0);
        vec2 n4 = needleAxisLayer(pixelP * 1.55, 0.7854 - domAng * 0.7, 4.0);
        vec2 n5 = needleAxisLayer(pixelP * 1.55, 1.8326 - domAng * 0.7, 5.0);

        float mainNeedles = max(n1.x, max(n2.x, n3.x));
        float subNeedles  = max(n4.x, n5.x) * 0.78;
        float needleField = clamp(max(mainNeedles, subNeedles) + micro1 * 0.16, 0.0, 1.0);

        float needleReach = fogDensity * (0.35 + 0.95 * needleField);
        float cut = smoothstep(0.02, 0.58, needleReach);

        float pattern = clamp(cut * (0.18 + 0.82 * needleField), 0.0, 1.0);
        float bevel   = (n1.y + n2.y * 0.8 - n3.y * 0.9 + n4.y * 0.5) * cut;
        float sparkle = (diamondGlint * 0.80 + pow(needleField * micro2, 2.0) * 0.80) * cut * softEnv;
        return vec3(pattern, sparkle, bevel);
    } else {
        vec4 c1 = voronoiIceCluster(pixelP * 0.045);
        vec4 c2 = voronoiIceCluster(pixelP * 0.110 + vec2(19.4, 7.2));
        float cloud = fbmRime(pixelP * 0.035);
        float stars = clamp(c1.x * 0.75 + c2.x * 0.65 + micro1 * 0.25, 0.0, 1.0);
        float edgeMask = smoothstep(0.0, 0.72, fogDensity * (0.55 + 0.60 * cloud));
        float pattern = clamp(edgeMask * (0.25 + 0.75 * stars), 0.0, 1.0);
        float sparkle = (diamondGlint * 0.75 + pow(stars * micro2, 2.2)) * edgeMask * softEnv;
        float bevel = c1.w * 0.45 * edgeMask;
        return vec3(pattern, sparkle, bevel);
    }
}

vec3 computeBorderGlowColor(vec2 p, vec2 res) {
    if (u_border_color_mode > 3.5) return u_border_color;
    if (u_border_color_mode < 0.5) return vec3(1.0);
    float ang = atan(p.y - 0.5 * res.y, p.x - 0.5 * res.x) + u_time * 0.8;
    if (u_border_color_mode < 1.5) return mix(vec3(0.45, 0.82, 1.0), vec3(0.85, 0.96, 1.0), 0.5 + 0.5 * sin(ang * 2.0));
    if (u_border_color_mode < 2.5) return mix(vec3(0.20, 0.95, 0.65), vec3(0.45, 0.60, 1.0), 0.5 + 0.5 * sin(ang * 2.0));
    return mix(vec3(1.0, 0.35, 0.65), vec3(1.0, 0.78, 0.25), 0.5 + 0.5 * sin(ang * 2.0));
}

void main() {
    vec2 res = max(u_resolution, vec2(1.0));
    vec2 texel = 1.0 / res;
    vec2 uv = qt_TexCoord0;
    vec2 pixelP = uv * res;

    vec4 rawWaterUnwarped = texture(u_waterMap, uv);
    // Искажение computeLiquidWarp применяется ТОЛЬКО к каплям, текущим по стеклу (rawWaterUnwarped.g > 0.08),
    // а сорвавшиеся падающие в воздухе капли (g == 0) падают строго прямо без «змейки»!
    float gNear = max(rawWaterUnwarped.g, texture(u_waterMap, uv + vec2(0.0, texel.y * 2.0)).g);
    float isSlidingOnGlass = smoothstep(0.04, 0.14, gNear);
    vec2 warpedUv = clamp(uv + computeLiquidWarp(pixelP, texel) * isSlidingOnGlass, vec2(0.001), vec2(0.999));
    vec4 rawWater = texture(u_waterMap, warpedUv);
    float streakMask = rawWaterUnwarped.r;

    float rawFogCode = abs(u_fog_strength);
    float fogAmt = fract(rawFogCode);
    if (rawFogCode >= 0.999 && fogAmt < 0.001) fogAmt = 1.0;
    float fogSpread = clamp(floor(rawFogCode) * 0.01, 0.0, 1.0);
    bool allowRimMist = (u_fog_strength > 0.001);

    float cardFog = (fogAmt > 0.01) ? sampleFogField(warpedUv, texel) : 0.0;
    float winWiped = sampleWinWiped(warpedUv, texel);

    float idleB = clamp(u_idle_fog_boost, 0.0, 1.0);
    float effFogSpread = clamp(fogSpread + idleB * (1.0 - fogSpread * 0.65), 0.0, 1.0);
    float effFogAmt    = clamp(fogAmt + idleB * (1.0 - fogAmt * 0.55), 0.0, 1.0);
    vec2 centered = (uv - vec2(0.5)) * 2.0;
    float rSq = dot(centered, centered) * 0.5;
    float rNorm = clamp(sqrt(rSq), 0.0, 1.0);
    float curveExp = mix(2.8, 0.45, effFogSpread);
    float smoothRadial = pow(rNorm, curveExp);
    float frostRawVal = mod(u_frost_strength, 10.0);
    float frostModeActive = step(0.005, frostRawVal);
    // Расстояние до ближайшего края экрана (0 в центре, 1 у самых рамок/углов экрана)
    vec2 edgeDist2 = abs(uv - vec2(0.5)) * 2.0;
    // Мягкий супер-эллипс без резких диагональных изломов в углах
    float softRect = pow(pow(edgeDist2.x, 2.8) + pow(edgeDist2.y, 2.8), 1.0 / 2.8);
    float cornerEdge = clamp(mix(rNorm, softRect, 0.52), 0.0, 1.0);
    float frostExp = mix(6.2, 1.15, pow(effFogSpread, 0.80));
    float smoothEdge = smoothstep(0.12, 1.0, cornerEdge);
    float frostRim = pow(smoothEdge, frostExp) * (0.40 + 0.90 * pow(effFogAmt, 0.85));
    float rainScreenFog = mix(mix(pow(fogSpread, 1.45) * 0.62, 0.96, idleB), 1.35 + idleB * 0.35, smoothRadial);
    float rawScreenFog = mix(rainScreenFog, frostRim, frostModeActive);

    float wStr = clamp(u_wipe_strength, 0.0, 1.0);
    float rimMist      = allowRimMist ? (rawScreenFog * (1.0 - winWiped * wStr)) : 0.0;
    float wipedCardFog = cardFog * (1.0 - winWiped * wStr);

    float isBlockDrop = (u_has_scene > 0.5) ? (1.0 - step(0.85, rawWater.a)) : 0.0;
    float underFog = isBlockDrop * clamp(max(wipedCardFog, rimMist * 1.15) * fogAmt * 1.35, 0.0, 1.0);

    float hCenter = sampleWaterField(warpedUv, texel, 1.45 + underFog * 0.30);
    float dropMask = smoothstep(0.12 - underFog * 0.02, 0.26 + underFog * 0.08, hCenter);

    vec2 lightningOrigin = vec2(clamp(u_lightning_x, 0.08, 0.92), -0.04);
    float distX = abs(uv.x - lightningOrigin.x);
    float horizFalloff = exp(-distX * distX * 6.5);
    float vertFalloff  = pow(max(0.0, 1.0 - uv.y * 2.9), 2.5);
    float topLightningFactor = vertFalloff * horizFalloff * u_lightning;

    vec4 waterCol = vec4(0.0);
    vec3 mistTint = vec3(0.74, 0.82, 0.92);

    // Капля проясняет туман внутри себя, создавая естественный контраст прозрачной воды на матовом стекле
    float outerDropCut = (1.0 - isBlockDrop) * dropMask * 0.78;
    float cardDropCut  = isBlockDrop * dropMask * (1.0 - underFog * 0.75) * 0.68;
    float visibleFog   = max(rimMist * (1.0 - outerDropCut), wipedCardFog * (1.0 - max(outerDropCut, cardDropCut)));

    float dither = fract(sin(dot(pixelP, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;

    if (visibleFog > 0.002) {
        vec2 fOff = vec2(texel.x * 2.2, texel.y * 2.0);
        vec4 sCenter = (u_has_scene > 0.5) ? texture(u_scene, uv) : vec4(0.0);
        vec3 blurC = (u_has_scene > 0.5) ? (
            sCenter.rgb * 2.0
            + texture(u_scene, clamp(uv + fOff, vec2(0.001), vec2(0.999))).rgb
            + texture(u_scene, clamp(uv - fOff, vec2(0.001), vec2(0.999))).rgb
        ) / 4.0 : vec3(0.09, 0.11, 0.15);

        int frostStyle = int(floor(u_frost_strength / 10.0 + 0.01));
        float frostW = clamp(mod(u_frost_strength, 10.0), 0.0, 2.0);
        float frostMode = step(0.005, frostW);
        vec3 frostData = (frostW > 0.005) ? computeFrostCrystal(pixelP, uv, res, visibleFog, frostStyle) : vec3(0.0);
        float frostPat = clamp(frostData.x * frostW, 0.0, 1.35);
        float frostSparkle = clamp(frostData.y * frostW, 0.0, 1.35);
        float frostBevel = frostData.z;

        vec2 iceRefrUv = clamp(uv + vec2(frostBevel, -frostBevel) * texel * 4.0 * frostW, vec2(0.001), vec2(0.999));
        vec3 iceScene = (u_has_scene > 0.5 && frostW > 0.005) ? texture(u_scene, iceRefrUv).rgb : blurC;

        vec3 baseMistTint = mix(mistTint, vec3(0.80, 0.91, 1.00), frostMode * 0.85);
        vec3 crystalRidgeCol = vec3(0.92, 0.97, 1.00) * max(0.0, frostBevel * 1.1 + frostPat * 0.38);
        vec3 softSteam = baseMistTint + vec3(dither * 0.010) + crystalRidgeCol;
        vec3 desktopSteam = mix(softSteam * 0.45, softSteam * 0.90, max(idleB * 0.65, frostPat * 0.75));
        vec3 mistRGB = mix(desktopSteam, mix(iceScene * 0.78, softSteam * 0.90, 0.40 + idleB * 0.28 + frostPat * 0.35), clamp(sCenter.a * 1.5, 0.0, 1.0));
        mistRGB += vec3(0.96, 0.99, 1.00) * (frostSparkle * 0.55);

        float smoothFade = mix(1.0, smoothstep(0.0, 0.55, visibleFog), frostMode);
        float patternedCoverage = visibleFog * smoothFade * (0.06 + 0.94 * frostPat);
        float frostRelief = mix(visibleFog, patternedCoverage, frostMode * 0.92);
        float intensityScale = mix(pow(effFogAmt, 0.72), pow(effFogAmt, 1.10), frostMode);
        float baseAlpha = frostRelief * intensityScale * (0.90 + idleB * 0.22);
        float fAlpha = clamp(baseAlpha + frostSparkle * 0.18 * intensityScale * smoothFade + dither * (1.5 / 255.0), 0.0, 0.95);
        waterCol = vec4(clamp(mistRGB, 0.0, 1.0) * fAlpha, fAlpha);
    }

    float totalWaterMask = clamp(dropMask + streakMask * 0.75, 0.0, 1.0);

    if (totalWaterMask > 0.002) {
        float rSample = 2.4 + underFog * 0.45;
        float hL = sampleWaterField(warpedUv - vec2(texel.x * rSample, 0.0), texel, 1.15);
        float hR = sampleWaterField(warpedUv + vec2(texel.x * rSample, 0.0), texel, 1.15);
        float hU = sampleWaterField(warpedUv - vec2(0.0, texel.y * rSample), texel, 1.15);
        float hD = sampleWaterField(warpedUv + vec2(0.0, texel.y * rSample), texel, 1.15);

        vec2 grad = vec2(hR - hL, hD - hU) * (9.2 - underFog * 2.8);
        float slopeMask = smoothstep(0.04, 0.48, length(grad));

        vec2 streakNormal = vec2((rawWaterUnwarped.a - 0.5) * 1.4, -0.25) * streakMask;
        vec2 totalGrad = grad * dropMask + streakNormal;

        float zProfile = sqrt(max(0.18, 1.0 - min(0.82, dot(totalGrad, totalGrad) * 0.14)));
        vec3 N = normalize(vec3(-totalGrad, zProfile));

        vec3 lightDir1 = normalize(vec3(-0.48, -0.72, 0.66));
        vec3 lightDir2 = normalize(vec3(0.42, 0.62, 0.66));
        vec3 viewDir   = vec3(0.0, 0.0, 1.0);

        float fogSoften = 1.0 - underFog * 0.72;
        float ndh1 = max(0.0, dot(N, normalize(lightDir1 + viewDir)));
        float ndh2 = max(0.0, dot(N, normalize(lightDir2 + viewDir)));

        // ПРОЗРАЧНАЯ ГЛУБИНА ВОДЫ (без чёрной заливки!):
        // Лёгкое 18%-ное притемнение только на самом крутом верхнем изгибе мениска + светлая водяная линза снизу
        float meniscusShadow = dropMask * (pow(slopeMask, 1.5) * 0.14 + max(0.0, -N.y) * slopeMask * 0.12);
        float bellyCaustic   = pow(max(0.0, dot(N.xy, vec2(0.30, 0.88))), 1.8) * 0.22 * fogSoften * dropMask * slopeMask;
        float crispSpec      = (smoothstep(0.948, 0.995, ndh1) * 0.38 + pow(ndh1, 38.0) * 0.12) * fogSoften * dropMask * slopeMask
                             + pow(ndh1, 36.0) * 0.12 * fogSoften * streakMask;
        float specSec        = pow(ndh2, 30.0) * 0.10 * fogSoften * dropMask * slopeMask;
        float edgeRim        = smoothstep(0.12, 0.21, hCenter) * (1.0 - smoothstep(0.21, 0.35, hCenter)) * 0.08 * fogSoften;

        vec3 dynamicGlowRGB = vec3(0.0);
        float dynamicGlowA  = 0.0;

        if (u_lightning > 0.01) {
            vec2 dirToStrike = normalize(lightningOrigin - uv);
            float dirCatch = max(0.0, dot(N.xy, dirToStrike));
            float dropL = dirCatch * (0.25 + 0.75 * horizFalloff) * pow(max(0.0, 1.0 - uv.y * 1.5), 1.4) * u_lightning;
            float lCatch = (slopeMask * 0.90 + edgeRim * 1.2 + streakMask * 0.45) * dropL;
            dynamicGlowRGB += vec3(0.82, 0.92, 1.0) * lCatch * 0.95;
            dynamicGlowA   += lCatch * 0.58;
        }

        if (u_cursor_strength > 0.01 && u_mouse.x > -50.0) {
            vec2 toMouse = u_mouse - pixelP;
            float distM = length(toMouse);
            if (distM < 280.0 && distM > 1.0) {
                float attenM = pow(1.0 - distM / 280.0, 1.6) * u_cursor_strength;
                vec2 dirM = toMouse / distM;
                float cL = sampleWaterField(uv - vec2(texel.x * 3.4, 0.0), texel, 1.7);
                float cR = sampleWaterField(uv + vec2(texel.x * 3.4, 0.0), texel, 1.7);
                float cU = sampleWaterField(uv - vec2(0.0, texel.y * 3.4), texel, 1.7);
                float cD = sampleWaterField(uv + vec2(0.0, texel.y * 3.4), texel, 1.7);
                vec2 cGrad = vec2(cR - cL, cD - cU) * 6.2;
                vec2 cNorm = normalize(vec3(-cGrad, 1.0)).xy;
                float cSlope = smoothstep(0.04, 0.34, length(cGrad));
                float catchM = pow(max(0.0, dot(cNorm, dirM)), 1.35) * cSlope * dropMask + 0.20 * streakMask;
                dynamicGlowRGB += u_cursor_color * catchM * attenM * 0.88;
                dynamicGlowA   += catchM * attenM * 0.50;
            }
        }

        if (u_border_strength > 0.01) {
            float dEdge = min(min(pixelP.x, res.x - pixelP.x), min(pixelP.y, res.y - pixelP.y));
            float maxReach = 90.0 + u_border_spread * 240.0;
            if (dEdge < maxReach) {
                float attenB = pow(1.0 - dEdge / maxReach, 1.6) * u_border_strength;
                vec2 toEdge = normalize(vec2(
                    (pixelP.x < res.x * 0.5) ? -1.0 : 1.0,
                    (pixelP.y < res.y * 0.5) ? -1.0 : 1.0
                ));
                float catchB = (0.30 + 0.70 * max(0.0, dot(N.xy, toEdge))) * (slopeMask * 0.85 + edgeRim + streakMask * 0.30);
                vec3 bCol = computeBorderGlowColor(pixelP, res);
                dynamicGlowRGB += bCol * catchB * attenB * 0.72;
                dynamicGlowA   += catchB * attenB * 0.42;
            }
        }

        if (u_has_scene > 0.5) {
            float refrScale = (0.032 * clamp(hCenter, 0.0, 0.88) * (0.30 + 0.70 * slopeMask) + 0.006 * streakMask) * u_refraction_strength;
            vec2 offset = N.xy * refrScale;

            vec4 sR = texture(u_scene, clamp(uv + offset * 1.06, vec2(0.001), vec2(0.999)));
            vec4 sG = texture(u_scene, clamp(uv + offset * 1.00, vec2(0.001), vec2(0.999)));
            vec4 sB = texture(u_scene, clamp(uv + offset * 0.94, vec2(0.001), vec2(0.999)));

            float hlMul     = clamp(u_drop_highlight, 0.1, 2.0);
            float tStr      = clamp(u_drop_tint_strength, 0.0, 1.0);
            float refrW     = dropMask * (0.45 + 0.35 * slopeMask);
            vec3 refrRGB    = mix(vec3(sR.r, sG.g, sB.b), u_drop_tint_color, tStr * 0.45) * (1.0 - meniscusShadow * (0.45 + tStr * 0.70));
            float lightA    = (crispSpec + specSec + bellyCaustic * 0.5 + edgeRim) * hlMul + streakMask * 0.14 + dynamicGlowA;
            float bodyAlpha = clamp(refrW + lightA * (1.0 - refrW * 0.5), 0.0, 0.92);
            vec3 hlTint     = mix(vec3(0.84, 0.85, 0.88), clamp(u_drop_tint_color + vec3(0.65), 0.0, 1.0), 0.25);
            vec3 outPremul  = refrRGB * refrW
                            + hlTint * ((edgeRim + bellyCaustic) * hlMul + streakMask * 0.12)
                            + vec3(0.96, 0.97, 0.98) * ((crispSpec + specSec) * hlMul)
                            + dynamicGlowRGB;

            vec4 dropCol = vec4(clamp(outPremul, 0.0, 1.0), bodyAlpha);
            waterCol = dropCol + waterCol * (1.0 - dropCol.a);
        } else {
            float hlMul     = clamp(u_drop_highlight, 0.1, 2.0);
            float tStr      = clamp(u_drop_tint_strength, 0.0, 1.0);
            // По умолчанию (tStr = 0.20) капля в 2 раза светлее и прозрачнее, а ползунком можно сделать от 0% (кристалл) до 100% (густой тон)
            float darkShade = meniscusShadow * (0.42 + tStr * 1.45) + dropMask * (tStr * 0.22);
            float lightPart = (bellyCaustic + edgeRim * 1.35 + crispSpec + specSec) * hlMul + streakMask * 0.15;
            float outA      = clamp(darkShade + lightPart + dynamicGlowA, 0.0, 0.88);
            vec3 hlTint     = mix(vec3(0.84, 0.85, 0.88), clamp(u_drop_tint_color + vec3(0.65), 0.0, 1.0), 0.25);
            vec3 bodyRGB    = u_drop_tint_color * darkShade
                            + hlTint * ((bellyCaustic + edgeRim * 1.35) * hlMul + streakMask * 0.12)
                            + vec3(0.96, 0.97, 0.98) * ((crispSpec + specSec) * hlMul)
                            + dynamicGlowRGB;

            vec4 dropCol = vec4(clamp(bodyRGB, 0.0, 1.0), outA);
            waterCol = dropCol + waterCol * (1.0 - dropCol.a);
        }
    }

    vec4 lightningSky = vec4(0.0);
    if (topLightningFactor > 0.003) {
        float skyGrad = topLightningFactor * 0.28;
        lightningSky = vec4(vec3(0.80, 0.90, 1.0) * skyGrad, skyGrad);
    }

    vec4 overCol = texture(u_overlayMap, uv);
    vec4 combined = overCol + waterCol * (1.0 - overCol.a);
    combined = combined + lightningSky * (1.0 - combined.a);
    fragColor = combined * qt_Opacity;
}
