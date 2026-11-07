// ============================================================================
// AXIS & ALLIES (2004) — PROCEDURAL C++ PCM AUDIO & SOUNDTRACK ENGINE
// Generates wartime brass/drum music & realistic weapon SFX in memory
// ============================================================================

#include "audio/sound_engine.hpp"
#include <cmath>
#include <cstdlib>
#include <vector>

namespace aa2004 {

namespace {

constexpr int SAMPLE_RATE = 44100;

float frandSym(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return (static_cast<float>((seed >> 8) & 0xFFFF) / 32768.0f) - 1.0f;
}

Sound makeSoundFromBuffer(const std::vector<int16_t>& samples) {
    Wave w{};
    w.frameCount = static_cast<unsigned int>(samples.size());
    w.sampleRate = SAMPLE_RATE;
    w.sampleSize = 16;
    w.channels = 1;
    short* copy = static_cast<short*>(MemAlloc(samples.size() * sizeof(int16_t)));
    for (size_t i = 0; i < samples.size(); ++i) {
        copy[i] = samples[i];
    }
    w.data = copy;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

} // namespace

SoundEngine::SoundEngine() = default;

SoundEngine::~SoundEngine() {
    shutdown();
}

void SoundEngine::init(bool headless) {
    headless_ = headless;
    if (headless_) {
        initialized_ = false;
        return;
    }

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        initialized_ = false;
        return;
    }

