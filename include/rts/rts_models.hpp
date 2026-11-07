#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — HIGH-DETAIL 3D MILITARY MODEL PROCEDURAL RENDERER
// Namespace: aa2004::models
// Hardware-Accelerated Procedural WWII Tanks, Half-Tracks, Katyusha Trucks,
// Multi-Part Infantry Squads, Fortified HQs, Airfields, Factories & Flora
// ============================================================================

#include "core/types.hpp"
#include "data/game_data.hpp"
#include "rts/rts_engine.hpp"

namespace aa2004 {

// Order type enum for regimental stance / animation state
enum class OrderType : int {
    STOP = 0,
    HOLD = 1,
    MOVE = 2,
    ATTACK = 3,
    ATTACK_MOVE = 4,
    ENTRENCH = 5
};

// Represents an individual squad soldier or crew member within a regiment
struct RegimentMemberInstance {
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 worldPos = {0.0f, 0.0f, 0.0f};
    Vector3 localOffset = {0.0f, 0.0f, 0.0f};
    float facingAngleRad = 0.0f;
    float turretAngleRad = 0.0f;
    float recoil = 0.0f;
    float animPhase = 0.0f;
    float walkPhase = 0.0f;
    float muzzleFlashTimer = 0.0f;
    bool alive = true;
    bool isAlive = true;

    RegimentMemberInstance() = default;
    RegimentMemberInstance(const RTSSubUnit& su)
        : pos(su.worldPos),
          worldPos(su.worldPos),
          localOffset(su.localOffset),
          facingAngleRad(0.0f),
          turretAngleRad(su.turretYaw * DEG2RAD),
          recoil(su.recoil),
          animPhase(su.animPhase),
          walkPhase(su.animPhase),
          muzzleFlashTimer((su.recoil > 0.32f) ? (su.recoil * 0.15f) : 0.0f),
          alive(su.alive),
          isAlive(su.alive) {}
};

// Represents a regiment instance on the 3D battlefield
struct RegimentInstance {
    int id = 0;
    int teamId = 0;
    NationId nation = NationId::USA;
    RegimentType type = RegimentType::RIFLE_REGIMENT;
    BuildingType packedBuildingType = BuildingType::CORPS_HQ;
    FormationStance stance = FormationStance::LINE;
    OrderType order = OrderType::STOP;

    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 targetPos = {0.0f, 0.0f, 0.0f};
    float facing = 0.0f;            // In degrees
    float facingAngleRad = 0.0f;    // In radians
    float health = 360.0f;
    float maxHealth = 360.0f;
    float organization = 100.0f;
    float maxOrganization = 100.0f;
    bool selected = false;
    bool inSupply = true;

    std::vector<RegimentMemberInstance> members;

    RegimentInstance() = default;
    RegimentInstance(const RTSRegiment& r)
        : id(r.id),
          teamId(r.teamId),
          nation(r.nation),
          type(r.type),
          packedBuildingType(r.packedBuildingType),
          stance(r.stance),
          pos(r.pos),
          targetPos(r.targetPos),
          facing(r.facing),
          facingAngleRad(r.facing * DEG2RAD),
          health(r.health),
          maxHealth(r.maxHealth),
          organization(r.organization),
          maxOrganization(r.maxOrganization),
          selected(r.selected),
          inSupply(r.inSupply) {
        order = (Vector3Distance(r.pos, r.targetPos) > 1.2f)
            ? (r.attackMove ? OrderType::ATTACK_MOVE : OrderType::MOVE)
            : OrderType::STOP;
        members.reserve(r.subUnits.size());
        for (const auto& su : r.subUnits) {
            members.emplace_back(su);
        }
    }
};

// Represents a building instance on the 3D battlefield
struct BuildingInstance {
    int id = 0;
    int teamId = 0;
    NationId nation = NationId::USA;
    BuildingType type = BuildingType::CORPS_HQ;
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    float facing = 0.0f;            // In degrees
    float facingAngleRad = 0.0f;    // In radians
    float health = 1000.0f;
    float maxHealth = 1000.0f;
    float supplyRadius = 0.0f;
    bool inSupply = true;
    bool selected = false;
    bool isPacking = false;
    float packProgress = 0.0f;
    bool isUnderConstruction = false;
    float constructProgress = 1.0f;
    Vector3 rallyPoint = {0.0f, 0.0f, 0.0f};
    float captureProgress = 0.0f;

