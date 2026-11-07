// ============================================================================
// AXIS & ALLIES (2004) — REAL-TIME STRATEGY (RTS) SIMULATION ENGINE
// Authentic TimeGate Studios 2004 Mechanics:
// - Mobile Pack/Unpack Corps HQ, Division HQs & Supply Depots
// - Supply Line Network & Organization/Morale Bar
// - Money / Ammo / Oil Economy
// - Multi-Entity Regimental Formations, Stances, Entrenchment & Ballistics
// - 20 Historical Generals & Active Special Operations
// ============================================================================

#include "rts/rts_engine.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <chrono>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace aa2004 {

namespace {

float distXZ(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

RTSEngine::RTSEngine() = default;

bool RTSEngine::isEnemy(int teamA, int teamB) const {
    if (teamA < 0 || teamB < 0) return false;
    if (teamA >= numPlayers_ || teamB >= numPlayers_) return false;
    if (teamA == teamB) return false;
    return players_[teamA].allianceId != players_[teamB].allianceId;
}

bool RTSEngine::isAllied(int teamA, int teamB) const {
    if (teamA < 0 || teamB < 0) return false;
    if (teamA >= numPlayers_ || teamB >= numPlayers_) return false;
    if (teamA == teamB) return true;
    return players_[teamA].allianceId == players_[teamB].allianceId;
}

float RTSEngine::getTerrainHeight(float x, float z) const {
    float h = hillAmp_ * std::sin(x * hillFreqA_ + hillPhaseX_) * std::cos(z * hillFreqB_ + hillPhaseZ_);
    if (config_.biome == BiomeType::EL_ALAMEIN_DESERT) {
        h += 0.65f * std::sin((x + z) * 0.025f + hillPhaseX_ * 0.5f);
    } else if (config_.biome == BiomeType::ARDENNES_WINTER) {
        h += 0.85f * std::cos(x * 0.04f + hillPhaseZ_ * 0.7f) * std::sin(z * 0.03f + hillPhaseX_ * 0.7f);
    }
    return std::max(0.0f, h);
}

bool RTSEngine::hasRiver() const {
    // Only temperate & winter European battlefields feature strategic rivers.
    // Arid desert biomes (e.g. El Alamein) have zero rivers!
    return config_.biome != BiomeType::EL_ALAMEIN_DESERT;
}

bool RTSEngine::isBridgeAt(float x, float z) const {
    if (!hasRiver()) return false;
    if (std::fabs(z) > 18.0f) return false;
    // 3 Fortified Stone/Steel River Bridges: Center (x=0), West (x=-240), East (x=+240)
    if (std::fabs(x) <= 10.5f) return true;
    if (std::fabs(x + 240.0f) <= 9.0f) return true;
    if (std::fabs(x - 240.0f) <= 9.0f) return true;
    return false;
}

bool RTSEngine::isWaterAt(float x, float z) const {
    if (!hasRiver()) return false;
    if (isBridgeAt(x, z)) return false;
    // 24m Central Strategic River Channel
    return std::fabs(z) < 12.0f;
}

int RTSEngine::getControlledTownCount(int teamId) const {
    if (teamId < 0 || teamId >= numPlayers_) return 0;
    return players_[teamId].controlledTowns;
}

float RTSEngine::getTownSynergyMultiplier(int teamId) const {
    if (teamId < 0 || teamId >= numPlayers_) return 1.0f;
    return players_[teamId].townSynergyMult;
}

int RTSEngine::getControlledOilCount(int teamId) const {
    if (teamId < 0 || teamId >= numPlayers_) return 0;
    return players_[teamId].controlledOil;
}

float RTSEngine::getOilSynergyMultiplier(int teamId) const {
    if (teamId < 0 || teamId >= numPlayers_) return 1.0f;
    return players_[teamId].oilSynergyMult;
}

void RTSEngine::pushLog(const std::string& msg) {
    combatLog_.push_back(msg);
    if (combatLog_.size() > 8) {
        combatLog_.erase(combatLog_.begin());
    }
}

float RTSEngine::getBuildingVisionRadius(BuildingType type) const {
    switch (type) {
        case BuildingType::CORPS_HQ:     return 92.0f;
        case BuildingType::CITY_DEPOT:   return 82.0f; // Capturing a Town grants a large 82m vision radius!
        case BuildingType::OIL_DERRICK:  return 68.0f;
        case BuildingType::SUPPLY_DEPOT: return 74.0f;
        case BuildingType::INFANTRY_HQ:
        case BuildingType::MECH_HQ:
        case BuildingType::ARMOR_HQ:
        case BuildingType::AIRFIELD:     return 72.0f;
        case BuildingType::BUNKER:
        case BuildingType::AA_BATTERY:   return 70.0f;
        default:                         return 65.0f;
    }
}

float RTSEngine::getRegimentVisionRadius(RegimentType type) const {
    if (type == RegimentType::RECON_HALFTRACK || type == RegimentType::SNIPER_REGIMENT) {
        return 90.0f; // Dedicated Recon scouts have long-range 90m line-of-sight
    }
    if (type == RegimentType::PARATROOP_REGIMENT) {
        return 70.0f;
    }
    const auto& def = GameData::instance().getRegiment(type);
    return std::max(58.0f, def.visionRange * 1.25f);
}

void RTSEngine::rebuildVisionCache() const {
    for (int t = 0; t < numPlayers_; ++t) {
        teamVisionCache_[t].clear();
        teamVisionCache_[t].reserve(buildings_.size() + regiments_.size() + airStrikes_.size() * 2);
        teamVisionSoA_[t].x.clear();
        teamVisionSoA_[t].z.clear();
        teamVisionSoA_[t].radiusSq.clear();
        teamVisionSoA_[t].x.reserve(buildings_.size() + regiments_.size() + airStrikes_.size() * 2 + 8);
        teamVisionSoA_[t].z.reserve(buildings_.size() + regiments_.size() + airStrikes_.size() * 2 + 8);
        teamVisionSoA_[t].radiusSq.reserve(buildings_.size() + regiments_.size() + airStrikes_.size() * 2 + 8);
    }

    auto addSource = [&](int teamId, float x, float z, float rSq) {
        if (teamId >= 0 && teamId < numPlayers_) {
            teamVisionCache_[teamId].push_back({x, z, rSq});
            teamVisionSoA_[teamId].x.push_back(x);
            teamVisionSoA_[teamId].z.push_back(z);
            teamVisionSoA_[teamId].radiusSq.push_back(rSq);
        }
    };

    for (const auto& b : buildings_) {
        if (b.health <= 0.0f) continue;
        float vr = getBuildingVisionRadius(b.type);
        addSource(b.teamId, b.pos.x, b.pos.z, vr * vr);
    }
    for (const auto& r : regiments_) {
        if (r.health <= 0.0f) continue;
        float vr = getRegimentVisionRadius(r.type);
        addSource(r.teamId, r.pos.x, r.pos.z, vr * vr);
    }
    for (const auto& as : airStrikes_) {
        if (as.health <= 0.0f) continue;
        addSource(as.teamId, as.currentPos.x, as.currentPos.z, 78.0f * 78.0f);
        addSource(as.teamId, as.targetPos.x, as.targetPos.z, 78.0f * 78.0f);
    }

    // Pad SoA arrays with dummy non-matching entries to multiple of 8 for AVX2 256-bit SIMD loads
    for (int t = 0; t < numPlayers_; ++t) {
        size_t rem = teamVisionSoA_[t].x.size() % 8;
        if (rem != 0) {
            size_t pad = 8 - rem;
            for (size_t p = 0; p < pad; ++p) {
                teamVisionSoA_[t].x.push_back(1e9f);
                teamVisionSoA_[t].z.push_back(1e9f);
                teamVisionSoA_[t].radiusSq.push_back(0.0f);
            }
        }
    }

    visionCacheValid_ = true;
}

bool RTSEngine::isPosVisibleToTeam(Vector3 pos, int teamId) const {
    if (revealAllShowcase_) return true;
    if (teamId < 0 || teamId >= numPlayers_) return false;

    if (!visionCacheValid_) {
        rebuildVisionCache();
    }

    int alliance = players_[teamId].allianceId;
    for (int t = 0; t < numPlayers_; ++t) {
        if (players_[t].allianceId != alliance) continue;
        if (players_[t].radarRevealTimer > 0.0f) return true;

#if defined(__AVX2__)
        const auto& soa = teamVisionSoA_[t];
        size_t count = soa.x.size();
        if (count > 0) {
            __m256 px = _mm256_set1_ps(pos.x);
            __m256 pz = _mm256_set1_ps(pos.z);
            const float* xPtr = soa.x.data();
            const float* zPtr = soa.z.data();
            const float* rPtr = soa.radiusSq.data();

            for (size_t i = 0; i < count; i += 8) {
                __m256 vx = _mm256_loadu_ps(xPtr + i);
                __m256 vz = _mm256_loadu_ps(zPtr + i);
                __m256 vr = _mm256_loadu_ps(rPtr + i);

                __m256 dx = _mm256_sub_ps(px, vx);
                __m256 dz = _mm256_sub_ps(pz, vz);

                // distSq = dx * dx + dz * dz using FMA
                __m256 dsq = _mm256_mul_ps(dx, dx);
                dsq = _mm256_fmadd_ps(dz, dz, dsq);

                // Compare: dsq <= vr -> within vision radius
                __m256 cmp = _mm256_cmp_ps(dsq, vr, _CMP_LE_OQ);
                int mask = _mm256_movemask_ps(cmp);
                if (mask != 0) {
                    return true;
                }
            }
            continue;
        }
#endif

        for (const auto& s : teamVisionCache_[t]) {
            float dx = pos.x - s.x;
            float dz = pos.z - s.z;
            if (dx * dx + dz * dz <= s.radiusSq) {
                return true;
            }
        }
    }
    return false;
}

bool RTSEngine::isPosExploredByPlayer(Vector3 pos) const {
    if (revealAllShowcase_ || players_[0].radarRevealTimer > 0.0f) return true;
    float nx = (pos.x + kMapHalfSpan) / (2.0f * kMapHalfSpan);
    float nz = (pos.z + kMapHalfSpan) / (2.0f * kMapHalfSpan);
    int gx = std::clamp(static_cast<int>(nx * kFogGridSize), 0, kFogGridSize - 1);
    int gz = std::clamp(static_cast<int>(nz * kFogGridSize), 0, kFogGridSize - 1);
    return fogGrid_[gz * kFogGridSize + gx] >= 1;
}

void RTSEngine::updateFogOfWar() {
    if (revealAllShowcase_ || players_[0].radarRevealTimer > 0.0f) {
        fogGrid_.fill(2);
        return;
    }
    // Downgrade previously visible cells (2) to explored fog (1); keep unexplored (0) as 0
    for (auto& cell : fogGrid_) {
        if (cell == 2) cell = 1;
    }

    float cellSpan = (2.0f * kMapHalfSpan) / kFogGridSize;
    auto revealCircle = [&](Vector3 center, float radius) {
        int minX = std::clamp(static_cast<int>(((center.x - radius) + kMapHalfSpan) / cellSpan), 0, kFogGridSize - 1);
        int maxX = std::clamp(static_cast<int>(((center.x + radius) + kMapHalfSpan) / cellSpan), 0, kFogGridSize - 1);
        int minZ = std::clamp(static_cast<int>(((center.z - radius) + kMapHalfSpan) / cellSpan), 0, kFogGridSize - 1);
        int maxZ = std::clamp(static_cast<int>(((center.z + radius) + kMapHalfSpan) / cellSpan), 0, kFogGridSize - 1);
        float rSq = radius * radius;
        for (int gz = minZ; gz <= maxZ; ++gz) {
            float cz = -kMapHalfSpan + (gz + 0.5f) * cellSpan;
            for (int gx = minX; gx <= maxX; ++gx) {
                float cx = -kMapHalfSpan + (gx + 0.5f) * cellSpan;
                float dx = cx - center.x;
                float dz = cz - center.z;
                if (dx * dx + dz * dz <= rSq) {
                    fogGrid_[gz * kFogGridSize + gx] = 2;
                }
            }
        }
    };

    for (const auto& b : buildings_) {
        if (b.health > 0.0f && isAllied(0, b.teamId)) {
            revealCircle(b.pos, getBuildingVisionRadius(b.type));
        }
    }
    for (const auto& r : regiments_) {
        if (r.health > 0.0f && isAllied(0, r.teamId)) {
            revealCircle(r.pos, getRegimentVisionRadius(r.type));
        }
    }
    for (const auto& as : airStrikes_) {
        if (isAllied(0, as.teamId) && !as.isShotDown) {
            revealCircle(as.currentPos, as.isRecon ? 85.0f : 68.0f);
            if (as.isRecon && as.droppedPayload) {
                revealCircle(as.targetPos, 110.0f);
            }
        }
    }
}

void RTSEngine::startBattle(const RTSBattleConfig& cfg, SoundEngine* sound) {
    config_ = cfg;
    sound_ = sound;
    result_ = {};
    result_.fromWW2Mode = cfg.fromWW2Mode;
    result_.contestedTerritoryId = cfg.contestedTerritoryId;
    result_.campaignMissionId = cfg.campaignMissionId;

    buildings_.clear();
    regiments_.clear();
    projectiles_.clear();
    particles_.clear();
    airStrikes_.clear();
    props_.clear();
    combatLog_.clear();
    fogGrid_.fill(0);
    revealAllShowcase_ = false;
    hasWarnedRiverCrossing_ = false;

    nextEntityId_ = 1;
    battleTime_ = 0.0f;
    supplyTickTimer_ = 0.0f;
    economyTickTimer_ = 0.0f;
    paused_ = false;
    pendingBuildType_ = -1;
    pendingSpecOpIdx_ = -1;
    pendingAirMissionType_ = -1;
    aiAirMissionTimer_ = 0.0f;
    alliedPingPos_ = {0.0f, 0.0f, 0.0f};
    alliedPingTimer_ = 0.0f;

    numPlayers_ = std::clamp(cfg.numPlayers, 2, kMaxPlayers);

    // Initialize Alliances & Players
    float aiMult = (cfg.aiDifficulty == 0) ? 0.85f : (cfg.aiDifficulty == 1 ? 1.05f : 1.35f);

    players_[0] = {};
    players_[0].teamId = 0;
    players_[0].allianceId = 0;
    players_[0].nation = cfg.playerNation;
    players_[0].generalIdx = cfg.playerGeneralIdx;
    players_[0].isAI = false;
    players_[0].money = static_cast<float>(cfg.startingMoney);
    players_[0].ammo = static_cast<float>(cfg.startingAmmo);
    players_[0].oil = static_cast<float>(cfg.startingOil);
    players_[0].commandPoints = 90.0f;
    players_[0].techTier = 1;

    players_[1] = {};
    players_[1].teamId = 1;
    players_[1].allianceId = 1;
    players_[1].nation = cfg.enemyNation;
    players_[1].generalIdx = cfg.enemyGeneralIdx;
    players_[1].isAI = true;
    players_[1].aiDifficulty = cfg.aiDifficulty;
    players_[1].money = cfg.startingMoney * aiMult;
    players_[1].ammo = cfg.startingAmmo * aiMult;
    players_[1].oil = cfg.startingOil * aiMult;
    players_[1].commandPoints = 80.0f;
    players_[1].techTier = 1;

    for (int i = 2; i < numPlayers_; ++i) {
        players_[i] = {};
        players_[i].teamId = i;
        size_t extraIdx = static_cast<size_t>(i - 2);
        players_[i].nation = (extraIdx < cfg.extraNations.size()) ? cfg.extraNations[extraIdx] : NationId::GER;
        players_[i].generalIdx = (extraIdx < cfg.extraGeneralIdxs.size()) ? cfg.extraGeneralIdxs[extraIdx] : 0;
        players_[i].isAI = true;
        players_[i].aiDifficulty = cfg.aiDifficulty;
        players_[i].money = cfg.startingMoney * aiMult;
        players_[i].ammo = cfg.startingAmmo * aiMult;
        players_[i].oil = cfg.startingOil * aiMult;
        players_[i].commandPoints = 80.0f;
        players_[i].techTier = 1;

        if (cfg.gameMode == BattleGameMode::ALLIED_2V2) {
            // In 2v2: Team 0 & Team 2 are Allies (Alliance 0); Team 1 & Team 3 are Axis (Alliance 1)
            players_[i].allianceId = (i == 2) ? 0 : 1;
        } else {
            // Free-For-All modes: Each faction is its own alliance
            players_[i].allianceId = i;
        }
    }

    // ========================================================================
    // Procedural Map & Terrain Generation (Randomized per match)
    // Rolling hills, valleys, and unpredictable tactical base sectors.
    // Base separation constraint: distance between player and any enemy >= 650m.
    // ========================================================================
    if (cfg.campaignMissionId >= 0) {
        mapSeed_ = 2004u + static_cast<uint32_t>(cfg.campaignMissionId) * 100u;
    } else if (cfg.mapSeed != 0) {
        mapSeed_ = cfg.mapSeed;
    } else {
        std::random_device rd;
        uint32_t t = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        mapSeed_ = (static_cast<uint32_t>(rd()) ^ t);
        if (mapSeed_ == 0) mapSeed_ = 1001u;
    }

    hillAmp_ = 0.35f + ((mapSeed_ % 9) * 0.045f);
    hillFreqA_ = 0.026f + ((mapSeed_ % 7) * 0.0025f);
    hillFreqB_ = 0.028f + (((mapSeed_ / 7) % 7) * 0.0025f);
    hillPhaseX_ = ((mapSeed_ % 67) / 67.0f) * 6.2831853f;
    hillPhaseZ_ = (((mapSeed_ / 67) % 67) / 67.0f) * 6.2831853f;

    float orientationAngle = (((mapSeed_ % 3600) / 3600.0f) * 6.2831853f);

    playerBasePositions_.resize(numPlayers_);
    if (numPlayers_ == 2) {
        // 1v1 Skirmish: Completely unpredictable base sectors across 360 degrees!
        // Player base can be anywhere around the map perimeter [0, 2PI)
        float pAngle = orientationAngle;
        float pDist  = 430.0f + (static_cast<int>((mapSeed_ / 11) % 25) - 12) * 2.0f; // 406m - 454m

        // Enemy base is placed in the opposite hemisphere with dynamic angular jitter (-50 deg to +50 deg)
        // Guaranteed separation >= 650m (approx 735m - 900m) while making the enemy position completely unpredictable!
        float jitter = ((static_cast<int>((mapSeed_ / 31) % 100) / 100.0f) - 0.5f) * 1.745f; // +/- 50 degrees
        float eAngle = pAngle + 3.14159265f + jitter;
        float eDist  = 430.0f + (static_cast<int>((mapSeed_ / 47) % 25) - 12) * 2.0f; // 406m - 454m

        playerBasePositions_[0] = { std::cos(pAngle) * pDist, 0.0f, std::sin(pAngle) * pDist };
        playerBasePositions_[1] = { std::cos(eAngle) * eDist, 0.0f, std::sin(eAngle) * eDist };
    } else if (numPlayers_ == 3) {
        // 3-Player Survival FFA: Randomized equilateral triangle
        float rot = orientationAngle;
        int slotOffset = static_cast<int>(mapSeed_ % 3);
        for (int i = 0; i < 3; ++i) {
            int slot = (i + slotOffset) % 3;
            float ang = rot + slot * (2.0f * 3.14159265f / 3.0f);
            float dist = 445.0f + (static_cast<int>((mapSeed_ / (i + 1)) % 15) - 7) * 2.0f;
            playerBasePositions_[i] = { std::cos(ang) * dist, 0.0f, std::sin(ang) * dist };
        }
    } else {
        if (cfg.gameMode == BattleGameMode::ALLIED_2V2) {
            // 2v2 Allied: Random confrontation axis across the map (East-West, North-South, Diagonal)
            float axisAngle = orientationAngle;
            float perpAngle = axisAngle + 1.5707963f;
            Vector3 forwardAxis = { std::cos(axisAngle), 0.0f, std::sin(axisAngle) };
            Vector3 perpAxis    = { std::cos(perpAngle), 0.0f, std::sin(perpAngle) };

            float baseDist = 425.0f;
            float teamSpread = 240.0f;

            // Allies (Player 0, Ally 2)
            playerBasePositions_[0] = Vector3Add(Vector3Scale(forwardAxis, baseDist), Vector3Scale(perpAxis,  teamSpread));
            playerBasePositions_[2] = Vector3Add(Vector3Scale(forwardAxis, baseDist), Vector3Scale(perpAxis, -teamSpread));

            // Axis (Enemy 1, Enemy 3)
            playerBasePositions_[1] = Vector3Add(Vector3Scale(forwardAxis, -baseDist), Vector3Scale(perpAxis,  teamSpread));
            playerBasePositions_[3] = Vector3Add(Vector3Scale(forwardAxis, -baseDist), Vector3Scale(perpAxis, -teamSpread));
        } else {
            // 4-Player FFA: 4 quadrant bases rotated by orientationAngle
            float rot = orientationAngle + 0.785398f;
            int slotOffset = static_cast<int>(mapSeed_ % 4);
            for (int i = 0; i < 4; ++i) {
                int slot = (i + slotOffset) % 4;
                float ang = rot + slot * 1.5707963f;
                float dist = 460.0f;
                playerBasePositions_[i] = { std::cos(ang) * dist, 0.0f, std::sin(ang) * dist };
            }
        }
    }

    // Safety check: ensure no base spawns in river water
    for (int i = 0; i < numPlayers_; ++i) {
        while (isWaterAt(playerBasePositions_[i].x, playerBasePositions_[i].z)) {
            playerBasePositions_[i].x += 20.0f;
            playerBasePositions_[i].z += 20.0f;
        }
    }

    playerBasePos_ = playerBasePositions_[0];
    enemyBasePos_ = playerBasePositions_[1];

    // Spawn Starting Bases (Corps HQ & Supply Depot) & Starting Units for all active factions
    for (int i = 0; i < numPlayers_; ++i) {
        Vector3 bPos = playerBasePositions_[i];
        int corpsId = spawnBuilding(i, BuildingType::CORPS_HQ, bPos);
        Vector3 toCenter = Vector3Normalize({ -bPos.x, 0.0f, -bPos.z });
        Vector3 depotPos = { bPos.x + toCenter.x * 32.0f, 0.0f, bPos.z + toCenter.z * 32.0f };
        spawnBuilding(i, BuildingType::SUPPLY_DEPOT, depotPos);

        if (i == 0) {
            for (auto& b : buildings_) {
                if (b.id == corpsId) b.selected = true;
            }
        }

        Vector3 perp = { -toCenter.z, 0.0f, toCenter.x };
        spawnRegiment(i, RegimentType::RECON_HALFTRACK, { bPos.x + toCenter.x * 24.0f + perp.x * 12.0f, 0.0f, bPos.z + toCenter.z * 24.0f + perp.z * 12.0f });
        spawnRegiment(i, RegimentType::RIFLE_REGIMENT,  { bPos.x + toCenter.x * 24.0f - perp.x * 12.0f, 0.0f, bPos.z + toCenter.z * 24.0f - perp.z * 12.0f });
    }

    // Rotate procedural towns and derricks according to orientationAngle
    auto rotateXZ = [&](float x, float z, float rad) -> Vector3 {
        float c = std::cos(rad);
        float s = std::sin(rad);
        return { x * c - z * s, 0.0f, x * s + z * c };
    };

    // 10 Capturable Strategic Towns (each grants +82m Vision Radius & Field Hospital Healing)
    float tjx = (static_cast<int>(mapSeed_ % 13) - 6) * 2.5f;
    float tjz = (static_cast<int>((mapSeed_ / 13) % 13) - 6) * 2.5f;
    Vector3 townCoords[10] = {
        rotateXZ(-185.0f + tjx,  275.0f + tjz, orientationAngle),
        rotateXZ(   0.0f + tjx,  240.0f + tjz, orientationAngle),
        rotateXZ( 185.0f + tjx,  275.0f + tjz, orientationAngle),
        rotateXZ(-240.0f,          72.0f,      orientationAngle),
        rotateXZ(   0.0f,          68.0f,      orientationAngle),
        rotateXZ(   0.0f,         -68.0f,      orientationAngle),
        rotateXZ( 240.0f,         -72.0f,      orientationAngle),
        rotateXZ(-185.0f - tjx, -275.0f - tjz, orientationAngle),
        rotateXZ(   0.0f - tjx, -240.0f - tjz, orientationAngle),
        rotateXZ( 185.0f - tjx, -275.0f - tjz, orientationAngle)
    };

    for (int t = 0; t < 10; ++t) {
        Vector3 tp = townCoords[t];
        while (isWaterAt(tp.x, tp.z)) {
            tp.x += 16.0f;
            tp.z += 16.0f;
        }
        spawnBuilding(-1, BuildingType::CITY_DEPOT, tp);
    }

    // 8 Capturable Oil Derricks (each grants +68m Vision Radius & Oil Income)
    Vector3 oilCoords[8] = {
        rotateXZ(-165.0f + tjx,  365.0f + tjz, orientationAngle),
        rotateXZ( 165.0f + tjx,  365.0f + tjz, orientationAngle),
        rotateXZ(-330.0f + tjx,  150.0f + tjz, orientationAngle),
        rotateXZ( 330.0f + tjx,  150.0f + tjz, orientationAngle),
        rotateXZ(-330.0f - tjx, -150.0f - tjz, orientationAngle),
        rotateXZ( 330.0f - tjx, -150.0f - tjz, orientationAngle),
        rotateXZ(-165.0f - tjx, -365.0f - tjz, orientationAngle),
        rotateXZ( 165.0f - tjx, -365.0f - tjz, orientationAngle)
    };

    for (int o = 0; o < 8; ++o) {
        Vector3 op = oilCoords[o];
        while (isWaterAt(op.x, op.z)) {
            op.x += 16.0f;
            op.z += 16.0f;
        }
        spawnBuilding(-1, BuildingType::OIL_DERRICK, op);
    }

    // Generate Decorative 3D Battlefield Props across the expanded 1120x1120 map
    uint32_t seed = 2004u + static_cast<uint32_t>(cfg.biome) * 77u;
    for (int i = 0; i < 580; ++i) {
        seed = seed * 1664525u + 1013904223u;
        float px = ((seed & 0xFFFF) / 65535.0f) * 1040.0f - 520.0f;
        seed = seed * 1664525u + 1013904223u;
        float pz = ((seed & 0xFFFF) / 65535.0f) * 1040.0f - 520.0f;
        // Avoid water river channels and main road centers
        if (std::fabs(pz) < 7.5f || std::fabs(px) < 11.0f || std::fabs(std::fabs(px) - 240.0f) < 12.0f) continue;

        bool nearBldg = false;
        for (const auto& b : buildings_) {
            if (distXZ({px, 0.0f, pz}, b.pos) < 13.5f) {
                nearBldg = true;
                break;
            }
        }
        if (nearBldg) continue;

        TerrainProp p{};
        p.pos = {px, getTerrainHeight(px, pz), pz};
        p.scale = 0.85f + ((seed >> 16) & 0xFF) / 512.0f;
        p.rotation = static_cast<float>(seed % 360);
        if (cfg.biome == BiomeType::EL_ALAMEIN_DESERT) {
            int m = i % 7;
            if (m == 0) p.propType = 2; // desert palm
            else if (m == 1 || m == 2) p.propType = 3; // rock formation
            else if (m == 3 || m == 4) p.propType = 7; // sandbag checkpoint
            else p.propType = 8; // Czech hedgehog
        } else if (cfg.biome == BiomeType::PACIFIC_ATOLL) {
            int m = i % 6;
            if (m <= 2) p.propType = 2; // palm
            else if (m == 3) p.propType = 3; // rock
            else if (m == 4) p.propType = 7; // sandbag checkpoint
            else p.propType = 8; // Czech hedgehog
        } else if (cfg.biome == BiomeType::STALINGRAD_WINTER || cfg.biome == BiomeType::BERLIN_RUINS) {
            int m = i % 7;
            if (m == 0) p.propType = 4; // ruined stone wall
            else if (m == 1) p.propType = 8; // Czech hedgehog
            else if (m == 2) p.propType = 7; // sandbag checkpoint
            else if (m == 3) p.propType = 1; // winter pine
            else if (m == 4) p.propType = 3; // rock formation
            else p.propType = 6; // telephone pole
        } else if (cfg.biome == BiomeType::ARDENNES_WINTER) {
            int m = i % 7;
            if (m <= 2) p.propType = 1; // heavy winter pine
            else if (m == 3) p.propType = 4; // ruined stone wall
            else if (m == 4) p.propType = 8; // Czech hedgehog
            else if (m == 5) p.propType = 3; // rock formation
            else p.propType = 7; // sandbag checkpoint
        } else {
            // Western Europe / Normandy: lush oaks, bocage hedgerows, stone walls, telephone poles, sandbags
            int m = i % 8;
            if (m <= 2) p.propType = 0; // lush deciduous oak
            else if (m == 3 || m == 4) p.propType = 5; // Normandy bocage hedge
            else if (m == 5) p.propType = 4; // ruined stone wall
            else if (m == 6) p.propType = 6; // telephone pole
            else p.propType = 7; // sandbag checkpoint
        }
        props_.push_back(p);
    }

    // Flank the main military highway with authentic utility/telephone poles
    for (float tz = -460.0f; tz <= 460.0f; tz += 36.0f) {
        if (std::fabs(tz) < 32.0f) continue; // skip river channel crossing
        TerrainProp pole{};
        pole.pos = {13.0f, getTerrainHeight(13.0f, tz), tz};
        pole.scale = 1.0f;
        pole.rotation = 0.0f;
        pole.propType = 6; // telephone pole
        props_.push_back(pole);
    }

    updateSupplyNetwork();
    updateFogOfWar();
    const auto& pNat = GameData::instance().getNation(cfg.playerNation);
    const auto& pGen = GameData::instance().getGeneral(cfg.playerNation, cfg.playerGeneralIdx);
    pushLog("HQ RADIO: " + pGen.name + " (" + pNat.name + ") has established Corps HQ in the South.");
    pushLog("RECON INTEL: Build Division HQs, scout with Recon, and capture Towns to expand Vision & Supply!");
}

int RTSEngine::spawnBuilding(int teamId, BuildingType type, Vector3 pos, bool underConstruction) {
    const auto& gd = GameData::instance();
    const auto& def = gd.getBuilding(type);

    RTSBuilding b{};
    b.id = nextEntityId_++;
    b.teamId = teamId;
    b.nation = (teamId >= 0 && teamId < numPlayers_) ? players_[teamId].nation : NationId::USA;
    b.type = type;
    b.pos = {pos.x, getTerrainHeight(pos.x, pos.z), pos.z};
    Vector3 toCenter = Vector3Normalize({ -pos.x, 0.0f, -pos.z });
    b.facing = std::atan2(toCenter.x, toCenter.z) * 57.29578f;

    float hpMult = 1.0f;
    float supplyMult = 1.0f;
    if (teamId >= 0 && teamId < numPlayers_) {
        const auto& gen = gd.getGeneral(players_[teamId].nation, players_[teamId].generalIdx);
        hpMult = gen.bunkerHealthMult;
        supplyMult = gen.supplyRadiusMult;
    }

    b.maxHealth = def.maxHealth * hpMult;
    b.supplyRadius = def.supplyRadius * supplyMult;
    b.inSupply = (type == BuildingType::CORPS_HQ || teamId >= 0);
    b.rallyPoint = {b.pos.x + toCenter.x * 20.0f, b.pos.y, b.pos.z + toCenter.z * 20.0f};
    if (teamId >= 0 && teamId < numPlayers_) {
        b.captureProgress = (players_[teamId].allianceId == 0) ? 100.0f : -100.0f;
    } else {
        b.captureProgress = 0.0f;
    }

    if (underConstruction && def.buildTime > 0.0f) {
        b.isUnderConstruction = true;
        b.constructProgress = 0.0f;
        b.constructTotalTime = def.buildTime;
        b.health = b.maxHealth * 0.15f; // Structural scaffolding initial integrity
        b.inSupply = false;
    } else {
        b.isUnderConstruction = false;
        b.constructProgress = 1.0f;
        b.constructTotalTime = 0.0f;
        b.health = b.maxHealth;
    }

    buildings_.push_back(b);
    return b.id;
}

int RTSEngine::spawnRegiment(int teamId, RegimentType type, Vector3 pos, BuildingType packedBldg) {
    const auto& gd = GameData::instance();
    const auto& def = gd.getRegiment(type);

    RTSRegiment r{};
    r.id = nextEntityId_++;
    r.teamId = teamId;
    r.nation = (teamId >= 0 && teamId < numPlayers_) ? players_[teamId].nation : NationId::USA;
    r.type = type;
    r.packedBuildingType = packedBldg;
    r.stance = FormationStance::LINE;
    r.pos = {pos.x, getTerrainHeight(pos.x, pos.z), pos.z};
    r.targetPos = r.pos;
    r.facing = (teamId == 0) ? 180.0f : 0.0f;
    r.maxHealth = def.maxHealth;
    r.health = r.maxHealth;
    r.maxOrganization = def.maxOrganization;
    r.organization = r.maxOrganization;
    r.inSupply = true;
    r.supplyBuffer = 75.0f;
    r.maxSupplyBuffer = 75.0f;

    int count = std::max(1, def.squadCount);
    r.subUnits.resize(count);
    for (int i = 0; i < count; ++i) {
        RTSSubUnit su{};
        if (count == 1) {
            su.localOffset = {0.0f, 0.0f, 0.0f};
        } else if (count == 2) {
            su.localOffset = {(i == 0 ? -2.2f : 2.2f), 0.0f, 0.0f};
        } else if (count == 3) {
            float ox = (i - 1) * 2.8f;
            float oz = (i == 1) ? -1.4f : 1.2f;
            su.localOffset = {ox, 0.0f, oz};
        } else {
            int row = i / 3;
            int col = i % 3;
            su.localOffset = {(col - 1) * 1.9f, 0.0f, (row == 0 ? -1.1f : 1.1f)};
        }
        su.worldPos = Vector3Add(r.pos, su.localOffset);
        su.turretYaw = r.facing;
        su.animPhase = i * 1.1f;
        su.alive = true;
        r.subUnits[i] = su;
    }

    regiments_.push_back(r);
    return r.id;
}

void RTSEngine::update(float dt) {
    if (paused_ || result_.finished) return;
    invalidateVisionCache();
    float simDt = dt * gameSpeed_;
    battleTime_ += simDt;
    result_.durationSec = battleTime_;

    supplyTickTimer_ += simDt;
    if (supplyTickTimer_ >= 0.4f) {
        supplyTickTimer_ = 0.0f;
        updateSupplyNetwork();
    }

    if (alliedPingTimer_ > 0.0f) {
        alliedPingTimer_ = std::max(0.0f, alliedPingTimer_ - simDt);
    }

    updateEconomy(simDt);
    updateBuildings(simDt);
    updateRegiments(simDt);
    updateProjectiles(simDt);
    updateAirStrikes(simDt);
    updateParticles(simDt);
    updateAI(simDt);

    fogTickTimer_ += simDt;
    if (fogTickTimer_ >= 0.18f) {
        fogTickTimer_ = 0.0f;
        updateFogOfWar();
    }

    checkVictoryConditions();
}

void RTSEngine::updateSupplyNetwork() {
    for (auto& b : buildings_) {
        if (b.teamId >= 0 && !b.isUnderConstruction && (b.type == BuildingType::CORPS_HQ || b.type == BuildingType::CITY_DEPOT)) {
            b.inSupply = true;
            b.connectedSourceId = b.id;
        } else {
            b.inSupply = false;
            b.connectedSourceId = -1;
        }
    }

    for (int pass = 0; pass < 4; ++pass) {
        for (auto& target : buildings_) {
            if (target.teamId < 0 || target.inSupply || target.isUnderConstruction) continue;
            for (const auto& src : buildings_) {
                if (!isAllied(src.teamId, target.teamId) || !src.inSupply || src.supplyRadius <= 0.0f || src.isUnderConstruction) continue;
                float d = distXZ(target.pos, src.pos);
                if (d <= src.supplyRadius + target.supplyRadius + 60.0f) {
                    target.inSupply = true;
                    target.connectedSourceId = src.id;
                    break;
                }
            }
        }
    }

    for (auto& r : regiments_) {
        bool inAura = false;
        r.atTownHealingStation = false;
        r.supplySourceId = -1;
        float bestDist = 1e9f;
        for (const auto& b : buildings_) {
            if (!isAllied(b.teamId, r.teamId) || !b.inSupply || b.supplyRadius <= 0.0f || b.isUnderConstruction) continue;
            float d = distXZ(r.pos, b.pos);
            if (d <= b.supplyRadius && d < bestDist) {
                bestDist = d;
                inAura = true;
                r.supplySourceId = b.id;
            }
        }

        if (inAura) {
            r.supplyBuffer = r.maxSupplyBuffer;
            r.inSupply = true;
        } else {
            r.inSupply = (r.supplyBuffer > 0.0f);
        }

        // Field Hospital Check: Controlled Towns in supply serve as active Healing Stations for all allied units!
        for (const auto& b : buildings_) {
            if (b.type == BuildingType::CITY_DEPOT && isAllied(b.teamId, r.teamId) && b.inSupply) {
                float d = distXZ(r.pos, b.pos);
                if (d <= b.supplyRadius + 15.0f) {
                    r.atTownHealingStation = true;
                    break;
                }
            }
        }
    }
}

void RTSEngine::updateEconomy(float dt) {
    const auto& gd = GameData::instance();

    for (int t = 0; t < numPlayers_; ++t) {
        auto& p = players_[t];
        const auto& gen = gd.getGeneral(p.nation, p.generalIdx);

        for (auto& cd : p.specOpCooldowns) {
            if (cd > 0.0f) cd = std::max(0.0f, cd - dt);
        }
        if (p.radarRevealTimer > 0.0f) {
            p.radarRevealTimer = std::max(0.0f, p.radarRevealTimer - dt);
        }
        for (auto it = p.activeBuffs.begin(); it != p.activeBuffs.end();) {
            it->remainingSec -= dt;
            if (it->remainingSec <= 0.0f) {
                it = p.activeBuffs.erase(it);
            } else {
                ++it;
            }
        }

        float mRate = 8.0f;
        float aRate = 6.0f;
        float oRate = 5.0f;

        int townCount = 0;
        int oilCount = 0;
        for (const auto& b : buildings_) {
            if (b.health <= 0.0f || !b.inSupply || b.isUnderConstruction) continue;
            bool isOurOrAllied = (b.teamId == t);
            if (config_.gameMode == BattleGameMode::ALLIED_2V2 && isAllied(b.teamId, t)) {
                isOurOrAllied = true;
            }
            if (!isOurOrAllied) continue;

            if (b.type == BuildingType::CITY_DEPOT) {
                townCount++;
            } else if (b.type == BuildingType::OIL_DERRICK) {
                oilCount++;
            } else if (b.teamId == t) {
                const auto& bDef = gd.getBuilding(b.type);
                mRate += bDef.incomeMoney;
                aRate += bDef.incomeAmmo;
                oRate += bDef.incomeOil;
            }
        }

        // Compounding Territorial Town Economy: Holding multiple towns unlocks regional commercial corridors!
        // 0 towns: Base treasury only ($8/s)
        // 1 town:  Standard urban trade ($22/s)
        // 2 towns: Inter-city trade route starts! (+20% synergy multiplier) -> $53/s
        // 3 towns: Regional economic integration! (+48% synergy multiplier) -> $98/s
        // 4 towns: Industrial supply chain dominance! (+79% synergy multiplier) -> $158/s
        // 5 towns: Total economic hegemony! (+113% synergy multiplier) -> $234/s
        // 6+ towns: War economy mobilization! (+150%+ synergy multiplier) -> $330+/s
        float townBaseMoney = gd.getBuilding(BuildingType::CITY_DEPOT).incomeMoney; // 22.0f
        float townBaseAmmo  = gd.getBuilding(BuildingType::CITY_DEPOT).incomeAmmo;  // 14.0f
        float townBaseOil   = gd.getBuilding(BuildingType::CITY_DEPOT).incomeOil;   // 6.0f

        float townSynergy = 1.0f;
        if (townCount >= 2) {
            townSynergy = 1.0f + 0.20f * std::pow(static_cast<float>(townCount - 1), 1.25f);
        }
        p.controlledTowns = townCount;
        p.townSynergyMult = townSynergy;

        mRate += townCount * townBaseMoney * townSynergy;
        aRate += townCount * townBaseAmmo  * (1.0f + 0.12f * std::max(0, townCount - 1));
        oRate += townCount * townBaseOil   * (1.0f + 0.10f * std::max(0, townCount - 1));

        // Compounding Oil Refining Network
        float oilBaseRate = gd.getBuilding(BuildingType::OIL_DERRICK).incomeOil; // 26.0f
        float oilSynergy = 1.0f;
        if (oilCount >= 2) {
            oilSynergy = 1.0f + 0.15f * static_cast<float>(oilCount - 1);
        }
        p.controlledOil = oilCount;
        p.oilSynergyMult = oilSynergy;
        oRate += oilCount * oilBaseRate * oilSynergy;

        for (const auto& r : regiments_) {
            if (r.teamId != t) continue;
            const auto& rDef = gd.getRegiment(r.type);
            aRate -= rDef.upkeepAmmo;
            oRate -= rDef.upkeepOil;
        }

        p.moneyRate = std::max(4.0f, mRate * gen.moneyRateMult);
        p.ammoRate  = std::max(2.0f, aRate * gen.ammoRateMult);
        p.oilRate   = std::max(2.0f, oRate * gen.oilRateMult);

        p.money = std::min(9999.0f, p.money + p.moneyRate * dt * 0.45f);
        p.ammo  = std::min(9999.0f, p.ammo  + p.ammoRate  * dt * 0.45f);
        p.oil   = std::min(9999.0f, p.oil   + p.oilRate   * dt * 0.45f);
        p.commandPoints = std::min(300.0f, p.commandPoints + 3.2f * dt);
    }
}

void RTSEngine::updateBuildings(float dt) {
    const auto& gd = GameData::instance();

    for (size_t i = 0; i < buildings_.size(); ++i) {
        auto& b = buildings_[i];

        if (b.combatTimer > 0.0f) {
            b.combatTimer = std::max(0.0f, b.combatTimer - dt);
        }

        // 1. Building Construction Progress
        if (b.isUnderConstruction) {
            if (b.combatTimer > 0.0f) {
                // Actively under enemy fire: construction progress is suspended, scaffolding is vulnerable!
            } else {
                float buildRate = 1.0f / std::max(0.1f, b.constructTotalTime);
                b.constructProgress += dt * buildRate;
                float healthGain = dt * buildRate * (0.85f * b.maxHealth);
                b.health = std::min(b.maxHealth, b.health + healthGain);
                if (b.constructProgress >= 1.0f) {
                    b.constructProgress = 1.0f;
                    b.isUnderConstruction = false;
                    b.health = b.maxHealth;
                    updateSupplyNetwork();
                    if (b.teamId == 0) {
                        if (sound_) sound_->playSfx(SfxId::HQ_UNPACK, 0.85f);
                        pushLog(TextFormat("CONSTRUCTION COMPLETE: %s is completed and fully operational!",
                                           gd.getBuilding(b.type).name.c_str()));
                    }
                }
            }
            continue; // Cannot muster regiments or fire defensive weapons while under construction!
        }

        if (b.health <= 0.0f) continue; // Dead buildings cannot produce units or fire

        float healthRatio = std::clamp(b.health / std::max(1.0f, b.maxHealth), 0.0f, 1.0f);
        if (b.isBombDamaged) {
            b.bombDamageTimer -= dt;
            if (b.bombDamageTimer <= 0.0f && healthRatio >= 0.99f) {
                b.isBombDamaged = false;
                b.bombDamageTimer = 0.0f;
            }
        }

        if (!b.prodQueue.empty() && b.teamId >= 0) {
            float speedMult = (0.20f + 0.80f * healthRatio);
            if (b.isBombDamaged) speedMult *= 0.50f; // Slower muster speed while burning from bomb damage
            b.prodProgress += dt * speedMult;
            if (b.prodProgress >= b.prodTotalTime) {
                RegimentType rt = b.prodQueue.front();
                b.prodQueue.erase(b.prodQueue.begin());
                b.prodProgress = 0.0f;

                float angle = static_cast<float>(nextEntityId_ % 12) * 0.52f;
                Vector3 spawnPos = {
                    b.rallyPoint.x + std::cos(angle) * 6.0f,
                    0.0f,
                    b.rallyPoint.z + std::sin(angle) * 6.0f
                };
                spawnRegiment(b.teamId, rt, spawnPos);
                if (b.teamId == 0) {
                    pushLog("DEPLOYED: " + gd.getRegimentDisplayName(b.nation, rt) + " ready for orders.");
                }
            }
        }

        const auto& def = gd.getBuilding(b.type);
        if (def.attackRange > 0.0f && b.teamId >= 0) {
            b.fireTimer -= dt;
            if (b.fireTimer <= 0.0f) {
                int bestTarget = -1;
                float bestDist = def.attackRange;
                for (int rIdx = 0; rIdx < static_cast<int>(regiments_.size()); ++rIdx) {
                    const auto& er = regiments_[rIdx];
                    if (isAllied(b.teamId, er.teamId)) continue;
                    float d = distXZ(b.pos, er.pos);
                    if (d <= bestDist) {
                        bestDist = d;
                        bestTarget = rIdx;
                    }
                }
                if (bestTarget >= 0) {
                    b.fireTimer = def.fireInterval;
                    const auto& targetReg = regiments_[bestTarget];
                    RTSProjectile proj{};
                    proj.startPos = {b.pos.x, b.pos.y + 3.0f, b.pos.z};
                    proj.pos = proj.startPos;
                    proj.targetPos = {targetReg.pos.x, targetReg.pos.y + 1.0f, targetReg.pos.z};
                    Vector3 dir = Vector3Normalize(Vector3Subtract(proj.targetPos, proj.startPos));
                    proj.velocity = Vector3Scale(dir, 75.0f);
                    proj.teamId = b.teamId;
                    proj.damageInf = def.attackDamage;
                    proj.damageVeh = (b.type == BuildingType::AA_BATTERY) ? def.attackDamage * 1.4f : def.attackDamage * 0.7f;
                    proj.damageStruct = def.attackDamage * 0.5f;
                    proj.orgDamage = 14.0f;
                    proj.splashRadius = (b.type == BuildingType::AA_BATTERY) ? 3.5f : 1.5f;
                    proj.color = (b.type == BuildingType::AA_BATTERY) ? ORANGE : YELLOW;
                    projectiles_.push_back(proj);

                    if (sound_) {
                        sound_->playSfx3D(b.type == BuildingType::AA_BATTERY ? SfxId::HEAVY_88_CANNON : SfxId::MG_BURST, b.pos, 0.60f);
                    }
                }
            }
        }
    }

    for (auto it = buildings_.begin(); it != buildings_.end();) {
        if (it->health <= 0.0f) {
            if (it->type == BuildingType::CITY_DEPOT || it->type == BuildingType::OIL_DERRICK) {
                it->teamId = -1;
                it->health = it->maxHealth * 0.15f; // Neutralized ruin at critical low health, NOT 50%!
                it->captureProgress = 0.0f;
                it->isBombDamaged = true;
                it->bombDamageTimer = 35.0f;
                spawnExplosion(it->pos, 6.5f, ORANGE);
                if (sound_) sound_->playSfx(SfxId::EXPLOSION_LARGE, 0.75f);
                pushLog("STRATEGIC POINT DISABLED: " + gd.getBuilding(it->type).name + " overrun & rendered neutral!");
                ++it;
            } else {
                spawnExplosion(it->pos, 8.0f, ORANGE);
                if (sound_) sound_->playSfx(SfxId::EXPLOSION_LARGE, 0.8f);
                pushLog("STRUCTURE DESTROYED: " + gd.getBuilding(it->type).name + " demolished!");
                it = buildings_.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void RTSEngine::updateRegiments(float dt) {
    const auto& gd = GameData::instance();

    for (size_t i = 0; i < regiments_.size(); ++i) {
        auto& r = regiments_[i];
        const auto& def = gd.getRegiment(r.type);
        const auto& gen = gd.getGeneral(r.nation, players_[r.teamId].generalIdx);

        float speedMult = (def.category == UnitCategory::INFANTRY) ? gen.infSpeedMult : gen.armorSpeedMult;
        float attackMult = (def.category == UnitCategory::INFANTRY) ? gen.infAttackMult :
                           (def.category == UnitCategory::ARTILLERY ? gen.artilleryAttackMult : gen.armorAttackMult);
        bool ignoreOutSupply = false;

        if (r.stance == FormationStance::COLUMN) {
            speedMult *= 1.25f;
        } else if (r.stance == FormationStance::ENTRENCHED) {
            speedMult = 0.0f;
            attackMult *= 1.08f;
        }

        for (const auto& buff : players_[r.teamId].activeBuffs) {
            speedMult *= buff.speedMult;
            attackMult *= buff.attackMult;
            if (buff.ignoreSupplyPenalty) ignoreOutSupply = true;
        }

        // Tactical Field Ability Timers & Obscuration Particle VFX
        if (r.smokeScreenTimer > 0.0f) {
            r.smokeScreenTimer = std::max(0.0f, r.smokeScreenTimer - dt);
            if (nextEntityId_ % 2 == 0) {
                RTSParticle p{};
                p.pos = {r.pos.x + ((nextEntityId_ % 7) - 3) * 1.4f, r.pos.y + 0.6f, r.pos.z + ((nextEntityId_ % 5) - 2) * 1.4f};
                p.vel = {0.0f, 1.35f, 0.0f};
                p.color = Color{235, 235, 235, 185};
                p.size = 2.0f;
                p.life = 1.2f;
                p.maxLife = 1.2f;
                particles_.push_back(p);
            }
        }
        if (r.rapidVolleyTimer > 0.0f) {
            r.rapidVolleyTimer = std::max(0.0f, r.rapidVolleyTimer - dt);
        }
        if (r.abilityCooldown > 0.0f) {
            r.abilityCooldown = std::max(0.0f, r.abilityCooldown - dt);
        }

        // Dynamic Tactical Terrain Cover (Craters, Boulders, Wrecks, Entrenchments)
        r.coverTier = 0;
        if (r.stance == FormationStance::ENTRENCHED) {
            r.coverTier = 2; // Entrenchment provides Heavy Fortified Cover (-40%)
        } else if (def.category == UnitCategory::INFANTRY) {
            // 1. Proximity to blast craters
            for (const auto& p : particles_) {
                if (p.isCrater && distXZ(r.pos, p.pos) <= 5.0f) {
                    r.coverTier = 2; // Heavy Crater Cover (-40%)
                    break;
                }
            }
            // 2. Proximity to boulder rock formations
            if (r.coverTier < 2) {
                for (const auto& prop : props_) {
                    if (prop.propType == 3 && distXZ(r.pos, prop.pos) <= 6.5f) {
                        r.coverTier = 2; // Heavy Boulder Cover (-40%)
                        break;
                    }
                }
            }
            // 3. Proximity to destroyed vehicle wrecks
            if (r.coverTier == 0) {
                for (const auto& w : wrecks_) {
                    if (distXZ(r.pos, w.pos) <= 7.5f) {
                        r.coverTier = 1; // Light Wreckage Cover (-25%)
                        break;
                    }
                }
            }
        }

        // Supply & Organization Logic (Dynamic Expedition Field Rations Buffer)
        bool hasSupplyAura = false;
        for (const auto& b : buildings_) {
            if (b.teamId == r.teamId && b.inSupply && !b.isUnderConstruction && b.supplyRadius > 0.0f) {
                if (distXZ(r.pos, b.pos) <= b.supplyRadius) {
                    hasSupplyAura = true;
                    break;
                }
            }
        }

        if (hasSupplyAura) {
            r.supplyBuffer = r.maxSupplyBuffer;
            r.inSupply = true;
        } else {
            r.supplyBuffer = std::max(0.0f, r.supplyBuffer - dt);
            r.inSupply = (r.supplyBuffer > 0.0f);
        }

        if (r.inSupply && !isWaterAt(r.pos.x, r.pos.z)) {
            float entrenchBonus = (r.stance == FormationStance::ENTRENCHED) ? 1.35f : 1.0f;
            r.organization = std::min(r.maxOrganization, r.organization + 9.0f * gen.orgRegenMult * entrenchBonus * dt);

            // CRITICAL 2004 AXIS & ALLIES MECHANIC:
            // Controlled towns serve as healing stations. WITHOUT controlled towns, wounded soldiers cannot heal HP!
            if (r.atTownHealingStation && r.health < r.maxHealth) {
                float healRate = (def.category == UnitCategory::INFANTRY) ? 22.0f : 16.0f;
                r.health = std::min(r.maxHealth, r.health + healRate * dt);
                int expectedAlive = std::max(1, static_cast<int>(std::ceil((r.health / r.maxHealth) * def.squadCount)));
                int currentAlive = 0;
                for (const auto& su : r.subUnits) if (su.alive) ++currentAlive;
                if (currentAlive < expectedAlive) {
                    for (auto& su : r.subUnits) {
                        if (!su.alive) {
                            su.alive = true;
                            if (r.teamId == 0) {
                                pushLog("CASUALTY TREATED: Field Hospital at Town reinforced " + gd.getRegimentDisplayName(r.nation, r.type));
                            }
                            break;
                        }
                    }
                }
            }
        } else if (!ignoreOutSupply) {
            float decay = 1.6f * gen.outOfSupplyPenaltyMult * dt;
            if (r.stance == FormationStance::ENTRENCHED) decay *= 0.50f;
            r.organization = std::max(10.0f, r.organization - decay);
            if (r.organization <= 15.0f) {
                float minHp = r.maxHealth * 0.40f; // Starvation caps at 40% HP; units will not die of pure starvation
                r.health = std::max(minHp, r.health - 1.2f * dt);
            }
        }

        float orgFactor = 0.55f + 0.45f * (r.organization / std::max(1.0f, r.maxOrganization));

        // Capture Cities & Oil Refineries (Infantry & Recon capture at full speed; Armor assists at support speed)
        bool canCap = def.canCapture || (def.category == UnitCategory::ARMOR);
        if (canCap) {
            for (auto& b : buildings_) {
                if (b.type != BuildingType::CITY_DEPOT && b.type != BuildingType::OIL_DERRICK) continue;
                if (distXZ(r.pos, b.pos) < 28.0f) {
                    float capRate = def.canCapture ? 34.0f : 18.0f;
                    int rAlliance = (r.teamId >= 0 && r.teamId < numPlayers_) ? players_[r.teamId].allianceId : 0;
                    bool isAlliance0 = (rAlliance == 0);
                    float capDelta = (isAlliance0 ? capRate : -capRate) * dt;
                    b.captureProgress = std::clamp(b.captureProgress + capDelta, -100.0f, 100.0f);
                    if (b.captureProgress >= 99.0f && !isAllied(0, b.teamId)) {
                        b.teamId = r.teamId;
                        b.nation = players_[r.teamId].nation;
                        if (r.teamId == 0) result_.buildingsCaptured++;
                        updateSupplyNetwork();
                        updateFogOfWar();
                        if (r.teamId == 0) {
                            pushLog("TOWN CAPTURED: " + gd.getBuilding(b.type).name + " secured! (+Vision, Supply & Field Hospital)");
                            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.7f);
                        } else {
                            const auto& aNat = gd.getNation(players_[r.teamId].nation);
                            pushLog(TextFormat("ALLIED FORCES [%s]: Secured %s! (+Coordinated Supply)",
                                               aNat.shortName.c_str(), gd.getBuilding(b.type).name.c_str()));
                            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.55f);
                        }

                        // Award capturing squad substantial battle experience!
                        r.xp += 45.0f;
                        if (r.xp >= 100.0f && r.veterancy < 3) {
                            r.veterancy++;
                            r.xp = 0.0f;
                            spawnExplosion(r.pos, 3.5f, GOLD);
                            const char* vName = (r.veterancy == 1) ? "Veteran (*)" : (r.veterancy == 2 ? "Elite (**)" : "Heroic (***)");
                            pushLog(TextFormat("UNIT PROMOTION: %s promoted to %s! (+%d%% Firepower, +%d%% Defense)",
                                               gd.getRegimentDisplayName(r.nation, r.type).c_str(), vName,
                                               r.veterancy * 15, r.veterancy * 14));
                            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.85f);
                        }
                    } else if (b.captureProgress <= -99.0f && (b.teamId < 0 || isAllied(0, b.teamId))) {
                        b.teamId = r.teamId;
                        b.nation = players_[r.teamId].nation;
                        updateSupplyNetwork();
                        updateFogOfWar();
                        pushLog("ALERT: Strategic point overrun by hostile forces!");
                        if (sound_) sound_->playSfx(SfxId::OUT_OF_SUPPLY_ALERT, 0.65f);
                    }
                }
            }
        }

        // Combat Engineer Repair (Aura + Targeted Orders)
        if (def.canRepair || r.type == RegimentType::ENGINEER_FLAME_REGIMENT) {
            if (r.targetRepairBuildingId != -1) {
                RTSBuilding* bTarget = nullptr;
                for (auto& b : buildings_) {
                    if (b.id == r.targetRepairBuildingId) {
                        bTarget = &b;
                        break;
                    }
                }
                if (bTarget && isAllied(r.teamId, bTarget->teamId) && (bTarget->health < bTarget->maxHealth || bTarget->isBombDamaged)) {
                    float d = distXZ(r.pos, bTarget->pos);
                    if (d <= 22.0f) {
                        // Under active enemy fire, repair crews take cover: repair rate is 75% suppressed
                        float repairSpeed = (bTarget->combatTimer > 0.0f) ? 15.0f : 65.0f;
                        bTarget->health = std::min(bTarget->maxHealth, bTarget->health + repairSpeed * dt);
                        if (bTarget->isBombDamaged) {
                            bTarget->bombDamageTimer = std::max(0.0f, bTarget->bombDamageTimer - 4.0f * dt);
                            if (bTarget->bombDamageTimer <= 0.0f || bTarget->health >= bTarget->maxHealth * 0.99f) {
                                bTarget->isBombDamaged = false;
                                bTarget->bombDamageTimer = 0.0f;
                            }
                        }
                        if (bTarget->health >= bTarget->maxHealth && !bTarget->isBombDamaged) {
                            r.targetRepairBuildingId = -1;
                            if (r.teamId == 0) {
                                pushLog(TextFormat("ENGINEER REPAIRS COMPLETE: %s restored to 100%% capacity!",
                                                   gd.getBuilding(bTarget->type).name.c_str()));
                                if (sound_) sound_->playSfx(SfxId::HQ_UNPACK, 0.75f);
                            }
                        }
                    } else {
                        r.targetPos = bTarget->pos;
                    }
                } else {
                    r.targetRepairBuildingId = -1;
                }
            }

            // Passive repair aura (strictly suppressed while structure is under active hostile fire)
            for (auto& b : buildings_) {
                if (isAllied(r.teamId, b.teamId) && (b.health < b.maxHealth || b.isBombDamaged) && distXZ(r.pos, b.pos) < 20.0f) {
                    if (b.combatTimer <= 0.0f) {
                        b.health = std::min(b.maxHealth, b.health + 18.0f * dt);
                        if (b.isBombDamaged && b.health >= b.maxHealth * 0.99f) {
                            b.isBombDamaged = false;
                            b.bombDamageTimer = 0.0f;
                        }
                    }
                }
            }
        }

        // Target Acquisition (Prioritizes player right-clicked targets, then opportunistic vision targets)
        RTSRegiment* enemyRegTarget = nullptr;
        RTSBuilding* enemyBldgTarget = nullptr;

        // 1. Explicitly designated target regiment
        if (r.targetRegimentId != -1) {
            for (auto& other : regiments_) {
                if (other.id == r.targetRegimentId && isEnemy(r.teamId, other.teamId) && other.health > 0.0f) {
                    if (isPosVisibleToTeam(other.pos, r.teamId)) {
                        enemyRegTarget = &other;
                    }
                    break;
                }
            }
            if (!enemyRegTarget) {
                r.targetRegimentId = -1;
            }
        }
        // 2. Explicitly designated target building
        if (!enemyRegTarget && r.targetBuildingId != -1) {
            for (auto& b : buildings_) {
                if (b.id == r.targetBuildingId && isEnemy(r.teamId, b.teamId) && b.health > 0.0f) {
                    if (isPosVisibleToTeam(b.pos, r.teamId)) {
                        enemyBldgTarget = &b;
                    }
                    break;
                }
            }
            if (!enemyBldgTarget) {
                r.targetBuildingId = -1;
            }
        }

        // 3. Opportunistic auto-target acquisition if no explicit target assigned
        if (!enemyRegTarget && !enemyBldgTarget) {
            float bestEnemyDist = std::max(def.attackRange, getRegimentVisionRadius(r.type));
            for (auto& other : regiments_) {
                if (!isEnemy(r.teamId, other.teamId) || other.health <= 0.0f) continue;
                float d = distXZ(r.pos, other.pos);
                if (d < bestEnemyDist && isPosVisibleToTeam(other.pos, r.teamId)) {
                    bestEnemyDist = d;
                    enemyRegTarget = &other;
                }
            }
            if (!enemyRegTarget) {
                for (auto& b : buildings_) {
                    if (b.teamId < 0 || !isEnemy(r.teamId, b.teamId) || b.health <= 0.0f) continue;
                    float d = distXZ(r.pos, b.pos);
                    if (d < bestEnemyDist && isPosVisibleToTeam(b.pos, r.teamId)) {
                        bestEnemyDist = d;
                        enemyBldgTarget = &b;
                    }
                }
            }
        }

        bool inFireRange = false;
        Vector3 fireTargetPos = r.pos;
        if (enemyRegTarget && distXZ(r.pos, enemyRegTarget->pos) <= def.attackRange) {
            inFireRange = true;
            fireTargetPos = enemyRegTarget->pos;
        } else if (enemyBldgTarget && distXZ(r.pos, enemyBldgTarget->pos) <= def.attackRange) {
            inFireRange = true;
            fireTargetPos = enemyBldgTarget->pos;
        }

        // Movement Logic
        Vector3 moveDest = r.targetPos;
        if (enemyRegTarget && (r.targetRegimentId == enemyRegTarget->id || r.attackMove) && !inFireRange) {
            moveDest = enemyRegTarget->pos;
        } else if (enemyBldgTarget && (r.targetBuildingId == enemyBldgTarget->id || r.attackMove) && !inFireRange) {
            moveDest = enemyBldgTarget->pos;
        }

        float distToDest = distXZ(r.pos, moveDest);
        bool isMoving = false;
        if (r.stance != FormationStance::ENTRENCHED && distToDest > 1.6f && (!inFireRange || !r.attackMove)) {
            Vector3 dir = {moveDest.x - r.pos.x, 0.0f, moveDest.z - r.pos.z};
            dir = Vector3Normalize(dir);
            float waterFactor = isWaterAt(r.pos.x, r.pos.z) ? 0.28f : 1.0f; // Severe -72% speed penalty wading deep river water!
            float actualSpeed = def.moveSpeed * speedMult * orgFactor * waterFactor;
            Vector3 nextPos = Vector3Add(r.pos, Vector3Scale(dir, actualSpeed * dt));

            r.pos.x = std::clamp(nextPos.x, -535.0f, 535.0f);
            r.pos.z = std::clamp(nextPos.z, -535.0f, 535.0f);
            r.pos.y = getTerrainHeight(r.pos.x, r.pos.z);
            r.facing = std::atan2(dir.x, dir.z) * RAD2DEG;
            isMoving = true;

            if (isWaterAt(r.pos.x, r.pos.z)) {
                // Wading deep water drains troop stamina and organization
                r.organization = std::max(5.0f, r.organization - 5.0f * dt);
                if (r.teamId == 0 && !hasWarnedRiverCrossing_) {
                    hasWarnedRiverCrossing_ = true;
                    pushLog("RIVER CROSSING: Units wading through deep water suffer heavy -72% speed penalty and morale drain! Use Bridges to cross safely.");
                }
            }

            if (def.category == UnitCategory::ARMOR || def.category == UnitCategory::MECHANIZED) {
                if (static_cast<int>(battleTime_ * 3.5f) != static_cast<int>((battleTime_ - dt) * 3.5f)) {
                    if (particles_.size() < 400) {
                        RTSParticle tread{};
                        tread.pos = r.pos;
                        tread.isTread = true;
                        tread.size = (def.category == UnitCategory::ARMOR) ? 1.7f : 1.2f;
                        tread.life = 12.0f;
                        tread.maxLife = 12.0f;
                        particles_.push_back(tread);
                    }
                }
            }
        }

        // Soft separation
        for (size_t j = 0; j < regiments_.size(); ++j) {
            if (i == j) continue;
            float d = distXZ(r.pos, regiments_[j].pos);
            if (d < 4.5f && d > 0.01f) {
                Vector3 push = {(r.pos.x - regiments_[j].pos.x) / d, 0.0f, (r.pos.z - regiments_[j].pos.z) / d};
                r.pos.x += push.x * 3.0f * dt;
                r.pos.z += push.z * 3.0f * dt;
            }
        }

        // Update sub-units
        float rad = r.facing * DEG2RAD;
        float cosF = std::cos(rad);
        float sinF = std::sin(rad);
        float targetYaw = r.facing;
        if (inFireRange) {
            targetYaw = std::atan2(fireTargetPos.x - r.pos.x, fireTargetPos.z - r.pos.z) * RAD2DEG;
        }

        for (auto& su : r.subUnits) {
            if (!su.alive) continue;
            float rx = su.localOffset.x * cosF + su.localOffset.z * sinF;
            float rz = -su.localOffset.x * sinF + su.localOffset.z * cosF;
            Vector3 desired = {r.pos.x + rx, getTerrainHeight(r.pos.x + rx, r.pos.z + rz), r.pos.z + rz};
            su.worldPos = Vector3Lerp(su.worldPos, desired, std::min(1.0f, dt * 10.0f));
            su.turretYaw = targetYaw;
            if (su.recoil > 0.0f) su.recoil = std::max(0.0f, su.recoil - dt * 3.0f);
            if (isMoving) su.animPhase += dt * 9.0f;

            // Vehicle slope alignment (Terrain Normal IK Pitch & Roll)
            if (def.category == UnitCategory::ARMOR || def.category == UnitCategory::MECHANIZED || def.category == UnitCategory::ARTILLERY) {
                float fRad = r.facing * DEG2RAD;
                Vector3 fwd = { std::sin(fRad), 0.0f, std::cos(fRad) };
                Vector3 rgt = { fwd.z, 0.0f, -fwd.x };
                float hF = getTerrainHeight(su.worldPos.x + fwd.x * 2.2f, su.worldPos.z + fwd.z * 2.2f);
                float hB = getTerrainHeight(su.worldPos.x - fwd.x * 2.2f, su.worldPos.z - fwd.z * 2.2f);
                float targetPitch = std::atan2(hF - hB, 4.4f) * RAD2DEG;

                float hR = getTerrainHeight(su.worldPos.x + rgt.x * 1.5f, su.worldPos.z + rgt.z * 1.5f);
                float hL = getTerrainHeight(su.worldPos.x - rgt.x * 1.5f, su.worldPos.z - rgt.z * 1.5f);
                float targetRoll = std::atan2(hR - hL, 3.0f) * RAD2DEG;

                su.pitchDeg = std::lerp(su.pitchDeg, targetPitch, std::min(1.0f, dt * 7.5f));
                su.rollDeg  = std::lerp(su.rollDeg,  targetRoll,  std::min(1.0f, dt * 7.5f));
            }
        }

        // Weapon Firing
        if (def.attackRange > 0.0f && inFireRange) {
            r.fireTimer -= dt * orgFactor;
            if (r.fireTimer <= 0.0f) {
                r.fireTimer = def.fireInterval;

                int aliveSubUnits = 0;
                for (const auto& su : r.subUnits) if (su.alive) ++aliveSubUnits;
                float squadScale = static_cast<float>(aliveSubUnits) / std::max(1, def.squadCount);
                float vetScale = 1.0f + r.veterancy * 0.15f; // +15% firepower per star (up to +45% at 3 stars)
                float totalScale = attackMult * squadScale * vetScale;

                Vector3 muzzlePos = {r.pos.x, r.pos.y + 1.4f, r.pos.z};
                for (auto& su : r.subUnits) {
                    if (su.alive) {
                        muzzlePos = {su.worldPos.x, su.worldPos.y + 1.3f, su.worldPos.z};
                        su.recoil = 0.45f;
                        break;
                    }
                }

                int bursts = (r.type == RegimentType::KATYUSHA_ROCKET) ? 3 : 1;
                for (int bIdx = 0; bIdx < bursts; ++bIdx) {
                    RTSProjectile proj{};
                    proj.startPos = muzzlePos;
                    proj.pos = muzzlePos;
                    float spread = (bursts > 1) ? (bIdx - 1) * 3.5f : 0.0f;
                    proj.targetPos = {fireTargetPos.x + spread, fireTargetPos.y + 0.8f, fireTargetPos.z - spread};
                    Vector3 dir = Vector3Normalize(Vector3Subtract(proj.targetPos, proj.startPos));
                    float speed = def.isIndirectFire ? 42.0f : 78.0f;
                    proj.velocity = Vector3Scale(dir, speed);
                    proj.teamId = r.teamId;
                    proj.damageInf = (def.dmgVsInfantry * totalScale) / bursts;
                    proj.damageVeh = (def.dmgVsVehicle * totalScale) / bursts;
                    proj.damageStruct = (def.dmgVsStructure * totalScale) / bursts;
                    proj.orgDamage = def.orgDamage / bursts;
                    proj.splashRadius = def.splashRadius;
                    proj.isArcing = def.isIndirectFire;
                    proj.isRocket = (r.type == RegimentType::KATYUSHA_ROCKET || r.type == RegimentType::AT_BAZOOKA_REGIMENT);
                    proj.isFlame = (r.type == RegimentType::ENGINEER_FLAME_REGIMENT ||
                                   (r.type == RegimentType::ELITE_SIGNATURE_TANK && r.nation == NationId::UK));
                    proj.color = proj.isFlame ? ORANGE : (proj.isRocket ? RED : YELLOW);
                    projectiles_.push_back(proj);
                }

                // Eject hot brass shell casing with physics velocity and realistic ground clatter
                if (r.type != RegimentType::KATYUSHA_ROCKET) {
                    RTSParticle casing{};
                    casing.pos = muzzlePos;
                    float ejectYaw = (r.facing + 90.0f + ((nextEntityId_ % 7) - 3) * 12.0f) * DEG2RAD;
                    float ejectSpd = 2.4f + (nextEntityId_ % 4) * 0.4f;
                    casing.vel = {std::sin(ejectYaw) * ejectSpd, 2.8f + (nextEntityId_ % 3) * 0.5f, std::cos(ejectYaw) * ejectSpd};
                    casing.color = GOLD;
                    casing.size = (def.category == UnitCategory::ARMOR) ? 0.28f : 0.10f;
                    casing.life = 14.0f;
                    casing.maxLife = 14.0f;
                    casing.isCasing = true;
                    casing.rotation = static_cast<float>(nextEntityId_ % 360);
                    particles_.push_back(casing);
                }

                r.xp += 8.0f;
                if (r.xp >= 100.0f && r.veterancy < 3) {
                    r.veterancy++;
                    r.xp = 0.0f;
                    spawnExplosion(r.pos, 3.5f, GOLD);
                    if (r.teamId == 0) {
                        const char* vName = (r.veterancy == 1) ? "Veteran (*)" : (r.veterancy == 2 ? "Elite (**)" : "Heroic (***)");
                        pushLog(TextFormat("UNIT PROMOTION: %s promoted to %s! (+%d%% Firepower, +%d%% Defense)",
                                           gd.getRegimentDisplayName(r.nation, r.type).c_str(), vName,
                                           r.veterancy * 15, r.veterancy * 14));
                        if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.85f);
                    }
                }

                if (sound_) {
                    if (r.type == RegimentType::KATYUSHA_ROCKET) sound_->playSfx3D(SfxId::KATYUSHA_SALVO, r.pos, 0.70f);
                    else if (def.category == UnitCategory::ARMOR) sound_->playSfx3D(SfxId::TANK_CANNON, r.pos, 0.65f);
                    else if (def.category == UnitCategory::ARTILLERY) sound_->playSfx3D(SfxId::ARTILLERY_BOOM, r.pos, 0.75f);
                    else if (r.type == RegimentType::MG_REGIMENT) sound_->playSfx3D(SfxId::MG_BURST, r.pos, 0.45f);
                    else if (r.type == RegimentType::MORTAR_REGIMENT) sound_->playSfx3D(SfxId::MORTAR_THUMP, r.pos, 0.55f);
                    else sound_->playSfx3D(SfxId::RIFLE_VOLLEY, r.pos, 0.38f);
                }
            }
        }
    }

    for (auto it = regiments_.begin(); it != regiments_.end();) {
        if (it->health <= 0.0f) {
            spawnExplosion(it->pos, 4.5f, ORANGE);
            const auto& def = gd.getRegiment(it->type);
            if (def.category == UnitCategory::ARMOR || def.category == UnitCategory::MECHANIZED) {
                wrecks_.push_back({it->id, it->pos, it->facing, it->type, it->nation, 42.0f, 42.0f});
            }
            if (it->teamId == 0) {
                result_.playerRegimentsLost++;
            } else {
                result_.enemyRegimentsDestroyed++;
                players_[0].commandPoints = std::min(300.0f, players_[0].commandPoints + 18.0f);
                // Award Battle Victory XP to all surviving friendly units that fought nearby
                for (auto& f : regiments_) {
                    if (f.teamId == 0 && f.health > 0.0f && distXZ(f.pos, it->pos) < 48.0f) {
                        f.xp += 35.0f;
                        if (f.xp >= 100.0f && f.veterancy < 3) {
                            f.veterancy++;
                            f.xp = 0.0f;
                            spawnExplosion(f.pos, 3.5f, GOLD);
                            const char* vName = (f.veterancy == 1) ? "Veteran (*)" : (f.veterancy == 2 ? "Elite (**)" : "Heroic (***)");
                            pushLog(TextFormat("UNIT PROMOTION: %s promoted to %s! (+%d%% Firepower, +%d%% Defense)",
                                               gd.getRegimentDisplayName(f.nation, f.type).c_str(), vName,
                                               f.veterancy * 15, f.veterancy * 14));
                            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.85f);
                        }
                    }
                }
            }
            it = regiments_.erase(it);
        } else {
            ++it;
        }
    }
}

void RTSEngine::updateProjectiles(float dt) {
    const auto& gd = GameData::instance();

    for (auto it = projectiles_.begin(); it != projectiles_.end();) {
        it->life -= dt;
        it->pos = Vector3Add(it->pos, Vector3Scale(it->velocity, dt));

        if (it->isRocket || it->isFlame) {
            RTSParticle trail{};
            trail.pos = it->pos;
            trail.vel = {0.0f, 1.5f, 0.0f};
            trail.color = it->isFlame ? Color{255, 120, 20, 200} : Color{200, 200, 200, 180};
            trail.size = it->isFlame ? 1.4f : 0.7f;
            trail.life = 0.35f;
            trail.maxLife = 0.35f;
            particles_.push_back(trail);
        }

        float dTarget = distXZ(it->pos, it->targetPos);
        if (dTarget <= 2.2f || it->life <= 0.0f) {
            float hitRadius = std::max(3.2f, it->splashRadius);

            for (auto& r : regiments_) {
                if (isAllied(it->teamId, r.teamId)) continue;
                float d = distXZ(it->targetPos, r.pos);
                if (d <= hitRadius) {
                    const auto& rDef = gd.getRegiment(r.type);
                    const auto& defGen = gd.getGeneral(r.nation, players_[r.teamId].generalIdx);
                    float rawDmg = (rDef.category == UnitCategory::INFANTRY) ? it->damageInf : it->damageVeh;
                    float falloff = 1.0f - 0.45f * (d / hitRadius);
                    float stanceDef = (r.stance == FormationStance::ENTRENCHED) ? 1.35f :
                                      (r.stance == FormationStance::COLUMN ? 0.90f : 1.0f);
                    float vetDef = 1.0f + r.veterancy * 0.14f; // +14% armor damage resistance per star
                    float waterVuln = isWaterAt(r.pos.x, r.pos.z) ? 1.35f : 1.0f; // Caught in open river water without cover (+35% damage)
                    float coverMult = (r.coverTier == 2) ? 0.60f : (r.coverTier == 1 ? 0.75f : 1.0f); // Heavy (-40%) or Light (-25%) Cover
                    float smokeEvasion = (r.smokeScreenTimer > 0.0f) ? 0.45f : 1.0f; // Smoke screen evasion (-55% damage)
                    float finalDmg = ((rawDmg * falloff * coverMult * smokeEvasion) / std::max(0.5f, defGen.defenseMult * stanceDef * vetDef)) * waterVuln;

                    r.health -= finalDmg;
                    r.organization = std::max(0.0f, r.organization - (it->orgDamage * falloff) / (stanceDef * vetDef));
                    if (r.teamId == 0) {
                        setAlertPos(r.pos);
                    }

                    if (rDef.category == UnitCategory::ARMOR && rawDmg < 16.0f) {
                        // Light shell or MG round bounced off thick frontal armor plate!
                        if (sound_ && (r.teamId == 0 || distXZ(r.pos, {0.0f, 0.0f, 0.0f}) < 150.0f)) {
                            sound_->playSfx(SfxId::RICOCHET, 0.45f);
                        }
                        for (int sp = 0; sp < 6; ++sp) {
                            RTSParticle ric{};
                            ric.pos = it->targetPos;
                            float ricAng = (sp * 60.0f) * DEG2RAD;
                            ric.vel = {std::cos(ricAng) * 9.5f, 6.5f + (sp % 3) * 2.0f, std::sin(ricAng) * 9.5f};
                            ric.color = (sp % 2 == 0) ? YELLOW : GOLD;
                            ric.size = 0.45f;
                            ric.life = 0.28f;
                            ric.maxLife = 0.28f;
                            particles_.push_back(ric);
                        }
                    }

                    int expectedAlive = std::max(1, static_cast<int>(std::ceil((std::max(0.0f, r.health) / r.maxHealth) * rDef.squadCount)));
                    int curAlive = 0;
                    for (const auto& su : r.subUnits) if (su.alive) ++curAlive;
                    for (int k = static_cast<int>(r.subUnits.size()) - 1; k >= 0 && curAlive > expectedAlive; --k) {
                        if (r.subUnits[k].alive) {
                            r.subUnits[k].alive = false;
                            if (rDef.category == UnitCategory::INFANTRY) {
                                // Gritty authentic infantry casualty: fallen helmet/gear + ground dust puff
                                RTSParticle puff{};
                                puff.pos = r.subUnits[k].worldPos;
                                puff.vel = {0.0f, 1.4f, 0.0f};
                                puff.color = {160, 155, 142, 175};
                                puff.size = 0.75f;
                                puff.life = 0.50f;
                                puff.maxLife = 0.50f;
                                particles_.push_back(puff);

                                RTSParticle gear{};
                                gear.pos = r.subUnits[k].worldPos;
                                gear.vel = {((nextEntityId_ % 5) - 2) * 0.4f, 1.2f, ((nextEntityId_ % 5) - 2) * 0.4f};
                                gear.color = {62, 68, 55, 255};
                                gear.size = 0.22f;
                                gear.life = 20.0f;
                                gear.maxLife = 20.0f;
                                gear.isCasing = true;
                                gear.rotation = static_cast<float>(nextEntityId_ % 360);
                                particles_.push_back(gear);
                            } else {
                                spawnExplosion(r.subUnits[k].worldPos, 1.8f, ORANGE);
                            }
                            --curAlive;
                        }
                    }
                }
            }

            for (auto& b : buildings_) {
                if (b.teamId < 0 || isAllied(it->teamId, b.teamId)) continue;
                float d = distXZ(it->targetPos, b.pos);
                if (d <= hitRadius + 4.5f) {
                    b.health -= it->damageStruct;
                    b.combatTimer = 4.0f; // Put under hostile fire: suppresses out-healing & halts construction
                }
            }

            if (it->splashRadius >= 3.0f || it->isRocket) {
                spawnExplosion(it->targetPos, it->splashRadius, it->color);
            } else {
                RTSParticle spark{};
                spark.pos = it->targetPos;
                spark.vel = {0.0f, 3.0f, 0.0f};
                spark.color = YELLOW;
                spark.size = 0.8f;
                spark.life = 0.22f;
                spark.maxLife = 0.22f;
                particles_.push_back(spark);
            }

            it = projectiles_.erase(it);
        } else {
            ++it;
        }
    }
}

void RTSEngine::updateAirStrikes(float dt) {
    for (auto it = airStrikes_.begin(); it != airStrikes_.end();) {
        if (it->isShotDown) {
            // Plane is in a burning downward spiral crash dive
            it->currentPos.y -= it->crashFallSpeed * dt;
            it->crashFallSpeed += 18.0f * dt;
            Vector3 flightDir = Vector3Normalize(Vector3Subtract(it->targetPos, it->startPos));
            it->currentPos = Vector3Add(it->currentPos, Vector3Scale(flightDir, 28.0f * dt));
            it->pitchAngle += dt * 45.0f;
            it->rollAngle += dt * 90.0f;

            // Trailing flame & thermal vapor particles (No harsh black balls)
            RTSParticle smoke{};
            smoke.pos = it->currentPos;
            smoke.vel = {((nextEntityId_ % 7) - 3) * 0.35f, 1.4f, ((nextEntityId_ % 5) - 2) * 0.35f};
            smoke.color = (nextEntityId_ % 2 == 0) ? Color{255, 140, 30, 220} : Color{255, 205, 60, 180};
            smoke.size = 0.95f;
            smoke.life = 0.45f;
            smoke.maxLife = 0.45f;
            particles_.push_back(smoke);

            if (it->currentPos.y <= 0.8f) {
                // Plane crashes into terrain!
                spawnExplosion(it->currentPos, 14.0f, ORANGE);
                spawnExplosion({it->currentPos.x + 3.0f, 0.0f, it->currentPos.z + 3.0f}, 9.0f, RED);
                RTSParticle crater{};
                crater.pos = it->currentPos;
                crater.isCrater = true;
                crater.size = 5.5f;
                crater.life = 25.0f;
                crater.maxLife = 25.0f;
                particles_.push_back(crater);
                if (sound_) sound_->playSfx3D(SfxId::EXPLOSION_LARGE, it->targetPos, 0.95f, 1.0f, 320.0f);
                it = airStrikes_.erase(it);
                continue;
            }
            ++it;
            continue;
        }

        // Active Flight
        float flightSpeed = (it->speed > 0.0f) ? it->speed : 0.35f;
        it->progress += dt * flightSpeed;
        Vector3 endPos = {
            it->targetPos.x + (it->targetPos.x - it->startPos.x),
            it->isKamikaze ? 0.0f : 44.0f,
            it->targetPos.z + (it->targetPos.z - it->startPos.z)
        };
        it->currentPos = Vector3Lerp(it->startPos, endPos, it->progress);

        // ANTI-AIRCRAFT (AA) ENGAGEMENT:
        // Hostile AA Half-Tracks (88m) and 88mm Flak Batteries (108m) intercept this aircraft!
        float aaDmgTaken = 0.0f;
        Vector3 aaFiringPos{0, 0, 0};
        bool aaFired = false;

        // Check hostile AA half-tracks
        for (auto& r : regiments_) {
            if (!isEnemy(it->teamId, r.teamId) || r.health <= 0.0f) continue;
            if (r.type == RegimentType::AA_HALFTRACK) {
                float dist = distXZ(r.pos, it->currentPos);
                if (dist <= 88.0f) {
                    aaDmgTaken += 54.0f * dt;
                    aaFiringPos = r.pos;
                    aaFired = true;
                    r.facing = std::atan2(it->currentPos.x - r.pos.x, it->currentPos.z - r.pos.z);
                }
            }
        }
        // Check hostile AA batteries
        for (const auto& b : buildings_) {
            if (!isEnemy(it->teamId, b.teamId) || b.health <= 0.0f) continue;
            if (b.type == BuildingType::AA_BATTERY) {
                float dist = distXZ(b.pos, it->currentPos);
                if (dist <= 108.0f) {
                    aaDmgTaken += 96.0f * dt;
                    aaFiringPos = b.pos;
                    aaFired = true;
                }
            }
        }

        if (aaFired) {
            it->health -= aaDmgTaken;
            it->flakBurstTimer += dt;
            if (it->flakBurstTimer >= 0.16f) {
                it->flakBurstTimer = 0.0f;
                // Spawn flak tracer projectile from ground to sky
                RTSProjectile flakProj{};
                flakProj.pos = {aaFiringPos.x, aaFiringPos.y + 2.0f, aaFiringPos.z};
                flakProj.velocity = Vector3Scale(Vector3Normalize(Vector3Subtract(it->currentPos, flakProj.pos)), 160.0f);
                flakProj.color = YELLOW;
                flakProj.life = 0.32f;
                projectiles_.push_back(flakProj);

                // Spawn flak burst explosion in sky near the plane
                Vector3 burstPos = {
                    it->currentPos.x + ((nextEntityId_ % 9) - 4) * 0.9f,
                    it->currentPos.y + ((nextEntityId_ % 5) - 2) * 0.7f,
                    it->currentPos.z + ((nextEntityId_ % 9) - 4) * 0.9f
                };
                spawnExplosion(burstPos, 3.4f, Color{255, 175, 55, 245});
                if (sound_) sound_->playSfx(SfxId::MG_BURST, 0.40f);
            }

            if (it->health <= 0.0f) {
                it->isShotDown = true;
                it->crashFallSpeed = 5.0f;
                if (it->teamId == 0) {
                    pushLog("AA INTERCEPT: Enemy anti-aircraft flak SHOT DOWN our aircraft!");
                } else {
                    pushLog("AA VICTORY: Our anti-aircraft fire SHOT DOWN enemy aircraft!");
                }
                if (sound_) sound_->playSfx3D(SfxId::EXPLOSION_LARGE, it->currentPos, 0.85f);
                ++it;
                continue;
            }
        }

        // Air Superiority Fighter Interception & Aerial Dogfights
        if (it->isFighter && !it->isShotDown) {
            int targetIdx = -1;
            float closestDist = 160.0f;
            for (size_t o = 0; o < airStrikes_.size(); ++o) {
                const auto& other = airStrikes_[o];
                if (other.id == it->id || isAllied(it->teamId, other.teamId) || other.health <= 0.0f || other.isShotDown) continue;
                float d = Vector3Distance(it->currentPos, other.currentPos);
                if (d < closestDist) {
                    closestDist = d;
                    targetIdx = static_cast<int>(o);
                }
            }

            if (targetIdx != -1) {
                auto& other = airStrikes_[targetIdx];
                it->dogfightTargetId = other.id;
                it->dogfightTargetPos = other.currentPos;
                it->rollAngle = std::sin(it->progress * 16.0f) * 44.0f; // High-G banking turn

                // Autocannon cannon burst damage
                float interceptDmg = 135.0f * dt;
                other.health -= interceptDmg;

                it->dogfightTimer += dt;
                if (it->dogfightTimer >= 0.12f) {
                    it->dogfightTimer = 0.0f;
                    RTSProjectile tracer{};
                    tracer.pos = it->currentPos;
                    tracer.velocity = Vector3Scale(Vector3Normalize(Vector3Subtract(other.currentPos, it->currentPos)), 240.0f);
                    tracer.color = Color{255, 235, 90, 240};
                    tracer.life = 0.28f;
                    projectiles_.push_back(tracer);
                    if (sound_) sound_->playSfx(SfxId::MG_BURST, 0.40f);
                }

                if (other.health <= 0.0f) {
                    other.isShotDown = true;
                    other.crashFallSpeed = 6.5f;
                    if (it->teamId == 0) {
                        pushLog("AERIAL INTERCEPT: Our fighters SHOT DOWN enemy aircraft over the sector!");
                        setAlertPos(other.currentPos);
                    } else if (other.teamId == 0) {
                        pushLog("AIR COMBAT LOSS: Hostile interceptors SHOT DOWN our aircraft!");
                        setAlertPos(other.currentPos);
                    }
                    if (sound_) sound_->playSfx3D(SfxId::EXPLOSION_LARGE, other.currentPos, 0.90f);
                }
            } else {
                it->dogfightTargetId = -1;
                it->rollAngle = std::sin(it->progress * 5.0f) * 15.0f;
            }
        }

        // Payload Delivery
        if (!it->droppedPayload && it->progress >= 0.50f) {
            it->droppedPayload = true;
            if (it->isFighter) {
                if (it->teamId == 0) {
                    pushLog("AIR COMBAT PATROL: Fighters sweeping target sector for enemy air traffic.");
                }
            } else if (it->isRecon) {
                if (it->teamId == 0) {
                    pushLog("PHOTO RECON: Target coordinates photographed; enemy positions exposed.");
                    if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.8f);
                }
            } else if (it->isParatroopDrop) {
                spawnRegiment(it->teamId, RegimentType::PARATROOP_REGIMENT, {it->targetPos.x - 5.0f, 0.0f, it->targetPos.z});
                spawnRegiment(it->teamId, RegimentType::AT_BAZOOKA_REGIMENT, {it->targetPos.x + 5.0f, 0.0f, it->targetPos.z});
                spawnExplosion({it->targetPos.x, 12.0f, it->targetPos.z}, 6.0f, WHITE);
                if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.8f);
            } else if (it->isSupplyDrop) {
                players_[it->teamId].money += 220.0f;
                players_[it->teamId].ammo  += 220.0f;
                players_[it->teamId].oil   += 220.0f;
                for (auto& r : regiments_) {
                    if (r.teamId == it->teamId && distXZ(r.pos, it->targetPos) <= it->radius) {
                        r.health = r.maxHealth;
                        r.organization = r.maxOrganization;
                        r.supplyBuffer = r.maxSupplyBuffer;
                        r.inSupply = true;
                    }
                }
                spawnExplosion(it->targetPos, 8.0f, SKYBLUE);
            } else {
                // Bombing Run
                float dmg = (it->damage > 0.0f) ? it->damage : 260.0f;
                spawnExplosion(it->targetPos, it->radius * 0.65f, ORANGE);
                spawnExplosion({it->targetPos.x + 8.0f, 0.0f, it->targetPos.z - 6.0f}, it->radius * 0.50f, RED);
                spawnExplosion({it->targetPos.x - 8.0f, 0.0f, it->targetPos.z + 6.0f}, it->radius * 0.50f, ORANGE);
                RTSParticle crater{};
                crater.pos = it->targetPos;
                crater.isCrater = true;
                crater.size = 5.2f;
                crater.life = 20.0f;
                crater.maxLife = 20.0f;
                particles_.push_back(crater);
                if (sound_) sound_->playSfx3D(SfxId::EXPLOSION_LARGE, it->targetPos, 0.95f, 1.0f, 320.0f);

                for (auto& r : regiments_) {
                    if (isAllied(it->teamId, r.teamId)) continue;
                    if (distXZ(r.pos, it->targetPos) <= it->radius) {
                        r.health -= dmg;
                        r.organization = std::max(0.0f, r.organization - 60.0f);
                    }
                }
                for (auto& b : buildings_) {
                    if (b.teamId < 0 || isAllied(it->teamId, b.teamId)) continue;
                    if (distXZ(b.pos, it->targetPos) <= it->radius + 5.0f) {
                        b.health -= dmg * 1.4f;
                        b.combatTimer = 6.0f;
                        b.isBombDamaged = true;
                        b.bombDamageTimer = 45.0f;
                    }
                }
                if (it->teamId == 0) {
                    pushLog("TACTICAL BOMBING: Payload delivered on target coordinates!");
                }
            }
        }

