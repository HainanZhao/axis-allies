#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — AAA 3D OPENGL 3.30 GLSL BATTLEFIELD RENDERER & HUD
// GLSL Splatmapped 3D Terrain, Animated Water Shader, Multi-Part 3D Tanks & HQs,
// Volumetric-Style Combat VFX, Weather, Supply Rings & Brushed-Metal HUD
// ============================================================================

#include "core/types.hpp"
#include "rts/rts_engine.hpp"

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace aa2004 {

// Dynamic Hardware Scalability Presets (Runs smoothly on Intel HD/Iris laptops up to high-end Apple Silicon & discrete GPUs)
enum class GraphicQuality {
    LOW = 0,     // Integrated Intel Iris / low-power laptops (60+ FPS lock, aggressive culling, simplified VFX)
    MEDIUM = 1,  // Standard laptops / older discrete GPUs (balanced visual fidelity & performance)
    HIGH = 2,    // Modern discrete GPUs / Apple Silicon (high detail, dynamic particles)
    ULTRA = 3    // Full AAA cinematic fidelity (120-170+ FPS high-refresh rate, maximum FBM terrain, full weather)
};

// Mathematically proven 6-plane View Frustum (Gribb/Hartmann algorithm)
struct FrustumPlane {
    Vector3 normal = {0.0f, 0.0f, 0.0f};
    float distance = 0.0f;
};

struct Frustum {
    FrustumPlane planes[6]; // Left, Right, Bottom, Top, Near, Far

    // Aligned planar buffers for AVX2 256-bit vectorized sphere testing (8 single-precision floats)
    alignas(32) float pNormX[8] = {0};
    alignas(32) float pNormY[8] = {0};
    alignas(32) float pNormZ[8] = {0};
    alignas(32) float pDist[8]  = {0};

    void packSIMD() {
        for (int i = 0; i < 6; ++i) {
            pNormX[i] = planes[i].normal.x;
            pNormY[i] = planes[i].normal.y;
            pNormZ[i] = planes[i].normal.z;
            pDist[i]  = planes[i].distance;
        }
        for (int i = 6; i < 8; ++i) {
            pNormX[i] = 0.0f;
            pNormY[i] = 0.0f;
            pNormZ[i] = 0.0f;
            pDist[i]  = 999999.0f; // Always passes (distance >= -radius)
        }
    }

#if defined(__AVX2__)
    inline bool containsSphere(Vector3 center, float radius) const {
        __m256 cx = _mm256_set1_ps(center.x);
        __m256 cy = _mm256_set1_ps(center.y);
        __m256 cz = _mm256_set1_ps(center.z);
        __m256 rNeg = _mm256_set1_ps(-radius);

        __m256 nx = _mm256_load_ps(pNormX);
        __m256 ny = _mm256_load_ps(pNormY);
        __m256 nz = _mm256_load_ps(pNormZ);
        __m256 dist = _mm256_load_ps(pDist);

        // d = dist + nx * cx + ny * cy + nz * cz using FMA
        __m256 d = _mm256_fmadd_ps(nx, cx, dist);
        d = _mm256_fmadd_ps(ny, cy, d);
        d = _mm256_fmadd_ps(nz, cz, d);

        // Compare: d < -radius -> outside
        __m256 cmp = _mm256_cmp_ps(d, rNeg, _CMP_LT_OQ);
        int mask = _mm256_movemask_ps(cmp);
        return (mask & 0x3F) == 0;
    }
#else
    inline bool containsSphere(Vector3 center, float radius) const {
        for (int i = 0; i < 6; ++i) {
            float d = planes[i].normal.x * center.x +
                      planes[i].normal.y * center.y +
                      planes[i].normal.z * center.z +
                      planes[i].distance;
            if (d < -radius) return false;
        }
        return true;
    }
#endif
};

struct PerformanceTelemetry {
    float fps = 60.0f;
    float frameTimeMs = 16.6f;
    int renderedEntities = 0;
    int culledEntities = 0;
    int drawCalls = 0;
    bool showOverlay = false;

    // Detailed Microsecond Subsystem Timers
    float timeFogTexUs = 0.0f;
    float timeTerrainUs = 0.0f;
    float timePropsUs = 0.0f;
    float timeBuildingsUs = 0.0f;
    float timeRegimentsUs = 0.0f;
    float timeParticlesUs = 0.0f;
    float timeOverheadPlatesUs = 0.0f;
};

class RTSRenderer {
public:
    RTSRenderer();
    ~RTSRenderer();

    void initForBattle(const RTSEngine& engine);
    void setCameraFocus(Vector3 target, float distance = 86.0f, float yaw = 0.0f, float pitch = 0.62f);
    void updateShowcaseCamera(float dt);
    void updateCameraAndInput(RTSEngine& engine, SoundEngine& sound, float dt);
    void render3DBattlefield(const RTSEngine& engine);
    void renderTacticalHUD(RTSEngine& engine, SoundEngine& sound, bool& outReturnToMenu, bool& outFinishBattle);

    Vector3 screenToGround(Vector2 screenPos, const RTSEngine& engine) const;
    const Camera3D& getCamera() const { return camera_; }

    // Dynamic Hardware Scalability & Telemetry API
    void setGraphicQuality(GraphicQuality q) { quality_ = q; }
    GraphicQuality getGraphicQuality() const { return quality_; }
    void cycleGraphicQuality();
    void toggleTelemetryOverlay() { telemetry_.showOverlay = !telemetry_.showOverlay; }
    bool isTelemetryOverlayVisible() const { return telemetry_.showOverlay; }
    const PerformanceTelemetry& getTelemetry() const { return telemetry_; }
    const Frustum& getFrustum() const { return frustum_; }

private:
    void ensureShadersAndMeshesLoaded();
    void updateFrustum();
    void drawAtmosphericSkyBackdrop(BiomeType biome) const;
    void drawTerrainAndWater(const RTSEngine& engine);
    void drawBridge3D(Vector3 center, float width, float length) const;
    void drawWeatherEffects3D(const RTSEngine& engine);
    void updateFogTexture(const RTSEngine& engine);
    void drawSupplyNetwork(const RTSEngine& engine);
    void drawBuilding3D(const RTSBuilding& b, const RTSEngine& engine);
    void drawRegiment3D(const RTSRegiment& r, const RTSEngine& engine);
    void drawAirStrikes3D(const RTSEngine& engine);
    void drawProjectilesAndParticles3D(const RTSEngine& engine);
    void drawUnitWorldBarsAndLabels(const RTSEngine& engine);
    void drawTacticalRadioLog(const RTSEngine& engine);
    void drawMinimap(RTSEngine& engine, Rectangle rect);
    void drawTelemetryHUD() const;
    void addCameraShake(float intensity, float duration = 0.35f);

    Camera3D camera_{};
    Vector3 cameraTarget_ = {0.0f, 0.0f, 82.0f};
    float cameraDistance_ = 88.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.62f; // Fixed standard RTS isometric pitch (radians)
    Frustum frustum_{};

    GraphicQuality quality_ = GraphicQuality::HIGH;
    mutable PerformanceTelemetry telemetry_{};
    float qualityToastTimer_ = 0.0f;
    std::string qualityToastMsg_;

    bool isDraggingSelect_ = false;
    Vector2 dragStartScreen_ = {0.0f, 0.0f};
    Vector2 dragCurrentScreen_ = {0.0f, 0.0f};
    bool attackMoveMode_ = false;
    bool showControlsHelp_ = false;
    Vector3 moveOrderIndicatorPos_ = {0.0f, 0.0f, 0.0f};
    float moveOrderIndicatorTimer_ = 0.0f;
    bool moveOrderIndicatorIsAttack_ = false;
    float waterAnimTime_ = 0.0f;
    float cameraShakeIntensity_ = 0.0f;
    float cameraShakeTimer_ = 0.0f;
    int lastGroupPressed_ = -1;
    float lastGroupPressTime_ = 0.0f;

    // OpenGL 3.30 GLSL Shaders & 3D Terrain/Water Models
    bool gpuResourcesLoaded_ = false;
    Shader terrainShader_{};
    Shader waterShader_{};
    Shader unitShader_{};
    int locTerrainCamPos_ = -1;
    int locTerrainBiome_  = -1;
    int locTerrainTime_   = -1;
    int locTerrainFogTex_ = -1;
    int locTerrainUseFog_ = -1;
    int locWaterCamPos_   = -1;
    int locWaterTime_     = -1;
    int locWaterFogTex_   = -1;
    int locWaterUseFog_   = -1;
    int locUnitCamPos_    = -1;
    int locUnitBiome_     = -1;
    int locUnitTime_      = -1;

    Model terrainModel_{};
    Model waterModel_{};
    Texture2D fogTexture_{};
    uint32_t lastUploadedFogVersion_ = 0;
    bool lastRevealAll_ = false;

    // AAA Particle Textures (Soft Volumetric Gaussian Billboards)
    Texture2D smokeParticleTex_{};
    Texture2D glowParticleTex_{};
};

} // namespace aa2004
