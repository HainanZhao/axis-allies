#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — REAL-TIME STRATEGY (RTS) SIMULATION ENGINE
// Authentic TimeGate Studios 2004 Mechanics:
// - Mobile Pack/Unpack Corps HQ & Division HQs
// - Supply Line Network & Organization/Morale Bar
// - Money / Ammo / Oil Economy
// - Multi-Entity Regimental Formations, Stances, Entrenchment & Ballistics
// - 20 Historical Generals & Active Special Operations
// ============================================================================

#include "core/types.hpp"
#include "data/game_data.hpp"
#include "audio/sound_engine.hpp"

namespace aa2004 {

struct ActiveBuff {
    std::string name;
    float remainingSec = 0.0f;
    float speedMult = 1.0f;
    float attackMult = 1.0f;
    float defenseMult = 1.0f;
    bool ignoreSupplyPenalty = false;
};

struct RTSPlayerState {
    int teamId = 0; // 0..kMaxPlayers-1
    int allianceId = 0; // Players with the same allianceId are allies; different are hostile enemies
    NationId nation = NationId::USA;
    int generalIdx = 0;
    bool isAI = false;
    int aiDifficulty = 1; // 0=Recruit, 1=Veteran, 2=Field Marshal

    // 3-Resource Economy + Command Points
    float money = 500.0f;
    float ammo = 400.0f;
    float oil = 350.0f;
    float commandPoints = 60.0f;

    float moneyRate = 25.0f;
    float ammoRate = 18.0f;
    float oilRate = 15.0f;

    // Compounding Territorial Economy
    int controlledTowns = 0;
    float townSynergyMult = 1.0f;
    int controlledOil = 0;
    float oilSynergyMult = 1.0f;

    int techTier = 1; // 1..3
    std::array<float, 3> specOpCooldowns = {0.0f, 0.0f, 0.0f};
    std::vector<ActiveBuff> activeBuffs;
    float radarRevealTimer = 0.0f;

    // AI timers
    float aiDecisionTimer = 0.0f;
    float aiAttackTimer = 0.0f;
    float aiAirMissionTimer = 0.0f;
};

struct RTSBuilding {
    int id = 0;
    int teamId = 0; // 0 = Player, 1 = Enemy, -1 = Neutral (for cities/oil)
    NationId nation = NationId::USA;
    BuildingType type = BuildingType::CORPS_HQ;
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    float facing = 0.0f;
    float health = 1000.0f;
    float maxHealth = 1000.0f;
    float supplyRadius = 0.0f;
    bool inSupply = true;
    bool selected = false;

    // Pack / Unpack Mobile HQ State
    bool isPacking = false;
    float packProgress = 0.0f;

    // Construction State
    bool isUnderConstruction = false;
    float constructProgress = 1.0f; // 0.0f (just started) to 1.0f (finished)
    float constructTotalTime = 0.0f;

    // Production Queue
    std::vector<RegimentType> prodQueue;
    float prodProgress = 0.0f;
    float prodTotalTime = 6.0f;
    Vector3 rallyPoint = {0.0f, 0.0f, 0.0f};

    // Defensive Turret / Capture State
    float fireTimer = 0.0f;
    float captureProgress = 0.0f; // -100 (Team 1) to +100 (Team 0)
    int connectedSourceId = -1;   // For visualizing supply network links

    // Bomb Damage & Repair State
    bool isBombDamaged = false;
    float bombDamageTimer = 0.0f;
    float combatTimer = 0.0f; // > 0 means actively under fire: suppresses construction & out-healing
};

struct RTSSubUnit {
    Vector3 localOffset = {0.0f, 0.0f, 0.0f};
    Vector3 worldPos = {0.0f, 0.0f, 0.0f};
    float turretYaw = 0.0f;
    float recoil = 0.0f;
    float animPhase = 0.0f;
    float pitchDeg = 0.0f;
    float rollDeg = 0.0f;
    bool alive = true;
};

struct RTSRegiment {
    int id = 0;
    int teamId = 0;
    NationId nation = NationId::USA;
    RegimentType type = RegimentType::RIFLE_REGIMENT;
    BuildingType packedBuildingType = BuildingType::CORPS_HQ; // Used when type == PACKED_HQ_TRUCK
    FormationStance stance = FormationStance::LINE;

    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 targetPos = {0.0f, 0.0f, 0.0f};
    float facing = 0.0f;
    float health = 360.0f;
    float maxHealth = 360.0f;
    float organization = 100.0f;
    float maxOrganization = 100.0f;