        if (it->progress >= 1.0f || (it->isKamikaze && it->droppedPayload)) {
            it = airStrikes_.erase(it);
        } else {
            ++it;
        }
    }
}

void RTSEngine::updateParticles(float dt) {
    for (auto it = particles_.begin(); it != particles_.end();) {
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = particles_.erase(it);
        } else {
            if (it->isCasing) {
                float groundY = getTerrainHeight(it->pos.x, it->pos.z) + 0.04f;
                if (it->pos.y > groundY) {
                    it->pos = Vector3Add(it->pos, Vector3Scale(it->vel, dt));
                    it->vel.y -= 14.0f * dt;
                    if (it->pos.y <= groundY) {
                        it->pos.y = groundY;
                        it->vel = {0.0f, 0.0f, 0.0f}; // Settles on the ground
                    }
                }
            } else if (!it->isRing && !it->isCrater && !it->isTread) {
                it->pos = Vector3Add(it->pos, Vector3Scale(it->vel, dt));
                it->vel.y -= 9.8f * dt;
                float groundY = getTerrainHeight(it->pos.x, it->pos.z) + 0.1f;
                if (it->pos.y < groundY) {
                    it->pos.y = groundY;
                    it->vel.y *= -0.3f;
                }
            } else if (it->isRing) {
                it->size += dt * 18.0f;
            }
            ++it;
        }
    }

    for (auto it = wrecks_.begin(); it != wrecks_.end();) {
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = wrecks_.erase(it);
        } else {
            // Periodic smoldering smoke wisp from burned-out engine deck
            if (static_cast<int>(it->life * 3.5f) != static_cast<int>((it->life - dt) * 3.5f)) {
                RTSParticle smoke{};
                smoke.pos = {it->pos.x, it->pos.y + 1.2f, it->pos.z};
                smoke.vel = {((nextEntityId_ % 5) - 2) * 0.15f, 1.4f, ((nextEntityId_ % 5) - 2) * 0.15f};
                smoke.color = {55, 52, 48, 140};
                smoke.size = 0.95f;
                smoke.life = 0.65f;
                smoke.maxLife = 0.65f;
                particles_.push_back(smoke);
            }
            ++it;
        }
    }
}

