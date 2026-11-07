#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform vec3 camPos;
uniform float timeSec;
uniform sampler2D fogTexture;
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
    for (int i = 0; i < 3; ++i) {
        v += a * vnoise(p);
        p *= 2.05;
        a *= 0.5;
    }
    return v;
}

// Cinematic ACES Filmic Tone Mapping Curve
vec3 acesTone(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 p = fragPosition.xz;

    // Multi-layered animated river ripples flowing east-to-west
    float w1 = sin(p.x * 0.42 + timeSec * 2.4) * cos(p.y * 0.52 - timeSec * 1.8);
    float w2 = sin((p.x * 0.85 + p.y * 0.65) - timeSec * 3.2) * 0.5;
    float w3 = cos(p.x * 1.65 - p.y * 1.45 + timeSec * 3.9) * 0.28;
    float w4 = sin(p.x * 3.2 + p.y * 2.8 - timeSec * 5.2) * 0.14;
    float wave = (w1 + w2 + w3 + w4) * 0.32;

    vec3 N = normalize(vec3(wave * 0.28, 1.0, wave * 0.32));
    vec3 V = normalize(camPos - fragPosition);
    vec3 L = normalize(vec3(0.55, 0.80, 0.34));
    vec3 H = normalize(L + V);

    // Dynamic Cloud Shadows on water surface
    vec2 cloudUV = p * 0.0035 + vec2(timeSec * 0.014, timeSec * 0.007);
    float cloudNoise = fbm(cloudUV);
    float cloudShadow = smoothstep(0.42, 0.68, cloudNoise) * 0.38;
    float sunExposure = 1.0 - cloudShadow;

    // Deep river channel vs shallow bank coloration & caustics
    float bankDist = 15.0 - abs(p.y);
    float depthFactor = clamp(bankDist / 9.5, 0.0, 1.0);

    // Animated subtle caustic light web on shallow river bottom
    float caustic1 = sin(p.x * 1.8 + p.y * 1.6 + timeSec * 2.2);
    float caustic2 = cos(p.x * 1.4 - p.y * 2.1 - timeSec * 1.8);
    float caustics = clamp(pow((caustic1 + caustic2) * 0.5 + 0.5, 3.5), 0.0, 1.0) * (1.0 - depthFactor) * 0.25;

    // Calibrated natural deep water tones (not glaring bright)
    vec3 shallowCol = vec3(0.12, 0.28, 0.32) + vec3(0.18, 0.24, 0.15) * caustics;
    vec3 deepCol    = vec3(0.04, 0.12, 0.20);
    vec3 skyReflect = vec3(0.32, 0.44, 0.58);

    // Physical Schlick Fresnel Reflection
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 4.5);
    vec3 waterCol = mix(shallowCol, deepCol, depthFactor);
    waterCol = mix(waterCol, skyReflect, clamp(fresnel * 0.70 + 0.10, 0.0, 0.80));

    // Sun specular glints (Calibrated lux)
    float spec = pow(max(dot(N, H), 0.0), 64.0) * 0.85;
    vec3 sunCol = vec3(0.92, 0.88, 0.80);
    waterCol += sunCol * spec * sunExposure;

    // Soft white shoreline foam along riverbanks & turbulent wake around bridge piers
    float foamNoise = vnoise(p * 2.8 + vec2(timeSec * 1.2, 0.0));
    float bankFoam = smoothstep(2.8, 0.2, abs(bankDist) + wave * 0.85 + (foamNoise - 0.5) * 0.6);
    float bridgeDist = min(abs(p.x), abs(abs(p.x) - 240.0));
    float pierWake = exp(-pow((bridgeDist - 9.0) * 0.6, 2.0)) * (0.45 + 0.25 * sin(p.y * 1.5 + timeSec * 4.0));
    float foam = clamp(bankFoam * 0.60 + pierWake, 0.0, 0.82);
    waterCol = mix(waterCol, vec3(0.78, 0.84, 0.88), foam);

    // Shadow cast onto water surface directly underneath the 3 stone bridges
    float underBridge = 1.0 - smoothstep(8.5, 12.0, bridgeDist);
    waterCol *= (1.0 - underBridge * 0.45);

    // Ambient sky light modulation
    waterCol *= (0.65 + 0.35 * sunExposure);

    // AAA Dynamic Fog of War Surface Shading
    if (useFogTexture == 1) {
        vec2 fogUV = clamp((fragPosition.xz + 560.0) / 1120.0, 0.0, 1.0);
        float fogVal = texture(fogTexture, fogUV).r;
        vec3 shroudColor = vec3(0.010, 0.012, 0.011);
        vec3 exploredShade = waterCol * vec3(0.32, 0.36, 0.40);
        vec3 fogged = mix(shroudColor, exploredShade, smoothstep(0.05, 0.28, fogVal));
        fogged = mix(fogged, waterCol, smoothstep(0.28, 0.95, fogVal));
        waterCol = fogged;
    }

    // Cinematic ACES Filmic Tone Mapping
    waterCol = acesTone(waterCol * 0.95);

    finalColor = vec4(clamp(waterCol, 0.0, 1.0), 0.95);
}