    initialized_ = true;
    for (int i = 0; i < static_cast<int>(SfxId::COUNT); ++i) {
        sfx_[i] = generateSfxSound(static_cast<SfxId>(i));
    }
    strategicTheme_ = generateMarchLoop(false);
    combatTheme_ = generateMarchLoop(true);
}

void SoundEngine::shutdown() {
    if (!initialized_) return;
    for (auto& s : sfx_) {
        if (s.frameCount > 0) UnloadSound(s);
        s = {};
    }
    if (strategicTheme_.frameCount > 0) UnloadSound(strategicTheme_);
    if (combatTheme_.frameCount > 0) UnloadSound(combatTheme_);
    strategicTheme_ = {};
    combatTheme_ = {};
    CloseAudioDevice();
    initialized_ = false;
}

void SoundEngine::update(float dt, bool inCombat) {
    if (!initialized_ || headless_ || muted_ || !musicEnabled_) {
        if (initialized_) {
            if (IsSoundPlaying(strategicTheme_)) StopSound(strategicTheme_);
            if (IsSoundPlaying(combatTheme_)) StopSound(combatTheme_);
        }
        return;
    }

    // Dynamic AAA Orchestral Soundtrack Cross-Fader
    float targetIntensity = inCombat ? 1.0f : 0.0f;
    combatIntensity_ = std::lerp(combatIntensity_, targetIntensity, std::min(1.0f, dt * 1.8f));

    if (!IsSoundPlaying(strategicTheme_)) {
        PlaySound(strategicTheme_);
    }
    if (!IsSoundPlaying(combatTheme_)) {
        PlaySound(combatTheme_);
    }

    float stratVol = std::clamp(0.20f * (1.0f - combatIntensity_), 0.0f, 0.20f);
    float combatVol = std::clamp(0.26f * combatIntensity_, 0.0f, 0.26f);

    SetSoundVolume(strategicTheme_, stratVol);
    SetSoundVolume(combatTheme_, combatVol);

    musicTimer_ += dt;
}

void SoundEngine::setListener(Vector3 pos, Vector3 forward) {
    listenerPos_ = pos;
    float flen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (flen > 1e-4f) {
        listenerForward_ = { forward.x / flen, 0.0f, forward.z / flen };
    } else {
        listenerForward_ = { 0.0f, 0.0f, -1.0f };
    }
    // Perpendicular right vector in XZ plane
    listenerRight_ = { -listenerForward_.z, 0.0f, listenerForward_.x };
}

void SoundEngine::playSfx(SfxId id, float volume, float pitch) {
    if (!initialized_ || headless_ || muted_) return;
    size_t idx = static_cast<size_t>(id);
    if (idx >= sfx_.size()) return;
    SetSoundVolume(sfx_[idx], std::clamp(volume, 0.0f, 1.0f));
    SetSoundPitch(sfx_[idx], std::clamp(pitch, 0.5f, 2.0f));
    SetSoundPan(sfx_[idx], 0.5f); // Center stereo
    PlaySound(sfx_[idx]);
}

void SoundEngine::playSfx3D(SfxId id, Vector3 worldPos, float baseVolume, float pitch, float maxDist) {
    if (!initialized_ || headless_ || muted_) return;

    Vector3 toSource = { worldPos.x - listenerPos_.x, 0.0f, worldPos.z - listenerPos_.z };
    float dist = std::sqrt(toSource.x * toSource.x + toSource.z * toSource.z);
    if (dist > maxDist) return;

    // Realistic inverse-square acoustic attenuation with smooth roll-off
    float att = 1.0f / (1.0f + (dist / 55.0f) * (dist / 55.0f));
    float finalVol = std::clamp(baseVolume * att, 0.0f, 1.0f);
    if (finalVol < 0.02f) return;

    // Stereo panning based on source angle relative to listener orientation (0.0 = full left, 0.5 = center, 1.0 = full right)
    float pan = 0.5f;
    if (dist > 1.0f) {
        float relX = (toSource.x * listenerRight_.x + toSource.z * listenerRight_.z) / dist;
        pan = std::clamp(0.5f + 0.42f * relX, 0.06f, 0.94f);
    }

    size_t idx = static_cast<size_t>(id);
    if (idx >= sfx_.size()) return;

    SetSoundVolume(sfx_[idx], finalVol);
    SetSoundPitch(sfx_[idx], std::clamp(pitch, 0.5f, 2.0f));
    SetSoundPan(sfx_[idx], pan);
    PlaySound(sfx_[idx]);
}

Sound SoundEngine::generateSfxSound(SfxId id) {
    float duration = 0.25f;
    switch (id) {
        case SfxId::UI_CLICK:        duration = 0.06f; break;
        case SfxId::UI_CONFIRM:      duration = 0.22f; break;
        case SfxId::HQ_UNPACK:       duration = 0.55f; break;
        case SfxId::HQ_PACK:         duration = 0.45f; break;
        case SfxId::RIFLE_VOLLEY:    duration = 0.18f; break;
        case SfxId::MG_BURST:        duration = 0.28f; break;
        case SfxId::MORTAR_THUMP:    duration = 0.35f; break;
        case SfxId::TANK_CANNON:     duration = 0.50f; break;
        case SfxId::HEAVY_88_CANNON: duration = 0.65f; break;
        case SfxId::ARTILLERY_BOOM:  duration = 0.80f; break;
        case SfxId::KATYUSHA_SALVO:  duration = 0.95f; break;
        case SfxId::STUKA_SIREN:     duration = 1.10f; break;
        case SfxId::EXPLOSION_LARGE: duration = 0.90f; break;
        case SfxId::AIR_RAID_SIREN:  duration = 1.20f; break;
        case SfxId::FLAMETHROWER:    duration = 0.50f; break;
        case SfxId::BANZAI_HORN:     duration = 0.70f; break;
        case SfxId::DICE_ROLL:       duration = 0.35f; break;
        case SfxId::VICTORY_FANFARE: duration = 1.30f; break;
        case SfxId::RADIO_BEEP:          duration = 0.16f; break;
        case SfxId::RADIO_SQUELCH:       duration = 0.13f; break;
        case SfxId::RICOCHET:            duration = 0.40f; break;
        case SfxId::OUT_OF_SUPPLY_ALERT: duration = 0.35f; break;
        default:                         duration = 0.20f; break;
    }

    int count = static_cast<int>(duration * SAMPLE_RATE);
    std::vector<int16_t> pcm(count, 0);
    uint32_t seed = 1337u + static_cast<uint32_t>(id) * 997u;
    float lowpass = 0.0f;

    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float norm = static_cast<float>(i) / count;
        float env = std::exp(-norm * 4.5f);
        float noise = frandSym(seed);
        float sample = 0.0f;

        switch (id) {
            case SfxId::UI_CLICK:
                sample = std::sin(2.0f * PI * 920.0f * t) * std::exp(-norm * 12.0f);
                break;
            case SfxId::UI_CONFIRM: {
                float f = (norm < 0.45f) ? 523.25f : 659.25f;
                sample = (0.7f * std::sin(2.0f * PI * f * t) + 0.3f * std::sin(4.0f * PI * f * t)) * (1.0f - norm);
                break;
            }
            case SfxId::HQ_UNPACK: {
                float f = 140.0f + norm * 240.0f;
                sample = (0.6f * std::sin(2.0f * PI * f * t) + 0.4f * noise) * (1.0f - norm * 0.7f);
                break;
            }
            case SfxId::HQ_PACK: {
                float f = 340.0f - norm * 200.0f;
                sample = (0.6f * std::sin(2.0f * PI * f * t) + 0.4f * noise) * (1.0f - norm * 0.7f);
                break;
            }
            case SfxId::RIFLE_VOLLEY:
                lowpass = lowpass * 0.65f + noise * 0.35f;
                sample = (lowpass * 0.8f + std::sin(2.0f * PI * 210.0f * t) * 0.4f) * std::exp(-norm * 8.0f);
                break;
            case SfxId::MG_BURST: {
                float pulse = (std::fmod(t, 0.065f) < 0.028f) ? 1.0f : 0.1f;
                lowpass = lowpass * 0.6f + noise * 0.4f;
                sample = lowpass * pulse * 0.9f;
                break;
            }
            case SfxId::MORTAR_THUMP:
                sample = (std::sin(2.0f * PI * (160.0f * std::exp(-norm * 4.0f)) * t) * 0.8f + noise * 0.3f) * env;
                break;
            case SfxId::TANK_CANNON:
            case SfxId::HEAVY_88_CANNON: {
                float baseF = (id == SfxId::HEAVY_88_CANNON) ? 95.0f : 120.0f;
                lowpass = lowpass * 0.85f + noise * 0.15f;
                sample = (std::sin(2.0f * PI * baseF * std::exp(-norm * 3.0f) * t) * 0.75f + lowpass * 0.85f) * std::exp(-norm * 3.5f);
                break;
            }
            case SfxId::ARTILLERY_BOOM:
            case SfxId::EXPLOSION_LARGE:
                lowpass = lowpass * 0.90f + noise * 0.10f;
                sample = (lowpass * 1.1f + std::sin(2.0f * PI * 65.0f * t) * 0.5f) * std::exp(-norm * 2.6f);
                break;
            case SfxId::KATYUSHA_SALVO: {
                float scream = std::sin(2.0f * PI * (680.0f + 420.0f * std::sin(t * 18.0f) - norm * 300.0f) * t);
                lowpass = lowpass * 0.75f + noise * 0.25f;
                sample = (0.55f * scream + 0.55f * lowpass) * (1.0f - norm * 0.6f);
                break;
            }
            case SfxId::STUKA_SIREN: {
                float sirenF = 1150.0f - norm * 620.0f;
                sample = (0.7f * std::sin(2.0f * PI * sirenF * t) + 0.3f * std::sin(4.0f * PI * sirenF * t)) * (1.0f - norm * 0.4f);
                break;
            }
            case SfxId::AIR_RAID_SIREN: {
                float f = 420.0f + 260.0f * std::sin(PI * norm);
                sample = 0.7f * std::sin(2.0f * PI * f * t);
                break;
            }
            case SfxId::FLAMETHROWER:
                lowpass = lowpass * 0.82f + noise * 0.18f;
                sample = lowpass * (1.0f - norm * 0.5f);
                break;
            case SfxId::BANZAI_HORN: {
                float f = (norm < 0.4f) ? 392.0f : 523.25f;
                sample = (0.65f * std::sin(2.0f * PI * f * t) + 0.35f * std::sin(6.0f * PI * f * t)) * (1.0f - norm * 0.5f);
                break;
            }
            case SfxId::DICE_ROLL: {
                float tap = (std::fmod(t, 0.075f) < 0.018f) ? 1.0f : 0.05f;
                sample = (noise * 0.6f + std::sin(2.0f * PI * 800.0f * t) * 0.4f) * tap;
                break;
            }
            case SfxId::VICTORY_FANFARE: {
                float f = (norm < 0.25f) ? 392.0f : (norm < 0.50f ? 493.88f : (norm < 0.75f ? 587.33f : 783.99f));
                sample = (0.65f * std::sin(2.0f * PI * f * t) + 0.35f * std::sin(4.0f * PI * f * t)) * (1.0f - norm * 0.3f);
                break;
            }
            case SfxId::RADIO_BEEP:
                sample = std::sin(2.0f * PI * 1240.0f * t) * (1.0f - norm);
                break;
            case SfxId::RADIO_SQUELCH: {
                // Authentic WWII field radio push-to-talk release & squelch burst
                float staticHiss = (norm < 0.65f) ? (noise * (1.0f - norm / 0.65f) * 0.75f) : 0.0f;
                float chirpFreq = (norm < 0.32f) ? 1480.0f : (norm < 0.68f ? 1020.0f : 660.0f);
                float chirp = std::sin(2.0f * PI * chirpFreq * t) * std::exp(-norm * 4.8f);
                sample = (staticHiss * 0.7f + chirp * 0.5f) * (1.0f - norm);
                break;
            }
            case SfxId::RICOCHET: {
                // Ballistic armor deflection zing: sharp metallic crack followed by whining swept whistle
                float metallicCrack = (norm < 0.08f) ? (noise * 0.8f + std::sin(2.0f * PI * 2100.0f * t) * 0.7f) : 0.0f;
                float sweepFreq = 3400.0f * std::exp(-norm * 3.0f) + 380.0f * std::sin(t * 60.0f);
                float whine = std::sin(2.0f * PI * sweepFreq * t) * (1.0f - norm);
                sample = (metallicCrack * 0.75f + whine * 0.60f) * std::exp(-norm * 2.2f);
                break;
            }
            case SfxId::OUT_OF_SUPPLY_ALERT: {
                // Rapid dual-tone military buzzer alert
                float bPulse = (std::fmod(t, 0.075f) < 0.042f) ? 1.0f : 0.08f;
                float toneF = (norm < 0.5f) ? 820.0f : 540.0f;
                sample = (std::sin(2.0f * PI * toneF * t) * 0.75f + std::sin(4.0f * PI * toneF * t) * 0.25f) * bPulse * (1.0f - norm * 0.4f);
                break;
            }
            default:
                sample = noise * env;
                break;
        }

        sample = std::clamp(sample, -1.0f, 1.0f);
        pcm[i] = static_cast<int16_t>(sample * 24000.0f);
    }