void RTSEngine::spawnExplosion(Vector3 pos, float radius, Color col) {
    if (sound_ && radius >= 2.5f) {
        sound_->playSfx3D(SfxId::EXPLOSION_LARGE, pos, std::clamp(radius / 5.5f, 0.45f, 0.95f), 1.0f, 260.0f);
    }

    RTSParticle ring{};
    ring.pos = {pos.x, getTerrainHeight(pos.x, pos.z) + 0.3f, pos.z};
    ring.color = col;
    ring.size = 1.5f;
    ring.life = 0.45f;
    ring.maxLife = 0.45f;
    ring.isRing = true;
    particles_.push_back(ring);

    if (radius >= 2.5f) {
        RTSParticle crater{};
        crater.pos = {pos.x, getTerrainHeight(pos.x, pos.z) + 0.04f, pos.z};
        crater.color = {36, 32, 28, 200};
        crater.size = std::min(5.2f, radius * 0.92f);
        crater.life = 28.0f;
        crater.maxLife = 28.0f;
        crater.isCrater = true;
        particles_.push_back(crater);
    }

    int debrisCount = std::clamp(static_cast<int>(radius * 2.2f), 6, 22);
    for (int i = 0; i < debrisCount; ++i) {
        float ang = i * (2.0f * PI / debrisCount);
        float spd = 6.0f + (i % 5) * 3.2f;
        RTSParticle p{};
        p.pos = {pos.x, getTerrainHeight(pos.x, pos.z) + 0.8f, pos.z};
        p.vel = {std::cos(ang) * spd, 7.0f + (i % 4) * 2.5f, std::sin(ang) * spd};
        p.color = (i % 2 == 0) ? col : ((i % 4 == 1) ? Color{255, 215, 80, 240} : Color{255, 130, 35, 220});
        p.size = 0.5f + (i % 3) * 0.35f;
        p.life = 0.45f + (i % 4) * 0.12f;
        p.maxLife = p.life;
        particles_.push_back(p);
    }
}