    bool inSupply = true;
    bool atTownHealingStation = false;
    int supplySourceId = -1;
    float supplyBuffer = 75.0f; // Carry-on expedition rations and fuel supplies (75s endurance outside supply lines)
    float maxSupplyBuffer = 75.0f;
    bool selected = false;
    bool attackMove = false;
    int targetRegimentId = -1;
    int targetBuildingId = -1;
    int targetRepairBuildingId = -1; // Set when Combat Engineers are ordered to repair a damaged building
    int controlGroup = -1; // 1..9

    float fireTimer = 0.0f;
    float resupplyTimer = 0.0f;
    int veterancy = 0; // 0..3 stars
    float xp = 0.0f;

    // Tactical Terrain Cover & Heroic Field Abilities
    int coverTier = 0; // 0=Open Field, 1=Light Cover (-25% dmg), 2=Heavy Cover (-40% dmg)
    float smokeScreenTimer = 0.0f; // Active smoke screen obscuring unit & breaking line of sight
    float rapidVolleyTimer = 0.0f; // Rapid fire suppression volley booster
    float abilityCooldown = 0.0f; // Tactical field ability cooldown

    std::vector<RTSSubUnit> subUnits;
};

struct RTSProjectile {
    Vector3 startPos = {0.0f, 0.0f, 0.0f};
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 targetPos = {0.0f, 0.0f, 0.0f};
    Vector3 velocity = {0.0f, 0.0f, 0.0f};
    int teamId = 0;
    float damageInf = 15.0f;
    float damageVeh = 15.0f;
    float damageStruct = 15.0f;
    float orgDamage = 10.0f;
    float splashRadius = 0.0f;
    bool isArcing = false;
    bool isRocket = false;
    bool isFlame = false;
    Color color = YELLOW;
    float life = 3.0f;
};

struct RTSParticle {
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 vel = {0.0f, 0.0f, 0.0f};
    Color color = ORANGE;
    float size = 1.0f;
    float life = 1.0f;
    float maxLife = 1.0f;
    bool isRing = false;
    bool isCrater = false;
    bool isTread = false;
    bool isCasing = false;
    float rotation = 0.0f;
};

// Smoldering armored vehicle and artillery hulks with rising smoke and embers
struct RTSWreck {
    int id = 0;
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    float facing = 0.0f;
    RegimentType type = RegimentType::MEDIUM_TANK;
    NationId nation = NationId::USA;
    float life = 45.0f;
    float maxLife = 45.0f;
};

struct RTSAirStrike {
    int id = 0;
    int teamId = 0;
    NationId nation = NationId::USA;
    Vector3 startPos = {0.0f, 45.0f, 0.0f};
    Vector3 targetPos = {0.0f, 0.0f, 0.0f};
    Vector3 currentPos = {0.0f, 45.0f, 0.0f};
    float facing = 0.0f;
    float progress = 0.0f;
    float speed = 0.35f;
    bool droppedPayload = false;
    bool isRecon = false;
    bool isBombing = false;
    bool isParatroopDrop = false;
    bool isSupplyDrop = false;
    bool isKamikaze = false;
    bool isFighter = false; // Air Superiority Interceptor (Dogfights & bomber intercepts)
    int dogfightTargetId = -1;
    float dogfightTimer = 0.0f;
    Vector3 dogfightTargetPos = {0.0f, 0.0f, 0.0f};
    float radius = 25.0f;
    float damage = 220.0f;
    float health = 150.0f;
    float maxHealth = 150.0f;
    bool isShotDown = false;
    float crashFallSpeed = 0.0f;
    float flakBurstTimer = 0.0f;
    float rollAngle = 0.0f;
    float pitchAngle = 0.0f;
};

struct TerrainProp {
    Vector3 pos = {0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
    float rotation = 0.0f;
    int propType = 0; // 0=Tree, 1=Pine, 2=Palm, 3=Rock, 4=RuinedWall, 5=Hedge
};

struct RTSBattleConfig {
    BattleGameMode gameMode = BattleGameMode::STANDARD_1V1;
    int numPlayers = 2; // 2, 3, or 4 players
    NationId playerNation = NationId::USA;
    int playerGeneralIdx = 0;
    NationId enemyNation = NationId::GER;
    int enemyGeneralIdx = 0;
    std::vector<NationId> extraNations; // For 3P FFA, 4P FFA, or 2v2 Allies
    std::vector<int> extraGeneralIdxs;
    BiomeType biome = BiomeType::NORMANDY_BOCAGE;
    std::string scenarioTitle = "Normandy Bocage Breakthrough";
    std::string objectiveDesc = "Destroy all Enemy HQs or capture 4+ Strategic Cities.";
    int startingMoney = 600;
    int startingAmmo = 450;
    int startingOil = 400;
    int aiDifficulty = 1;
    bool fromWW2Mode = false;
    int contestedTerritoryId = -1;
    int campaignMissionId = -1;
    uint32_t mapSeed = 0; // 0 = dynamic procedural entropy seed per battle
    StrategicArmyStack playerArmyBonus{};
    StrategicArmyStack enemyArmyBonus{};
};

struct RTSBattleResult {
    bool finished = false;
    bool playerWon = false;
    bool fromWW2Mode = false;
    int contestedTerritoryId = -1;
    int campaignMissionId = -1;
    int playerRegimentsLost = 0;
    int enemyRegimentsDestroyed = 0;
    int buildingsCaptured = 0;
    float durationSec = 0.0f;
    StrategicArmyStack survivingPlayerUnits{};
    StrategicArmyStack survivingEnemyUnits{};
};

class RTSEngine {
public:
    RTSEngine();

