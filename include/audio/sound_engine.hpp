#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — PROCEDURAL C++ PCM AUDIO & SOUNDTRACK ENGINE
// Generates wartime brass/drum music & realistic weapon SFX in memory
// ============================================================================

#include "core/types.hpp"

namespace aa2004 {

enum class SfxId : int {
    UI_CLICK = 0,
    UI_CONFIRM,
    HQ_UNPACK,
    HQ_PACK,
    RIFLE_VOLLEY,
    MG_BURST,
    MORTAR_THUMP,
    TANK_CANNON,
    HEAVY_88_CANNON,
    ARTILLERY_BOOM,
    KATYUSHA_SALVO,
    STUKA_SIREN,
    EXPLOSION_LARGE,
    AIR_RAID_SIREN,
    FLAMETHROWER,
    BANZAI_HORN,
    DICE_ROLL,
    VICTORY_FANFARE,
    RADIO_BEEP,
    RADIO_SQUELCH,
    RICOCHET,
    OUT_OF_SUPPLY_ALERT,
    COUNT
};

class SoundEngine {
public:
    SoundEngine();
    ~SoundEngine();

    void init(bool headless = false);
    void shutdown();
    void update(float dt, bool inCombat = false);

    void playSfx(SfxId id, float volume = 0.85f, float pitch = 1.0f);
    void playSfx3D(SfxId id, Vector3 worldPos, float baseVolume = 0.85f, float pitch = 1.0f, float maxDist = 220.0f);
    void setListener(Vector3 pos, Vector3 forward);
    void setMuted(bool muted) { muted_ = muted; }
    bool isMuted() const { return muted_; }
    void toggleMute() { muted_ = !muted_; }
    void setMusicEnabled(bool enabled) { musicEnabled_ = enabled; }
    bool isMusicEnabled() const { return musicEnabled_; }

private:
    Sound generateSfxSound(SfxId id);
    Sound generateMarchLoop(bool combatTheme);

    bool initialized_ = false;
    bool headless_ = false;
    bool muted_ = false;
    bool musicEnabled_ = true;
    float musicTimer_ = 0.0f;
    float combatIntensity_ = 0.0f;

    Vector3 listenerPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 listenerForward_ = {0.0f, 0.0f, -1.0f};
    Vector3 listenerRight_ = {1.0f, 0.0f, 0.0f};

    std::array<Sound, static_cast<size_t>(SfxId::COUNT)> sfx_{};
    Sound strategicTheme_{};
    Sound combatTheme_{};
};

} // namespace aa2004