void RTSEngine::updateAI(float dt) {
    for (int t = 1; t < numPlayers_; ++t) {
        auto& ai = players_[t];
        if (!ai.isAI) continue;

        float interval = (ai.aiDifficulty == 0) ? 4.5f : (ai.aiDifficulty == 1 ? 2.8f : 1.8f);
        ai.aiDecisionTimer += dt;
        ai.aiAttackTimer += dt;
        ai.aiAirMissionTimer += dt;

        if (ai.aiDecisionTimer < interval) continue;
        ai.aiDecisionTimer = 0.0f;

        // Faction command authority: check for active Corps HQ or Mobile HQ Truck
        bool hasCorpsHQ = false;
        for (const auto& b : buildings_) {
            if (b.teamId == t && b.type == BuildingType::CORPS_HQ && b.health > 0.0f && !b.isUnderConstruction) {
                hasCorpsHQ = true;
                break;
            }
        }
        if (!hasCorpsHQ) {
            for (const auto& r : regiments_) {
                if (r.teamId == t && r.type == RegimentType::PACKED_HQ_TRUCK && r.health > 0.0f) {
                    unpackTruckRegiment(r.id);
                    hasCorpsHQ = true;
                    break;
                }
            }
        }

        bool hasInfHq = false;
        bool hasMechHq = false;
        bool hasArmorHq = false;
        bool hasAirfield = false;
        int infHqReadyId = -1;
        int mechHqReadyId = -1;
        int armorHqReadyId = -1;
        int depotCount = 0;

        for (const auto& b : buildings_) {
            if (b.teamId != t || b.health <= 0.0f) continue;
            if (b.type == BuildingType::INFANTRY_HQ) {
                hasInfHq = true;
                if (!b.isUnderConstruction) infHqReadyId = b.id;
            }
            if (b.type == BuildingType::MECH_HQ) {
                hasMechHq = true;
                if (!b.isUnderConstruction) mechHqReadyId = b.id;
            }
            if (b.type == BuildingType::ARMOR_HQ) {
                hasArmorHq = true;
                if (!b.isUnderConstruction) armorHqReadyId = b.id;
            }
            if (b.type == BuildingType::AIRFIELD) {
                hasAirfield = true;
            }
            if (b.type == BuildingType::SUPPLY_DEPOT) {
                depotCount++;
            }
        }

        Vector3 bPos = (t < static_cast<int>(playerBasePositions_.size())) ? playerBasePositions_[t] : enemyBasePos_;
        Vector3 toCenter = Vector3Normalize({ -bPos.x, 0.0f, -bPos.z });
        Vector3 perp = { -toCenter.z, 0.0f, toCenter.x };

        auto findValidAiBuildPos = [&](Vector3 pref) -> Vector3 {
            Vector3 p = pref;
            for (int k = 0; k < 6; ++k) {
                if (!isWaterAt(p.x, p.z)) {
                    bool overlap = false;
                    for (const auto& b : buildings_) {
                        if (b.health > 0.0f && distXZ(p, b.pos) < 16.0f) {
                            overlap = true;
                            break;
                        }
                    }
                    if (!overlap) return p;
                }
                p.x += toCenter.x * 18.0f;
                p.z += toCenter.z * 18.0f;
            }
            return pref;
        };

        // Base Building Construction & Tech Upgrades strictly require an active Corps HQ
        if (hasCorpsHQ) {
            if (!hasInfHq && ai.money >= 180.0f && ai.ammo >= 40.0f) {
                Vector3 pos = findValidAiBuildPos({ bPos.x + perp.x * -36.0f + toCenter.x * 20.0f, 0.0f, bPos.z + perp.z * -36.0f + toCenter.z * 20.0f });
                constructSupportBuilding(BuildingType::INFANTRY_HQ, pos, t);
            } else if (!hasMechHq && ai.money >= 240.0f && ai.ammo >= 60.0f && ai.oil >= 50.0f) {
                Vector3 pos = findValidAiBuildPos({ bPos.x + perp.x * 36.0f + toCenter.x * 20.0f, 0.0f, bPos.z + perp.z * 36.0f + toCenter.z * 20.0f });
                constructSupportBuilding(BuildingType::MECH_HQ, pos, t);
            } else if (ai.techTier < 2 && ai.money >= 260.0f) {
                upgradeCorpsHQTech(t);
            } else if (!hasArmorHq && ai.techTier >= 2 && ai.money >= 320.0f && ai.ammo >= 90.0f && ai.oil >= 80.0f) {
                Vector3 pos = findValidAiBuildPos({ bPos.x + toCenter.x * 44.0f, 0.0f, bPos.z + toCenter.z * 44.0f });
                constructSupportBuilding(BuildingType::ARMOR_HQ, pos, t);
            } else if (!hasAirfield && ai.money >= 220.0f && ai.ammo >= 50.0f && ai.oil >= 50.0f) {
                Vector3 pos = findValidAiBuildPos({ bPos.x + perp.x * -42.0f + toCenter.x * 44.0f, 0.0f, bPos.z + perp.z * -42.0f + toCenter.z * 44.0f });
                constructSupportBuilding(BuildingType::AIRFIELD, pos, t);
            } else if (depotCount < 2 && ai.money >= 130.0f) {
                Vector3 pos = findValidAiBuildPos({ bPos.x + perp.x * 42.0f + toCenter.x * 52.0f, 0.0f, bPos.z + perp.z * 42.0f + toCenter.z * 52.0f });
                constructSupportBuilding(BuildingType::SUPPLY_DEPOT, pos, t);
            } else if (ai.techTier < 3 && ai.money >= 380.0f) {
                upgradeCorpsHQTech(t);
            }
        }

        int aiRegCount = 0;
        for (const auto& r : regiments_) if (r.teamId == t && r.health > 0.0f) ++aiRegCount;

        // Reinforcement Production requires active Corps HQ command authority
        if (hasCorpsHQ && aiRegCount < 16) {
            if (armorHqReadyId != -1 && ai.money >= 280.0f && ai.oil >= 120.0f && ai.ammo >= 80.0f) {
                if (ai.techTier >= 3 && ai.money >= 430.0f && ai.oil >= 140.0f) {
                    queueUnitProduction(armorHqReadyId, RegimentType::ELITE_SIGNATURE_TANK);
                } else {
                    queueUnitProduction(armorHqReadyId, RegimentType::MEDIUM_TANK);
                }
            } else if (mechHqReadyId != -1 && ai.money >= 180.0f && ai.oil >= 60.0f && ai.ammo >= 60.0f) {
                RegimentType mechPick = (aiRegCount % 4 == 0) ? RegimentType::AA_HALFTRACK :
                                        ((ai.nation == NationId::USSR || ai.techTier >= 2) ? RegimentType::KATYUSHA_ROCKET : RegimentType::MOBILE_ARTILLERY);
                queueUnitProduction(mechHqReadyId, mechPick);
            } else if (infHqReadyId != -1 && ai.money >= 115.0f && ai.ammo >= 40.0f) {
                RegimentType infPick = (aiRegCount % 3 == 0) ? RegimentType::AT_BAZOOKA_REGIMENT :
                                        (aiRegCount % 2 == 0 ? RegimentType::MG_REGIMENT : RegimentType::RIFLE_REGIMENT);
                queueUnitProduction(infHqReadyId, infPick);
            }
        }

        // Tactical Objective & Target Sector Determination (Fog of War & Multi-Enemy Aware)
        // In 2v2 / Survival / FFA: The AI selects its focus dynamically among hostile enemies.
        // Friendly AI Ally shares live vision and player's explored map; Enemy AI strictly respects Fog of War!
        Vector3 primarySectorTarget = {0.0f, 0.0f, 0.0f};
        bool hasScoutedEnemyBase = false;
        float closestScoutedDist = 1e9f;

        for (int other = 0; other < numPlayers_; ++other) {
            if (!isEnemy(t, other) || other >= static_cast<int>(playerBasePositions_.size())) continue;
            Vector3 otherBase = playerBasePositions_[other];
            bool baseKnown = isAllied(0, t) ? (isPosVisibleToTeam(otherBase, t) || isPosExploredByPlayer(otherBase))
                                            : isPosVisibleToTeam(otherBase, t);
            if (baseKnown) {
                float d = distXZ(bPos, otherBase);
                if (d < closestScoutedDist) {
                    closestScoutedDist = d;
                    primarySectorTarget = otherBase;
                    hasScoutedEnemyBase = true;
                }
            }
        }

        if (!hasScoutedEnemyBase) {
            if (config_.gameMode == BattleGameMode::SURVIVAL_FFA_3) {
                float patrolAng = (t == 1) ? 0.35f : 2.80f;
                primarySectorTarget = { std::cos(patrolAng) * 160.0f, 0.0f, std::sin(patrolAng) * 80.0f };
            } else if (config_.gameMode == BattleGameMode::ALLIED_2V2) {
                // In 2v2: Patrol towards the front line corridor to contest the crossing
                primarySectorTarget = { -bPos.x * 0.65f, 0.0f, -bPos.z * 0.65f };
            } else {
                primarySectorTarget = {0.0f, 0.0f, 0.0f};
            }
        }

        // Mutual Coalition Defense check: Is our own base or our ally's HQ under threat?
        Vector3 defenseReliefPos = {0.0f, 0.0f, 0.0f};
        bool needDefenseRelief = false;
        for (const auto& r : regiments_) {
            if (isEnemy(t, r.teamId) && r.health > 0.0f) {
                // Immediate threat to our own base
                if (distXZ(bPos, r.pos) < 140.0f) {
                    defenseReliefPos = r.pos;
                    needDefenseRelief = true;
                    break;
                }
                // Threat to allied base in 2v2
                for (int ally = 0; ally < numPlayers_; ++ally) {
                    if (isAllied(t, ally) && ally != t && ally < static_cast<int>(playerBasePositions_.size())) {
                        if (distXZ(playerBasePositions_[ally], r.pos) < 130.0f) {
                            defenseReliefPos = r.pos;
                            needDefenseRelief = true;
                            break;
                        }
                    }
                }
                if (needDefenseRelief) break;
            }
        }

        // AI Air Missions (Independent sortie cycle for all AI nations)
        // STRICT FOG OF WAR: Air Bombing runs ONLY target spotted enemies in vision!
        if (ai.aiAirMissionTimer >= 26.0f) {
            ai.aiAirMissionTimer = 0.0f;
            Vector3 targetCluster = primarySectorTarget;
            bool enemySpotted = false;
            float bestDist = 1e9f;

            for (const auto& r : regiments_) {
                if (isEnemy(t, r.teamId) && r.health > 0.0f) {
                    if (isPosVisibleToTeam(r.pos, t)) {
                        float d = distXZ(bPos, r.pos);
                        if (d < bestDist) {
                            bestDist = d;
                            targetCluster = r.pos;
                            enemySpotted = true;
                        }
                    }
                }
            }

            if (enemySpotted && ai.techTier >= 2 && ai.money >= 165.0f && ai.oil >= 45.0f) {
                launchAirBombingMission(t, targetCluster);
            } else if (ai.money >= 75.0f && ai.oil >= 20.0f) {
                // Air Recon can scout towards unexplored central/enemy sectors to reveal the shroud
                launchAirReconMission(t, primarySectorTarget);
            }
        }

        if (ai.aiAttackTimer >= 14.0f) {
            ai.aiAttackTimer = 0.0f;
            Vector3 objectivePos = primarySectorTarget;
            float closestDist = 1e9f;

            if (needDefenseRelief) {
                objectivePos = defenseReliefPos;
            } else if (alliedPingTimer_ > 0.0f && isAllied(0, t)) {
                // Obey human player's tactical coordinate ping!
                objectivePos = alliedPingPos_;
            } else {
                // 1. Search for neutral cities or oil derricks to capture, or enemy-held strategic points
                for (const auto& b : buildings_) {
                    if (b.type != BuildingType::CITY_DEPOT && b.type != BuildingType::OIL_DERRICK) continue;
                    if (b.teamId < 0 || isEnemy(t, b.teamId)) {
                        float d = distXZ(bPos, b.pos);
                        if (d < closestDist) {
                            closestDist = d;
                            objectivePos = b.pos;
                        }
                    }
                }

                // 2. If army is ready (>= 4 combat units) or near towns are secured, assault scouted enemy structures / HQ!
                if (hasScoutedEnemyBase && (aiRegCount >= 4 || closestDist > 250.0f)) {
                    objectivePos = primarySectorTarget;
                }
            }

            int idx = 0;
            for (auto& r : regiments_) {
                if (r.teamId != t || r.health <= 0.0f) continue;
                if (r.type == RegimentType::RECON_HALFTRACK) {
                    // Fast reconnaissance halftracks scout directly towards strategic points
                    r.targetPos = objectivePos;
                    r.attackMove = true;
                } else if (!needDefenseRelief && idx == 0 && aiRegCount >= 4) {
                    // Base Garrison: 1 squad stays near HQ perimeter for continuous base security
                    r.targetPos = { bPos.x + toCenter.x * 24.0f, 0.0f, bPos.z + toCenter.z * 24.0f };
                    r.attackMove = true;
                } else {
                    float spreadX = ((idx % 5) - 2) * 8.0f;
                    r.targetPos = {objectivePos.x + spreadX, 0.0f, objectivePos.z};
                    r.attackMove = true;
                }
                ++idx;
            }

            if (ai.commandPoints >= 150.0f) {
                Vector3 enemyCluster{0.0f, 0.0f, 0.0f};
                bool spottedForSpecOp = false;
                float bestClusterDist = 1e9f;
                for (const auto& r : regiments_) {
                    if (isEnemy(t, r.teamId) && r.health > 0.0f && isPosVisibleToTeam(r.pos, t)) {
                        float d = distXZ(bPos, r.pos);
                        if (d < bestClusterDist) {
                            bestClusterDist = d;
                            enemyCluster = r.pos;
                            spottedForSpecOp = true;
                        }
                    }
                }
                if (spottedForSpecOp) {
                    triggerSpecialOp(t, 0, enemyCluster);
                }
            }
        }
    }
}