    void startBattle(const RTSBattleConfig& cfg, SoundEngine* sound = nullptr);
    void update(float dt);

    // Player Commands
    void selectInBox(Vector3 worldMin, Vector3 worldMax, bool additive);
    void selectSingleAt(Vector3 worldPos, bool additive);
    void selectAllPlayerCombatUnits();
    void issueRightClickOrder(Vector3 worldPos);
    void issueAttackMoveOrder(Vector3 worldPos);
    void stopSelectedUnits();
    void setSelectedFormationStance(FormationStance stance);
    FormationStance getSelectedFormationStance() const;
    void assignControlGroup(int groupNum);
    bool selectControlGroup(int groupNum, Vector3& outCenterPos);

    // Building & Production Commands
    bool queueUnitProduction(int buildingId, RegimentType regType);
    bool cancelProductionQueueSlot(int buildingId, int slotIdx);
    bool cancelUnitTypeProduction(int buildingId, RegimentType regType);
    bool constructSupportBuilding(BuildingType bType, Vector3 pos, int teamId = 0);
    bool togglePackBuilding(int buildingId);
    bool unpackTruckRegiment(int regimentId);
    bool upgradeCorpsHQTech(int teamId = 0);

    // Special Operations
    bool triggerSpecialOp(int teamId, int opIndex, Vector3 targetPos);

    // Allied Tactical Coordination & Co-op Support (2v2 Allied Mode)
    int getAlliedTeamId() const;
    void issueAlliedPingOrder(Vector3 targetPos);
    Vector3 getAlliedPingPos() const { return alliedPingPos_; }
    float getAlliedPingTimer() const { return alliedPingTimer_; }

    // Air Force Operations (Strictly limited by Operational Airfields: 2 sorties max per Airfield)
    int getOperationalAirfieldCount(int teamId) const;
    int getActiveAirSortiesCount(int teamId) const;
    int getMaxAirSorties(int teamId) const;
    const RTSBuilding* findAvailableAirfield(int teamId, Vector3 targetPos) const;
    bool launchAirReconMission(int teamId, Vector3 targetPos);
    bool launchAirBombingMission(int teamId, Vector3 targetPos);
    bool launchAirSupplyDrop(int teamId, Vector3 targetPos);
    bool launchAirParatroopDrop(int teamId, Vector3 targetPos);
    bool launchAirFighterSweep(int teamId, Vector3 targetPos);
    bool triggerRegimentAbility(int regimentId, int abilityIdx = 0);
    Vector3 getLastAlertPos() const { return lastAlertPos_; }
    void setAlertPos(Vector3 pos) { lastAlertPos_ = pos; }
    int pendingAirMissionType() const { return pendingAirMissionType_; }
    void setPendingAirMissionType(int type) { pendingAirMissionType_ = type; pendingBuildType_ = -1; pendingSpecOpIdx_ = -1; }

