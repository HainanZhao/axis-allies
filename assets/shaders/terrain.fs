#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0; // Primary biome ground texture (grass / sand / snow)
uniform sampler2D fogTexture; // Dynamic 64x64 Fog of War texture (hardware bilinear filtered)
uniform vec3 camPos;
uniform int biomeId;
uniform float timeSec;
uniform int useFogTexture;

out vec4 finalColor;

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * vnoise(p);
        p *= 2.07;
        a *= 0.5;
    }
    return v;
}

// Cinematic ACES Filmic Tone Mapping
vec3 acesTone(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec2 wp = fragPosition.xz;

    // Dual-scale texture sampling: macro scale prevents tiling pattern repetition, micro adds crisp detail
    vec3 texMacro = texture(texture0, wp * 0.016).rgb;
    vec3 texMicro = texture(texture0, wp * 0.125).rgb;
    vec3 texSample = mix(texMacro, texMicro, 0.50);

    float nMacro = fbm(wp * 0.024);
    float nMicro = fbm(wp * 0.18);
    float nFine  = vnoise(wp * 1.6);
    float nGrass = vnoise(wp * 5.8);

    // Micro-surface normal perturbation for tactile grass blades and soil clods
    vec3 bumpN = normalize(N + vec3((nMicro - 0.5) * 0.14 + (nGrass - 0.5) * 0.08, 0.0, (nFine - 0.5) * 0.14));

    // Organic European WWII agricultural bocage plots (tilted, irregular field parcels with hedgerow borders)
    mat2 rotField = mat2(0.939, -0.342, 0.342, 0.939); // 20-degree natural field orientation
    vec2 fieldCoord = (rotField * wp) * 0.012 + vec2(fbm(wp * 0.006), fbm(wp.yx * 0.006)) * 0.9;
    vec2 cellId = floor(fieldCoord);
    vec2 cellF = fract(fieldCoord);
    float fieldRand = vnoise(cellId * 8.73);
    float borderDist = min(min(cellF.x, 1.0 - cellF.x), min(cellF.y, 1.0 - cellF.y));
    float fieldBorder = smoothstep(0.03, 0.14, borderDist);

    // Calibrated natural palette (rich deep olive European terrain, soft on eyes)
    vec3 colLushGrass   = texSample * vec3(0.68, 0.76, 0.58);
    vec3 colGoldenWheat = mix(vec3(0.52, 0.45, 0.26), vec3(0.62, 0.54, 0.32), nMicro);
    vec3 colPlowedSoil  = mix(vec3(0.28, 0.22, 0.16), vec3(0.36, 0.28, 0.20), nFine);
    vec3 colRock        = mix(vec3(0.34, 0.35, 0.36), vec3(0.44, 0.42, 0.40), nMicro);
    vec3 colRoad        = mix(vec3(0.35, 0.30, 0.22), vec3(0.42, 0.36, 0.28), nFine);

    if (biomeId == 1) { // North Africa Desert (Warm weathered terracotta/khaki sand, NOT blinding white)
        colLushGrass   = texSample * vec3(0.72, 0.62, 0.46);
        colGoldenWheat = mix(vec3(0.65, 0.54, 0.38), vec3(0.72, 0.62, 0.44), nMacro);
        colPlowedSoil  = vec3(0.52, 0.42, 0.30);
        colRoad        = mix(vec3(0.48, 0.40, 0.28), vec3(0.56, 0.48, 0.34), nFine);
    } else if (biomeId == 2 || biomeId == 4) { // Stalingrad / Ardennes Snow
        colLushGrass   = texSample * vec3(0.68, 0.72, 0.78);
        colGoldenWheat = mix(vec3(0.58, 0.62, 0.68), vec3(0.70, 0.74, 0.78), nMacro);
        colPlowedSoil  = vec3(0.36, 0.38, 0.40);
        colRoad        = mix(vec3(0.32, 0.30, 0.28), vec3(0.44, 0.45, 0.48), nFine);
    } else if (biomeId == 6) { // Pacific Atoll
        colLushGrass   = texSample * vec3(0.65, 0.78, 0.58);
        colGoldenWheat = vec3(0.62, 0.58, 0.42);
        colPlowedSoil  = vec3(0.42, 0.35, 0.25);
    }

    // Blend pasture, cultivated golden crop fields, and plowed soil
    vec3 albedo = colLushGrass;
    if (biomeId == 0 || biomeId == 3 || biomeId == 5) {
        if (fieldRand > 0.60) {
            albedo = mix(albedo, colGoldenWheat, fieldBorder * 0.70);
        } else if (fieldRand < 0.32) {
            albedo = mix(albedo, colPlowedSoil, fieldBorder * 0.65);
        }
    }
    albedo *= (0.92 + 0.10 * nGrass);

    // Steep slope craggy rock splatting on ridgelines and sunken riverbanks
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);
    float rockMask = smoothstep(0.12, 0.34, slope);
    albedo = mix(albedo, colRock, rockMask);

    // Riverbank mud & pebble gravel transition near river (|z| < 8)
    float riverDist = abs(wp.y);
    float bankMask = 1.0 - smoothstep(2.5, 7.5, riverDist);
    vec3 bankCol = (biomeId == 2 || biomeId == 4) ? vec3(0.38, 0.42, 0.46) : mix(vec3(0.32, 0.26, 0.18), vec3(0.40, 0.34, 0.24), nFine);
    albedo = mix(albedo, bankCol, bankMask * 0.55);

    // Strategic WWII Main Highways & Connector Roads (Worn dirt tracks with dark vehicle ruts)
    float dCenterRoad = abs(wp.x);
    float dFlankRoad  = abs(abs(wp.x) - 240.0);
    float dOutpostRoad = abs(abs(wp.y) - 260.0);
    float dBridgeTownRoad = abs(abs(wp.y) - 70.0);
    float lateralRoad = min(dOutpostRoad, dBridgeTownRoad);
    float lateralMask = smoothstep(360.0, 320.0, abs(wp.x));
    float roadDist = min(min(dCenterRoad, dFlankRoad), mix(999.0, lateralRoad, lateralMask));
    float roadNoise = (nMicro - 0.5) * 1.6;

    float roadMask = 1.0 - smoothstep(7.2, 11.0, roadDist + roadNoise);
    float rutL = exp(-pow((roadDist - 3.2) * 1.1, 2.0));
    float rutR = exp(-pow((roadDist - 0.8) * 1.1, 2.0));
    float ruts = max(rutL, rutR);
    vec3 rCol = mix(colRoad, colRoad * 0.70, ruts * 0.70);
    albedo = mix(albedo, rCol, roadMask * 0.90);

    // Dynamic Soft Volumetric Cloud Shadows drifting across the landscape
    vec2 cloudUV = wp * 0.0035 + vec2(timeSec * 0.014, timeSec * 0.007);
    float cloudNoise = fbm(cloudUV);
    float cloudShadow = smoothstep(0.42, 0.68, cloudNoise) * 0.38;
    float sunExposure = 1.0 - cloudShadow;

    // Directional Sun Key Light + Sky Hemisphere Fill (Calibrated to pleasant non-glaring lux)
    vec3 lightDir = normalize(vec3(0.55, 0.80, 0.34));
    vec3 viewDir  = normalize(camPos - fragPosition);
    vec3 halfVec  = normalize(lightDir + viewDir);

    float ndotl = max(dot(bumpN, lightDir), 0.0);
    float skyFactor = 0.5 + 0.5 * bumpN.y;
    vec3 ambient = mix(vec3(0.12, 0.14, 0.16), vec3(0.24, 0.28, 0.35), skyFactor);
    vec3 sunCol  = vec3(0.92, 0.88, 0.80);

    // Backlit Foliage Subsurface Translucency
    float backLight = pow(max(dot(-viewDir, lightDir), 0.0), 3.0) * max(bumpN.y, 0.0);
    vec3 sssGlow = (biomeId == 0 || biomeId == 3 || biomeId == 5) ? vec3(0.06, 0.10, 0.04) : vec3(0.0);

    float spec = pow(max(dot(bumpN, halfVec), 0.0), 32.0) * (0.02 + 0.12 * bankMask + 0.08 * rockMask);
    vec3 lit = albedo * (ambient + sunCol * (ndotl * sunExposure * 0.82 + sssGlow * sunExposure)) + sunCol * spec * sunExposure;

    // Atmospheric distance aerial haze toward horizon
    float camDist = length(camPos - fragPosition);
    float fogFactor = clamp(smoothstep(240.0, 580.0, camDist), 0.0, 0.55);
    vec3 fogColor = (biomeId == 1) ? vec3(0.58, 0.52, 0.42) :
                    ((biomeId == 2 || biomeId == 4) ? vec3(0.52, 0.58, 0.65) : vec3(0.38, 0.46, 0.54));
    lit = mix(lit, fogColor, fogFactor);

    // AAA Dynamic Fog of War Surface Shading (Immune to Z-fighting & zebra stripes)
    if (useFogTexture == 1) {
        vec2 fogUV = clamp((fragPosition.xz + 560.0) / 1120.0, 0.0, 1.0);
        float fogVal = texture(fogTexture, fogUV).r;
        vec3 shroudColor = vec3(0.015, 0.018, 0.016);
        vec3 exploredShade = lit * vec3(0.35, 0.38, 0.42);
        vec3 fogged = mix(shroudColor, exploredShade, smoothstep(0.05, 0.28, fogVal));
        fogged = mix(fogged, lit, smoothstep(0.28, 0.95, fogVal));
        lit = fogged;
    }

    // Cinematic ACES Filmic Tone Mapping (Comfortable contrast, no blown highlights, no double-gamma washing)
    lit = acesTone(lit * 0.92);

    finalColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}