void RTSEngine::populateSurvivingStacks() {
    result_.survivingPlayerUnits = {};
    result_.survivingEnemyUnits = {};
    const auto& gd = GameData::instance();

    for (const auto& r : regiments_) {
        if (r.type == RegimentType::PACKED_HQ_TRUCK) continue;
        auto& st = isAllied(0, r.teamId) ? result_.survivingPlayerUnits : result_.survivingEnemyUnits;
        const auto& def = gd.getRegiment(r.type);
        if (def.category == UnitCategory::INFANTRY) st.infantry++;
        else if (def.category == UnitCategory::ARTILLERY) st.artillery++;
        else if (def.category == UnitCategory::MECHANIZED) st.mechanized++;
        else if (r.type == RegimentType::ELITE_SIGNATURE_TANK || r.type == RegimentType::HEAVY_TANK) st.eliteArmor++;
        else st.armor++;
    }
}

void RTSEngine::finishBattleManual(bool playerWon) {
    populateSurvivingStacks();
    result_.finished = true;
    result_.playerWon = playerWon;
}

void RTSEngine::checkVictoryConditions() {
    int alliedHQs = 0;
    int enemyHQs = 0;
    int alliedCities = 0;
    int enemyCities = 0;
    int playerAlliance = players_[0].allianceId;

    for (const auto& b : buildings_) {
        if (b.health <= 0.0f) continue;
        if (b.type == BuildingType::CORPS_HQ || b.type == BuildingType::INFANTRY_HQ ||
            b.type == BuildingType::MECH_HQ || b.type == BuildingType::ARMOR_HQ ||
            b.type == BuildingType::AIRFIELD) {
            if (b.teamId >= 0 && b.teamId < numPlayers_) {
                if (players_[b.teamId].allianceId == playerAlliance) ++alliedHQs;
                else ++enemyHQs;
            }
        }
        if (b.type == BuildingType::CITY_DEPOT) {
            if (b.teamId >= 0 && b.teamId < numPlayers_) {
                if (players_[b.teamId].allianceId == playerAlliance) ++alliedCities;
                else ++enemyCities;
            }
        }
    }

    int alliedCombatRegs = 0;
    int enemyCombatRegs = 0;
    for (const auto& r : regiments_) {
        if (r.health <= 0.0f) continue;
        if (r.teamId >= 0 && r.teamId < numPlayers_) {
            bool isAlliedWithPlayer = (players_[r.teamId].allianceId == playerAlliance);
            if (r.type == RegimentType::PACKED_HQ_TRUCK) {
                if (isAlliedWithPlayer) ++alliedHQs;
                else ++enemyHQs;
            } else {
                if (isAlliedWithPlayer) ++alliedCombatRegs;
                else ++enemyCombatRegs;
            }
        }
    }

    // 1. Decisive Early Knockout (Total Wipeout of all HQs AND all combat regiments)
    if (battleTime_ > 60.0f) {
        if (enemyHQs == 0 && enemyCombatRegs == 0) {
            finishBattleManual(true);
            return;
        }
        if (alliedHQs == 0 && alliedCombatRegs == 0) {
            finishBattleManual(false);
            return;
        }
    }

    // 2. 30-Minute Match Time Limit Conclusion
    constexpr float kMatchDurationLimit = 1800.0f; // 30 minutes
    if (battleTime_ >= kMatchDurationLimit) {
        float alliedMoney = 0.0f;
        float enemyMoney = 0.0f;
        for (int i = 0; i < numPlayers_; ++i) {
            if (players_[i].allianceId == playerAlliance) alliedMoney += players_[i].money;
            else enemyMoney += players_[i].money;
        }
        float alliedScore = alliedCities * 150.0f + alliedHQs * 80.0f + alliedCombatRegs * 20.0f + alliedMoney * 0.1f;
        float enemyScore = enemyCities * 150.0f + enemyHQs * 80.0f + enemyCombatRegs * 20.0f + enemyMoney * 0.1f;
        bool playerWon = (alliedScore >= enemyScore);
        finishBattleManual(playerWon);
        pushLog(playerWon ? "30-MINUTE TIMEOUT: Strategic Victory awarded on points!" : "30-MINUTE TIMEOUT: Enemy holds decisive strategic points.");
    }
}