    // Save / Load RTS Battle State
    bool saveBattleToFile(const std::string& path) const;
    bool loadBattleFromFile(const std::string& path, SoundEngine* sound = nullptr);

    // Force Finish (Surrender or Decisive Finish)
    void finishBattleManual(bool playerWon);

    // Territory & Compounding Economy Helpers
    int getControlledTownCount(int teamId) const;
    float getTownSynergyMultiplier(int teamId) const;
    int getControlledOilCount(int teamId) const;
    float getOilSynergyMultiplier(int teamId) const;

    // Terrain & Spatial Helpers
    float getTerrainHeight(float x, float z) const;
    bool hasRiver() const;
    bool isWaterAt(float x, float z) const;
    bool isBridgeAt(float x, float z) const;

    // Accessors
    const RTSBattleConfig& getConfig() const { return config_; }
    const RTSBattleResult& getResult() const { return result_; }
    const RTSPlayerState& getPlayer(int teamId) const { return players_[teamId]; }
    RTSPlayerState& getPlayerMut(int teamId) { return players_[teamId]; }
    const std::vector<RTSBuilding>& getBuildings() const { return buildings_; }
    std::vector<RTSBuilding>& getBuildingsMut() { return buildings_; }
    const std::vector<RTSRegiment>& getRegiments() const { return regiments_; }
    std::vector<RTSRegiment>& getRegimentsMut() { return regiments_; }
    const std::vector<RTSProjectile>& getProjectiles() const { return projectiles_; }
    const std::vector<RTSParticle>& getParticles() const { return particles_; }
    const std::vector<RTSWreck>& getWrecks() const { return wrecks_; }
    std::vector<RTSWreck>& getWrecksMut() { return wrecks_; }
    const std::vector<RTSAirStrike>& getAirStrikes() const { return airStrikes_; }
    const std::vector<TerrainProp>& getProps() const { return props_; }
    const std::vector<std::string>& getCombatLog() const { return combatLog_; }
    void spawnExplosion(Vector3 pos, float radius, Color col);

    bool showSupplyOverlay() const { return showSupplyOverlay_; }
    void toggleSupplyOverlay() { showSupplyOverlay_ = !showSupplyOverlay_; }
    int pendingBuildType() const { return pendingBuildType_; }
    void setPendingBuildType(int bType) { pendingBuildType_ = bType; pendingSpecOpIdx_ = -1; pendingAirMissionType_ = -1; }
    int pendingSpecOpIdx() const { return pendingSpecOpIdx_; }
    void setPendingSpecOpIdx(int idx) { pendingSpecOpIdx_ = idx; pendingBuildType_ = -1; pendingAirMissionType_ = -1; }
    float getGameSpeed() const { return gameSpeed_; }
    void setGameSpeed(float s) { gameSpeed_ = std::clamp(s, 0.5f, 3.0f); }
    float getBattleTime() const { return battleTime_; }
    bool isPaused() const { return paused_; }
    void togglePause() { paused_ = !paused_; }

    static constexpr int kFogGridSize = 96;
    static constexpr float kMapHalfSpan = 560.0f;
    static constexpr int kMaxPlayers = 4;

    bool isEnemy(int teamA, int teamB) const;
    bool isAllied(int teamA, int teamB) const;
    int getNumPlayers() const { return numPlayers_; }
    const std::vector<Vector3>& getPlayerBasePositions() const { return playerBasePositions_; }