    return makeSoundFromBuffer(pcm);
}

Sound SoundEngine::generateMarchLoop(bool combatTheme) {
    // 8-second loopable WWII orchestral brass & snare cadence
    constexpr float duration = 8.0f;
    int count = static_cast<int>(duration * SAMPLE_RATE);
    std::vector<int16_t> pcm(count, 0);

    const float melodyStrategic[16] = {
        196.00f, 196.00f, 261.63f, 293.66f,
        329.63f, 293.66f, 261.63f, 246.94f,
        220.00f, 261.63f, 293.66f, 329.63f,
        293.66f, 261.63f, 246.94f, 196.00f
    };
    const float melodyCombat[16] = {
        146.83f, 146.83f, 220.00f, 233.08f,
        220.00f, 196.00f, 174.61f, 164.81f,
        146.83f, 174.61f, 220.00f, 261.63f,
        233.08f, 220.00f, 174.61f, 146.83f
    };

    uint32_t seed = combatTheme ? 9001u : 4001u;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        int step = static_cast<int>(t * 2.0f) % 16;
        float stepTime = std::fmod(t * 2.0f, 1.0f);

        float freq = combatTheme ? melodyCombat[step] : melodyStrategic[step];
        float brassEnv = std::exp(-stepTime * 1.8f);
        float brass = (0.60f * std::sin(2.0f * PI * freq * t)
                     + 0.28f * std::sin(4.0f * PI * freq * t)
                     + 0.12f * std::sin(6.0f * PI * freq * t)) * brassEnv;

        float bassFreq = freq * 0.5f;
        float bass = 0.45f * std::sin(2.0f * PI * bassFreq * t) * std::exp(-stepTime * 2.4f);

        // Military snare drum cadence
        float beatSub = std::fmod(t * 4.0f, 1.0f);
        float snare = frandSym(seed) * std::exp(-beatSub * 9.0f) * 0.25f;

        float mix = (brass * 0.55f + bass * 0.35f + snare) * 0.65f;
        mix = std::clamp(mix, -1.0f, 1.0f);
        pcm[i] = static_cast<int16_t>(mix * 18000.0f);
    }

    return makeSoundFromBuffer(pcm);
}

} // namespace aa2004