// ============================================================================
// Player Selection, Movement, Stances, Control Groups & Special Operations
// ============================================================================

void RTSEngine::selectInBox(Vector3 worldMin, Vector3 worldMax, bool additive) {
    float minX = std::min(worldMin.x, worldMax.x);
    float maxX = std::max(worldMin.x, worldMax.x);
    float minZ = std::min(worldMin.z, worldMax.z);
    float maxZ = std::max(worldMin.z, worldMax.z);

    if (!additive) {
        for (auto& r : regiments_) r.selected = false;
        for (auto& b : buildings_) b.selected = false;
    }

    for (auto& r : regiments_) {
        if (r.teamId != 0) continue;
        if (r.pos.x >= minX && r.pos.x <= maxX && r.pos.z >= minZ && r.pos.z <= maxZ) {
            r.selected = true;
        }
    }
}

void RTSEngine::selectSingleAt(Vector3 worldPos, bool additive) {
    if (!additive) {
        for (auto& r : regiments_) r.selected = false;
        for (auto& b : buildings_) b.selected = false;
    }

    for (auto& r : regiments_) {
        if (r.teamId != 0) continue;
        if (distXZ(r.pos, worldPos) <= 6.5f) {
            r.selected = true;
            if (sound_) sound_->playSfx(SfxId::RADIO_SQUELCH, 0.55f);
            return;
        }
    }
    for (auto& b : buildings_) {
        if (b.teamId != 0) continue;
        if (distXZ(b.pos, worldPos) <= 9.0f) {
            b.selected = true;
            if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.5f);
            return;
        }
    }
}

void RTSEngine::selectAllPlayerCombatUnits() {
    for (auto& b : buildings_) b.selected = false;
    for (auto& r : regiments_) {
        if (r.teamId == 0 && r.type != RegimentType::PACKED_HQ_TRUCK) {
            r.selected = true;
        }
    }
    if (sound_) sound_->playSfx(SfxId::RADIO_SQUELCH, 0.65f);
}

void RTSEngine::setSelectedFormationStance(FormationStance stance) {
    int count = 0;
    for (auto& r : regiments_) {
        if (r.teamId == 0 && r.selected) {
            r.stance = stance;
            if (stance == FormationStance::ENTRENCHED) {
                r.targetPos = r.pos;
            }
            ++count;
        }
    }
    if (count > 0 && sound_) {
        sound_->playSfx(SfxId::UI_CLICK, 0.6f);
        const char* sName = (stance == FormationStance::LINE) ? "LINE FORMATION" :
                            (stance == FormationStance::COLUMN ? "ROAD COLUMN (+25% SPEED)" : "ENTRENCHED COVER (+35% DEF)");
        pushLog(std::string("FORMATION ORDER: ") + std::to_string(count) + " regiments set to " + sName + ".");
    }
}

FormationStance RTSEngine::getSelectedFormationStance() const {
    for (const auto& r : regiments_) {
        if (r.teamId == 0 && r.selected) return r.stance;
    }
    return FormationStance::LINE;
}

void RTSEngine::assignControlGroup(int groupNum) {
    int count = 0;
    for (auto& r : regiments_) {
        if (r.teamId != 0) continue;
        if (r.selected) {
            r.controlGroup = groupNum;
            ++count;
        } else if (r.controlGroup == groupNum) {
            r.controlGroup = -1;
        }
    }
    if (count > 0) {
        if (sound_) sound_->playSfx(SfxId::RADIO_BEEP, 0.5f);
        pushLog("CONTROL GROUP #" + std::to_string(groupNum) + " assigned (" + std::to_string(count) + " regiments).");
    }
}

bool RTSEngine::selectControlGroup(int groupNum, Vector3& outCenterPos) {
    int count = 0;
    Vector3 sum = {0.0f, 0.0f, 0.0f};
    for (auto& r : regiments_) {
        if (r.teamId == 0 && r.controlGroup == groupNum) {
            ++count;
            sum = Vector3Add(sum, r.pos);
        }
    }
    if (count == 0) return false;

    for (auto& b : buildings_) b.selected = false;
    for (auto& r : regiments_) {
        r.selected = (r.teamId == 0 && r.controlGroup == groupNum);
    }
    outCenterPos = Vector3Scale(sum, 1.0f / count);
    if (sound_) sound_->playSfx(SfxId::RADIO_SQUELCH, 0.55f);
    return true;
}

void RTSEngine::issueRightClickOrder(Vector3 worldPos) {
    int selectedCount = 0;
    for (const auto& r : regiments_) {
        if (r.teamId == 0 && r.selected && r.health > 0.0f) ++selectedCount;
    }

    if (selectedCount > 0) {
        // Detect if right-click was on an enemy unit or building to focus target
        int enemyRegId = -1;
        float bestRegDist = 11.0f;
        for (const auto& er : regiments_) {
            if (isEnemy(0, er.teamId) && er.health > 0.0f) {
                float d = distXZ(er.pos, worldPos);
                if (d < bestRegDist) {
                    bestRegDist = d;
                    enemyRegId = er.id;
                }
            }
        }
        int enemyBldgId = -1;
        float bestBldgDist = 15.0f;
        if (enemyRegId == -1) {
            for (const auto& eb : buildings_) {
                if (isEnemy(0, eb.teamId) && eb.health > 0.0f) {
                    float d = distXZ(eb.pos, worldPos);
                    if (d < bestBldgDist) {
                        bestBldgDist = d;
                        enemyBldgId = eb.id;
                    }
                }
            }
        }

        // Detect if right-click was on a friendly damaged building for Engineer repair dispatch
        int friendlyDamagedBldgId = -1;
        float bestRepairDist = 18.0f;
        for (const auto& fb : buildings_) {
            if (isAllied(0, fb.teamId) && (fb.health < fb.maxHealth || fb.isBombDamaged)) {
                float d = distXZ(fb.pos, worldPos);
                if (d < bestRepairDist) {
                    bestRepairDist = d;
                    friendlyDamagedBldgId = fb.id;
                }
            }
        }

        int idx = 0;
        int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(selectedCount)))));
        for (auto& r : regiments_) {
            if (r.teamId != 0 || !r.selected || r.health <= 0.0f) continue;
            if (r.stance == FormationStance::ENTRENCHED) {
                r.stance = FormationStance::LINE; // Automatically leave entrenchment on move order
            }
            int row = idx / cols;
            int col = idx % cols;
            float ox = (col - (cols - 1) * 0.5f) * 14.5f;
            float oz = row * 11.5f;
            r.targetPos = {
                std::clamp(worldPos.x + ox, -535.0f, 535.0f),
                0.0f,
                std::clamp(worldPos.z + oz, -535.0f, 535.0f)
            };

            if (r.type == RegimentType::ENGINEER_FLAME_REGIMENT && friendlyDamagedBldgId != -1) {
                r.targetRepairBuildingId = friendlyDamagedBldgId;
                r.targetRegimentId = -1;
                r.targetBuildingId = -1;
                r.attackMove = false;
                pushLog("COMBAT ENGINEERS: Order confirmed. Deploying to repair damaged structure!");
            } else {
                r.targetRepairBuildingId = -1;
                r.targetRegimentId = enemyRegId;
                r.targetBuildingId = enemyBldgId;
                r.attackMove = (enemyRegId != -1 || enemyBldgId != -1);
            }
            ++idx;
        }
        if (sound_) {
            if (enemyRegId != -1 || enemyBldgId != -1 || friendlyDamagedBldgId != -1) {
                sound_->playSfx(SfxId::RADIO_SQUELCH, 0.70f);
            } else {
                sound_->playSfx(SfxId::RADIO_SQUELCH, 0.52f);
            }
        }
    } else {
        for (auto& b : buildings_) {
            if (b.teamId == 0 && b.selected) {
                b.rallyPoint = worldPos;
                if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.4f);
            }
        }
    }
}

void RTSEngine::issueAttackMoveOrder(Vector3 worldPos) {
    issueRightClickOrder(worldPos);
    for (auto& r : regiments_) {
        if (r.teamId == 0 && r.selected && r.health > 0.0f) {
            r.attackMove = true;
        }
    }
}

void RTSEngine::stopSelectedUnits() {
    for (auto& r : regiments_) {
        if (r.teamId == 0 && r.selected) {
            r.targetPos = r.pos;
            r.attackMove = false;
            r.targetRegimentId = -1;
            r.targetBuildingId = -1;
        }
    }
}

bool RTSEngine::queueUnitProduction(int buildingId, RegimentType regType) {
    const auto& gd = GameData::instance();
    const auto& rDef = gd.getRegiment(regType);

    for (auto& b : buildings_) {
        if (b.id != buildingId || b.teamId < 0) continue;
        if (b.health <= 0.0f) {
            if (b.teamId == 0) pushLog("STRUCTURE RUINED: Cannot muster regiments from a destroyed building!");
            return false;
        }
        if (b.isUnderConstruction) {
            if (b.teamId == 0) pushLog("UNDER CONSTRUCTION: Structure must finish building before mustering regiments!");
            return false;
        }

        // Must have an operational Corps HQ or Mobile HQ Convoy to muster regiments
        bool hasCorpsHQ = false;
        for (const auto& otherB : buildings_) {
            if (otherB.teamId == b.teamId && otherB.type == BuildingType::CORPS_HQ && otherB.health > 0.0f && !otherB.isUnderConstruction) {
                hasCorpsHQ = true;
                break;
            }
        }
        if (!hasCorpsHQ) {
            for (const auto& r : regiments_) {
                if (r.teamId == b.teamId && r.type == RegimentType::PACKED_HQ_TRUCK && r.health > 0.0f) {
                    hasCorpsHQ = true;
                    break;
                }
            }
        }
        if (!hasCorpsHQ) {
            if (b.teamId == 0) pushLog("COMMAND SEVERED: Corps HQ destroyed! Rebuild or unpack Corps HQ to muster regiments.");
            return false;
        }

        auto& p = players_[b.teamId];
        const auto& gen = gd.getGeneral(p.nation, p.generalIdx);

        if (b.prodQueue.size() >= 10) {
            if (b.teamId == 0) pushLog("PRODUCTION QUEUE FULL: Maximum 10 units in queue.");
            return false;
        }

        if (p.techTier < rDef.techTierRequired) {
            if (b.teamId == 0) pushLog("TECH REQUIRED: Upgrade Corps HQ to Tier " + std::to_string(rDef.techTierRequired) + " first!");
            return false;
        }

        float costMult = (rDef.category == UnitCategory::INFANTRY) ? gen.infCostMult :
                         (rDef.category == UnitCategory::ARMOR ? gen.armorCostMult : 1.0f);
        float mCost = rDef.costMoney * costMult;
        float aCost = static_cast<float>(rDef.costAmmo);
        float oCost = static_cast<float>(rDef.costOil);

        if (p.money < mCost || p.ammo < aCost || p.oil < oCost) {
            if (b.teamId == 0) pushLog("INSUFFICIENT SUPPLIES: Need more Money, Ammo, or Oil.");
            return false;
        }

        p.money -= mCost;
        p.ammo -= aCost;
        p.oil -= oCost;
        b.prodQueue.push_back(regType);
        if (b.teamId == 0 && sound_) sound_->playSfx(SfxId::UI_CLICK, 0.65f);
        return true;
    }
    return false;
}

