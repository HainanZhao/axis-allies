#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — CORE C++20 TYPES, ENUMS & DATA STRUCTURES
// High-Performance Native C++20 + Raylib 3D Strategy Simulator for MacBook Pro
// Faithful to the 2004 TimeGate Studios / Atari PC Video Game
// ============================================================================

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <functional>

namespace aa2004 {

// ----------------------------------------------------------------------------
// 1. Alliances & The 5 Superpowers
// ----------------------------------------------------------------------------
enum class Alliance : int {
    ALLIES = 0,
    AXIS   = 1,
    NEUTRAL = 2
};

enum class NationId : int {
    USSR = 0,
    GER  = 1,
    UK   = 2,
    JPN  = 3,
    USA  = 4,
    COUNT = 5
};

// ----------------------------------------------------------------------------
// 2. Game Screen / Mode States
// ----------------------------------------------------------------------------
enum class ScreenState : int {
    MAIN_MENU = 0,
    CAMPAIGN_SELECT,
    SKIRMISH_SETUP,
    RTS_BATTLE,
    RTS_POST_BATTLE
};

// ----------------------------------------------------------------------------
// 3. Special Operations & Generals (20 Historical Commanders)
// ----------------------------------------------------------------------------
enum class SpecOpTargetType : int {
    GLOBAL = 0,
    GROUND_AREA = 1,
    HQ_SPAWN = 2
};

struct SpecialOpDef {
    std::string id;
    std::string name;
    std::string shortCode;
    std::string description;
    int commandPointCost = 100;
    float cooldownSec = 45.0f;
    float durationSec = 0.0f;
    float radius = 25.0f;
    SpecOpTargetType targetType = SpecOpTargetType::GLOBAL;
};

struct GeneralDef {
    std::string id;
    std::string name;
    std::string nickname;
    std::string specialty;
    std::string bio;
    std::string passiveDesc;
    NationId nation = NationId::USA;
    Color portraitColor = DARKGREEN;

    // Passive Doctrine Multipliers
    float infAttackMult = 1.0f;
    float infSpeedMult = 1.0f;
    float infCostMult = 1.0f;
    float armorAttackMult = 1.0f;
    float armorSpeedMult = 1.0f;
    float armorCostMult = 1.0f;
    float artilleryAttackMult = 1.0f;
    float defenseMult = 1.0f;
    float supplyRadiusMult = 1.0f;
    float moneyRateMult = 1.0f;
    float ammoRateMult = 1.0f;
    float oilRateMult = 1.0f;
    float orgRegenMult = 1.0f;
    float outOfSupplyPenaltyMult = 1.0f;
    float bunkerHealthMult = 1.0f;

    std::array<SpecialOpDef, 3> specialOps;
};

struct NationDef {
    NationId id = NationId::USA;
    std::string code;
    std::string name;
    std::string shortName;
    std::string motto;
    std::string description;
    std::string signatureUnitName;
    Alliance alliance = Alliance::ALLIES;
    Color primaryColor = GREEN;
    Color accentColor = LIME;
    Color mapColor = GREEN;
    int startingIPC = 30;
    int capitalTerritoryId = 0;

    // Nation-Specific Vehicle Names
    std::string lightTankName;
    std::string mediumTankName;
    std::string tankDestroyerName;
    std::string heavyTankName;
    std::string eliteTankName;
    std::string spgName;

    std::vector<GeneralDef> generals;
};

// ----------------------------------------------------------------------------
// 4. Buildings / Mobile HQs & Regimental Unit Types (TimeGate 2004 System)
// ----------------------------------------------------------------------------
enum class BuildingType : int {
    CORPS_HQ = 0,       // Main Command Center, packs/unpacks, Tech Tiers 1-3
    SUPPLY_DEPOT = 1,   // Extends Supply Line radius, generates Money/Ammo/Oil
    INFANTRY_HQ = 2,    // Deploys Infantry Regiments
    MECH_HQ = 3,        // Deploys Mechanized & Artillery Regiments
    ARMOR_HQ = 4,       // Deploys Medium/Heavy/Elite Tank Regiments
    AIRFIELD = 5,       // Air Operations & Paratroopers
    BUNKER = 6,         // Fortified Concrete Pillbox (MG + AT gun)
    AA_BATTERY = 7,     // 88mm / 40mm Dual-Purpose Flak & Anti-Tank Battery
    CITY_DEPOT = 8,     // Neutral/Capturable City & Resource Hub on Map
    OIL_DERRICK = 9,    // Capturable Oil Refinery Node
    COUNT = 10
};

struct BuildingDef {
    BuildingType type = BuildingType::CORPS_HQ;
    std::string id;
    std::string name;
    std::string description;
    int costMoney = 100;
    int costAmmo = 0;
    int costOil = 0;
    float maxHealth = 1000.0f;
    float supplyRadius = 0.0f;
    bool canPackMove = true;
    int techTierRequired = 1;
    float incomeMoney = 0.0f;
    float incomeAmmo = 0.0f;
    float incomeOil = 0.0f;
    float attackRange = 0.0f;
    float attackDamage = 0.0f;
    float fireInterval = 1.5f;
    float buildTime = 8.0f;
};

enum class BattleGameMode : int {
    STANDARD_1V1 = 0,   // 1 vs 1 Skirmish
    ALLIED_2V2   = 1,   // 2 vs 2 Allied Team Battle (Player & Ally vs 2 Axis/Enemies)
    SURVIVAL_FFA_3 = 2, // 3-Player Survival (Free-For-All: Last Survivor Wins)
    FFA_4_PLAYER = 3    // 4-Player Free-For-All: Total War
};

enum class UnitCategory : int {
    INFANTRY = 0,
    MECHANIZED = 1,
    ARMOR = 2,
    ARTILLERY = 3,
    AIR_SUPPORT = 4,
    SUPPLY_TRUCK = 5
};

enum class FormationStance : int {
    LINE = 0,        // Balanced frontage & accuracy
    COLUMN = 1,      // +25% Road/March speed, -10% defense
    ENTRENCHED = 2   // Sandbag/hull-down cover: +35% defense, +30% organization resilience
};

enum class RegimentType : int {
    // Infantry Division HQ Regiments
    RIFLE_REGIMENT = 0,
    MG_REGIMENT = 1,
    MORTAR_REGIMENT = 2,
    AT_BAZOOKA_REGIMENT = 3,
    ENGINEER_FLAME_REGIMENT = 4,
    SNIPER_REGIMENT = 5,
    PARATROOP_REGIMENT = 6,

