#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — MASTER APPLICATION UI & SCREEN CONTROLLER
// ============================================================================

#include "core/types.hpp"
#include "data/game_data.hpp"
#include "audio/sound_engine.hpp"
#include "rts/rts_engine.hpp"
#include "rts/rts_renderer.hpp"

namespace aa2004 {

class AxisAlliesApp {
public:
    AxisAlliesApp();
    ~AxisAlliesApp();

    void init(bool headless = false);
    void shutdown();
    void updateAndDrawFrame();
    bool shouldClose() const { return quitRequested_; }

    // Automated Self-Test Suite (verifies all 3D RTS systems, Supply, Formations, Save/Load)
    bool runSelfTestSuite();
    void capturePreviewScreenshots(const std::string& outDir);

private:
    void renderMainMenu();
    void renderCampaignSelectScreen();
    void renderSkirmishSetupScreen();
    void renderPostBattleScreen();
    void renderTopHardwareTelemetryBar();

    void startShowcaseBattle();
    void startInstantRTSBattle();
    void launchCampaignMission(int missionId);
    void saveCampaignProfile() const;
    void loadCampaignProfile();

    ScreenState screen_ = ScreenState::MAIN_MENU;
    bool quitRequested_ = false;
    bool showHardwareHUD_ = false;
    bool showManualInMenu_ = false;
    float menuAnimTime_ = 0.0f;

    // Setup selections
    NationId selectedNation_ = NationId::USA;
    int selectedGeneralIdx_ = 0;
    NationId enemyNation_ = NationId::GER;
    int enemyGeneralIdx_ = 0;
    int selectedMapIdx_ = 0;
    int selectedDifficulty_ = 1;
    int selectedResourcesPreset_ = 1; // 0=Standard, 1=High, 2=Total War
    int selectedGameMode_ = 0;        // 0=1v1 Duel, 1=2v2 Allied, 2=3-Player Survival, 3=4-Player FFA
    int selectedCampaignTab_ = 0;     // 0=Allied Campaign, 1=Axis Campaign
    uint32_t completedMissionsMask_ = 0;

    SoundEngine sound_;
    RTSEngine rtsEngine_;
    RTSRenderer rtsRenderer_;
};

} // namespace aa2004