bool RTSEngine::cancelProductionQueueSlot(int buildingId, int slotIdx) {
    const auto& gd = GameData::instance();
    for (auto& b : buildings_) {
        if (b.id != buildingId || b.teamId < 0) continue;
        if (slotIdx < 0 || slotIdx >= static_cast<int>(b.prodQueue.size())) return false;

        RegimentType rt = b.prodQueue[slotIdx];
        const auto& rDef = gd.getRegiment(rt);
        auto& p = players_[b.teamId];
        const auto& gen = gd.getGeneral(p.nation, p.generalIdx);

        float costMult = (rDef.category == UnitCategory::INFANTRY) ? gen.infCostMult :
                         (rDef.category == UnitCategory::ARMOR ? gen.armorCostMult : 1.0f);
        float mCost = rDef.costMoney * costMult;
        float aCost = static_cast<float>(rDef.costAmmo);
        float oCost = static_cast<float>(rDef.costOil);

        p.money += mCost;
        p.ammo  += aCost;
        p.oil   += oCost;

        b.prodQueue.erase(b.prodQueue.begin() + slotIdx);
        if (slotIdx == 0) {
            b.prodProgress = 0.0f;
        }

        if (b.teamId == 0) {
            if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.7f);
            pushLog("CANCELED: " + gd.getRegimentDisplayName(b.nation, rt) + " (+$" +
                    std::to_string(static_cast<int>(mCost)) + ", +" +
                    std::to_string(static_cast<int>(aCost)) + "A, +" +
                    std::to_string(static_cast<int>(oCost)) + "O refunded).");
        }
        return true;
    }
    return false;
}

bool RTSEngine::cancelUnitTypeProduction(int buildingId, RegimentType regType) {
    for (auto& b : buildings_) {
        if (b.id != buildingId || b.teamId < 0) continue;
        // Search backwards to cancel the latest queued unit of this type
        for (int i = static_cast<int>(b.prodQueue.size()) - 1; i >= 0; --i) {
            if (b.prodQueue[i] == regType) {
                return cancelProductionQueueSlot(buildingId, i);
            }
        }
    }
    return false;
}

int RTSEngine::getOperationalAirfieldCount(int teamId) const {
    int count = 0;
    for (const auto& b : buildings_) {
        if (b.teamId == teamId && b.type == BuildingType::AIRFIELD && b.health > 0.0f && !b.isUnderConstruction) {
            count++;
        }
    }
    return count;
}

int RTSEngine::getActiveAirSortiesCount(int teamId) const {
    int count = 0;
    for (const auto& as : airStrikes_) {
        if (as.teamId == teamId && !as.isShotDown && !as.isKamikaze) {
            count++;
        }
    }
    return count;
}

int RTSEngine::getMaxAirSorties(int teamId) const {
    int capacity = 0;
    for (const auto& b : buildings_) {
        if (b.teamId == teamId && b.type == BuildingType::AIRFIELD && b.health > 0.0f && !b.isUnderConstruction) {
            float healthRatio = b.health / b.maxHealth;
            if (healthRatio < 0.35f) {
                // Heavily damaged runway disabled until repaired by Engineers!
                capacity += 0;
            } else if (b.isBombDamaged || healthRatio < 0.75f) {
                // Damaged or bomb-struck airfield: turnaround/servicing degraded to 1 sortie
                capacity += 1;
            } else {
                capacity += 2; // Fully operational airfield: 2 concurrent sorties
            }
        }
    }
    return capacity;
}

const RTSBuilding* RTSEngine::findAvailableAirfield(int teamId, Vector3 targetPos) const {
    const RTSBuilding* best = nullptr;
    float bestDist = 1e9f;
    for (const auto& b : buildings_) {
        if (b.teamId == teamId && b.type == BuildingType::AIRFIELD && b.health > 0.0f && !b.isUnderConstruction) {
            float d = distXZ(b.pos, targetPos);
            if (d < bestDist) {
                bestDist = d;
                best = &b;
            }
        }
    }
    return best;
}

bool RTSEngine::launchAirReconMission(int teamId, Vector3 targetPos) {
    if (teamId < 0 || teamId >= numPlayers_) return false;
    auto& p = players_[teamId];

    // Airfield Requirement: Team must possess at least one operational Airfield
    int airfieldCount = getOperationalAirfieldCount(teamId);
    if (airfieldCount <= 0) {
        if (teamId == 0) pushLog("AIR OPERATIONS GROUNDED: Operational Airfield required! Construct an Airfield base to launch air missions.");
        return false;
    }

    // Sortie Capacity Limit: 2 active flights per operational Airfield
    int activeSorties = getActiveAirSortiesCount(teamId);
    int maxSorties = getMaxAirSorties(teamId);
    if (activeSorties >= maxSorties) {
        if (teamId == 0) {
            pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for aircraft to return or construct an additional Airfield.",
                               activeSorties, maxSorties));
        }
        return false;
    }

    float costMoney = 75.0f;
    float costOil = 20.0f;
    if (p.money < costMoney || p.oil < costOil) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES: Need $75 Money and 20 Oil for Air Recon.");
        return false;
    }

    const RTSBuilding* af = findAvailableAirfield(teamId, targetPos);
    if (!af) return false;

    p.money -= costMoney;
    p.oil -= costOil;

    RTSAirStrike as{};
    as.id = nextEntityId_++;
    as.teamId = teamId;
    as.nation = p.nation;
    as.isRecon = true;
    as.speed = 0.38f;
    as.health = 130.0f;
    as.maxHealth = 130.0f;
    as.radius = 85.0f;
    as.damage = 0.0f;

    Vector3 toTarget = Vector3Subtract(targetPos, af->pos);
    float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    Vector3 flightDir = (d > 1.0f) ? Vector3Scale(toTarget, 1.0f / d) : Vector3{0.0f, 0.0f, (teamId == 0 ? -1.0f : 1.0f)};
    float runup = std::max(18.0f, std::min(45.0f, d * 0.20f));
    as.startPos = {af->pos.x - flightDir.x * runup, 24.0f, af->pos.z - flightDir.z * runup};
    as.targetPos = targetPos;
    as.currentPos = as.startPos;
    as.facing = std::atan2(flightDir.x, flightDir.z);

    airStrikes_.push_back(as);
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.75f);
        pushLog(TextFormat("AIR RECON DISPATCHED (%d/%d Sorties Active): Photo recon plane launched from Airfield.",
                           activeSorties + 1, maxSorties));
    } else {
        pushLog("AIR WARNING: Hostile reconnaissance plane spotted overhead!");
    }
    return true;
}

bool RTSEngine::launchAirBombingMission(int teamId, Vector3 targetPos) {
    if (teamId < 0 || teamId >= numPlayers_) return false;
    auto& p = players_[teamId];

    // Airfield Requirement: Team must possess at least one operational Airfield
    int airfieldCount = getOperationalAirfieldCount(teamId);
    if (airfieldCount <= 0) {
        if (teamId == 0) pushLog("AIR OPERATIONS GROUNDED: Operational Airfield required! Construct an Airfield base to launch bombing runs.");
        return false;
    }

    // Sortie Capacity Limit: 2 active flights per operational Airfield
    int activeSorties = getActiveAirSortiesCount(teamId);
    int maxSorties = getMaxAirSorties(teamId);
    if (activeSorties >= maxSorties) {
        if (teamId == 0) {
            pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for bombers to return or construct an additional Airfield.",
                               activeSorties, maxSorties));
        }
        return false;
    }

    if (p.techTier < 2) {
        if (teamId == 0) pushLog("TECH REQUIRED: Upgrade Corps HQ to Tier 2 for Tactical Bombing Runs!");
        return false;
    }
    float costMoney = 165.0f;
    float costAmmo = 40.0f;
    float costOil = 45.0f;
    if (p.money < costMoney || p.ammo < costAmmo || p.oil < costOil) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES: Need $165 Money, 40 Ammo, 45 Oil for Bombing Run.");
        return false;
    }

    const RTSBuilding* af = findAvailableAirfield(teamId, targetPos);
    if (!af) return false;

    p.money -= costMoney;
    p.ammo -= costAmmo;
    p.oil -= costOil;

    RTSAirStrike as{};
    as.id = nextEntityId_++;
    as.teamId = teamId;
    as.nation = p.nation;
    as.isBombing = true;
    as.speed = 0.28f;
    as.health = 220.0f;
    as.maxHealth = 220.0f;
    as.radius = 28.0f;
    as.damage = 260.0f;

    Vector3 toTarget = Vector3Subtract(targetPos, af->pos);
    float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    Vector3 flightDir = (d > 1.0f) ? Vector3Scale(toTarget, 1.0f / d) : Vector3{0.0f, 0.0f, (teamId == 0 ? -1.0f : 1.0f)};
    float runup = std::max(18.0f, std::min(45.0f, d * 0.20f));
    as.startPos = {af->pos.x - flightDir.x * runup, 24.0f, af->pos.z - flightDir.z * runup};
    as.targetPos = targetPos;
    as.currentPos = as.startPos;
    as.facing = std::atan2(flightDir.x, flightDir.z);

    airStrikes_.push_back(as);
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.75f);
        pushLog(TextFormat("BOMBERS EN ROUTE (%d/%d Sorties Active): Tactical bomber squadron launched from Airfield.",
                           activeSorties + 1, maxSorties));
    } else {
        pushLog("AIR RAID SIREN: Hostile bomber squadron incoming! Scramble AA defenses!");
    }
    return true;
}

bool RTSEngine::launchAirSupplyDrop(int teamId, Vector3 targetPos) {
    if (teamId < 0 || teamId >= numPlayers_) return false;
    auto& p = players_[teamId];

    // Airfield Requirement: Team must possess at least one operational Airfield
    int airfieldCount = getOperationalAirfieldCount(teamId);
    if (airfieldCount <= 0) {
        if (teamId == 0) pushLog("AIR OPERATIONS GROUNDED: Operational Airfield required! Construct an Airfield base to call Air Supply Drops.");
        return false;
    }

    // Sortie Capacity Limit: 2 active flights per operational Airfield
    int activeSorties = getActiveAirSortiesCount(teamId);
    int maxSorties = getMaxAirSorties(teamId);
    if (activeSorties >= maxSorties) {
        if (teamId == 0) {
            pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for transport to return or construct an additional Airfield.",
                               activeSorties, maxSorties));
        }
        return false;
    }

    float costMoney = 120.0f;
    float costOil = 20.0f;
    if (p.money < costMoney || p.oil < costOil) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES: Need $120 Money and 20 Oil for Supply Drop.");
        return false;
    }

    const RTSBuilding* af = findAvailableAirfield(teamId, targetPos);
    if (!af) return false;

    p.money -= costMoney;
    p.oil -= costOil;

    RTSAirStrike as{};
    as.id = nextEntityId_++;
    as.teamId = teamId;
    as.nation = p.nation;
    as.isSupplyDrop = true;
    as.speed = 0.32f;
    as.health = 180.0f;
    as.maxHealth = 180.0f;
    as.radius = 24.0f;

    Vector3 toTarget = Vector3Subtract(targetPos, af->pos);
    float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    Vector3 flightDir = (d > 1.0f) ? Vector3Scale(toTarget, 1.0f / d) : Vector3{0.0f, 0.0f, (teamId == 0 ? -1.0f : 1.0f)};
    float runup = std::max(18.0f, std::min(45.0f, d * 0.20f));
    as.startPos = {af->pos.x - flightDir.x * runup, 24.0f, af->pos.z - flightDir.z * runup};
    as.targetPos = targetPos;
    as.currentPos = as.startPos;
    as.facing = std::atan2(flightDir.x, flightDir.z);

    airStrikes_.push_back(as);
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.75f);
        pushLog(TextFormat("SUPPLY FLIGHT (%d/%d Sorties Active): Transport plane launched from Airfield with emergency supplies.",
                           activeSorties + 1, maxSorties));
    }
    return true;
}

bool RTSEngine::launchAirParatroopDrop(int teamId, Vector3 targetPos) {
    if (teamId < 0 || teamId >= numPlayers_) return false;
    auto& p = players_[teamId];

    // Airfield Requirement: Team must possess at least one operational Airfield
    int airfieldCount = getOperationalAirfieldCount(teamId);
    if (airfieldCount <= 0) {
        if (teamId == 0) pushLog("AIR OPERATIONS GROUNDED: Operational Airfield required! Construct an Airfield base to launch Airborne drops.");
        return false;
    }

    // Sortie Capacity Limit: 2 active flights per operational Airfield
    int activeSorties = getActiveAirSortiesCount(teamId);
    int maxSorties = getMaxAirSorties(teamId);
    if (activeSorties >= maxSorties) {
        if (teamId == 0) {
            pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for transport to return or construct an additional Airfield.",
                               activeSorties, maxSorties));
        }
        return false;
    }

    if (p.techTier < 2) {
        if (teamId == 0) pushLog("TECH REQUIRED: Upgrade Corps HQ to Tier 2 for Airborne Operations!");
        return false;
    }
    float costMoney = 180.0f;
    float costAmmo = 35.0f;
    if (p.money < costMoney || p.ammo < costAmmo) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES: Need $180 Money and 35 Ammo for Airborne Drop.");
        return false;
    }

    const RTSBuilding* af = findAvailableAirfield(teamId, targetPos);
    if (!af) return false;

    p.money -= costMoney;
    p.ammo -= costAmmo;

    RTSAirStrike as{};
    as.id = nextEntityId_++;
    as.teamId = teamId;
    as.nation = p.nation;
    as.isParatroopDrop = true;
    as.speed = 0.30f;
    as.health = 190.0f;
    as.maxHealth = 190.0f;
    as.radius = 18.0f;

    Vector3 toTarget = Vector3Subtract(targetPos, af->pos);
    float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    Vector3 flightDir = (d > 1.0f) ? Vector3Scale(toTarget, 1.0f / d) : Vector3{0.0f, 0.0f, (teamId == 0 ? -1.0f : 1.0f)};
    float runup = std::max(18.0f, std::min(45.0f, d * 0.20f));
    as.startPos = {af->pos.x - flightDir.x * runup, 24.0f, af->pos.z - flightDir.z * runup};
    as.targetPos = targetPos;
    as.currentPos = as.startPos;
    as.facing = std::atan2(flightDir.x, flightDir.z);

    airStrikes_.push_back(as);
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.75f);
        pushLog(TextFormat("AIRBORNE INSERTION (%d/%d Sorties Active): Transport plane airborne with Paratrooper & Bazooka squad.",
                           activeSorties + 1, maxSorties));
    }
    return true;
}

bool RTSEngine::launchAirFighterSweep(int teamId, Vector3 targetPos) {
    if (teamId < 0 || teamId >= numPlayers_) return false;
    auto& p = players_[teamId];

    int airfieldCount = getOperationalAirfieldCount(teamId);
    if (airfieldCount <= 0) {
        if (teamId == 0) pushLog("AIR COMMAND: Operational Airfield required to scramble Fighter Interceptors!");
        return false;
    }

    int activeSorties = getActiveAirSortiesCount(teamId);
    int maxSorties = getMaxAirSorties(teamId);
    if (activeSorties >= maxSorties) {
        if (teamId == 0) {
            pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for fighters to return or construct an additional Airfield.",
                               activeSorties, maxSorties));
        }
        return false;
    }

    float costMoney = 140.0f;
    float costAmmo = 30.0f;
    float costOil = 35.0f;
    if (p.money < costMoney || p.ammo < costAmmo || p.oil < costOil) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES: Need $140 Money, 30 Ammo, 35 Oil for Fighter Combat Air Patrol.");
        return false;
    }

    const RTSBuilding* af = findAvailableAirfield(teamId, targetPos);
    if (!af) return false;

    p.money -= costMoney;
    p.ammo -= costAmmo;
    p.oil -= costOil;

    RTSAirStrike as{};
    as.id = nextEntityId_++;
    as.teamId = teamId;
    as.nation = p.nation;
    as.isFighter = true;
    as.speed = 0.48f;
    as.health = 185.0f;
    as.maxHealth = 185.0f;
    as.radius = 45.0f;

    Vector3 toTarget = Vector3Subtract(targetPos, af->pos);
    float d = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    Vector3 flightDir = (d > 1.0f) ? Vector3Scale(toTarget, 1.0f / d) : Vector3{0.0f, 0.0f, (teamId == 0 ? -1.0f : 1.0f)};
    float runup = std::max(22.0f, std::min(55.0f, d * 0.22f));
    as.startPos = {af->pos.x - flightDir.x * runup, 38.0f, af->pos.z - flightDir.z * runup};
    as.targetPos = targetPos;
    as.currentPos = as.startPos;
    as.facing = std::atan2(flightDir.x, flightDir.z);

    airStrikes_.push_back(as);
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.75f);
        pushLog(TextFormat("COMBAT AIR PATROL (%d/%d Sorties Active): Interceptor fighter squadron scrambled for air superiority sweep.",
                           activeSorties + 1, maxSorties));
        setAlertPos(targetPos);
    }
    return true;
}

bool RTSEngine::triggerRegimentAbility(int regimentId, int abilityIdx) {
    (void)abilityIdx;
    for (auto& r : regiments_) {
        if (r.id != regimentId || r.health <= 0.0f || r.teamId != 0) continue;
        if (r.veterancy < 1) {
            pushLog("ABILITY LOCKED: Unit must achieve Veteran rank (*) to unlock specialized field tactics!");
            return false;
        }
        if (r.abilityCooldown > 0.0f) {
            pushLog(TextFormat("TACTICAL COOLDOWN: Ability ready in %.1fs.", r.abilityCooldown));
            return false;
        }

        const auto& gd = GameData::instance();
        const auto& def = gd.getRegiment(r.type);
        if (def.category == UnitCategory::ARMOR || def.category == UnitCategory::MECHANIZED) {
            // Smoke Screen Canister Discharge
            r.smokeScreenTimer = 9.0f;
            r.abilityCooldown = 24.0f;
            for (int i = 0; i < 14; ++i) {
                float ang = i * (2.0f * PI / 14.0f);
                float dist = 4.0f + (i % 3) * 3.5f;
                RTSParticle p{};
                p.pos = {r.pos.x + std::cos(ang) * dist, r.pos.y + 0.8f, r.pos.z + std::sin(ang) * dist};
                p.vel = {std::cos(ang) * 1.5f, 1.8f, std::sin(ang) * 1.5f};
                p.color = Color{235, 235, 235, 220};
                p.size = 2.4f;
                p.life = 7.5f;
                p.maxLife = 7.5f;
                particles_.push_back(p);
            }
            if (sound_) sound_->playSfx3D(SfxId::MORTAR_THUMP, r.pos, 0.7f);
            pushLog("SMOKE CANISTERS DISCHARGED: Dense smoke obscures " + gd.getRegimentDisplayName(r.nation, r.type) + " (-55% enemy targeting accuracy).");
            return true;
        } else {
            // Rapid Suppression Volley
            r.rapidVolleyTimer = 8.0f;
            r.abilityCooldown = 20.0f;
            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.8f);
            pushLog("RAPID SUPPRESSION VOLLEY: " + gd.getRegimentDisplayName(r.nation, r.type) + " unleashed sustained fire! (+100% rate of fire).");
            return true;
        }
    }
    return false;
}