    // Mechanized Division HQ Regiments
    RECON_HALFTRACK = 7,
    AA_HALFTRACK = 8,
    MOBILE_ARTILLERY = 9,
    KATYUSHA_ROCKET = 10,

    // Armored Division HQ Regiments
    LIGHT_TANK = 11,
    MEDIUM_TANK = 12,
    TANK_DESTROYER = 13,
    HEAVY_TANK = 14,
    ELITE_SIGNATURE_TANK = 15, // King Tiger / Churchill Crocodile / Super Pershing / IS-3 / Ho-Ri

    // Mobile Packed HQ / Supply Truck
    PACKED_HQ_TRUCK = 16,

    COUNT = 17
};

struct RegimentDef {
    RegimentType type = RegimentType::RIFLE_REGIMENT;
    std::string id;
    std::string name;
    std::string description;
    UnitCategory category = UnitCategory::INFANTRY;
    BuildingType producedBy = BuildingType::INFANTRY_HQ;
    int techTierRequired = 1;

    // Economy Costs & Upkeep
    int costMoney = 80;
    int costAmmo = 30;
    int costOil = 0;
    float upkeepAmmo = 0.5f;
    float upkeepOil = 0.0f;

    // Regimental Composition & Stats
    int squadCount = 6;            // e.g., 6 soldiers per regiment, 3 tanks per armor regiment
    float maxHealth = 360.0f;
    float maxOrganization = 100.0f;
    float moveSpeed = 10.0f;
    float attackRange = 28.0f;
    float visionRange = 45.0f;
    float fireInterval = 1.2f;

    // Damage Profile
    float dmgVsInfantry = 18.0f;
    float dmgVsVehicle = 10.0f;
    float dmgVsStructure = 8.0f;
    float orgDamage = 12.0f;
    float splashRadius = 0.0f;
    bool isIndirectFire = false;
    bool canCapture = false;
    bool canRepair = false;

    // Strategic Simulator Ratings (Cost in $M, Firepower Index, Armor/Resilience Index)
    int strategicCost = 3;
    int firepowerRating = 1;
    int resilienceRating = 2;
};

// ----------------------------------------------------------------------------
// 5. Biome & Map Scenarios (RTS & Campaign)
// ----------------------------------------------------------------------------
enum class BiomeType : int {
    NORMANDY_BOCAGE = 0,
    EL_ALAMEIN_DESERT = 1,
    STALINGRAD_WINTER = 2,
    PACIFIC_ATOLL = 3,
    KURSK_STEPPE = 4,
    ARDENNES_WINTER = 5,
    RHINE_CROSSING = 6,
    BERLIN_RUINS = 7
};

struct SkirmishMapDef {
    int id = 0;
    std::string name;
    std::string theater;
    std::string description;
    BiomeType biome = BiomeType::NORMANDY_BOCAGE;
    float mapSize = 360.0f;
    bool hasRiver = true;
    int cityCount = 5;
    int oilCount = 3;
};

struct CampaignMissionDef {
    int id = 0;
    bool isAxisCampaign = false;
    int missionNumber = 1;
    std::string title;
    std::string dateStr;
    std::string locationStr;
    std::string briefing;
    std::string objectiveText;
    NationId playerNation = NationId::USA;
    int playerGeneralIdx = 0;
    NationId enemyNation = NationId::GER;
    int enemyGeneralIdx = 0;
    BiomeType biome = BiomeType::NORMANDY_BOCAGE;
    int startingMoney = 500;
    int startingAmmo = 400;
    int startingOil = 350;
    int aiDifficulty = 1; // 0=Recruit, 1=Veteran, 2=Field Marshal
};

// ----------------------------------------------------------------------------
// 6. World War II Mode (Strategic Theater Simulator Divisions)
// ----------------------------------------------------------------------------
struct StrategicArmyStack {
    int infantry = 0;
    int artillery = 0;
    int mechanized = 0;
    int armor = 0;
    int eliteArmor = 0;
    int fighters = 0;
    int bombers = 0;
    int aaGuns = 0;

    int totalUnits() const {
        return infantry + artillery + mechanized + armor + eliteArmor + fighters + bombers + aaGuns;
    }
    bool empty() const { return totalUnits() <= 0; }
};

struct TerritoryDef {
    int id = 0;
    std::string key;
    std::string name;
    std::string region;
    NationId originalOwner = NationId::USA;
    int ipcValue = 2;
    bool isCapital = false;
    bool hasFactory = false;
    bool isCoastal = false;
    BiomeType biome = BiomeType::NORMANDY_BOCAGE;
    Vector2 mapPos = {0.5f, 0.5f}; // Normalized [0..1] on strategic map
    std::vector<int> neighbors;
    StrategicArmyStack startingUnits;
};

} // namespace aa2004