    // Vision & Fog of War (Authentic 2004 Recon, Unit & Captured Town Vision Radius)
    struct VisionSource {
        float x = 0.0f;
        float z = 0.0f;
        float radiusSq = 0.0f;
    };
    void updateFogOfWar();
    void invalidateVisionCache() { visionCacheValid_ = false; }
    float getBuildingVisionRadius(BuildingType type) const;
    float getRegimentVisionRadius(RegimentType type) const;
    bool isPosVisibleToTeam(Vector3 pos, int teamId) const;
    bool isPosExploredByPlayer(Vector3 pos) const;
    uint8_t getFogCellState(int gx, int gz) const {
        if (gx < 0 || gx >= kFogGridSize || gz < 0 || gz >= kFogGridSize) return 0;
        return fogGrid_[gz * kFogGridSize + gx];
    }
    void setRevealAllForShowcase(bool reveal) { revealAllShowcase_ = reveal; updateFogOfWar(); }
    bool isRevealAllShowcase() const { return revealAllShowcase_; }
    Vector3 getPlayerBasePos() const { return playerBasePos_; }
    Vector3 getEnemyBasePos() const { return enemyBasePos_; }

    int spawnRegiment(int teamId, RegimentType type, Vector3 pos, BuildingType packedBldg = BuildingType::CORPS_HQ);
    int spawnBuilding(int teamId, BuildingType type, Vector3 pos, bool underConstruction = false);
    bool cancelConstruction(int buildingId);
    void pushLog(const std::string& msg);

private:
    void updateSupplyNetwork();
    void updateEconomy(float dt);
    void updateBuildings(float dt);
    void updateRegiments(float dt);
    void updateProjectiles(float dt);
    void updateAirStrikes(float dt);
    void updateParticles(float dt);
    void updateAI(float dt);
    void checkVictoryConditions();
    void populateSurvivingStacks();
    void rebuildVisionCache() const;

    struct TeamVisionSoA {
        std::vector<float> x;
        std::vector<float> z;
        std::vector<float> radiusSq;
    };
    mutable std::array<std::vector<VisionSource>, kMaxPlayers> teamVisionCache_{};
    mutable std::array<TeamVisionSoA, kMaxPlayers> teamVisionSoA_{};
    mutable bool visionCacheValid_ = false;

    RTSBattleConfig config_{};
    RTSBattleResult result_{};
    SoundEngine* sound_ = nullptr;

    std::array<RTSPlayerState, kMaxPlayers> players_{};
    int numPlayers_ = 2;
    std::vector<Vector3> playerBasePositions_;
    std::vector<RTSBuilding> buildings_;
    std::vector<RTSRegiment> regiments_;
    std::vector<RTSProjectile> projectiles_;
    std::vector<RTSParticle> particles_;
    std::vector<RTSWreck> wrecks_;
    std::vector<RTSAirStrike> airStrikes_;
    std::vector<TerrainProp> props_;
    std::vector<std::string> combatLog_;
    std::array<uint8_t, kFogGridSize * kFogGridSize> fogGrid_{};
    bool revealAllShowcase_ = false;
    bool hasWarnedRiverCrossing_ = false;
    Vector3 playerBasePos_ = {0.0f, 0.0f, 440.0f};
    Vector3 enemyBasePos_  = {0.0f, 0.0f, -440.0f};
    Vector3 lastAlertPos_  = {0.0f, 0.0f, 440.0f};
    Vector3 alliedPingPos_ = {0.0f, 0.0f, 0.0f};
    float alliedPingTimer_ = 0.0f;

    // Procedural Map & Terrain Parameters (Unique rolling hills & valleys per game)
    uint32_t mapSeed_ = 2004u;
    float hillFreqA_ = 0.035f;
    float hillFreqB_ = 0.035f;
    float hillAmp_ = 0.45f;
    float hillPhaseX_ = 0.0f;
    float hillPhaseZ_ = 0.0f;
    float bridgeWestX_ = -240.0f;
    float bridgeEastX_ = 240.0f;

    int nextEntityId_ = 1;
    float battleTime_ = 0.0f;
    float supplyTickTimer_ = 0.0f;
    float fogTickTimer_ = 0.0f;
    float economyTickTimer_ = 0.0f;
    float gameSpeed_ = 1.0f;
    bool paused_ = false;
    bool showSupplyOverlay_ = true;
    int pendingBuildType_ = -1;
    int pendingSpecOpIdx_ = -1;
    int pendingAirMissionType_ = -1;
    float aiAirMissionTimer_ = 0.0f;
};

} // namespace aa2004
