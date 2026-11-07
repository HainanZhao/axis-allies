#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform vec4 colDiffuse;
uniform vec3 camPos;
uniform int biomeId;
uniform float timeSec;

out vec4 finalColor;

// --- Fast Procedural Hash & Noise Functions ---
float hash31(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

float vnoise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i);
    float n100 = hash31(i + vec3(1, 0, 0));
    float n010 = hash31(i + vec3(0, 1, 0));
    float n110 = hash31(i + vec3(1, 1, 0));
    float n001 = hash31(i + vec3(0, 0, 1));
    float n101 = hash31(i + vec3(1, 0, 1));
    float n011 = hash31(i + vec3(0, 1, 1));
    float n111 = hash31(i + vec3(1, 1, 1));
    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z);
}

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float vnoise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm2(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; ++i) {
        v += a * vnoise2(p);
        p *= 2.05;
        a *= 0.5;
    }
    return v;
}

// --- Cinematic ACES Filmic Tone Mapping Curve ---
vec3 acesTone(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 V = normalize(camPos - fragPosition);

    // Compute true world-space geometric normal via screen-space derivatives
    vec3 dpdx = dFdx(fragPosition);
    vec3 dpdy = dFdy(fragPosition);
    vec3 N = cross(dpdx, dpdy);
    float nLen = length(N);
    if (nLen > 1e-6) {
        N /= nLen;
    } else {
        N = vec3(0.0, 1.0, 0.0);
    }
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    // Procedural rolled-steel armor plate grain & micro-texture
    float grainCoarse = vnoise3(fragPosition * 2.4);
    float grainFine   = vnoise3(fragPosition * 10.5);
    float microTex    = 0.90 + 0.12 * grainCoarse + 0.05 * (grainFine - 0.5);

    // Subtle normal perturbation for tactile steel casting / canvas weave
    vec3 bumpN = normalize(N + vec3((grainFine - 0.5) * 0.08, 0.0, (grainCoarse - 0.5) * 0.08));

    vec3 rawColor = fragColor.rgb * colDiffuse.rgb;

    // Automatic material parameter estimation from base color characteristics
    float maxC = max(rawColor.r, max(rawColor.g, rawColor.b));
    float minC = min(rawColor.r, min(rawColor.g, rawColor.b));
    float sat = (maxC > 0.001) ? (maxC - minC) / maxC : 0.0;
    float lum = dot(rawColor, vec3(0.299, 0.587, 0.114));

    float metallic = 0.15;
    float roughness = 0.60;
    if (lum < 0.28 && sat < 0.25) {
        // Gun barrel steel, tracks, dark engine grilles
        metallic = 0.82;
        roughness = 0.42;
    } else if (lum > 0.65 && sat < 0.35) {
        // Roadwheels, metallic rims, polished fittings
        metallic = 0.68;
        roughness = 0.38;
    } else if (sat > 0.45) {
        // Distinct national paint, cloth uniforms, camouflage coats
        metallic = 0.08;
        roughness = 0.68;
    }

    // Ground-contact Ambient Occlusion: natural darkening near ground/tracks
    float groundAO = mix(0.55, 1.0, clamp(fragPosition.y / 2.5, 0.0, 1.0));

    // Curvature-based Edge Crease: reveals bare oxidized steel on worn corners
    float edgeCrease = clamp(length(fwidth(N)) * 0.42, 0.0, 0.28);
    vec3 bareSteel = vec3(0.52, 0.55, 0.58);

    vec3 baseCol = mix(rawColor * microTex, bareSteel, edgeCrease * 0.55);

    // Environmental Weathering (Mud, Desert Dust, Winter Frost)
    float mudFactor = clamp((1.8 - fragPosition.y) / 1.8, 0.0, 1.0) * (0.65 + 0.35 * grainFine);
    if (biomeId == 1) { // Desert Sand Dust
        vec3 dustCol = vec3(0.62, 0.52, 0.38);
        baseCol = mix(baseCol, dustCol, mudFactor * 0.58);
        roughness = mix(roughness, 0.85, mudFactor * 0.50);
    } else if (biomeId == 2 || biomeId == 4) { // Winter Frost & Slush
        vec3 snowFrost = vec3(0.68, 0.72, 0.78);
        float frostTop = clamp(bumpN.y, 0.0, 1.0) * (grainFine * 0.4 + 0.3);
        baseCol = mix(baseCol, snowFrost, max(mudFactor * 0.50, frostTop * 0.40));
    } else { // European Bocage Damp Earth Splatter
        vec3 mudCol = vec3(0.24, 0.18, 0.12);
        baseCol = mix(baseCol, mudCol, mudFactor * 0.55);
    }

    // Directional Sun Key Light + Half Vector
    vec3 L = normalize(vec3(0.55, 0.80, 0.34));
    vec3 H = normalize(L + V);

    // Dynamic Soft Volumetric Cloud Shadows drifting across the battlefield
    vec2 cloudUV = fragPosition.xz * 0.0035 + vec2(timeSec * 0.014, timeSec * 0.007);
    float cloudNoise = fbm2(cloudUV);
    float cloudShadow = smoothstep(0.42, 0.68, cloudNoise) * 0.38;
    float sunExposure = 1.0 - cloudShadow;

    // --- Physically-Based GGX Specular BRDF ---
    float NdotL = max(dot(bumpN, L), 0.0);
    float NdotV = max(dot(bumpN, V), 0.001);
    float NdotH = max(dot(bumpN, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float alpha = max(0.04, roughness * roughness);
    float alpha2 = alpha * alpha;

    // GGX Normal Distribution
    float denomD = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    float D = alpha2 / (3.14159265 * denomD * denomD);

    // Schlick-Smith Geometric Masking
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1L = NdotL / (NdotL * (1.0 - k) + k);
    float G1V = NdotV / (NdotV * (1.0 - k) + k);
    float G = G1L * G1V;

    // Schlick Fresnel
    vec3 F0 = mix(vec3(0.04), baseCol, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    vec3 specBRDF = (D * F * G) / max(4.0 * NdotL * NdotV, 0.001);
    vec3 diffBRDF = (1.0 - F) * (1.0 - metallic) * baseCol / 3.14159265;

    // Direct Sun Radiance (Calibrated, pleasant lux)
    vec3 sunCol = vec3(0.92, 0.88, 0.80);
    vec3 directLight = (diffBRDF * 3.14159265 + specBRDF) * sunCol * NdotL * sunExposure;

    // Hemispherical Ambient Sky Fill + Warm Ground Bounce Light
    float hemi = 0.5 + 0.5 * bumpN.y;
    vec3 skyAmbient = mix(vec3(0.12, 0.13, 0.16), vec3(0.24, 0.30, 0.38), hemi);
    vec3 groundBounce = vec3(0.18, 0.14, 0.10) * max(-bumpN.y, 0.0);
    vec3 ambient = baseCol * (skyAmbient + groundBounce) * groundAO;

    // Rim Lighting Grazing Highlight for Silhouette Separation
    float rim = pow(1.0 - NdotV, 3.5) * (1.0 - roughness * 0.5);
    vec3 rimCol = mix(vec3(0.35, 0.45, 0.58), sunCol, NdotL) * rim * 0.24;

    vec3 lit = directLight + ambient + rimCol;

    // Cinematic ACES Filmic Tone Mapping (Natural contrast, no blown highlights)
    lit = acesTone(lit * 0.95);

    finalColor = vec4(clamp(lit, 0.0, 1.0), fragColor.a * colDiffuse.a);
}