bool RTSEngine::constructSupportBuilding(BuildingType bType, Vector3 pos, int teamId) {
    const auto& gd = GameData::instance();
    const auto& def = gd.getBuilding(bType);
    auto& p = players_[teamId];

    // Building support structures strictly requires an operational Corps HQ or Mobile HQ Truck!
    // If the faction's Corps HQ was demolished, base expansion is blocked until HQ is recovered.
    bool hasCorpsHQ = false;
    for (const auto& b : buildings_) {
        if (b.teamId == teamId && b.type == BuildingType::CORPS_HQ && b.health > 0.0f && !b.isUnderConstruction) {
            hasCorpsHQ = true;
            break;
        }
    }
    for (const auto& r : regiments_) {
        if (r.teamId == teamId && r.type == RegimentType::PACKED_HQ_TRUCK && r.health > 0.0f) {
            hasCorpsHQ = true;
            break;
        }
    }
    if (!hasCorpsHQ) {
        if (teamId == 0) pushLog("CORPS HQ DESTROYED: Deploy or unpack a Corps HQ before constructing support structures!");
        return false;
    }

    if (p.techTier < def.techTierRequired) {
        if (teamId == 0) pushLog("TECH REQUIRED: Upgrade Corps HQ to Tier " + std::to_string(def.techTierRequired) + " first!");
        return false;
    }
    if (p.money < def.costMoney || p.ammo < def.costAmmo || p.oil < def.costOil) {
        if (teamId == 0) pushLog("INSUFFICIENT RESOURCES to deploy " + def.name + "!");
        return false;
    }

    if (isWaterAt(pos.x, pos.z)) {
        if (teamId == 0) pushLog("INVALID SITE: Cannot construct buildings in river or water!");
        return false;
    }
    for (const auto& existing : buildings_) {
        if (existing.health <= 0.0f) continue;
        if (distXZ(pos, existing.pos) < 16.0f) {
            if (teamId == 0) pushLog("INVALID SITE: Location blocked by another structure!");
            return false;
        }
    }

    p.money -= def.costMoney;
    p.ammo -= def.costAmmo;
    p.oil -= def.costOil;

    // Construct with realistic construction duration & active scaffolding site
    spawnBuilding(teamId, bType, pos, true);
    updateSupplyNetwork();
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.75f);
        pushLog(TextFormat("CONSTRUCTING [%s]: Site established (%.0fs build time). Construction crews dispatched.",
                           def.name.c_str(), def.buildTime));
    }
    return true;
}

bool RTSEngine::cancelConstruction(int buildingId) {
    const auto& gd = GameData::instance();
    for (auto it = buildings_.begin(); it != buildings_.end(); ++it) {
        if (it->id != buildingId || !it->isUnderConstruction) continue;
        const auto& def = gd.getBuilding(it->type);
        int team = it->teamId;
        // Refund 100% of resources
        players_[team].money += def.costMoney;
        players_[team].ammo  += def.costAmmo;
        players_[team].oil   += def.costOil;

        if (team == 0) {
            if (sound_) sound_->playSfx(SfxId::UI_CLICK, 0.7f);
            pushLog(TextFormat("CONSTRUCTION CANCELED: %s site cleared. 100%% resources refunded ($%d, %dA, %dO).",
                               def.name.c_str(), def.costMoney, def.costAmmo, def.costOil));
        }
        buildings_.erase(it);
        updateSupplyNetwork();
        return true;
    }
    return false;
}

bool RTSEngine::togglePackBuilding(int buildingId) {
    const auto& gd = GameData::instance();
    for (auto it = buildings_.begin(); it != buildings_.end(); ++it) {
        if (it->id != buildingId) continue;
        const auto& def = gd.getBuilding(it->type);
        if (!def.canPackMove) {
            pushLog("STATIC STRUCTURE: " + def.name + " cannot pack into a truck.");
            return false;
        }

        Vector3 pos = it->pos;
        int team = it->teamId;
        BuildingType bt = it->type;
        buildings_.erase(it);

        int truckId = spawnRegiment(team, RegimentType::PACKED_HQ_TRUCK, pos, bt);
        for (auto& r : regiments_) {
            if (r.id == truckId) r.selected = (team == 0);
        }
        updateSupplyNetwork();
        if (sound_) sound_->playSfx(SfxId::HQ_PACK, 0.75f);
        pushLog("MOBILE HQ PACKED: " + def.name + " is now a mobile supply convoy!");
        return true;
    }
    return false;
}

bool RTSEngine::unpackTruckRegiment(int regimentId) {
    const auto& gd = GameData::instance();
    for (auto it = regiments_.begin(); it != regiments_.end(); ++it) {
        if (it->id != regimentId || it->type != RegimentType::PACKED_HQ_TRUCK) continue;
        if (isWaterAt(it->pos.x, it->pos.z)) {
            if (it->teamId == 0) pushLog("CANNOT UNPACK: Cannot establish headquarters in river or water!");
            return false;
        }
        Vector3 pos = it->pos;
        int team = it->teamId;
        BuildingType bt = it->packedBuildingType;
        regiments_.erase(it);

        int bId = spawnBuilding(team, bt, pos);
        for (auto& b : buildings_) {
            if (b.id == bId) b.selected = (team == 0);
        }
        updateSupplyNetwork();
        if (sound_) sound_->playSfx(SfxId::HQ_UNPACK, 0.8f);
        pushLog("HQ DEPLOYED: " + gd.getBuilding(bt).name + " unpacked and projecting supply lines!");
        return true;
    }
    return false;
}

bool RTSEngine::upgradeCorpsHQTech(int teamId) {
    auto& p = players_[teamId];
    if (p.techTier >= 3) return false;

    // Upgrading tech tier requires an active, operational Corps HQ
    bool hasCorpsHQ = false;
    for (const auto& b : buildings_) {
        if (b.teamId == teamId && b.type == BuildingType::CORPS_HQ && b.health > 0.0f && !b.isUnderConstruction) {
            hasCorpsHQ = true;
            break;
        }
    }
    if (!hasCorpsHQ) {
        if (teamId == 0) pushLog("CORPS HQ REQUIRED: Cannot advance Tech Tier without an operational Corps HQ!");
        return false;
    }

    float cost = (p.techTier == 1) ? 260.0f : 380.0f;
    if (p.money < cost) {
        if (teamId == 0) pushLog("NEED $" + std::to_string(static_cast<int>(cost)) + " to upgrade Corps HQ Tech Tier.");
        return false;
    }
    p.money -= cost;
    p.techTier++;
    if (teamId == 0) {
        if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.85f);
        pushLog("TECH UPGRADED: Corps HQ advanced to Tech Tier " + std::to_string(p.techTier) + "!");
    }
    return true;
}

bool RTSEngine::triggerSpecialOp(int teamId, int opIndex, Vector3 targetPos) {
    if (opIndex < 0 || opIndex >= 3) return false;
    auto& p = players_[teamId];
    const auto& gd = GameData::instance();
    const auto& gen = gd.getGeneral(p.nation, p.generalIdx);
    const auto& op = gen.specialOps[opIndex];

    if (p.specOpCooldowns[opIndex] > 0.0f) return false;
    if (p.commandPoints < op.commandPointCost) {
        if (teamId == 0) pushLog("INSUFFICIENT COMMAND POINTS (" + std::to_string(op.commandPointCost) + " CP required).");
        return false;
    }

    p.commandPoints -= op.commandPointCost;
    p.specOpCooldowns[opIndex] = op.cooldownSec;

    pushLog("SPECIAL OPERATION: " + gen.name + " unleashed [" + op.name + "]!");

    if (op.targetType == SpecOpTargetType::GLOBAL) {
        ActiveBuff buff{};
        buff.name = op.name;
        buff.remainingSec = std::max(14.0f, op.durationSec);
        buff.speedMult = 1.35f;
        buff.attackMult = 1.28f;
        buff.defenseMult = 1.25f;
        buff.ignoreSupplyPenalty = true;
        p.activeBuffs.push_back(buff);
        p.radarRevealTimer = std::max(p.radarRevealTimer, 20.0f);

        for (auto& r : regiments_) {
            if (r.teamId == teamId) {
                r.organization = r.maxOrganization;
                r.health = std::min(r.maxHealth, r.health + r.maxHealth * 0.25f);
            }
        }
        for (auto& b : buildings_) {
            if (b.teamId == teamId) {
                b.health = std::min(b.maxHealth, b.health + b.maxHealth * 0.25f);
            }
        }
        if (op.id == "ural_gear_up") {
            p.money += 200.0f;
            p.ammo += 200.0f;
            p.oil += 200.0f;
        }
        if (sound_) {
            sound_->playSfx(p.nation == NationId::JPN ? SfxId::BANZAI_HORN : SfxId::VICTORY_FANFARE, 0.75f);
        }
    } else if (op.targetType == SpecOpTargetType::HQ_SPAWN) {
        Vector3 hqPos = (teamId < static_cast<int>(playerBasePositions_.size())) ? playerBasePositions_[teamId] : Vector3{0.0f, 0.0f, (teamId == 0 ? 440.0f : -440.0f)};
        for (const auto& b : buildings_) {
            if (b.teamId == teamId && b.type == BuildingType::CORPS_HQ) {
                hqPos = b.rallyPoint;
                break;
            }
        }
        for (const auto& r : regiments_) {
            if (r.teamId == teamId && r.type == RegimentType::PACKED_HQ_TRUCK) {
                hqPos = r.pos;
                break;
            }
        }
        if (op.id == "tiger_callin") {
            spawnRegiment(teamId, RegimentType::HEAVY_TANK, hqPos);
        } else if (op.id == "guards_reserve") {
            spawnRegiment(teamId, RegimentType::MEDIUM_TANK, {hqPos.x - 6.0f, 0.0f, hqPos.z});
            spawnRegiment(teamId, RegimentType::RIFLE_REGIMENT, {hqPos.x + 6.0f, 0.0f, hqPos.z});
        } else if (op.id == "motherland_calls") {
            spawnRegiment(teamId, RegimentType::RIFLE_REGIMENT, {hqPos.x - 6.0f, 0.0f, hqPos.z});
            spawnRegiment(teamId, RegimentType::SNIPER_REGIMENT, {hqPos.x + 6.0f, 0.0f, hqPos.z});
        } else {
            spawnRegiment(teamId, RegimentType::RIFLE_REGIMENT, {hqPos.x - 6.0f, 0.0f, hqPos.z});
            spawnRegiment(teamId, RegimentType::ENGINEER_FLAME_REGIMENT, {hqPos.x + 6.0f, 0.0f, hqPos.z});
        }
        if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.8f);
    } else {
        if (op.id == "airborne_101" || op.id == "fallschirmjager" || op.id == "chindit_drop" || op.id == "partisan_uprising" || op.id == "imperial_flank") {
            RTSAirStrike as{};
            as.teamId = teamId;
            as.nation = p.nation;
            as.startPos = {-150.0f, 45.0f, targetPos.z};
            as.targetPos = targetPos;
            as.currentPos = as.startPos;
            as.isParatroopDrop = true;
            as.radius = op.radius;
            airStrikes_.push_back(as);
            if (sound_) sound_->playSfx(SfxId::AIR_RAID_SIREN, 0.65f);
        } else if (op.id == "c47_supply") {
            RTSAirStrike as{};
            as.teamId = teamId;
            as.nation = p.nation;
            as.startPos = {-150.0f, 45.0f, targetPos.z};
            as.targetPos = targetPos;
            as.currentPos = as.startPos;
            as.isSupplyDrop = true;
            as.radius = op.radius;
            airStrikes_.push_back(as);
            if (sound_) sound_->playSfx(SfxId::UI_CONFIRM, 0.7f);
        } else if (op.id == "flak_ambush" || op.id == "tunnel_network") {
            spawnBuilding(teamId, op.id == "flak_ambush" ? BuildingType::AA_BATTERY : BuildingType::BUNKER, targetPos);
            spawnRegiment(teamId, RegimentType::AT_BAZOOKA_REGIMENT, {targetPos.x + 5.0f, 0.0f, targetPos.z});
            if (sound_) sound_->playSfx(SfxId::HQ_UNPACK, 0.75f);
        } else {
            RTSAirStrike as{};
            as.teamId = teamId;
            as.nation = p.nation;
            as.startPos = {targetPos.x - 130.0f, 48.0f, targetPos.z - 40.0f};
            as.targetPos = targetPos;
            as.currentPos = as.startPos;
            as.isKamikaze = (op.id == "kamikaze_strike" || op.id == "ohka_strike" || op.id == "v2_strike");
            as.radius = op.radius;
            as.damage = 280.0f;
            airStrikes_.push_back(as);

            if (sound_) {
                if (op.id == "stuka_dive") sound_->playSfx3D(SfxId::STUKA_SIREN, targetPos, 0.95f, 1.0f, 360.0f);
                else if (op.id == "katyusha_salvo" || op.id == "volga_rockets" || op.id == "nebelwerfer") sound_->playSfx3D(SfxId::KATYUSHA_SALVO, targetPos, 0.90f, 1.0f, 360.0f);
                else sound_->playSfx3D(SfxId::AIR_RAID_SIREN, targetPos, 0.85f, 1.0f, 360.0f);
            }
        }
    }
    return true;
}

int RTSEngine::getAlliedTeamId() const {
    for (int i = 1; i < numPlayers_; ++i) {
        if (isAllied(0, i)) return i;
    }
    return -1;
}

void RTSEngine::issueAlliedPingOrder(Vector3 targetPos) {
    int allyTeam = getAlliedTeamId();
    if (allyTeam < 0) return;

    alliedPingPos_ = targetPos;
    alliedPingTimer_ = 8.0f; // 8-second operational coordination window

    const auto& gd = GameData::instance();
    const auto& aNat = gd.getNation(players_[allyTeam].nation);
    const auto& aGen = gd.getGeneral(players_[allyTeam].nation, players_[allyTeam].generalIdx);
    pushLog(TextFormat("ALLIED COALITION [%s - %s]: Roger command! Dispatching mechanized vanguard to designated coordinates!",
                       aNat.shortName.c_str(), aGen.name.c_str()));
    if (sound_) sound_->playSfx(SfxId::RADIO_SQUELCH, 0.75f);
}

// ============================================================================
// In-Battle RTS Quick Save & Quick Load
// ============================================================================

bool RTSEngine::saveBattleToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    uint32_t magic = 0xAA2004B2u;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&battleTime_), sizeof(battleTime_));

    for (int t = 0; t < 2; ++t) {
        const auto& p = players_[t];
        out.write(reinterpret_cast<const char*>(&p.nation), sizeof(p.nation));
        out.write(reinterpret_cast<const char*>(&p.generalIdx), sizeof(p.generalIdx));
        out.write(reinterpret_cast<const char*>(&p.money), sizeof(p.money));
        out.write(reinterpret_cast<const char*>(&p.ammo), sizeof(p.ammo));
        out.write(reinterpret_cast<const char*>(&p.oil), sizeof(p.oil));
        out.write(reinterpret_cast<const char*>(&p.commandPoints), sizeof(p.commandPoints));
        out.write(reinterpret_cast<const char*>(&p.techTier), sizeof(p.techTier));
    }

    size_t bCount = buildings_.size();
    out.write(reinterpret_cast<const char*>(&bCount), sizeof(bCount));
    for (const auto& b : buildings_) {
        out.write(reinterpret_cast<const char*>(&b.id), sizeof(b.id));
        out.write(reinterpret_cast<const char*>(&b.teamId), sizeof(b.teamId));
        out.write(reinterpret_cast<const char*>(&b.nation), sizeof(b.nation));
        out.write(reinterpret_cast<const char*>(&b.type), sizeof(b.type));
        out.write(reinterpret_cast<const char*>(&b.pos), sizeof(b.pos));
        out.write(reinterpret_cast<const char*>(&b.health), sizeof(b.health));
        out.write(reinterpret_cast<const char*>(&b.maxHealth), sizeof(b.maxHealth));
        out.write(reinterpret_cast<const char*>(&b.supplyRadius), sizeof(b.supplyRadius));
        out.write(reinterpret_cast<const char*>(&b.captureProgress), sizeof(b.captureProgress));
    }

    size_t rCount = regiments_.size();
    out.write(reinterpret_cast<const char*>(&rCount), sizeof(rCount));
    for (const auto& r : regiments_) {
        out.write(reinterpret_cast<const char*>(&r.teamId), sizeof(r.teamId));
        out.write(reinterpret_cast<const char*>(&r.type), sizeof(r.type));
        out.write(reinterpret_cast<const char*>(&r.packedBuildingType), sizeof(r.packedBuildingType));
        out.write(reinterpret_cast<const char*>(&r.stance), sizeof(r.stance));
        out.write(reinterpret_cast<const char*>(&r.pos), sizeof(r.pos));
        out.write(reinterpret_cast<const char*>(&r.targetPos), sizeof(r.targetPos));
        out.write(reinterpret_cast<const char*>(&r.health), sizeof(r.health));
        out.write(reinterpret_cast<const char*>(&r.organization), sizeof(r.organization));
        out.write(reinterpret_cast<const char*>(&r.controlGroup), sizeof(r.controlGroup));
    }

    return out.good();
}

bool RTSEngine::loadBattleFromFile(const std::string& path, SoundEngine* sound) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0xAA2004B2u) return false;

    if (sound) sound_ = sound;
    in.read(reinterpret_cast<char*>(&battleTime_), sizeof(battleTime_));

    for (int t = 0; t < 2; ++t) {
        auto& p = players_[t];
        in.read(reinterpret_cast<char*>(&p.nation), sizeof(p.nation));
        in.read(reinterpret_cast<char*>(&p.generalIdx), sizeof(p.generalIdx));
        in.read(reinterpret_cast<char*>(&p.money), sizeof(p.money));
        in.read(reinterpret_cast<char*>(&p.ammo), sizeof(p.ammo));
        in.read(reinterpret_cast<char*>(&p.oil), sizeof(p.oil));
        in.read(reinterpret_cast<char*>(&p.commandPoints), sizeof(p.commandPoints));
        in.read(reinterpret_cast<char*>(&p.techTier), sizeof(p.techTier));
    }

    size_t bCount = 0;
    in.read(reinterpret_cast<char*>(&bCount), sizeof(bCount));
    buildings_.clear();
    for (size_t i = 0; i < bCount; ++i) {
        RTSBuilding b{};
        in.read(reinterpret_cast<char*>(&b.id), sizeof(b.id));
        in.read(reinterpret_cast<char*>(&b.teamId), sizeof(b.teamId));
        in.read(reinterpret_cast<char*>(&b.nation), sizeof(b.nation));
        in.read(reinterpret_cast<char*>(&b.type), sizeof(b.type));
        in.read(reinterpret_cast<char*>(&b.pos), sizeof(b.pos));
        in.read(reinterpret_cast<char*>(&b.health), sizeof(b.health));
        in.read(reinterpret_cast<char*>(&b.maxHealth), sizeof(b.maxHealth));
        in.read(reinterpret_cast<char*>(&b.supplyRadius), sizeof(b.supplyRadius));
        in.read(reinterpret_cast<char*>(&b.captureProgress), sizeof(b.captureProgress));
        b.rallyPoint = {b.pos.x, b.pos.y, b.pos.z + (b.teamId == 0 ? -18.0f : 18.0f)};
        buildings_.push_back(b);
    }

    size_t rCount = 0;
    in.read(reinterpret_cast<char*>(&rCount), sizeof(rCount));
    regiments_.clear();
    for (size_t i = 0; i < rCount; ++i) {
        int teamId = 0;
        RegimentType rType = RegimentType::RIFLE_REGIMENT;
        BuildingType packedBt = BuildingType::CORPS_HQ;
        FormationStance stance = FormationStance::LINE;
        Vector3 pos{}, targetPos{};
        float hp = 300.0f, org = 100.0f;
        int cg = -1;

        in.read(reinterpret_cast<char*>(&teamId), sizeof(teamId));
        in.read(reinterpret_cast<char*>(&rType), sizeof(rType));
        in.read(reinterpret_cast<char*>(&packedBt), sizeof(packedBt));
        in.read(reinterpret_cast<char*>(&stance), sizeof(stance));
        in.read(reinterpret_cast<char*>(&pos), sizeof(pos));
        in.read(reinterpret_cast<char*>(&targetPos), sizeof(targetPos));
        in.read(reinterpret_cast<char*>(&hp), sizeof(hp));
        in.read(reinterpret_cast<char*>(&org), sizeof(org));
        in.read(reinterpret_cast<char*>(&cg), sizeof(cg));

        int id = spawnRegiment(teamId, rType, pos, packedBt);
        for (auto& r : regiments_) {
            if (r.id == id) {
                r.stance = stance;
                r.targetPos = targetPos;
                r.health = hp;
                r.organization = org;
                r.controlGroup = cg;
                break;
            }
        }
    }

    updateSupplyNetwork();
    pushLog("SYSTEM: Loaded saved 3D RTS Battlefield state.");
    return in.good();
}

} // namespace aa2004