    BuildingInstance() = default;
    BuildingInstance(const RTSBuilding& b)
        : id(b.id),
          teamId(b.teamId),
          nation(b.nation),
          type(b.type),
          pos(b.pos),
          facing(b.facing),
          facingAngleRad(b.facing * DEG2RAD),
          health(b.health),
          maxHealth(b.maxHealth),
          supplyRadius(b.supplyRadius),
          inSupply(b.inSupply),
          selected(b.selected),
          isPacking(b.isPacking),
          packProgress(b.packProgress),
          isUnderConstruction(b.isUnderConstruction),
          constructProgress(b.constructProgress),
          rallyPoint(b.rallyPoint),
          captureProgress(b.captureProgress) {}
};

namespace models {

using aa2004::OrderType;
using aa2004::RegimentMemberInstance;
using aa2004::RegimentInstance;
using aa2004::BuildingInstance;

// ============================================================================
// Core High-Detail 3D Procedural Military Model Renderers
// ============================================================================

// 1. Draw Detailed Regiment with multi-part soldiers/vehicles, stance entrenchments & shadow discs
void drawDetailedRegiment3D(const RegimentInstance& reg, const NationDef& nat, const RegimentDef& rDef, float timeSec, int lod = 0);
void drawDetailedRegiment3D(const RTSRegiment& reg, const NationDef& nat, const RegimentDef& rDef, float timeSec, int lod = 0);

// 2. Draw Detailed Building (Corps HQ, Barracks, Airfield, Factory, Refinery, City, Bunker, AA Battery)
void drawDetailedBuilding3D(const BuildingInstance& b, const NationDef& nat, const BuildingDef& bDef, float timeSec);
void drawDetailedBuilding3D(const RTSBuilding& b, const NationDef& nat, const BuildingDef& bDef, float timeSec);
void drawConstructionSite3D(const RTSBuilding& b, const NationDef& nat, const BuildingDef& bDef, float timeSec);

// 3. Draw Detailed Biome Tree (Winter Pines with snow, Normandy Oaks, Pacific/Desert Palms)
void drawDetailedTree3D(Vector3 pos, float scale, BiomeType biome, float timeSec = 0.0f, int lod = 0);

// 4. Draw Detailed Battlefield Environment Props
void drawBocageHedge3D(Vector3 pos, float scale, float rotation);
void drawRuinedStoneWall3D(Vector3 pos, float scale, float rotation);
void drawCzechHedgehog3D(Vector3 pos, float scale);
void drawTelephonePole3D(Vector3 pos, float scale);
void drawSandbagCheckpoint3D(Vector3 pos, float scale);
void drawRockFormation3D(Vector3 pos, float scale, float rotation);

// Modular Sub-Component Renderers
void drawSoldier3D(Vector3 pos, float facingDeg, float walkPhase, bool isWalking, float muzzleFlashTimer,
                   const NationDef& nat, RegimentType type, float timeSec, int memberIdx = 0,
                   bool isPinned = false, bool isCrouched = false, int lod = 0);

void drawTank3D(Vector3 pos, float hullFacingDeg, float turretFacingDeg, float recoil, float muzzleFlashTimer,
                const NationDef& nat, RegimentType type, float timeSec,
                float pitchDeg = 0.0f, float rollDeg = 0.0f, int lod = 0);

void drawVehicle3D(Vector3 pos, float facingDeg, float recoil, float muzzleFlashTimer,
                   const NationDef& nat, RegimentType type, BuildingType packedBldg, float timeSec,
                   float pitchDeg = 0.0f, float rollDeg = 0.0f, int lod = 0);

void drawAircraft3D(Vector3 pos, float facingDeg, const NationDef& nat, float propAngleDeg);
void drawFighterAircraft3D(Vector3 pos, float facingDeg, float rollDeg, const NationDef& nat, float propAngleDeg, bool firingGuns = false);

void drawEntrenchmentRing3D(Vector3 pos, float radius);

void drawNationalInsignia3D(Vector3 center, float radius, NationId nation);

} // namespace models

// Expose models symbols into aa2004 root namespace as well for convenience
using models::drawDetailedRegiment3D;
using models::drawDetailedBuilding3D;
using models::drawDetailedTree3D;

} // namespace aa2004
