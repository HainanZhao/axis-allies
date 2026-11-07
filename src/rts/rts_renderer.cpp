// ============================================================================
// AXIS & ALLIES (2004) — AAA 3D OPENGL 3.30 GLSL BATTLEFIELD RENDERER & HUD
// ============================================================================

#include "rts/rts_renderer.hpp"
#include "rts/rts_models.hpp"
#include "ui/ui_theme.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace aa2004 {

namespace {

constexpr float kMapHalfExtent = 540.0f;
constexpr float kMapFullSpan   = 1120.0f;

bool DrawHUDButton(Rectangle rect, const char* label, Color bgCol, Color borderCol, int fontSize = 13, bool active = false, const Texture2D* icon = nullptr) {
    return UITheme::instance().drawMilitaryButton(rect, label, bgCol, borderCol, static_cast<float>(fontSize), active, icon);
}

// Generates a subdivided 3D terrain mesh with sunken riverbed and rolling perimeter ridges
Mesh GenerateBattlefieldMesh(float width, float length, int resX, int resZ) {
    Mesh m = GenMeshPlane(width, length, resX, resZ);
    int vCount = m.vertexCount;
    float* verts = m.vertices;
    float* norms = m.normals;

    auto evalHeight = [](float x, float z) -> float {
        float roadDist = std::min(std::fabs(x), std::fabs(std::fabs(x) - 240.0f));
        float roadFlatten = std::clamp((roadDist - 12.0f) / 18.0f, 0.0f, 1.0f);
        float edgeFactor = std::clamp((std::max(std::fabs(x), std::fabs(z)) - 460.0f) / 90.0f, 0.0f, 1.0f);
        float hills = (std::sin(x * 0.045f) * std::cos(z * 0.045f) * 1.8f +
                       std::sin(x * 0.11f + z * 0.09f) * 0.65f) * edgeFactor * roadFlatten;
        return hills;
    };

    for (int i = 0; i < vCount; ++i) {
        float vx = verts[i * 3 + 0];
        float vz = verts[i * 3 + 2];
        verts[i * 3 + 1] = evalHeight(vx, vz);

        float eps = 1.5f;
        float hL = evalHeight(vx - eps, vz);
        float hR = evalHeight(vx + eps, vz);
        float hD = evalHeight(vx, vz - eps);
        float hU = evalHeight(vx, vz + eps);
        Vector3 n = Vector3Normalize({hL - hR, 2.0f * eps, hD - hU});
        norms[i * 3 + 0] = n.x;
        norms[i * 3 + 1] = n.y;
        norms[i * 3 + 2] = n.z;
    }
    UpdateMeshBuffer(m, 0, m.vertices, m.vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(m, 2, m.normals,  m.vertexCount * 3 * sizeof(float), 0);
    return m;
}

} // namespace

RTSRenderer::RTSRenderer() {
    camera_.position = {0.0f, 65.0f, 145.0f};
    camera_.target   = {0.0f, 0.0f, 75.0f};
    camera_.up       = {0.0f, 1.0f, 0.0f};
    camera_.fovy     = 42.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

RTSRenderer::~RTSRenderer() {
    if (smokeParticleTex_.id != 0) {
        UnloadTexture(smokeParticleTex_);
        smokeParticleTex_.id = 0;
    }
    if (glowParticleTex_.id != 0) {
        UnloadTexture(glowParticleTex_);
        glowParticleTex_.id = 0;
    }
    if (fogTexture_.id != 0) {
        UnloadTexture(fogTexture_);
        fogTexture_.id = 0;
    }
}

void RTSRenderer::ensureShadersAndMeshesLoaded() {
    if (gpuResourcesLoaded_ || !IsWindowReady()) return;

    if (FileExists("assets/shaders/terrain.vs") && FileExists("assets/shaders/terrain.fs")) {
        terrainShader_ = LoadShader("assets/shaders/terrain.vs", "assets/shaders/terrain.fs");
        locTerrainCamPos_ = GetShaderLocation(terrainShader_, "camPos");
        locTerrainBiome_  = GetShaderLocation(terrainShader_, "biomeId");
        locTerrainTime_   = GetShaderLocation(terrainShader_, "timeSec");
        locTerrainFogTex_ = GetShaderLocation(terrainShader_, "fogTexture");
        locTerrainUseFog_ = GetShaderLocation(terrainShader_, "useFogTexture");
    }
    if (FileExists("assets/shaders/terrain.vs") && FileExists("assets/shaders/water.fs")) {
        waterShader_ = LoadShader("assets/shaders/terrain.vs", "assets/shaders/water.fs");
        locWaterCamPos_ = GetShaderLocation(waterShader_, "camPos");
        locWaterTime_   = GetShaderLocation(waterShader_, "timeSec");
        locWaterFogTex_ = GetShaderLocation(waterShader_, "fogTexture");
        locWaterUseFog_ = GetShaderLocation(waterShader_, "useFogTexture");
    }
    if (FileExists("assets/shaders/terrain.vs") && FileExists("assets/shaders/unit_pbr.fs")) {
        unitShader_ = LoadShader("assets/shaders/terrain.vs", "assets/shaders/unit_pbr.fs");
        locUnitCamPos_ = GetShaderLocation(unitShader_, "camPos");
        locUnitBiome_  = GetShaderLocation(unitShader_, "biomeId");
        locUnitTime_   = GetShaderLocation(unitShader_, "timeSec");
    }

    if (fogTexture_.id == 0) {
        Image fogImg = GenImageColor(RTSEngine::kFogGridSize, RTSEngine::kFogGridSize, BLACK);
        fogTexture_ = LoadTextureFromImage(fogImg);
        UnloadImage(fogImg);
        SetTextureFilter(fogTexture_, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(fogTexture_, TEXTURE_WRAP_CLAMP);
    }

    Mesh tMesh = GenerateBattlefieldMesh(1400.0f, 1400.0f, 180, 180);
    terrainModel_ = LoadModelFromMesh(tMesh);
    if (terrainShader_.id != 0) {
        terrainModel_.materials[0].shader = terrainShader_;
        terrainModel_.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture = fogTexture_;
        terrainShader_.locs[SHADER_LOC_MAP_DIFFUSE + MATERIAL_MAP_OCCLUSION] = locTerrainFogTex_;
    }

    Mesh wMesh = GenMeshPlane(1400.0f, 12.0f, 48, 8);
    waterModel_ = LoadModelFromMesh(wMesh);
    if (waterShader_.id != 0) {
        waterModel_.materials[0].shader = waterShader_;
        waterModel_.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture = fogTexture_;
        waterShader_.locs[SHADER_LOC_MAP_DIFFUSE + MATERIAL_MAP_OCCLUSION] = locWaterFogTex_;
    }

    if (smokeParticleTex_.id == 0) {
        Image smokeImg = GenImageGradientRadial(128, 128, 0.0f, Color{255, 255, 255, 210}, Color{255, 255, 255, 0});
        smokeParticleTex_ = LoadTextureFromImage(smokeImg);
        UnloadImage(smokeImg);
        SetTextureFilter(smokeParticleTex_, TEXTURE_FILTER_BILINEAR);
    }
    if (glowParticleTex_.id == 0) {
        Image glowImg = GenImageGradientRadial(128, 128, 0.12f, Color{255, 255, 255, 240}, Color{255, 175, 45, 0});
        glowParticleTex_ = LoadTextureFromImage(glowImg);
        UnloadImage(glowImg);
        SetTextureFilter(glowParticleTex_, TEXTURE_FILTER_BILINEAR);
    }

    gpuResourcesLoaded_ = true;
}

void RTSRenderer::cycleGraphicQuality() {
    int next = (static_cast<int>(quality_) + 1) % 4;
    quality_ = static_cast<GraphicQuality>(next);
    qualityToastTimer_ = 2.8f;
    switch (quality_) {
        case GraphicQuality::LOW:
            qualityToastMsg_ = "GRAPHICS PRESET: LOW (Max FPS Lock, Aggressive Frustum Culling, iGPU Optimized)";
            break;
        case GraphicQuality::MEDIUM:
            qualityToastMsg_ = "GRAPHICS PRESET: MEDIUM (Balanced Visuals & High Framerate)";
            break;
        case GraphicQuality::HIGH:
            qualityToastMsg_ = "GRAPHICS PRESET: HIGH (Dynamic Volumetric VFX & PBR Shading)";
            break;
        case GraphicQuality::ULTRA:
            qualityToastMsg_ = "GRAPHICS PRESET: ULTRA (120-170+ FPS High-Refresh Cinematic Fidelity)";
            break;
    }
}

void RTSRenderer::updateFrustum() {
    float aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(std::max(1, GetScreenHeight()));
    Matrix view = MatrixLookAt(camera_.position, camera_.target, camera_.up);
    Matrix proj = MatrixPerspective(camera_.fovy * DEG2RAD, aspect, 1.0f, 650.0f);
    Matrix m = MatrixMultiply(view, proj);

    // Gribb-Hartmann Frustum Plane Extraction (row major indices in raylib Matrix)
    // Left
    frustum_.planes[0].normal = { m.m3 + m.m0, m.m7 + m.m4, m.m11 + m.m8 };
    frustum_.planes[0].distance = m.m15 + m.m12;
    // Right
    frustum_.planes[1].normal = { m.m3 - m.m0, m.m7 - m.m4, m.m11 - m.m8 };
    frustum_.planes[1].distance = m.m15 - m.m12;
    // Bottom
    frustum_.planes[2].normal = { m.m3 + m.m1, m.m7 + m.m5, m.m11 + m.m9 };
    frustum_.planes[2].distance = m.m15 + m.m13;
    // Top
    frustum_.planes[3].normal = { m.m3 - m.m1, m.m7 - m.m5, m.m11 - m.m9 };
    frustum_.planes[3].distance = m.m15 - m.m13;
    // Near
    frustum_.planes[4].normal = { m.m3 + m.m2, m.m7 + m.m6, m.m11 + m.m10 };
    frustum_.planes[4].distance = m.m15 + m.m14;
    // Far
    frustum_.planes[5].normal = { m.m3 - m.m2, m.m7 - m.m6, m.m11 - m.m10 };
    frustum_.planes[5].distance = m.m15 - m.m14;

    for (int i = 0; i < 6; ++i) {
        float len = Vector3Length(frustum_.planes[i].normal);
        if (len > 0.00001f) {
            frustum_.planes[i].normal = Vector3Scale(frustum_.planes[i].normal, 1.0f / len);
            frustum_.planes[i].distance /= len;
        }
    }
    frustum_.packSIMD();
}

void RTSRenderer::initForBattle(const RTSEngine& engine) {
    ensureShadersAndMeshesLoaded();
    // Center camera dynamically over Player's Procedural Home Base Sector so the player immediately
    // sees their Corps HQ, Supply Depot, starting reconnaissance half-track, and infantry regiment.
    Vector3 pBase = engine.getPlayerBasePos();
    setCameraFocus(pBase, 86.0f, 0.0f, 0.62f);
    isDraggingSelect_ = false;
    attackMoveMode_   = false;
    cameraShakeIntensity_ = 0.0f;
    cameraShakeTimer_ = 0.0f;
}

void RTSRenderer::addCameraShake(float intensity, float duration) {
    (void)intensity;
    (void)duration;
    cameraShakeIntensity_ = 0.0f;
    cameraShakeTimer_ = 0.0f;
}

void RTSRenderer::setCameraFocus(Vector3 target, float distance, float yaw, float pitch) {
    ensureShadersAndMeshesLoaded();
    cameraTarget_   = target;
    cameraDistance_ = distance;
    cameraYaw_      = yaw;
    cameraPitch_    = pitch;
    float horizDist = cameraDistance_ * std::cos(cameraPitch_);
    camera_.target   = cameraTarget_;
    camera_.position = {
        cameraTarget_.x - std::sin(cameraYaw_) * horizDist,
        cameraTarget_.y + cameraDistance_ * std::sin(cameraPitch_),
        cameraTarget_.z + std::cos(cameraYaw_) * horizDist
    };
    updateFrustum();
}

void RTSRenderer::updateShowcaseCamera(float dt) {
    ensureShadersAndMeshesLoaded();
    waterAnimTime_ += dt;
    cameraTarget_   = {0.0f, 0.0f, 10.0f};
    cameraDistance_ = 68.0f;
    cameraPitch_    = 0.56f;
    cameraYaw_      = -0.18f + std::sin(waterAnimTime_ * 0.16f) * 0.26f;
    float horizDist = cameraDistance_ * std::cos(cameraPitch_);
    camera_.target   = cameraTarget_;
    camera_.position = {
        cameraTarget_.x - std::sin(cameraYaw_) * horizDist,
        cameraTarget_.y + cameraDistance_ * std::sin(cameraPitch_),
        cameraTarget_.z + std::cos(cameraYaw_) * horizDist
    };
    updateFrustum();
}

Vector3 RTSRenderer::screenToGround(Vector2 screenPos, const RTSEngine& engine) const {
    Ray ray = GetMouseRay(screenPos, camera_);
    if (std::fabs(ray.direction.y) < 0.0001f) {
        return cameraTarget_;
    }
    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) return cameraTarget_;
    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    hit.x = std::clamp(hit.x, -kMapHalfExtent, kMapHalfExtent);
    hit.z = std::clamp(hit.z, -kMapHalfExtent, kMapHalfExtent);
    hit.y = engine.getTerrainHeight(hit.x, hit.z);
    return hit;
}

void RTSRenderer::updateCameraAndInput(RTSEngine& engine, SoundEngine& sound, float dt) {
    waterAnimTime_ += dt;
    if (moveOrderIndicatorTimer_ > 0.0f) {
        moveOrderIndicatorTimer_ = std::max(0.0f, moveOrderIndicatorTimer_ - dt);
    }

    float panSpeed = 155.0f * dt * (cameraDistance_ / 85.0f);
    Vector3 forward = {std::sin(cameraYaw_), 0.0f, -std::cos(cameraYaw_)};
    Vector3 right   = {std::cos(cameraYaw_), 0.0f,  std::sin(cameraYaw_)};

    // 1. Keyboard Map Panning (WASD or Arrow Keys)
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    cameraTarget_ = Vector3Add(cameraTarget_, Vector3Scale(forward, panSpeed));
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  cameraTarget_ = Vector3Subtract(cameraTarget_, Vector3Scale(forward, panSpeed));
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) cameraTarget_ = Vector3Add(cameraTarget_, Vector3Scale(right, panSpeed));
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  cameraTarget_ = Vector3Subtract(cameraTarget_, Vector3Scale(right, panSpeed));

    // 2. 4-Way Screen-Edge Panning (Top, Bottom, Left, Right)
    Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (mouse.x >= 0 && mouse.x < 18) {
        cameraTarget_ = Vector3Subtract(cameraTarget_, Vector3Scale(right, panSpeed * 0.75f));
    } else if (mouse.x > sw - 18 && mouse.x <= sw) {
        cameraTarget_ = Vector3Add(cameraTarget_, Vector3Scale(right, panSpeed * 0.75f));
    }
    if (mouse.y >= 0 && mouse.y < 14) {
        cameraTarget_ = Vector3Add(cameraTarget_, Vector3Scale(forward, panSpeed * 0.75f));
    } else if (mouse.y > sh - 14 && mouse.y <= sh) {
        cameraTarget_ = Vector3Subtract(cameraTarget_, Vector3Scale(forward, panSpeed * 0.75f));
    }

    // 3. Grab-and-Drag Map Panning (Middle-Mouse Drag OR Spacebar + Left-Drag)
    // Pressing Spacebar alone smoothly pans the camera directly to the latest tactical battle engagement!
    if (IsKeyPressed(KEY_SPACE) && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector3 alert = engine.getLastAlertPos();
        if (alert.x != 0.0f || alert.z != 0.0f) {
            cameraTarget_ = alert;
            sound.playSfx(SfxId::UI_CONFIRM, 0.6f);
        }
    }
    bool grabPanning = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (IsKeyDown(KEY_SPACE) && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    if (grabPanning) {
        Vector2 delta = GetMouseDelta();
        float dragScale = 0.22f * (cameraDistance_ / 85.0f);
        cameraTarget_ = Vector3Subtract(cameraTarget_, Vector3Scale(right, delta.x * dragScale));
        cameraTarget_ = Vector3Add(cameraTarget_, Vector3Scale(forward, delta.y * dragScale));
    }

    // 4. Camera Zoom (Mouse Wheel / + / -) -- Camera Yaw and Pitch remain fixed at authentic RTS perspective
    cameraYaw_   = 0.0f;
    cameraPitch_ = 0.62f;

    float wheel = GetMouseWheelMove();
    if (std::fabs(wheel) > 0.01f) {
        cameraDistance_ = std::clamp(cameraDistance_ - wheel * 10.0f, 38.0f, 210.0f);
    }
    if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) {
        cameraDistance_ = std::clamp(cameraDistance_ - 55.0f * dt, 38.0f, 210.0f);
    }
    if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) {
        cameraDistance_ = std::clamp(cameraDistance_ + 55.0f * dt, 38.0f, 210.0f);
    }

    cameraTarget_.x = std::clamp(cameraTarget_.x, -kMapHalfExtent, kMapHalfExtent);
    cameraTarget_.z = std::clamp(cameraTarget_.z, -kMapHalfExtent, kMapHalfExtent);

    float horizDist = cameraDistance_ * std::cos(cameraPitch_);
    camera_.target   = cameraTarget_;
    camera_.position = {
        cameraTarget_.x - std::sin(cameraYaw_) * horizDist,
        cameraTarget_.y + cameraDistance_ * std::sin(cameraPitch_),
        cameraTarget_.z + std::cos(cameraYaw_) * horizDist
    };

    Vector3 camDir = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
    sound.setListener(cameraTarget_, camDir);

    bool modDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER)   || IsKeyDown(KEY_RIGHT_SUPER) ||
                   IsKeyDown(KEY_LEFT_ALT)     || IsKeyDown(KEY_RIGHT_ALT);

    if (IsKeyPressed(KEY_TAB))   engine.toggleSupplyOverlay();
    if (IsKeyPressed(KEY_X))     engine.selectAllPlayerCombatUnits();
    if (IsKeyPressed(KEY_G))     cameraTarget_ = {0.0f, 0.0f, 14.0f};
    if (IsKeyPressed(KEY_SLASH)) showControlsHelp_ = !showControlsHelp_;
    if (IsKeyPressed(KEY_F10)) {
        cycleGraphicQuality();
        sound.playSfx(SfxId::UI_CLICK, 0.7f);
    }
    if (IsKeyPressed(KEY_F11)) {
        toggleTelemetryOverlay();
        sound.playSfx(SfxId::UI_CLICK, 0.7f);
    }
    if (qualityToastTimer_ > 0.0f) {
        qualityToastTimer_ = std::max(0.0f, qualityToastTimer_ - dt);
    }
    if (IsKeyPressed(KEY_M) && !modDown) {
        attackMoveMode_ = !attackMoveMode_;
        sound.playSfx(SfxId::UI_CLICK, 0.6f);
    }
    if (IsKeyPressed(KEY_H)) {
        for (auto& r : engine.getRegimentsMut()) r.selected = false;
        for (auto& b : engine.getBuildingsMut()) {
            b.selected = (b.teamId == 0 && b.type == BuildingType::CORPS_HQ);
            if (b.selected) {
                cameraTarget_ = b.pos;
                sound.playSfx(SfxId::UI_CLICK, 0.6f);
            }
        }
    }
    if (IsKeyPressed(KEY_P)) {
        for (const auto& b : engine.getBuildings()) {
            if (b.teamId == 0 && b.selected && b.id != -1) {
                engine.togglePackBuilding(b.id);
                break;
            }
        }
        for (const auto& r : engine.getRegiments()) {
            if (r.teamId == 0 && r.selected && r.type == RegimentType::PACKED_HQ_TRUCK) {
                engine.unpackTruckRegiment(r.id);
                break;
            }
        }
    }
    if (IsKeyPressed(KEY_U)) {
        engine.upgradeCorpsHQTech(0);
    }

    // Global Tactical Hotkey [B]: Call Tactical Bombing Run directly anytime from the battlefield!
    if (IsKeyPressed(KEY_B) && !modDown) {
        if (engine.pendingAirMissionType() == 2) {
            engine.setPendingAirMissionType(-1);
            sound.playSfx(SfxId::UI_CLICK, 0.6f);
        } else {
            int opAirfields = engine.getOperationalAirfieldCount(0);
            int activeSorties = engine.getActiveAirSortiesCount(0);
            int maxSorties = engine.getMaxAirSorties(0);
            const auto& p = engine.getPlayer(0);
            if (opAirfields <= 0) {
                engine.pushLog("AIR COMMAND: Operational Airfield required! Press [F] to select or build an Airfield.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.techTier < 2) {
                engine.pushLog("AIR COMMAND TECH REQUIRED: Upgrade Corps HQ to Tier 2 [U] to unlock Heavy Tactical Bombers!");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (activeSorties >= maxSorties) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for bombers to return or build an additional Airfield.",
                                          activeSorties, maxSorties));
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.money < 165.0f || p.ammo < 40.0f || p.oil < 45.0f) {
                engine.pushLog("INSUFFICIENT RESOURCES: Need $165 Money, 40 Ammo, 45 Oil for Tactical Bombing Run.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else {
                engine.setPendingBuildType(-1);
                engine.setPendingSpecOpIdx(-1);
                engine.setPendingAirMissionType(2);
                sound.playSfx(SfxId::UI_CLICK, 0.7f);
                engine.pushLog(TextFormat("TACTICAL BOMBING RUN [B] (%d/%d Sorties Active): Left-click target coordinates on battlefield to unleash bomber strike! [ESC to cancel]",
                                          activeSorties, maxSorties));
            }
        }
    }

    // Global Tactical Hotkey [E]: Call Recon Flight directly anytime from the battlefield!
    if (IsKeyPressed(KEY_E) && !modDown) {
        if (engine.pendingAirMissionType() == 1) {
            engine.setPendingAirMissionType(-1);
            sound.playSfx(SfxId::UI_CLICK, 0.6f);
        } else {
            int opAirfields = engine.getOperationalAirfieldCount(0);
            int activeSorties = engine.getActiveAirSortiesCount(0);
            int maxSorties = engine.getMaxAirSorties(0);
            const auto& p = engine.getPlayer(0);
            if (opAirfields <= 0) {
                engine.pushLog("AIR COMMAND: Operational Airfield required! Press [F] to select or build an Airfield.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (activeSorties >= maxSorties) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for reconnaissance aircraft to return or build an additional Airfield.",
                                          activeSorties, maxSorties));
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.money < 75.0f || p.oil < 20.0f) {
                engine.pushLog("INSUFFICIENT RESOURCES: Need $75 Money, 20 Oil for Air Reconnaissance flight.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else {
                engine.setPendingBuildType(-1);
                engine.setPendingSpecOpIdx(-1);
                engine.setPendingAirMissionType(1);
                sound.playSfx(SfxId::UI_CLICK, 0.7f);
                engine.pushLog(TextFormat("AIR RECONNAISSANCE [E] (%d/%d Sorties Active): Left-click battlefield coordinates to dispatch recon plane! [ESC to cancel]",
                                          activeSorties, maxSorties));
            }
        }
    }

    // Global Tactical Hotkey [R]: Call Air Supply Drop directly anytime from the battlefield!
    if (IsKeyPressed(KEY_R) && !modDown) {
        if (engine.pendingAirMissionType() == 4) {
            engine.setPendingAirMissionType(-1);
            sound.playSfx(SfxId::UI_CLICK, 0.6f);
        } else {
            int opAirfields = engine.getOperationalAirfieldCount(0);
            int activeSorties = engine.getActiveAirSortiesCount(0);
            int maxSorties = engine.getMaxAirSorties(0);
            const auto& p = engine.getPlayer(0);
            if (opAirfields <= 0) {
                engine.pushLog("AIR SUPPLY UNAVAILABLE: Airfield required! Press [F] to select or build an Airfield.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (activeSorties >= maxSorties) {
                engine.pushLog(TextFormat("AIR SUPPLY BUSY (%d/%d sorties airborne): All flight wings deployed! Wait for aircraft to return or build an additional Airfield.",
                                          activeSorties, maxSorties));
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.money < 120.0f || p.oil < 30.0f) {
                engine.pushLog("INSUFFICIENT RESOURCES: Need $120 Money, 30 Oil for Emergency Supply Drop.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else {
                engine.setPendingBuildType(-1);
                engine.setPendingSpecOpIdx(-1);
                engine.setPendingAirMissionType(4);
                sound.playSfx(SfxId::UI_CLICK, 0.7f);
                engine.pushLog(TextFormat("AIR SUPPLY DROP [R] (%d/%d Sorties Active): Click battlefield coordinates to call emergency C-47 supply drop! [ESC to cancel]",
                                          activeSorties, maxSorties));
            }
        }
    }

    // Global Tactical Hotkey [P]: Call Airborne Insertion directly anytime from the battlefield!
    if (IsKeyPressed(KEY_P) && !modDown) {
        if (engine.pendingAirMissionType() == 3) {
            engine.setPendingAirMissionType(-1);
            sound.playSfx(SfxId::UI_CLICK, 0.6f);
        } else {
            int opAirfields = engine.getOperationalAirfieldCount(0);
            int activeSorties = engine.getActiveAirSortiesCount(0);
            int maxSorties = engine.getMaxAirSorties(0);
            const auto& p = engine.getPlayer(0);
            if (opAirfields <= 0) {
                engine.pushLog("AIR COMMAND: Operational Airfield required! Press [F] to select or build an Airfield.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.techTier < 2) {
                engine.pushLog("AIR COMMAND TECH REQUIRED: Upgrade Corps HQ to Tier 2 [U] to unlock Airborne drops!");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (activeSorties >= maxSorties) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for transport to return or build an additional Airfield.",
                                          activeSorties, maxSorties));
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.money < 180.0f || p.ammo < 35.0f) {
                engine.pushLog("INSUFFICIENT RESOURCES: Need $180 Money, 35 Ammo for Airborne Drop.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else {
                engine.setPendingBuildType(-1);
                engine.setPendingSpecOpIdx(-1);
                engine.setPendingAirMissionType(3);
                sound.playSfx(SfxId::UI_CLICK, 0.7f);
                engine.pushLog(TextFormat("AIRBORNE INSERTION [P] (%d/%d Sorties Active): Left-click battlefield drop zone for Paratroopers & Bazooka squad! [ESC to cancel]",
                                          activeSorties, maxSorties));
            }
        }
    }

    // Global Tactical Hotkey [J]: Combat Air Patrol / Fighter Sweep interceptor sortie!
    if (IsKeyPressed(KEY_J) && !modDown) {
        if (engine.pendingAirMissionType() == 5) {
            engine.setPendingAirMissionType(-1);
            sound.playSfx(SfxId::UI_CLICK, 0.6f);
        } else {
            int opAirfields = engine.getOperationalAirfieldCount(0);
            int activeSorties = engine.getActiveAirSortiesCount(0);
            int maxSorties = engine.getMaxAirSorties(0);
            const auto& p = engine.getPlayer(0);
            if (opAirfields <= 0) {
                engine.pushLog("AIR COMMAND: Operational Airfield required! Press [F] to select or build an Airfield.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.techTier < 1) {
                engine.pushLog("AIR COMMAND TECH REQUIRED: Upgrade Corps HQ to Tier 1 to unlock Fighter Sweeps!");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (activeSorties >= maxSorties) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Wait for interceptors to return or build an additional Airfield.",
                                          activeSorties, maxSorties));
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else if (p.money < 140.0f || p.ammo < 30.0f || p.oil < 35.0f) {
                engine.pushLog("INSUFFICIENT RESOURCES: Need $140 Money, 30 Ammo, 35 Oil for Fighter Sweep CAP.");
                sound.playSfx(SfxId::UI_CLICK, 0.45f);
            } else {
                engine.setPendingBuildType(-1);
                engine.setPendingSpecOpIdx(-1);
                engine.setPendingAirMissionType(5);
                sound.playSfx(SfxId::UI_CLICK, 0.7f);
                engine.pushLog(TextFormat("COMBAT AIR PATROL [J] (%d/%d Sorties Active): Left-click patrol coordinates to scramble Fighter Interceptors! [ESC to cancel]",
                                          activeSorties, maxSorties));
            }
        }
    }

    // Unit Field Ability Hotkeys: [K] Smoke Screen Canister (Armor), [L] Rapid Suppression Volley (Infantry/Artillery)
    if (IsKeyPressed(KEY_K) && !modDown) {
        bool fired = false;
        for (const auto& r : engine.getRegiments()) {
            if (r.teamId == 0 && r.selected && r.health > 0.0f) {
                if (engine.triggerRegimentAbility(r.id, 0)) {
                    fired = true;
                }
            }
        }
        if (fired) sound.playSfx(SfxId::MORTAR_THUMP, 0.7f);
    }
    if (IsKeyPressed(KEY_L) && !modDown) {
        bool fired = false;
        for (const auto& r : engine.getRegiments()) {
            if (r.teamId == 0 && r.selected && r.health > 0.0f) {
                if (engine.triggerRegimentAbility(r.id, 1)) {
                    fired = true;
                }
            }
        }
        if (fired) sound.playSfx(SfxId::RIFLE_VOLLEY, 0.8f);
    }

    if (IsKeyPressed(KEY_Z)) {
        engine.setSelectedFormationStance(FormationStance::LINE);
        sound.playSfx(SfxId::UI_CLICK, 0.55f);
    }
    if (IsKeyPressed(KEY_V)) {
        engine.setSelectedFormationStance(FormationStance::COLUMN);
        sound.playSfx(SfxId::UI_CLICK, 0.55f);
    }
    if (IsKeyPressed(KEY_C)) {
        engine.setSelectedFormationStance(FormationStance::ENTRENCHED);
        sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
    }

    if (IsKeyPressed(KEY_F5)) {
        engine.saveBattleToFile("saves/quicksave_rts.dat");
        sound.playSfx(SfxId::UI_CONFIRM, 0.8f);
    }
    if (IsKeyPressed(KEY_F8)) {
        engine.loadBattleFromFile("saves/quicksave_rts.dat");
        sound.playSfx(SfxId::UI_CONFIRM, 0.8f);
    }

    // 5. Smart Control Groups (1-9):
    // - Ctrl/Cmd/Alt + 1..9 always assigns selected units to Group g.
    // - Pressing 1..9 when Group g is empty ALSO assigns selected units to Group g automatically!
    // - Pressing 1..9 when Group g has units selects Group g (double-tap centers camera).
    modDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
              IsKeyDown(KEY_LEFT_SUPER)   || IsKeyDown(KEY_RIGHT_SUPER) ||
              IsKeyDown(KEY_LEFT_ALT)     || IsKeyDown(KEY_RIGHT_ALT);
    for (int g = 1; g <= 9; ++g) {
        int key = KEY_ZERO + g;
        if (IsKeyPressed(key)) {
            int groupExistingCount = 0;
            int selectedFriendlyCount = 0;
            for (const auto& r : engine.getRegiments()) {
                if (r.teamId == 0 && r.health > 0.0f) {
                    if (r.controlGroup == g) ++groupExistingCount;
                    if (r.selected) ++selectedFriendlyCount;
                }
            }

            if (modDown || (groupExistingCount == 0 && selectedFriendlyCount > 0)) {
                engine.assignControlGroup(g);
                sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
            } else {
                Vector3 center{0, 0, 0};
                if (engine.selectControlGroup(g, center)) {
                    sound.playSfx(SfxId::UI_CLICK, 0.6f);
                    float now = static_cast<float>(GetTime());
                    if (lastGroupPressed_ == g && (now - lastGroupPressTime_) < 0.38f) {
                        cameraTarget_ = center;
                    }
                    lastGroupPressed_ = g;
                    lastGroupPressTime_ = now;
                }
            }
        }
    }

    if (IsKeyPressed(KEY_F1)) engine.triggerSpecialOp(0, 0, screenToGround(mouse, engine));
    if (IsKeyPressed(KEY_F2)) engine.triggerSpecialOp(0, 1, screenToGround(mouse, engine));
    if (IsKeyPressed(KEY_F3)) engine.triggerSpecialOp(0, 2, screenToGround(mouse, engine));

    bool onHUD = (mouse.y < 44.0f) || (mouse.y > static_cast<float>(sh - 196));
    if (onHUD || grabPanning) return;

    Vector3 groundHit = screenToGround(mouse, engine);

    // 6. Building Placement Mode (Left-Click on map to construct, Right-Click/ESC to cancel)
    if (engine.pendingBuildType() >= 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (engine.constructSupportBuilding(static_cast<BuildingType>(engine.pendingBuildType()), groundHit, 0)) {
                // Automatically select the newly constructed building so the player sees it operational!
                for (auto& r : engine.getRegimentsMut()) r.selected = false;
                for (auto& b : engine.getBuildingsMut()) b.selected = false;
                if (!engine.getBuildingsMut().empty()) {
                    engine.getBuildingsMut().back().selected = true;
                }
                engine.setPendingBuildType(-1);
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
            engine.setPendingBuildType(-1);
        }
        return;
    }

    // 7. Special Operation Area Targeting Mode
    if (engine.pendingSpecOpIdx() >= 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            engine.triggerSpecialOp(0, engine.pendingSpecOpIdx(), groundHit);
            engine.setPendingSpecOpIdx(-1);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
            engine.setPendingSpecOpIdx(-1);
        }
        return;
    }

    // 7B. Air Force Mission Targeting Mode (Left-Click to launch, Right-Click/ESC to cancel)
    if (engine.pendingAirMissionType() >= 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int mission = engine.pendingAirMissionType();
            if (mission == 1) engine.launchAirReconMission(0, groundHit);
            else if (mission == 2) engine.launchAirBombingMission(0, groundHit);
            else if (mission == 3) engine.launchAirParatroopDrop(0, groundHit);
            else if (mission == 4) engine.launchAirSupplyDrop(0, groundHit);
            else if (mission == 5) engine.launchAirFighterSweep(0, groundHit);
            engine.setPendingAirMissionType(-1);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
            engine.setPendingAirMissionType(-1);
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (attackMoveMode_) {
            attackMoveMode_ = false;
        } else {
            for (auto& r : engine.getRegimentsMut()) r.selected = false;
            for (auto& b : engine.getBuildingsMut()) b.selected = false;
        }
    }

    // 8. Explicit Move Order Mode ([M] button or Cmd/Option+Click)
    bool trackpadOrderMod = attackMoveMode_ || IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) || IsKeyDown(KEY_LEFT_SUPER);
    if (trackpadOrderMod && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        engine.issueRightClickOrder(groundHit);
        moveOrderIndicatorPos_   = groundHit;
        moveOrderIndicatorTimer_ = 1.25f;
        attackMoveMode_ = false;
        return;
    }

    // 9. Left-Click / Drag Box Selection & Intuitive Single-Click Move Orders
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        isDraggingSelect_  = true;
        dragStartScreen_   = mouse;
        dragCurrentScreen_ = mouse;
    }
    if (isDraggingSelect_) {
        dragCurrentScreen_ = mouse;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            isDraggingSelect_ = false;
            float dragDist = Vector2Distance(dragStartScreen_, dragCurrentScreen_);
            bool shiftAdd = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

            if (dragDist < 10.0f) {
                // Check if the player clicked on a friendly regiment or friendly building (in 2D screen space or 3D ground space)
                int clickedFriendlyRegId = -1;
                float bestRegDist = 34.0f; // 2D screen pixel radius for easy clicking on soldiers/tanks
                for (const auto& r : engine.getRegiments()) {
                    if (r.teamId != 0 || r.health <= 0.0f) continue;
                    Vector2 sp = GetWorldToScreen({r.pos.x, r.pos.y + 1.8f, r.pos.z}, camera_);
                    float d2 = Vector2Distance(mouse, sp);
                    float d3 = Vector2Distance({r.pos.x, r.pos.z}, {groundHit.x, groundHit.z});
                    if (d2 < bestRegDist || d3 < 6.5f) {
                        bestRegDist = std::min(bestRegDist, d2);
                        clickedFriendlyRegId = r.id;
                    }
                }

                int clickedFriendlyBldgId = -1;
                float bestRayDist = 1e9f;
                float bestGroundDist = 1e9f;
                Ray clickRay = GetMouseRay(mouse, camera_);
                if (clickedFriendlyRegId == -1) {
                    for (const auto& b : engine.getBuildings()) {
                        if (b.teamId != 0 || b.health <= 0.0f) continue;
                        float clickRadius = 11.0f;
                        float clickHeight = 7.0f;
                        if (b.type == BuildingType::AIRFIELD) { clickRadius = 18.0f; clickHeight = 9.0f; }
                        else if (b.type == BuildingType::CORPS_HQ || b.type == BuildingType::CITY_DEPOT) { clickRadius = 16.0f; clickHeight = 12.0f; }
                        else if (b.type == BuildingType::INFANTRY_HQ || b.type == BuildingType::MECH_HQ || b.type == BuildingType::ARMOR_HQ) { clickRadius = 13.5f; clickHeight = 8.0f; }

                        // 1. Ray-to-BoundingBox test for exact 3D building geometry picking
                        BoundingBox bBox = {
                            {b.pos.x - clickRadius * 0.55f, b.pos.y - 0.5f, b.pos.z - clickRadius * 0.55f},
                            {b.pos.x + clickRadius * 0.55f, b.pos.y + clickHeight, b.pos.z + clickRadius * 0.55f}
                        };
                        RayCollision col = GetRayCollisionBox(clickRay, bBox);
                        if (col.hit && col.distance < bestRayDist) {
                            bestRayDist = col.distance;
                            clickedFriendlyBldgId = b.id;
                        }

                        // 2. Ground-plane distance fallback within building footprint
                        float d3 = Vector2Distance({b.pos.x, b.pos.z}, {groundHit.x, groundHit.z});
                        if (bestRayDist > 1e8f && d3 <= clickRadius && d3 < bestGroundDist) {
                            bestGroundDist = d3;
                            clickedFriendlyBldgId = b.id;
                        }
                    }
                }

                if (clickedFriendlyRegId != -1) {
                    if (!shiftAdd) {
                        for (auto& r : engine.getRegimentsMut()) r.selected = false;
                        for (auto& b : engine.getBuildingsMut()) b.selected = false;
                    }
                    for (auto& r : engine.getRegimentsMut()) {
                        if (r.id == clickedFriendlyRegId) r.selected = true;
                    }
                    sound.playSfx(SfxId::RADIO_SQUELCH, 0.55f);
                } else if (clickedFriendlyBldgId != -1) {
                    for (auto& r : engine.getRegimentsMut()) r.selected = false;
                    for (auto& b : engine.getBuildingsMut()) {
                        b.selected = (b.id == clickedFriendlyBldgId);
                    }
                    sound.playSfx(SfxId::UI_CLICK, 0.55f);
                } else {
                    // Clicked on empty terrain or enemy: Left-click is ONLY for selecting, so clicking empty ground deselects!
                    if (!shiftAdd) {
                        for (auto& r : engine.getRegimentsMut()) r.selected = false;
                        for (auto& b : engine.getBuildingsMut()) b.selected = false;
                    }
                }
            } else {
                // 2D Screen-Space Box Selection: selects every friendly unit visually inside the green rectangle!
                Rectangle selBox = {
                    std::min(dragStartScreen_.x, dragCurrentScreen_.x),
                    std::min(dragStartScreen_.y, dragCurrentScreen_.y),
                    std::fabs(dragCurrentScreen_.x - dragStartScreen_.x),
                    std::fabs(dragCurrentScreen_.y - dragStartScreen_.y)
                };
                if (!shiftAdd) {
                    for (auto& r : engine.getRegimentsMut()) r.selected = false;
                    for (auto& b : engine.getBuildingsMut()) b.selected = false;
                }
                int selectedInBox = 0;
                for (auto& r : engine.getRegimentsMut()) {
                    if (r.teamId != 0 || r.health <= 0.0f) continue;
                    Vector2 sp = GetWorldToScreen({r.pos.x, r.pos.y + 1.5f, r.pos.z}, camera_);
                    if (CheckCollisionPointRec(sp, selBox)) {
                        r.selected = true;
                        ++selectedInBox;
                    }
                }
                // If no soldiers were inside the box, check if a friendly building was inside the box
                if (selectedInBox == 0) {
                    for (auto& b : engine.getBuildingsMut()) {
                        if (b.teamId != 0 || b.health <= 0.0f) continue;
                        Vector2 sp = GetWorldToScreen({b.pos.x, b.pos.y + 2.5f, b.pos.z}, camera_);
                        if (CheckCollisionPointRec(sp, selBox)) {
                            b.selected = true;
                            break;
                        }
                    }
                }
                if (selectedInBox > 0) {
                    sound.playSfx(SfxId::RADIO_SQUELCH, 0.58f);
                } else {
                    sound.playSfx(SfxId::UI_CLICK, 0.65f);
                }
            }
        }
    }

    // 10. RIGHT-CLICK TO TARGET, MOVE, ATTACK, OR SET RALLY POINT:
    // Left-click selects army; Right-click targets terrain/enemies!
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        bool hasSelectedUnits = false;
        for (const auto& r : engine.getRegiments()) {
            if (r.teamId == 0 && r.selected && r.health > 0.0f) {
                hasSelectedUnits = true;
                break;
            }
        }
        const RTSBuilding* selBldg = nullptr;
        for (const auto& b : engine.getBuildings()) {
            if (b.teamId == 0 && b.selected && b.health > 0.0f) {
                selBldg = &b;
                break;
            }
        }

        if (hasSelectedUnits || selBldg) {
            bool isTargetingEnemy = false;
            for (const auto& er : engine.getRegiments()) {
                if (engine.isEnemy(0, er.teamId) && er.health > 0.0f && engine.isPosVisibleToTeam(er.pos, 0)) {
                    if (Vector2Distance({er.pos.x, er.pos.z}, {groundHit.x, groundHit.z}) < 18.0f) {
                        isTargetingEnemy = true;
                        break;
                    }
                }
            }
            if (!isTargetingEnemy) {
                for (const auto& eb : engine.getBuildings()) {
                    if (engine.isEnemy(0, eb.teamId) && eb.health > 0.0f && engine.isPosVisibleToTeam(eb.pos, 0)) {
                        if (Vector2Distance({eb.pos.x, eb.pos.z}, {groundHit.x, groundHit.z}) < 24.0f) {
                            isTargetingEnemy = true;
                            break;
                        }
                    }
                }
            }

            engine.issueRightClickOrder(groundHit);
            moveOrderIndicatorPos_      = groundHit;
            moveOrderIndicatorTimer_    = 1.25f;
            moveOrderIndicatorIsAttack_ = isTargetingEnemy;
        }
    }
}

// ============================================================================
// AAA 3D OpenGL 3.30 Battlefield Rendering
// ============================================================================

void RTSRenderer::drawAtmosphericSkyBackdrop(BiomeType biome) const {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Color topSky = {14, 26, 46, 255};
    Color midSky = {46, 74, 108, 255};
    Color horSky = {136, 146, 154, 255};
    if (biome == BiomeType::EL_ALAMEIN_DESERT) {
        topSky = {24, 42, 70, 255};
        midSky = {68, 92, 118, 255};
        horSky = {165, 146, 118, 255};
    } else if (biome == BiomeType::STALINGRAD_WINTER || biome == BiomeType::ARDENNES_WINTER) {
        topSky = {18, 28, 44, 255};
        midSky = {56, 72, 92, 255};
        horSky = {125, 138, 148, 255};
    }
    // Atmospheric 3-stop vertical Rayleigh scattering sky gradient (Rich, deep tactical palette)
    DrawRectangleGradientV(0, 0, sw, sh / 2, topSky, midSky);
    DrawRectangleGradientV(0, sh / 2, sw, sh - sh / 2, midSky, horSky);

    // Natural Subtle Sun Glow (Muted, non-blinding directional sun without harsh white screen discs)
    Vector3 sunDir = Vector3Normalize({0.55f, 0.80f, 0.34f});
    Vector3 camForward = Vector3Normalize(Vector3Subtract(cameraTarget_, camera_.position));
    if (Vector3DotProduct(camForward, sunDir) > 0.10f) {
        Vector3 sunWorld = Vector3Add(camera_.position, Vector3Scale(sunDir, 500.0f));
        Vector2 sunScreen = GetWorldToScreen(sunWorld, camera_);
        if (sunScreen.x >= -60.0f && sunScreen.x <= sw + 60.0f &&
            sunScreen.y >= -60.0f && sunScreen.y <= sh + 60.0f) {
            DrawCircleV(sunScreen, 55.0f, ColorAlpha(GOLD, 0.06f));
            DrawCircleV(sunScreen, 24.0f, ColorAlpha(YELLOW, 0.12f));
            DrawCircleV(sunScreen, 10.0f, ColorAlpha(Color{255, 245, 215, 255}, 0.35f));
        }
    }
}

void RTSRenderer::render3DBattlefield(const RTSEngine& engine) {
    ensureShadersAndMeshesLoaded();
    updateFogTexture(engine);
    updateFrustum();

    telemetry_.fps = static_cast<float>(GetFPS());
    telemetry_.frameTimeMs = GetFrameTime() * 1000.0f;
    telemetry_.renderedEntities = 0;
    telemetry_.culledEntities = 0;
    telemetry_.drawCalls = 0;

    drawAtmosphericSkyBackdrop(engine.getConfig().biome);

    BeginMode3D(camera_);

    drawTerrainAndWater(engine);
    drawSupplyNetwork(engine);

    if (unitShader_.id != 0) {
        int biomeId = static_cast<int>(engine.getConfig().biome);
        SetShaderValue(unitShader_, locUnitCamPos_, &camera_.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(unitShader_, locUnitBiome_,  &biomeId,          SHADER_UNIFORM_INT);
        SetShaderValue(unitShader_, locUnitTime_,   &waterAnimTime_,   SHADER_UNIFORM_FLOAT);
        BeginShaderMode(unitShader_);
    }

    float maxDist = (cameraDistance_ * 1.8f + 85.0f);
    if (quality_ == GraphicQuality::LOW) maxDist *= 0.72f;
    float maxPropDistSq = maxDist * maxDist;

    for (const auto& prop : engine.getProps()) {
        float dx = prop.pos.x - cameraTarget_.x;
        float dz = prop.pos.z - cameraTarget_.z;
        if (dx * dx + dz * dz > maxPropDistSq) {
            telemetry_.culledEntities++;
            continue;
        }
        if (!frustum_.containsSphere(prop.pos, 4.5f * prop.scale)) {
            telemetry_.culledEntities++;
            continue;
        }
        if (!engine.isPosExploredByPlayer(prop.pos)) {
            telemetry_.culledEntities++;
            continue;
        }
        telemetry_.renderedEntities++;
        switch (prop.propType) {
            case 0:
            case 1:
            case 2:
                models::drawDetailedTree3D(prop.pos, prop.scale, engine.getConfig().biome, waterAnimTime_);
                break;
            case 3:
                models::drawRockFormation3D(prop.pos, prop.scale, prop.rotation);
                break;
            case 4:
                models::drawRuinedStoneWall3D(prop.pos, prop.scale, prop.rotation);
                break;
            case 5:
                models::drawBocageHedge3D(prop.pos, prop.scale, prop.rotation);
                break;
            case 6:
                models::drawTelephonePole3D(prop.pos, prop.scale);
                break;
            case 7:
                models::drawSandbagCheckpoint3D(prop.pos, prop.scale);
                break;
            case 8:
                models::drawCzechHedgehog3D(prop.pos, prop.scale);
                break;
            default:
                models::drawRockFormation3D(prop.pos, prop.scale, prop.rotation);
                break;
        }
    }

    for (const auto& b : engine.getBuildings()) {
        if (!frustum_.containsSphere(b.pos, 16.0f)) {
            telemetry_.culledEntities++;
            continue;
        }
        telemetry_.renderedEntities++;
        drawBuilding3D(b, engine);
    }

    for (const auto& r : engine.getRegiments()) {
        if (!frustum_.containsSphere(r.pos, 10.0f)) {
            telemetry_.culledEntities++;
            continue;
        }
        telemetry_.renderedEntities++;
        drawRegiment3D(r, engine);
    }

    // Smoldering, scorched vehicle hulks with ember glow
    for (const auto& w : engine.getWrecks()) {
        if (!frustum_.containsSphere(w.pos, 8.0f)) continue;
        float flicker = std::sin(waterAnimTime_ * 7.5f + w.pos.x) * 0.25f + 0.75f;
        DrawCircle3D({w.pos.x, w.pos.y + 0.05f, w.pos.z}, 3.8f, {1, 0, 0}, 90.0f, ColorAlpha(Color{18, 16, 14, 255}, 0.65f));
        DrawCube({w.pos.x, w.pos.y + 0.65f, w.pos.z}, 2.4f, 0.82f, 4.2f, {32, 34, 36, 255});
        DrawCubeWires({w.pos.x, w.pos.y + 0.65f, w.pos.z}, 2.42f, 0.85f, 4.22f, {18, 20, 22, 255});
        DrawCube({w.pos.x + 0.25f, w.pos.y + 1.30f, w.pos.z - 0.2f}, 1.7f, 0.62f, 1.9f, {26, 28, 30, 255});
        DrawSphere({w.pos.x, w.pos.y + 1.05f, w.pos.z}, 0.38f, ColorAlpha(Color{255, 115, 20, 255}, 0.75f * flicker));
    }

    drawAirStrikes3D(engine);

    if (unitShader_.id != 0) {
        EndShaderMode();
    }

    drawProjectilesAndParticles3D(engine);
    drawWeatherEffects3D(engine);

    if (engine.pendingBuildType() >= 0) {
        Vector3 hit = screenToGround(GetMousePosition(), engine);
        const auto& bDef = GameData::instance().getBuilding(static_cast<BuildingType>(engine.pendingBuildType()));
        if (bDef.supplyRadius > 1.0f) {
            DrawCircle3D({hit.x, hit.y + 0.35f, hit.z}, bDef.supplyRadius, {1, 0, 0}, 90.0f, {80, 235, 110, 175});
        }
        DrawCube({hit.x, hit.y + 2.5f, hit.z}, 8.5f, 5.0f, 8.5f, {80, 235, 100, 115});
        DrawCubeWires({hit.x, hit.y + 2.5f, hit.z}, 8.8f, 5.2f, 8.8f, LIME);
        DrawCircle3D({hit.x, hit.y + 0.38f, hit.z}, 7.0f, {1, 0, 0}, 90.0f, GOLD);
    } else if (engine.pendingSpecOpIdx() >= 0) {
        Vector3 hit = screenToGround(GetMousePosition(), engine);
        DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 24.0f, {1, 0, 0}, 90.0f, RED);
        DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 12.0f, {1, 0, 0}, 90.0f, GOLD);
    } else if (engine.pendingAirMissionType() >= 0) {
        Vector3 hit = screenToGround(GetMousePosition(), engine);
        int mission = engine.pendingAirMissionType();
        if (mission == 1) {
            // Recon Flight Vector & 85m Vision Reveal Swath
            DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 85.0f, {1, 0, 0}, 90.0f, {255, 225, 90, 95});
            DrawCircle3D({hit.x, hit.y + 0.45f, hit.z}, 14.0f, {1, 0, 0}, 90.0f, GOLD);
            DrawCircle3D({hit.x, hit.y + 0.45f, hit.z}, 4.0f, {1, 0, 0}, 90.0f, RAYWHITE);
        } else if (mission == 2) {
            // Tactical Bombing Run Blast Radius
            DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 28.0f, {1, 0, 0}, 90.0f, {230, 45, 45, 125});
            DrawCircle3D({hit.x, hit.y + 0.45f, hit.z}, 10.0f, {1, 0, 0}, 90.0f, RED);
            DrawLine3D({hit.x - 30.0f, hit.y + 0.45f, hit.z}, {hit.x + 30.0f, hit.y + 0.45f, hit.z}, RED);
            DrawLine3D({hit.x, hit.y + 0.45f, hit.z - 30.0f}, {hit.x, hit.y + 0.45f, hit.z + 30.0f}, RED);
        } else if (mission == 5) {
            // Combat Air Patrol / Fighter Sweep Vector (160m air acquisition radius)
            DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 160.0f, {1, 0, 0}, 90.0f, {65, 185, 255, 60});
            DrawCircle3D({hit.x, hit.y + 0.45f, hit.z}, 30.0f, {1, 0, 0}, 90.0f, {255, 205, 60, 160});
            DrawLine3D({hit.x - 35.0f, hit.y + 0.45f, hit.z}, {hit.x + 35.0f, hit.y + 0.45f, hit.z}, SKYBLUE);
            DrawLine3D({hit.x, hit.y + 0.45f, hit.z - 35.0f}, {hit.x, hit.y + 0.45f, hit.z + 35.0f}, SKYBLUE);
        } else {
            DrawCircle3D({hit.x, hit.y + 0.4f, hit.z}, 22.0f, {1, 0, 0}, 90.0f, SKYBLUE);
        }
    }

    // 3D Assembly Points (Rally Points) for Selected Production HQs / Airfield
    // Rendered with depth test disabled so assembly points shine cleanly through any dark fog or shroud!
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f || b.teamId != 0 || !b.selected) continue;
        if (b.type != BuildingType::CORPS_HQ && b.type != BuildingType::INFANTRY_HQ &&
            b.type != BuildingType::MECH_HQ && b.type != BuildingType::ARMOR_HQ &&
            b.type != BuildingType::AIRFIELD) continue;

        float rh = engine.getTerrainHeight(b.rallyPoint.x, b.rallyPoint.z);
        Vector3 rp = {b.rallyPoint.x, rh, b.rallyPoint.z};

        rlDisableDepthTest();

        // 1. Double Line from HQ to Assembly Point
        DrawLine3D({b.pos.x, b.pos.y + 1.2f, b.pos.z}, {rp.x, rp.y + 1.2f, rp.z}, Color{120, 225, 255, 230});
        DrawLine3D({b.pos.x, b.pos.y + 1.5f, b.pos.z}, {rp.x, rp.y + 1.5f, rp.z}, Color{255, 220, 60, 210});

        // 2. 18-Meter Vertical Assembly Beacon Column
        DrawCylinder({rp.x, rp.y + 9.0f, rp.z}, 0.35f, 0.35f, 18.0f, 8, Color{100, 225, 255, 245});
        DrawCylinder({rp.x, rp.y + 9.0f, rp.z}, 0.75f, 0.75f, 18.0f, 8, Color{255, 220, 80, 110});

        // 3. Glowing Beacon Diamond / Flag Marker at Top (y + 18m)
        float pulse = std::sin(waterAnimTime_ * 4.5f) * 0.35f;
        DrawSphere({rp.x, rp.y + 18.0f, rp.z}, 1.4f + pulse, Color{255, 220, 50, 255});
        DrawSphereWires({rp.x, rp.y + 18.0f, rp.z}, 1.8f + pulse, 6, 6, Color{255, 255, 255, 240});

        // 4. Concentric Assembly Rings at Ground
        DrawCircle3D({rp.x, rp.y + 0.4f, rp.z}, 4.5f + pulse, {1.0f, 0.0f, 0.0f}, 90.0f, Color{100, 225, 255, 240});
        DrawCircle3D({rp.x, rp.y + 0.45f, rp.z}, 2.6f, {1.0f, 0.0f, 0.0f}, 90.0f, Color{255, 220, 60, 255});
        DrawCircle3D({rp.x, rp.y + 0.5f, rp.z}, 1.0f, {1.0f, 0.0f, 0.0f}, 90.0f, WHITE);

        rlEnableDepthTest();
    }

    // AAA Tactical Move Paths & Animated Chevrons for Selected Player Regiments
    for (const auto& r : engine.getRegiments()) {
        if (!r.selected || r.teamId != 0 || r.health <= 0.0f) continue;
        float dx = r.pos.x - r.targetPos.x;
        float dz = r.pos.z - r.targetPos.z;
        float dDest = std::sqrt(dx * dx + dz * dz);
        if (dDest < 2.2f) continue;

        bool isAttackMove = r.attackMove;
        Color pathCol = isAttackMove ? Color{255, 65, 50, 220} : Color{65, 230, 110, 220};
        Color chevronCol = isAttackMove ? Color{255, 210, 60, 245} : Color{180, 255, 190, 245};

        // Draw segmented line conforming to terrain height
        int steps = std::clamp(static_cast<int>(dDest / 4.0f), 4, 32);
        Vector3 prevPt = {r.pos.x, engine.getTerrainHeight(r.pos.x, r.pos.z) + 0.22f, r.pos.z};

        for (int s = 1; s <= steps; ++s) {
            float t = static_cast<float>(s) / steps;
            float wx = std::lerp(r.pos.x, r.targetPos.x, t);
            float wz = std::lerp(r.pos.z, r.targetPos.z, t);
            float wy = engine.getTerrainHeight(wx, wz) + 0.22f;
            Vector3 currPt = {wx, wy, wz};

            if (s % 2 == 1) {
                DrawLine3D(prevPt, currPt, pathCol);
            }
            prevPt = currPt;
        }

        // Draw animated marching chevrons (>>>) along path
        float pathAnim = std::fmod(waterAnimTime_ * 1.8f, 1.0f);
        int chevronCount = std::clamp(static_cast<int>(dDest / 7.5f), 1, 6);
        Vector3 forwardDir = Vector3Normalize({r.targetPos.x - r.pos.x, 0.0f, r.targetPos.z - r.pos.z});
        Vector3 rightDir = { -forwardDir.z, 0.0f, forwardDir.x };

        for (int c = 0; c < chevronCount; ++c) {
            float cT = std::fmod(pathAnim + static_cast<float>(c) / chevronCount, 1.0f);
            if (cT < 0.05f || cT > 0.95f) continue;
            float cx = std::lerp(r.pos.x, r.targetPos.x, cT);
            float cz = std::lerp(r.pos.z, r.targetPos.z, cT);
            float cy = engine.getTerrainHeight(cx, cz) + 0.26f;

            // V-shaped tactical chevron pointing towards destination
            Vector3 tip = {cx + forwardDir.x * 0.9f, cy, cz + forwardDir.z * 0.9f};
            Vector3 leftWing = {cx - forwardDir.x * 0.6f + rightDir.x * 0.7f, cy, cz - forwardDir.z * 0.6f + rightDir.z * 0.7f};
            Vector3 rightWing = {cx - forwardDir.x * 0.6f - rightDir.x * 0.7f, cy, cz - forwardDir.z * 0.6f - rightDir.z * 0.7f};
            DrawLine3D(tip, leftWing, chevronCol);
            DrawLine3D(tip, rightWing, chevronCol);
        }

        // Destination Disc with concentric pulse
        float th = engine.getTerrainHeight(r.targetPos.x, r.targetPos.z) + 0.20f;
        DrawCircle3D({r.targetPos.x, th, r.targetPos.z}, 1.8f, {1, 0, 0}, 90.0f, ColorAlpha(pathCol, 0.35f));
        DrawCircle3D({r.targetPos.x, th + 0.02f, r.targetPos.z}, 0.8f, {1, 0, 0}, 90.0f, pathCol);
    }

    if (moveOrderIndicatorTimer_ > 0.0f) {
        float pulse = (1.25f - moveOrderIndicatorTimer_) * 6.5f;
        rlDisableDepthTest();
        if (moveOrderIndicatorIsAttack_) {
            // Tactical Red Pulsing Crosshairs & Target Brackets & Vertical Column
            DrawCircle3D({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.35f, moveOrderIndicatorPos_.z},
                         2.6f + pulse, {1, 0, 0}, 90.0f, RED);
            DrawCircle3D({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.38f, moveOrderIndicatorPos_.z},
                         1.4f, {1, 0, 0}, 90.0f, ORANGE);
            DrawLine3D({moveOrderIndicatorPos_.x - 3.8f, moveOrderIndicatorPos_.y + 0.4f, moveOrderIndicatorPos_.z},
                       {moveOrderIndicatorPos_.x + 3.8f, moveOrderIndicatorPos_.y + 0.4f, moveOrderIndicatorPos_.z}, RED);
            DrawLine3D({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.4f, moveOrderIndicatorPos_.z - 3.8f},
                       {moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.4f, moveOrderIndicatorPos_.z + 3.8f}, RED);
            DrawCylinder({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 7.0f, moveOrderIndicatorPos_.z},
                         0.25f, 0.25f, 14.0f, 6, RED);
            DrawSphere({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 14.0f, moveOrderIndicatorPos_.z}, 1.0f, ORANGE);
        } else {
            // Tactical Green Concentric Rally Rings & Vertical Waypoint Column
            DrawCircle3D({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.35f, moveOrderIndicatorPos_.z},
                         2.5f + pulse, {1, 0, 0}, 90.0f, LIME);
            DrawCircle3D({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 0.38f, moveOrderIndicatorPos_.z},
                         1.6f, {1, 0, 0}, 90.0f, GOLD);
            DrawCylinder({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 7.0f, moveOrderIndicatorPos_.z},
                         0.25f, 0.25f, 14.0f, 6, Color{100, 255, 140, 240});
            DrawSphere({moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.y + 14.0f, moveOrderIndicatorPos_.z}, 1.0f, GOLD);
        }
        rlEnableDepthTest();
    }

    EndMode3D();

    if (!engine.isRevealAllShowcase()) {
        drawUnitWorldBarsAndLabels(engine);
    }

    if (isDraggingSelect_) {
        float x = std::min(dragStartScreen_.x, dragCurrentScreen_.x);
        float y = std::min(dragStartScreen_.y, dragCurrentScreen_.y);
        float w = std::fabs(dragCurrentScreen_.x - dragStartScreen_.x);
        float h = std::fabs(dragCurrentScreen_.y - dragStartScreen_.y);
        DrawRectangleRec({x, y, w, h}, {95, 225, 115, 45});
        DrawRectangleLinesEx({x, y, w, h}, 2.0f, LIME);
    }
}

void RTSRenderer::drawBridge3D(Vector3 center, float width, float length) const {
    Color stoneCol = {132, 128, 120, 255};
    Color darkStone = {96, 92, 86, 255};
    Color deckCol  = {84, 80, 74, 255};
    Color curbCol  = {112, 108, 102, 255};
    Color railCol  = {72, 74, 76, 255};

    // Main stone deck & under-arch beam
    DrawCube({center.x, center.y + 0.38f, center.z}, width, 0.72f, length, deckCol);
    DrawCube({center.x, center.y - 0.15f, center.z}, width * 0.92f, 0.55f, length * 0.96f, darkStone);

    // Raised pedestrian sidewalks on left & right edges
    DrawCube({center.x - width * 0.41f, center.y + 0.82f, center.z}, width * 0.14f, 0.24f, length, curbCol);
    DrawCube({center.x + width * 0.41f, center.y + 0.82f, center.z}, width * 0.14f, 0.24f, length, curbCol);

    // Dashed centerline cobble dividers
    for (int s = -3; s <= 3; ++s) {
        DrawCube({center.x, center.y + 0.76f, center.z + s * (length * 0.13f)}, 0.42f, 0.06f, 2.2f, {148, 142, 128, 255});
    }

    // Heavy masonry bridge piers standing in the river water + stone parapet pillars + iron lamps
    for (int i = -2; i <= 2; ++i) {
        float pz = center.z + i * (length * 0.22f);
        // Stone parapet pillars
        DrawCube({center.x - width * 0.47f, center.y + 1.45f, pz}, 1.05f, 1.55f, 1.45f, stoneCol);
        DrawCube({center.x + width * 0.47f, center.y + 1.45f, pz}, 1.05f, 1.55f, 1.45f, stoneCol);
        DrawCube({center.x - width * 0.47f, center.y + 2.30f, pz}, 1.20f, 0.22f, 1.60f, curbCol);
        DrawCube({center.x + width * 0.47f, center.y + 2.30f, pz}, 1.20f, 0.22f, 1.60f, curbCol);

        // Submerged stone cutwater piers in river channel
        if (std::abs(i) <= 1) {
            DrawCylinder({center.x - width * 0.38f, center.y - 2.4f, pz}, 1.25f, 1.45f, 2.8f, 8, darkStone);
            DrawCylinder({center.x + width * 0.38f, center.y - 2.4f, pz}, 1.25f, 1.45f, 2.8f, 8, darkStone);
        } else {
            // Bridgehead bastions on north & south banks
            DrawCube({center.x - width * 0.52f, center.y + 1.1f, pz}, 1.8f, 2.2f, 2.2f, stoneCol);
            DrawCube({center.x + width * 0.52f, center.y + 1.1f, pz}, 1.8f, 2.2f, 2.2f, stoneCol);
        }
    }

    // Continuous stone parapet walls along left & right sides
    DrawCube({center.x - width * 0.47f, center.y + 1.28f, center.z}, 0.65f, 0.92f, length, railCol);
    DrawCube({center.x + width * 0.47f, center.y + 1.28f, center.z}, 0.65f, 0.92f, length, railCol);
}

void RTSRenderer::drawTerrainAndWater(const RTSEngine& engine) {
    const auto& ui = UITheme::instance();
    BiomeType biome = engine.getConfig().biome;
    int biomeId = static_cast<int>(biome);

    const Texture2D* groundTex = &ui.getGrassTex();
    if (biome == BiomeType::EL_ALAMEIN_DESERT) groundTex = &ui.getSandTex();
    else if (biome == BiomeType::STALINGRAD_WINTER || biome == BiomeType::ARDENNES_WINTER) groundTex = &ui.getSnowTex();

    if (terrainModel_.meshCount > 0) {
        terrainModel_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *groundTex;
        if (fogTexture_.id != 0) {
            terrainModel_.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture = fogTexture_;
        }
        if (terrainShader_.id != 0) {
            SetShaderValue(terrainShader_, locTerrainCamPos_, &camera_.position, SHADER_UNIFORM_VEC3);
            SetShaderValue(terrainShader_, locTerrainBiome_,  &biomeId, SHADER_UNIFORM_INT);
            SetShaderValue(terrainShader_, locTerrainTime_,   &waterAnimTime_, SHADER_UNIFORM_FLOAT);
            int useFog = 1;
            SetShaderValue(terrainShader_, locTerrainUseFog_, &useFog, SHADER_UNIFORM_INT);
        }
        DrawModel(terrainModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    }

    if (engine.hasRiver() && waterModel_.meshCount > 0) {
        if (fogTexture_.id != 0) {
            waterModel_.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture = fogTexture_;
        }
        if (waterShader_.id != 0) {
            SetShaderValue(waterShader_, locWaterCamPos_, &camera_.position, SHADER_UNIFORM_VEC3);
            SetShaderValue(waterShader_, locWaterTime_,   &waterAnimTime_, SHADER_UNIFORM_FLOAT);
            int useFog = 1;
            SetShaderValue(waterShader_, locWaterUseFog_, &useFog, SHADER_UNIFORM_INT);
        }
        DrawModel(waterModel_, {0.0f, -0.06f, 0.0f}, 1.0f, WHITE);
    }

    if (engine.hasRiver()) {
        if (unitShader_.id != 0) {
            int biomeId = static_cast<int>(engine.getConfig().biome);
            SetShaderValue(unitShader_, locUnitCamPos_, &camera_.position, SHADER_UNIFORM_VEC3);
            SetShaderValue(unitShader_, locUnitBiome_,  &biomeId,          SHADER_UNIFORM_INT);
            SetShaderValue(unitShader_, locUnitTime_,   &waterAnimTime_,   SHADER_UNIFORM_FLOAT);
            BeginShaderMode(unitShader_);
        }
        drawBridge3D({   0.0f, 0.0f, 0.0f}, 19.0f, 36.0f);
        drawBridge3D({-240.0f, 0.0f, 0.0f}, 16.0f, 36.0f);
        drawBridge3D({ 240.0f, 0.0f, 0.0f}, 16.0f, 36.0f);
        if (unitShader_.id != 0) {
            EndShaderMode();
        }
    }
}

void RTSRenderer::updateFogTexture(const RTSEngine& engine) {
    if (fogTexture_.id == 0) return;

    alignas(32) static Color pixels[RTSEngine::kFogGridSize * RTSEngine::kFogGridSize];
    bool revealAll = engine.isRevealAllShowcase() || engine.getPlayer(0).radarRevealTimer > 0.0f;

    for (int gz = 0; gz < RTSEngine::kFogGridSize; ++gz) {
        for (int gx = 0; gx < RTSEngine::kFogGridSize; ++gx) {
            int idx = gz * RTSEngine::kFogGridSize + gx;
            if (revealAll) {
                pixels[idx] = Color{255, 255, 255, 255};
            } else {
                uint8_t state = engine.getFogCellState(gx, gz);
                // 0: Unexplored shroud (0), 1: Explored twilight (90), 2: Full live sunlight (255)
                uint8_t val = (state == 2) ? 255 : (state == 1 ? 90 : 0);
                pixels[idx] = Color{val, val, val, 255};
            }
        }
    }
    UpdateTexture(fogTexture_, pixels);
}

void RTSRenderer::drawSupplyNetwork(const RTSEngine& engine) {
    // 1. 28.0m Strategic Capture Zone Rings around Towns and Oil Derricks
    // Only display capture zone when building is actively contested by opposing troops
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f) continue;
        if (b.type != BuildingType::CITY_DEPOT && b.type != BuildingType::OIL_DERRICK) continue;
        if (b.teamId != 0 && !engine.isPosVisibleToTeam(b.pos, 0) && !engine.isPosExploredByPlayer(b.pos)) continue;

        bool isContested = (b.captureProgress != 0.0f && std::fabs(b.captureProgress) < 98.0f);
        if (!isContested) continue; // Only draw contested capture ring when actively contested by troops

        Color ringCol = (b.teamId == 0) ? Color{80, 225, 120, 140} :
                        (engine.isAllied(0, b.teamId) ? Color{80, 200, 255, 140} :
                        (b.teamId == -1 ? Color{245, 215, 80, 150} : Color{235, 75, 75, 130}));
        DrawCircle3D({b.pos.x, b.pos.y + 0.30f, b.pos.z}, 28.0f, {1.0f, 0.0f, 0.0f}, 90.0f, ringCol);
    }

    if (!engine.showSupplyOverlay()) return;

    // 3. Supply Radius Rings (Green) & Field Hospital Rings when [Tab] Overlay is explicitly enabled
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f || b.teamId < 0 || b.isUnderConstruction) continue;
        if (b.teamId != 0 && !engine.isAllied(0, b.teamId) && !engine.isPosVisibleToTeam(b.pos, 0)) continue;

        if (b.supplyRadius > 1.0f) {
            Color ringCol = (b.teamId == 0) ? Color{85, 225, 115, 78} :
                            (engine.isAllied(0, b.teamId) ? Color{80, 180, 245, 78} : Color{235, 85, 75, 65});
            DrawCircle3D({b.pos.x, b.pos.y + 0.35f, b.pos.z}, b.supplyRadius, {1.0f, 0.0f, 0.0f}, 90.0f, ringCol);
        }
        if (b.type == BuildingType::CITY_DEPOT && b.teamId >= 0 && b.inSupply) {
            // Field Hospital Healing Station Radius Ring
            Color medCol = (b.teamId == 0) ? Color{80, 245, 130, 85} :
                           (engine.isAllied(0, b.teamId) ? Color{80, 200, 250, 85} : Color{240, 85, 75, 70});
            DrawCircle3D({b.pos.x, b.pos.y + 0.40f, b.pos.z}, 62.0f, {1.0f, 0.0f, 0.0f}, 90.0f, medCol);
        }
    }
}

void RTSRenderer::drawBuilding3D(const RTSBuilding& b, const RTSEngine& engine) {
    if (b.health <= 0.0f) return;
    // Conceal Enemy Buildings & Unexplored Neutral Towns until scouted by Player Vision or Recon!
    if (b.teamId != 0 && !engine.isAllied(0, b.teamId) && !engine.isPosVisibleToTeam(b.pos, 0) && !engine.isPosExploredByPlayer(b.pos)) {
        return;
    }

    const auto& gd = GameData::instance();
    const auto& nat = (b.teamId >= 0) ? gd.getNation(b.nation) : gd.getNation(NationId::USA);
    const auto& bDef = gd.getBuilding(b.type);

    if (b.isUnderConstruction) {
        models::drawConstructionSite3D(b, nat, bDef, waterAnimTime_);
    } else {
        models::drawDetailedBuilding3D(b, nat, bDef, waterAnimTime_);
    }

    // Atmospheric Smoke Rising from Bomb-Damaged Structure
    if (b.isBombDamaged) {
        float smokePhase = fmodf(waterAnimTime_ * 2.5f, 1.0f);
        for (int s = 0; s < 3; ++s) {
            float frac = fmodf(smokePhase + s * 0.33f, 1.0f);
            Vector3 sPos = { b.pos.x + (s - 1) * 2.2f, b.pos.y + 5.5f + frac * 8.0f, b.pos.z };
            DrawSphere(sPos, 1.3f + frac * 1.6f, ColorAlpha(DARKGRAY, 0.45f * (1.0f - frac)));
        }
    }

    if (b.selected) {
        DrawCircle3D({b.pos.x, b.pos.y + 0.32f, b.pos.z}, 9.2f, {1.0f, 0.0f, 0.0f}, 90.0f, ColorAlpha(GOLD, 0.55f));
    }
}

void RTSRenderer::drawRegiment3D(const RTSRegiment& r, const RTSEngine& engine) {
    if (r.health <= 0.0f) return;
    // Strict Fog of War: Hostile Regiments are COMPLETELY INVISIBLE unless inside live Player Vision / Recon!
    if (r.teamId != 0 && !engine.isAllied(0, r.teamId) && !engine.isPosVisibleToTeam(r.pos, 0)) {
        return;
    }

    const auto& gd = GameData::instance();
    const auto& nat = gd.getNation(r.nation);
    const auto& rDef = gd.getRegiment(r.type);

    if (r.selected) {
        DrawCircle3D({r.pos.x, r.pos.y + 0.22f, r.pos.z}, 4.8f, {1.0f, 0.0f, 0.0f}, 90.0f, ColorAlpha(LIME, 0.55f));

        // Explicit Target Line & Reticle
        if (r.targetRepairBuildingId != -1) {
            for (const auto& b : engine.getBuildings()) {
                if (b.id == r.targetRepairBuildingId && b.health > 0.0f) {
                    DrawLine3D({r.pos.x, r.pos.y + 1.2f, r.pos.z}, {b.pos.x, b.pos.y + 1.8f, b.pos.z}, ColorAlpha(SKYBLUE, 0.85f));
                    DrawCircle3D({b.pos.x, b.pos.y + 0.45f, b.pos.z}, 9.5f, {1, 0, 0}, 90.0f, ColorAlpha(SKYBLUE, 0.65f));
                    break;
                }
            }
        } else if (r.targetRegimentId != -1) {
            for (const auto& other : engine.getRegiments()) {
                if (other.id == r.targetRegimentId && other.health > 0.0f) {
                    DrawLine3D({r.pos.x, r.pos.y + 1.2f, r.pos.z}, {other.pos.x, other.pos.y + 1.2f, other.pos.z}, ColorAlpha(RED, 0.85f));
                    DrawCircle3D({other.pos.x, other.pos.y + 0.38f, other.pos.z}, 5.2f, {1, 0, 0}, 90.0f, ColorAlpha(RED, 0.65f));
                    break;
                }
            }
        } else if (r.targetBuildingId != -1) {
            for (const auto& eb : engine.getBuildings()) {
                if (eb.id == r.targetBuildingId && eb.health > 0.0f) {
                    DrawLine3D({r.pos.x, r.pos.y + 1.2f, r.pos.z}, {eb.pos.x, eb.pos.y + 2.5f, eb.pos.z}, ColorAlpha(RED, 0.85f));
                    DrawCircle3D({eb.pos.x, eb.pos.y + 0.45f, eb.pos.z}, 9.5f, {1, 0, 0}, 90.0f, ColorAlpha(RED, 0.65f));
                    break;
                }
            }
        } else if (Vector3Distance(r.pos, r.targetPos) > 2.5f) {
            Color lineCol = r.attackMove ? RED : SKYBLUE;
            DrawLine3D({r.pos.x, r.pos.y + 0.45f, r.pos.z}, {r.targetPos.x, r.targetPos.y + 0.45f, r.targetPos.z}, lineCol);
        }
    }

    // 3D Entrenchment Sandbag Revetments (+35% Defense Fortification Visual)
    if (r.stance == FormationStance::ENTRENCHED) {
        Color sandbagCol = {186, 168, 128, 255};
        Color sandbagDark = {142, 126, 92, 255};
        float rad = r.facing * DEG2RAD;
        for (int a = -3; a <= 3; ++a) {
            float ang = rad + a * 0.32f;
            float dist = 4.4f;
            Vector3 sPos = {r.pos.x + std::sin(ang) * dist, r.pos.y + 0.35f, r.pos.z + std::cos(ang) * dist};
            DrawCube(sPos, 1.25f, 0.65f, 0.75f, sandbagCol);
            DrawCubeWires(sPos, 1.28f, 0.68f, 0.78f, sandbagDark);
        }
    }

    if (r.atTownHealingStation && r.health < r.maxHealth) {
        float pulse = std::sin(waterAnimTime_ * 5.0f) * 0.25f;
        Vector3 mPos = {r.pos.x, r.pos.y + 4.2f + pulse, r.pos.z};
        DrawCube(mPos, 0.28f, 1.20f, 0.28f, {85, 255, 120, 230});
        DrawCube(mPos, 1.20f, 0.28f, 0.28f, {85, 255, 120, 230});
    }

    models::drawDetailedRegiment3D(r, nat, rDef, waterAnimTime_);
    (void)engine;
}

void RTSRenderer::drawAirStrikes3D(const RTSEngine& engine) {
    const auto& gd = GameData::instance();
    for (const auto& air : engine.getAirStrikes()) {
        const auto& nat = gd.getNation(air.nation);
        Vector3 dir = Vector3Normalize(Vector3Subtract(air.targetPos, air.startPos));
        Vector3 right = {-dir.z, 0.0f, dir.x};
        float facingDeg = std::atan2(dir.x, dir.z) * RAD2DEG;

        if (air.isShotDown) {
            // Render spiraling, diving shot-down plane with fire & smoke
            rlPushMatrix();
            rlTranslatef(air.currentPos.x, air.currentPos.y, air.currentPos.z);
            rlRotatef(facingDeg, 0.0f, 1.0f, 0.0f);
            rlRotatef(air.pitchAngle, 1.0f, 0.0f, 0.0f);
            rlRotatef(air.rollAngle, 0.0f, 0.0f, 1.0f);
            if (air.isFighter) {
                models::drawFighterAircraft3D({0, 0, 0}, 0.0f, 0.0f, nat, waterAnimTime_ * 300.0f, false);
            } else {
                models::drawAircraft3D({0, 0, 0}, 0.0f, nat, waterAnimTime_ * 300.0f);
            }
            rlPopMatrix();
            continue;
        }

        if (air.isFighter) {
            // Air Superiority Interceptor (P-51 Mustang / Bf 109 / Spitfire / Zero / Yak-9)
            // Rendered with high-speed prop rotation and dynamic banking roll physics
            models::drawFighterAircraft3D(air.currentPos, facingDeg, air.rollAngle, nat, waterAnimTime_ * 1600.0f, air.dogfightTargetId != -1);
            float gY = engine.getTerrainHeight(air.currentPos.x, air.currentPos.z) + 0.12f;
            DrawCylinder({air.currentPos.x, gY, air.currentPos.z}, 2.2f, 2.2f, 0.02f, 10, {0, 0, 0, 85});

            // 3D Aerial Dogfight Twin 20mm Tracer Lines towards air combat target
            if (air.dogfightTargetId != -1) {
                Vector3 muzzleL = Vector3Add(air.currentPos, Vector3Scale(right, -1.8f));
                Vector3 muzzleR = Vector3Add(air.currentPos, Vector3Scale(right,  1.8f));
                DrawLine3D(muzzleL, air.dogfightTargetPos, GOLD);
                DrawLine3D(muzzleR, air.dogfightTargetPos, ORANGE);
                DrawSphere(air.dogfightTargetPos, 0.65f, RED);
            }
        } else if (air.isRecon) {
            // Single reconnaissance aircraft with high-speed prop blur
            models::drawAircraft3D(air.currentPos, facingDeg, nat, waterAnimTime_ * 1400.0f);
            float gY = engine.getTerrainHeight(air.currentPos.x, air.currentPos.z) + 0.12f;
            DrawCylinder({air.currentPos.x, gY, air.currentPos.z}, 2.5f, 2.5f, 0.02f, 10, {0, 0, 0, 85});
        } else if (air.isParatroopDrop || air.isSupplyDrop) {
            // Transport Aircraft (C-47 / Ju-52) with airborne drop
            models::drawAircraft3D(air.currentPos, facingDeg, nat, waterAnimTime_ * 750.0f);
            float gY = engine.getTerrainHeight(air.currentPos.x, air.currentPos.z) + 0.12f;
            DrawCylinder({air.currentPos.x, gY, air.currentPos.z}, 3.0f, 3.0f, 0.02f, 10, {0, 0, 0, 85});

            // Parachutes drifting down to drop zone
            if (air.progress >= 0.48f && air.progress <= 0.92f) {
                float dropRatio = (air.progress - 0.48f) / 0.44f;
                float dropY = 40.0f * (1.0f - dropRatio) + 1.2f;
                Vector3 c1 = {air.targetPos.x - 3.5f, dropY, air.targetPos.z + 2.0f};
                Vector3 c2 = {air.targetPos.x + 3.5f, dropY + 2.5f, air.targetPos.z - 2.0f};

                // Canopy 1
                DrawSphere({c1.x, c1.y + 2.2f, c1.z}, 1.9f, air.isSupplyDrop ? SKYBLUE : RAYWHITE);
                DrawLine3D({c1.x - 1.5f, c1.y + 2.2f, c1.z}, c1, DARKGRAY);
                DrawLine3D({c1.x + 1.5f, c1.y + 2.2f, c1.z}, c1, DARKGRAY);
                DrawCube(c1, 1.0f, 0.9f, 1.0f, air.isSupplyDrop ? GOLD : Color{50, 75, 45, 255});

                // Canopy 2
                DrawSphere({c2.x, c2.y + 2.2f, c2.z}, 1.9f, air.isSupplyDrop ? SKYBLUE : RAYWHITE);
                DrawLine3D({c2.x - 1.5f, c2.y + 2.2f, c2.z}, c2, DARKGRAY);
                DrawLine3D({c2.x + 1.5f, c2.y + 2.2f, c2.z}, c2, DARKGRAY);
                DrawCube(c2, 1.0f, 0.9f, 1.0f, air.isSupplyDrop ? GOLD : Color{60, 50, 40, 255});
            }
        } else {
            // 3-Plane Tactical Bomber / Heavy Squadron Formation
            for (int idx = -1; idx <= 1; ++idx) {
                Vector3 planePos = Vector3Add(air.currentPos, Vector3Scale(right, idx * 9.5f));
                planePos.y += std::abs(idx) * 1.8f;
                models::drawAircraft3D(planePos, facingDeg, nat, waterAnimTime_ * 920.0f);
                float gY = engine.getTerrainHeight(planePos.x, planePos.z) + 0.12f;
                DrawCylinder({planePos.x, gY, planePos.z}, 2.8f, 2.8f, 0.02f, 10, {0, 0, 0, 95});
            }
        }
    }
}

void RTSRenderer::drawProjectilesAndParticles3D(const RTSEngine& engine) {
    for (const auto& proj : engine.getProjectiles()) {
        if (proj.life <= 0.0f) continue;
        if (proj.isRocket) {
            DrawSphere(proj.pos, 0.52f, proj.color);
            DrawSphere(proj.pos, 0.85f, ColorAlpha(ORANGE, 0.45f));
            Vector3 tail = Vector3Subtract(proj.pos, Vector3Scale(Vector3Normalize(proj.velocity), 4.8f));
            DrawCylinderEx(tail, proj.pos, 0.06f, 0.22f, 6, ColorAlpha(YELLOW, 0.85f));
        } else {
            // Luminous High-Velocity Armor-Piercing & Tracer Bullet
            Vector3 tail = Vector3Subtract(proj.pos, Vector3Scale(Vector3Normalize(proj.velocity), 3.4f));
            DrawSphere(proj.pos, 0.22f, WHITE);
            DrawLine3D(tail, proj.pos, ColorAlpha(YELLOW, 0.95f));
            DrawLine3D(tail, proj.pos, ColorAlpha(WHITE, 0.75f));
        }
    }

    bool hasSmoke = (smokeParticleTex_.id != 0);
    bool hasGlow  = (glowParticleTex_.id != 0);

    for (const auto& pt : engine.getParticles()) {
        if (!frustum_.containsSphere(pt.pos, pt.size * 2.5f + 1.0f)) continue;
        float lifeRatio = std::clamp(pt.life / std::max(0.01f, pt.maxLife), 0.0f, 1.0f);
        if (pt.isCasing) {
            float gh = engine.getTerrainHeight(pt.pos.x, pt.pos.z) + 0.05f;
            DrawCircle3D({pt.pos.x, gh - 0.02f, pt.pos.z}, pt.size * 0.9f, {1, 0, 0}, 90.0f, ColorAlpha(BLACK, 0.35f * lifeRatio));
            rlPushMatrix();
            rlTranslatef(pt.pos.x, gh, pt.pos.z);
            rlRotatef(pt.rotation, 0.0f, 1.0f, 0.0f);
            DrawCylinderEx({-pt.size * 1.2f, 0.0f, 0.0f}, {pt.size * 1.2f, 0.0f, 0.0f}, pt.size * 0.32f, pt.size * 0.32f, 6, ColorAlpha(GOLD, lifeRatio));
            rlPopMatrix();
        } else if (pt.isTread) {
            float gh = engine.getTerrainHeight(pt.pos.x, pt.pos.z) + 0.03f;
            unsigned char a = static_cast<unsigned char>(lifeRatio * 52.0f);
            DrawCircle3D({pt.pos.x, gh, pt.pos.z}, pt.size, {1.0f, 0.0f, 0.0f}, 90.0f, Color{38, 34, 28, a});
        } else if (pt.isCrater) {
            float gh = engine.getTerrainHeight(pt.pos.x, pt.pos.z) + 0.08f;
            unsigned char aOuter = static_cast<unsigned char>(lifeRatio * 155.0f);
            unsigned char aInner = static_cast<unsigned char>(lifeRatio * 215.0f);
            DrawCircle3D({pt.pos.x, gh, pt.pos.z}, pt.size, {1.0f, 0.0f, 0.0f}, 90.0f, Color{36, 32, 28, aOuter});
            DrawCircle3D({pt.pos.x, gh + 0.02f, pt.pos.z}, pt.size * 0.52f, {1.0f, 0.0f, 0.0f}, 90.0f, Color{18, 16, 14, aInner});
        } else if (pt.isRing) {
            Color rc = {255, 195, 95, static_cast<unsigned char>(lifeRatio * 180.0f)};
            DrawCircle3D({pt.pos.x, 0.35f, pt.pos.z}, pt.size * (1.4f - lifeRatio * 0.6f), {1.0f, 0.0f, 0.0f}, 90.0f, rc);
        } else {
            bool isFire = (pt.color.r > 180 && pt.color.g > 60);
            if (isFire) {
                if (hasGlow) {
                    float fSize = pt.size * (1.8f - lifeRatio * 0.5f);
                    unsigned char coreA  = static_cast<unsigned char>(lifeRatio * 235.0f);
                    unsigned char outerA = static_cast<unsigned char>(lifeRatio * 160.0f);
                    DrawBillboard(camera_, glowParticleTex_, pt.pos, fSize * 2.2f, Color{255, 115, 25, outerA});
                    DrawBillboard(camera_, glowParticleTex_, pt.pos, fSize * 1.3f, Color{255, 225, 115, coreA});
                } else {
                    DrawSphere(pt.pos, pt.size * 0.95f, {255, 135, 30, static_cast<unsigned char>(lifeRatio * 220.0f)});
                    DrawSphere(pt.pos, pt.size * 1.25f, {255, 185, 60, static_cast<unsigned char>(lifeRatio * 85.0f)});
                }
            } else {
                if (hasSmoke) {
                    // AAA Soft Gaussian Billboard Smoke: expanding, fading warm charcoal vapor, zero faceted 3D black spheres
                    float currentSize = pt.size * (1.6f + (1.0f - lifeRatio) * 2.2f);
                    unsigned char smokeAlpha = static_cast<unsigned char>(lifeRatio * 135.0f);
                    Color smokeTint = {120, 116, 112, smokeAlpha};
                    DrawBillboard(camera_, smokeParticleTex_, pt.pos, currentSize, smokeTint);
                } else {
                    unsigned char smokeAlpha = static_cast<unsigned char>(lifeRatio * 55.0f);
                    Color softSmoke = {195, 190, 180, smokeAlpha};
                    DrawSphere(pt.pos, pt.size * (1.05f + (1.0f - lifeRatio) * 0.45f), softSmoke);
                }
            }
        }
    }
}

void RTSRenderer::drawWeatherEffects3D(const RTSEngine& engine) {
    BiomeType biome = engine.getConfig().biome;
    bool isSnow = (biome == BiomeType::STALINGRAD_WINTER || biome == BiomeType::ARDENNES_WINTER);
    bool isSand = (biome == BiomeType::EL_ALAMEIN_DESERT);
    bool isRain = (biome == BiomeType::NORMANDY_BOCAGE || biome == BiomeType::BERLIN_RUINS);
    if (!isSnow && !isSand && !isRain) return;

    float t = waterAnimTime_;
    int maxDrops = 85;
    if (quality_ == GraphicQuality::LOW) maxDrops = 24;
    else if (quality_ == GraphicQuality::MEDIUM) maxDrops = 48;
    else if (quality_ == GraphicQuality::HIGH) maxDrops = 85;
    else if (quality_ == GraphicQuality::ULTRA) maxDrops = 135;

    for (int i = 0; i < maxDrops; ++i) {
        float seedX = static_cast<float>((i * 73) % 100) - 50.0f;
        float seedZ = static_cast<float>((i * 41) % 100) - 50.0f;
        float cycle = std::fmod(t * (isRain ? 26.0f : 11.0f) + static_cast<float>(i * 1.9f), 36.0f);
        float py = 36.0f - cycle;
        float px = cameraTarget_.x + seedX * 1.7f + (isSand ? cycle * 1.3f : 0.0f);
        float pz = cameraTarget_.z + seedZ * 1.7f;

        if (isSnow) {
            DrawSphere({px + std::sin(t * 2.0f + i) * 1.4f, py, pz}, 0.22f, {245, 250, 255, 195});
        } else if (isSand) {
            DrawLine3D({px, py * 0.5f, pz}, {px + 2.6f, py * 0.5f - 0.3f, pz + 0.5f}, {225, 192, 132, 130});
        } else if (isRain) {
            DrawLine3D({px, py, pz}, {px - 0.35f, py - 2.2f, pz}, {175, 205, 235, 145});
        }
    }
}

void RTSRenderer::drawUnitWorldBarsAndLabels(const RTSEngine& engine) {
    const auto& gd = GameData::instance();
    const auto& ui = UITheme::instance();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f) continue;
        // Conceal Enemy Building labels unless inside live Player Vision
        if (b.teamId != 0 && b.teamId != -1 && !engine.isAllied(0, b.teamId) && !engine.isPosVisibleToTeam(b.pos, 0)) continue;
        // Conceal Neutral Town/Oil labels until explored by Player Vision / Recon
        if (b.teamId == -1 && !engine.isPosExploredByPlayer(b.pos)) continue;

        Vector2 sp = GetWorldToScreen({b.pos.x, b.pos.y + 8.2f, b.pos.z}, camera_);
        if (sp.x < 20 || sp.x > sw - 20 || sp.y < 66 || sp.y > sh - 170) continue;

        float bw = 68.0f;
        float pct = std::clamp(b.health / b.maxHealth, 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(sp.x - bw * 0.5f - 1), static_cast<int>(sp.y - 1), static_cast<int>(bw + 2), 8, {0, 0, 0, 210});
        Color hpCol = (b.teamId == 0) ? LIME : (engine.isAllied(0, b.teamId) ? SKYBLUE : (b.teamId == -1 ? LIGHTGRAY : RED));
        DrawRectangle(static_cast<int>(sp.x - bw * 0.5f), static_cast<int>(sp.y), static_cast<int>(bw * pct), 6, hpCol);

        if (b.isBombDamaged) {
            const char* dmgTxt = "BOMB DAMAGED [ENGINEERS NEEDED]";
            float dtw = ui.measureWidth(dmgTxt, 9.0f, true);
            ui.drawText(dmgTxt, sp.x - dtw * 0.5f, sp.y - 10.0f, 9.0f, ORANGE, true);
        }

        if (b.type == BuildingType::CITY_DEPOT || b.type == BuildingType::OIL_DERRICK || b.selected) {
            const char* bName = (b.type == BuildingType::CITY_DEPOT)
                ? (engine.isAllied(0, b.teamId) ? "STRATEGIC TOWN & FIELD HOSPITAL (Active Healing Station & +82m Vision)"
                                                : (engine.isEnemy(0, b.teamId) ? "STRATEGIC TOWN (Hostile Field Hospital)"
                                                                               : "STRATEGIC TOWN (Capture to Heal Wounded Soldiers)"))
                : (b.type == BuildingType::OIL_DERRICK ? "OIL REFINERY (+OIL & VISION)" : gd.getBuilding(b.type).name.c_str());
            float tw = ui.measureWidth(bName, 12.0f, true);
            ui.drawText(bName, sp.x - tw * 0.5f, sp.y - (b.isBombDamaged ? 23.0f : 15.0f), 12.0f, GOLD, true);
        }

        // Tactical 3D Capture Progress Bar & Status for Cities & Oil Derricks
        if (b.type == BuildingType::CITY_DEPOT || b.type == BuildingType::OIL_DERRICK) {
            float capBarW = 76.0f;
            DrawRectangle(static_cast<int>(sp.x - capBarW * 0.5f - 1), static_cast<int>(sp.y + 8), static_cast<int>(capBarW + 2), 7, {0, 0, 0, 220});
            DrawRectangle(static_cast<int>(sp.x - 1), static_cast<int>(sp.y + 7), 2, 9, WHITE); // Neutral center line
            if (b.captureProgress > 0.0f) {
                float fillW = (b.captureProgress / 100.0f) * (capBarW * 0.5f);
                DrawRectangle(static_cast<int>(sp.x), static_cast<int>(sp.y + 9), static_cast<int>(fillW), 5, SKYBLUE);
            } else if (b.captureProgress < 0.0f) {
                float fillW = (-b.captureProgress / 100.0f) * (capBarW * 0.5f);
                DrawRectangle(static_cast<int>(sp.x - fillW), static_cast<int>(sp.y + 9), static_cast<int>(fillW), 5, RED);
            }

            std::string capText;
            Color capColor = LIGHTGRAY;
            if (engine.isAllied(0, b.teamId) && b.captureProgress >= 95.0f) {
                capText = (b.teamId == 0) ? "SECURED [PLAYER CONTROL]" : "SECURED [ALLIED FORCE]";
                capColor = LIME;
            } else if (engine.isEnemy(0, b.teamId) && b.captureProgress <= -95.0f) {
                capText = "HELD BY HOSTILE ENEMY";
                capColor = Color{245, 95, 85, 255};
            } else if (b.captureProgress > 0.0f) {
                capText = TextFormat("SECURING: %.0f%% (Allied Control)", b.captureProgress);
                capColor = SKYBLUE;
            } else if (b.captureProgress < 0.0f) {
                capText = TextFormat("LOSING CONTROL: %.0f%% (Hostile)", -b.captureProgress);
                capColor = Color{255, 120, 100, 255};
            } else {
                capText = "NEUTRAL (Move Troops within 28m Zone)";
                capColor = GOLD;
            }
            float ctw = ui.measureWidth(capText.c_str(), 10.0f, true);
            ui.drawText(capText.c_str(), sp.x - ctw * 0.5f, sp.y + 17.0f, 10.0f, capColor, true);
        }
    }

    // 2D Tactical Screen-Space Assembly Point Indicator (Crystal Clear even in dark fog)
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f || b.teamId != 0 || !b.selected) continue;
        if (b.type != BuildingType::CORPS_HQ && b.type != BuildingType::INFANTRY_HQ &&
            b.type != BuildingType::MECH_HQ && b.type != BuildingType::ARMOR_HQ &&
            b.type != BuildingType::AIRFIELD) continue;

        float rh = engine.getTerrainHeight(b.rallyPoint.x, b.rallyPoint.z);
        Vector2 sp = GetWorldToScreen({b.rallyPoint.x, rh + 0.6f, b.rallyPoint.z}, camera_);

        bool onScreen = (sp.x >= 25 && sp.x <= sw - 25 && sp.y >= 65 && sp.y <= sh - 175);
        float badgeX = std::clamp(sp.x, 80.0f, sw - 80.0f);
        float badgeY = std::clamp(sp.y, 85.0f, sh - 195.0f);

        // Tactical 2D Assembly Badge
        float bw = 140.0f;
        float bh = 24.0f;
        Rectangle bRec = {badgeX - bw * 0.5f, badgeY - 28.0f, bw, bh};
        DrawRectangleRec(bRec, {12, 18, 16, 245});
        DrawRectangleLinesEx(bRec, 1.8f, GOLD);
        ui.drawTextCentered("▲ ASSEMBLY POINT", bRec, 11.0f, GOLD, true);

        // Downward pointer chevron
        DrawTriangle({badgeX - 7.0f, badgeY - 4.0f}, {badgeX + 7.0f, badgeY - 4.0f}, {badgeX, badgeY + 4.0f}, GOLD);

        if (onScreen) {
            DrawCircle(static_cast<int>(sp.x), static_cast<int>(sp.y), 4.0f, SKYBLUE);
            DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), 8.0f, GOLD);
        }
    }

    if (moveOrderIndicatorTimer_ > 0.0f) {
        float mh = engine.getTerrainHeight(moveOrderIndicatorPos_.x, moveOrderIndicatorPos_.z);
        Vector2 mp = GetWorldToScreen({moveOrderIndicatorPos_.x, mh + 0.8f, moveOrderIndicatorPos_.z}, camera_);
        if (mp.x >= 20 && mp.x <= sw - 20 && mp.y >= 60 && mp.y <= sh - 170) {
            Color wCol = moveOrderIndicatorIsAttack_ ? RED : LIME;
            DrawCircleLines(static_cast<int>(mp.x), static_cast<int>(mp.y), 12.0f, wCol);
            DrawCircle(static_cast<int>(mp.x), static_cast<int>(mp.y), 4.0f, wCol);
            const char* wText = moveOrderIndicatorIsAttack_ ? "TARGET AREA" : "ORDER: MOVE HERE";
            float tw = ui.measureWidth(wText, 10.0f, true);
            DrawRectangle(static_cast<int>(mp.x - tw * 0.5f - 4), static_cast<int>(mp.y - 22), static_cast<int>(tw + 8), 15, {10, 14, 12, 230});
            DrawRectangleLinesEx({mp.x - tw * 0.5f - 4, mp.y - 22, tw + 8, 15}, 1.0f, wCol);
            ui.drawText(wText, mp.x - tw * 0.5f, mp.y - 21.0f, 10.0f, wCol, true);
        }
    }

    int selectedCount = 0;
    for (const auto& r : engine.getRegiments()) {
        if (r.health > 0.0f && r.selected) ++selectedCount;
    }

    auto getClassBadge = [](RegimentType t) -> const char* {
        switch (t) {
            case RegimentType::RIFLE_REGIMENT:        return "INF";
            case RegimentType::MG_REGIMENT:           return "MG";
            case RegimentType::MORTAR_REGIMENT:       return "MRT";
            case RegimentType::AT_BAZOOKA_REGIMENT:   return "AT";
            case RegimentType::ENGINEER_FLAME_REGIMENT: return "ENG";
            case RegimentType::SNIPER_REGIMENT:       return "SNP";
            case RegimentType::PARATROOP_REGIMENT:    return "AIR";
            case RegimentType::RECON_HALFTRACK:       return "REC";
            case RegimentType::MOBILE_ARTILLERY:      return "ART";
            case RegimentType::KATYUSHA_ROCKET:       return "RKT";
            case RegimentType::AA_HALFTRACK:          return "AA";
            case RegimentType::LIGHT_TANK:            return "LT";
            case RegimentType::MEDIUM_TANK:           return "TNK";
            case RegimentType::TANK_DESTROYER:        return "TD";
            case RegimentType::HEAVY_TANK:            return "HVY";
            case RegimentType::ELITE_SIGNATURE_TANK:  return "ELT";
            case RegimentType::PACKED_HQ_TRUCK:       return "HQ";
            default:                                  return "UNT";
        }
    };

    for (const auto& r : engine.getRegiments()) {
        if (r.health <= 0.0f) continue;
        // Strict Fog of War: Never show Enemy Regiment plates unless spotted by Player Vision / Recon!
        if (r.teamId != 0 && !engine.isAllied(0, r.teamId) && !engine.isPosVisibleToTeam(r.pos, 0)) continue;

        Vector2 sp = GetWorldToScreen({r.pos.x, r.pos.y + 3.8f, r.pos.z}, camera_);
        if (sp.x < 20 || sp.x > sw - 20 || sp.y < 66 || sp.y > sh - 170) continue;

        float bw = 44.0f;
        float hpPct  = std::clamp(r.health / r.maxHealth, 0.0f, 1.0f);
        float orgPct = std::clamp(r.organization / r.maxOrganization, 0.0f, 1.0f);

        bool isPlayer = (r.teamId == 0);
        bool isAlly = (!isPlayer && engine.isAllied(0, r.teamId));
        bool isE1 = (!isPlayer && !isAlly && r.teamId == 1);
        bool isE2 = (!isPlayer && !isAlly && r.teamId == 2);
        bool isE3 = (!isPlayer && !isAlly && r.teamId >= 3);

        Color factionCol = isPlayer ? LIME :
                          (isAlly ? SKYBLUE :
                          (isE2 ? Color{245, 155, 45, 255} :
                          (isE3 ? Color{195, 110, 245, 255} : Color{230, 65, 65, 255})));

        // Sleek NATO/Tactical Unit Plate (Class badge + dual HP/Org bars + Control Group badge)
        Rectangle plate = {sp.x - bw * 0.5f - 14.0f, sp.y - 2.0f, bw + 16.0f, 13.0f};
        DrawRectangleRec(plate, {10, 14, 12, 220});
        Color borderCol = r.selected ? GOLD : ColorAlpha(factionCol, 0.85f);
        DrawRectangleLinesEx(plate, 1.0f, borderCol);
        Color textCol = isPlayer ? Color{190, 245, 195, 255} :
                       (isAlly ? Color{180, 230, 255, 255} :
                       (isE2 ? Color{255, 215, 160, 255} :
                       (isE3 ? Color{235, 190, 255, 255} : Color{255, 185, 180, 255})));
        ui.drawText(getClassBadge(r.type), plate.x + 2.0f, plate.y + 1.0f, 10.0f,
                    textCol, true, false);

        if (isPlayer && r.controlGroup >= 1 && r.controlGroup <= 9) {
            Rectangle cgBox = {plate.x - 15.0f, plate.y, 14.0f, 13.0f};
            DrawRectangleRec(cgBox, {215, 175, 45, 245});
            DrawRectangleLinesEx(cgBox, 1.0f, BLACK);
            ui.drawText(TextFormat("%d", r.controlGroup), cgBox.x + 3.5f, cgBox.y + 1.0f, 10.0f, BLACK, true, false);
        } else if (isAlly) {
            Rectangle alBox = {plate.x - 18.0f, plate.y, 17.0f, 13.0f};
            DrawRectangleRec(alBox, {24, 45, 62, 235});
            DrawRectangleLinesEx(alBox, 1.0f, SKYBLUE);
            ui.drawText("AL", alBox.x + 2.5f, alBox.y + 1.0f, 10.0f, SKYBLUE, true, false);
        } else if (isE1) {
            Rectangle eBox = {plate.x - 18.0f, plate.y, 17.0f, 13.0f};
            DrawRectangleRec(eBox, {54, 20, 20, 235});
            DrawRectangleLinesEx(eBox, 1.0f, Color{230, 65, 65, 255});
            ui.drawText("E1", eBox.x + 2.5f, eBox.y + 1.0f, 10.0f, Color{255, 160, 160, 255}, true, false);
        } else if (isE2) {
            Rectangle eBox = {plate.x - 18.0f, plate.y, 17.0f, 13.0f};
            DrawRectangleRec(eBox, {54, 38, 16, 235});
            DrawRectangleLinesEx(eBox, 1.0f, Color{245, 155, 45, 255});
            ui.drawText("E2", eBox.x + 2.5f, eBox.y + 1.0f, 10.0f, Color{255, 200, 140, 255}, true, false);
        } else if (isE3) {
            Rectangle eBox = {plate.x - 18.0f, plate.y, 17.0f, 13.0f};
            DrawRectangleRec(eBox, {45, 18, 54, 235});
            DrawRectangleLinesEx(eBox, 1.0f, Color{195, 110, 245, 255});
            ui.drawText("E3", eBox.x + 2.5f, eBox.y + 1.0f, 10.0f, Color{235, 180, 255, 255}, true, false);
        }

        if (r.veterancy > 0) {
            const char* vStars = (r.veterancy == 1) ? "[*]" : ((r.veterancy == 2) ? "[**]" : "[***]");
            ui.drawText(vStars, plate.x + plate.width - 18.0f, plate.y - 12.0f, 9.0f, GOLD, true, false);
        }

        DrawRectangle(static_cast<int>(sp.x - bw * 0.5f + 12), static_cast<int>(sp.y), static_cast<int>((bw - 12.0f) * hpPct), 5,
                      factionCol);
        DrawRectangle(static_cast<int>(sp.x - bw * 0.5f + 12), static_cast<int>(sp.y + 5), static_cast<int>((bw - 12.0f) * orgPct), 4,
                      r.inSupply ? SKYBLUE : ORANGE);

        if ((isPlayer || isAlly) && engine.isWaterAt(r.pos.x, r.pos.z)) {
            ui.drawText("WADING RIVER (-72% SPD)", sp.x - 48.0f, sp.y - 14.0f, 9.0f, SKYBLUE, true);
        } else if ((isPlayer || isAlly) && r.atTownHealingStation && r.health < r.maxHealth) {
            ui.drawText("+ HEALING", sp.x - 24.0f, sp.y - 14.0f, 10.0f, Color{100, 255, 140, 255}, true);
        } else if (r.smokeScreenTimer > 0.0f) {
            ui.drawText("SMOKE OBSCURATION (-55%)", sp.x - 56.0f, sp.y - 14.0f, 9.0f, Color{225, 230, 235, 255}, true);
        } else if (r.rapidVolleyTimer > 0.0f) {
            ui.drawText("RAPID VOLLEY (+100% ROF)", sp.x - 54.0f, sp.y - 14.0f, 9.0f, Color{255, 205, 65, 255}, true);
        } else if (r.coverTier == 2) {
            ui.drawText("[HEAVY COVER -40%]", sp.x - 46.0f, sp.y - 14.0f, 9.0f, Color{130, 225, 255, 255}, true);
        } else if (r.coverTier == 1) {
            ui.drawText("[LIGHT COVER -25%]", sp.x - 46.0f, sp.y - 14.0f, 9.0f, Color{195, 235, 185, 255}, true);
        } else if (!r.inSupply) {
            float blink = std::sin(waterAnimTime_ * 7.0f);
            Color alertCol = (blink > 0.0f) ? Color{255, 75, 65, 255} : Color{255, 160, 45, 255};
            ui.drawText("! NO SUPPLY", sp.x - 28.0f, sp.y - 14.0f, 10.0f, alertCol, true);
        } else if (r.teamId == 0 && r.supplyBuffer < 25.0f && r.supplySourceId == -1) {
            ui.drawText(TextFormat("RATIONS (%.0fs)", r.supplyBuffer), sp.x - 32.0f, sp.y - 14.0f, 9.0f, Color{255, 220, 90, 240}, true);
        } else if (r.organization < 30.0f) {
            ui.drawText("PINNED", sp.x - 18.0f, sp.y - 14.0f, 10.0f, ORANGE, true);
        } else if (r.stance == FormationStance::ENTRENCHED) {
            ui.drawText("FORTIFIED (+35%)", sp.x - 42.0f, sp.y - 14.0f, 9.0f, Color{235, 215, 140, 255}, true);
        } else if (r.selected && selectedCount <= 2) {
            std::string disp = gd.getRegimentDisplayName(r.nation, r.type);
            float tw = ui.measureWidth(disp.c_str(), 12.0f, true);
            DrawRectangle(static_cast<int>(sp.x - tw * 0.5f - 4), static_cast<int>(sp.y - 17), static_cast<int>(tw + 8), 14, {10, 14, 12, 210});
            ui.drawText(disp.c_str(), sp.x - tw * 0.5f, sp.y - 16.0f, 12.0f, RAYWHITE, true);
        }
    }
}

// ============================================================================
// AAA Brushed-Metal & Brass Tactical Command HUD
// ============================================================================

void RTSRenderer::renderTacticalHUD(RTSEngine& engine, SoundEngine& sound, bool& outReturnToMenu, bool& outFinishBattle) {
    outReturnToMenu = false;
    outFinishBattle = false;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    const auto& gd = GameData::instance();
    const auto& ui = UITheme::instance();
    const auto& p0 = engine.getPlayer(0);
    const auto& nat = gd.getNation(p0.nation);
    const auto& gen = gd.getGeneral(p0.nation, p0.generalIdx);

    // =========================================================================
    // 1. TOP BAR: RESOURCES, GENERAL SPECIAL OPERATIONS (WITH RED RECHARGE BARS) & SYSTEM BUTTONS
    // =========================================================================
    DrawRectangleGradientV(0, 0, sw, 44, {28, 36, 32, 252}, {14, 18, 16, 254});
    DrawLineEx({0.0f, 44.0f}, {static_cast<float>(sw), 44.0f}, 2.0f, {178, 154, 96, 255});

    // Nation & General Badge
    DrawRectangleGradientH(6, 4, 196, 36, ColorAlpha(nat.primaryColor, 0.85f), {20, 26, 22, 240});
    DrawRectangleLinesEx({6.0f, 4.0f, 196.0f, 36.0f}, 1.5f, nat.accentColor);
    const Texture2D& flagTex = ui.getNationFlag(p0.nation);
    if (flagTex.id != 0) {
        DrawTexturePro(flagTex, {0, 0, static_cast<float>(flagTex.width), static_cast<float>(flagTex.height)},
                       {10.0f, 7.0f, 30.0f, 30.0f}, {0, 0}, 0.0f, WHITE);
    }
    ui.drawText(nat.name.c_str(), 44.0f, 6.0f, 14.0f, RAYWHITE, true);
    ui.drawText(gen.name.c_str(), 44.0f, 22.0f, 11.0f, nat.accentColor, false);

    // 3-Resource Economy + Army Experience (Command Points) + Non-linear Town & Oil Synergies
    ui.drawText(TextFormat("$%.0f (+%.0f/s)", p0.money, p0.moneyRate), 210.0f, 7.0f, 13.0f, {255, 222, 95, 255}, true);
    if (p0.controlledTowns > 1) {
        ui.drawText(TextFormat("%d Towns (+%.0f%% Synergy)", p0.controlledTowns, (p0.townSynergyMult - 1.0f) * 100.0f),
                    210.0f, 23.0f, 10.0f, {225, 215, 160, 235}, false);
    } else {
        ui.drawText(TextFormat("%d Town (Base Yield)", p0.controlledTowns), 210.0f, 23.0f, 10.0f, {185, 180, 165, 210}, false);
    }

    ui.drawText(TextFormat("AMMO: %.0f (+%.0f)", p0.ammo, p0.ammoRate), 376.0f, 7.0f, 13.0f, {255, 182, 75, 255}, true);
    ui.drawText("Supply Line Network", 376.0f, 23.0f, 10.0f, {185, 180, 165, 210}, false);

    ui.drawText(TextFormat("OIL: %.0f (+%.0f)", p0.oil, p0.oilRate), 512.0f, 7.0f, 13.0f, {115, 208, 255, 255}, true);
    if (p0.controlledOil > 1) {
        ui.drawText(TextFormat("%d Derricks (+%.0f%% Syn)", p0.controlledOil, (p0.oilSynergyMult - 1.0f) * 100.0f),
                    512.0f, 23.0f, 10.0f, {150, 215, 245, 220}, false);
    } else {
        ui.drawText(TextFormat("%d Oil Derrick", p0.controlledOil), 512.0f, 23.0f, 10.0f, {170, 195, 210, 210}, false);
    }

    ui.drawText(TextFormat("XP: %.0f CP", p0.commandPoints), 648.0f, 13.0f, 14.0f, LIME, true);

    // Authentic 2004 General Special Operations Bar (with Red Recharge Progress Bar when on cooldown)
    for (int op = 0; op < 3; ++op) {
        const auto& sDef = gen.specialOps[op];
        float cd = p0.specOpCooldowns[op];
        bool ready = (cd <= 0.0f) && (p0.commandPoints >= sDef.commandPointCost);
        bool targeting = (engine.pendingSpecOpIdx() == op);

        Rectangle sr = {742.0f + op * 144.0f, 6.0f, 138.0f, 32.0f};
        DrawRectangleRec(sr, ready ? Color{48, 44, 28, 255} : Color{26, 28, 28, 255});
        if (cd > 0.0f && sDef.cooldownSec > 0.0f) {
            float fillPct = std::clamp(1.0f - (cd / sDef.cooldownSec), 0.0f, 1.0f);
            DrawRectangle(static_cast<int>(sr.x), static_cast<int>(sr.y + sr.height - 5.0f),
                          static_cast<int>(sr.width * fillPct), 5, {210, 45, 45, 255});
        }
        char opLbl[64];
        std::string shortName = sDef.name.size() > 15 ? (sDef.name.substr(0, 14) + ".") : sDef.name;
        std::snprintf(opLbl, sizeof(opLbl), "F%d: %s", op + 1, shortName.c_str());
        if (DrawHUDButton(sr, opLbl, ready ? Color{56, 48, 30, 220} : Color{28, 30, 30, 220},
                          targeting ? LIME : (ready ? GOLD : GRAY), 11, targeting || ready)) {
            if (ready) {
                if (sDef.targetType == SpecOpTargetType::GROUND_AREA) {
                    engine.setPendingSpecOpIdx(op);
                    sound.playSfx(SfxId::UI_CLICK);
                } else {
                    engine.triggerSpecialOp(0, op, cameraTarget_);
                }
            }
        }
    }

    // 30-Minute Tactical Match Clock (MM:SS / 30:00)
    float bTime = engine.getBattleTime();
    int curMin = static_cast<int>(bTime) / 60;
    int curSec = static_cast<int>(bTime) % 60;
    Rectangle timeBox = {static_cast<float>(sw - 396), 6.0f, 126.0f, 32.0f};
    DrawRectangleRec(timeBox, {22, 28, 25, 230});
    DrawRectangleLinesEx(timeBox, 1.2f, {150, 140, 95, 255});
    ui.drawTextCentered(TextFormat("TIME: %02d:%02d / 30:00", curMin, curSec), timeBox, 10.5f, GOLD, true);

    // Top-Right Utility Buttons
    if (DrawHUDButton({static_cast<float>(sw - 262), 6.0f, 84.0f, 32.0f},
                      "CONTROLS [?]", {42, 52, 68, 255}, GOLD, 11, showControlsHelp_)) {
        showControlsHelp_ = !showControlsHelp_;
        sound.playSfx(SfxId::UI_CLICK);
    }
    if (DrawHUDButton({static_cast<float>(sw - 172), 6.0f, 80.0f, 32.0f},
                      engine.isPaused() ? "RESUME" : "PAUSE", {56, 48, 38, 255}, ORANGE, 11, engine.isPaused())) {
        engine.togglePause();
    }
    if (DrawHUDButton({static_cast<float>(sw - 86), 6.0f, 78.0f, 32.0f},
                      "MENU", {74, 34, 34, 255}, RED, 11)) {
        outReturnToMenu = true;
    }

    // Active Tactical Mode Instruction Banner (for Building Placement, Special Op Targeting, Air Missions, or Move Order)
    if (engine.pendingBuildType() >= 0 || engine.pendingSpecOpIdx() >= 0 || engine.pendingAirMissionType() >= 0 || attackMoveMode_) {
        const char* bannerMsg = "MOVE / ATTACK ORDER: Left-Click anywhere on battlefield to advance  |  Right-Click or ESC to cancel";
        if (engine.pendingBuildType() >= 0) {
            bannerMsg = TextFormat("PLACE [%s]: Left-Click on battlefield to construct, OR click button again to Auto-Build near HQ  |  ESC to cancel",
                                   gd.getBuilding(static_cast<BuildingType>(engine.pendingBuildType())).name.c_str());
        } else if (engine.pendingSpecOpIdx() >= 0) {
            bannerMsg = TextFormat("SPECIAL OPERATION TARGETING: Left-Click target zone for [%s]  |  Right-Click or ESC to cancel",
                                   gen.specialOps[engine.pendingSpecOpIdx()].name.c_str());
        } else if (engine.pendingAirMissionType() >= 0) {
            int m = engine.pendingAirMissionType();
            bannerMsg = (m == 1) ? "AIR RECON MISSION: Left-Click on battlefield to dispatch high-speed reconnaissance plane (clears Fog of War)  |  ESC to cancel" :
                        (m == 2 ? "TACTICAL BOMBING RUN: Left-Click target zone to unleash heavy bomber strike (260 HE damage)  |  ESC to cancel" :
                        (m == 3 ? "AIRBORNE INSERTION: Left-Click drop zone for Paratroopers & Bazooka squad  |  ESC to cancel" :
                        (m == 4 ? "EMERGENCY SUPPLY DROP: Left-Click drop zone for +220 resources & field repairs  |  ESC to cancel" :
                                  "COMBAT AIR PATROL [J]: Left-Click patrol coordinates for Fighter Interceptor sweep & aerial dogfighting  |  ESC to cancel")));
        }
        float bw = ui.measureWidth(bannerMsg, 14.0f, true) + 36.0f;
        Rectangle bBox = {(sw - bw) * 0.5f, 52.0f, bw, 28.0f};
        DrawRectangleRec(bBox, {18, 26, 20, 240});
        DrawRectangleLinesEx(bBox, 1.8f, GOLD);
        ui.drawTextCentered(bannerMsg, bBox, 14.0f, GOLD, true);
    }

    // Count HQs, Cities, Regiments, and Control Groups
    int pCities = 0;
    int corpsHqId = -1, infHqId = -1, mechHqId = -1, armorHqId = -1, airfieldId = -1;
    const RTSBuilding* selectedBldg = nullptr;
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f) continue;
        if (b.type == BuildingType::CITY_DEPOT && b.teamId == 0) {
            ++pCities;
        }
        if (b.teamId == 0) {
            if (b.selected) selectedBldg = &b;
            if (b.type == BuildingType::CORPS_HQ)    { corpsHqId = b.id; }
            if (b.type == BuildingType::INFANTRY_HQ) { infHqId = b.id; }
            if (b.type == BuildingType::MECH_HQ)     { mechHqId = b.id; }
            if (b.type == BuildingType::ARMOR_HQ)    { armorHqId = b.id; }
            if (b.type == BuildingType::AIRFIELD)    { airfieldId = b.id; }
        }
    }

    int pRegs = 0;
    int groupCounts[6] = {0, 0, 0, 0, 0, 0};
    std::vector<const RTSRegiment*> selectedRegs;
    for (const auto& r : engine.getRegiments()) {
        if (r.health <= 0.0f) continue;
        if (r.teamId == 0) {
            ++pRegs;
            if (r.selected) selectedRegs.push_back(&r);
            if (r.controlGroup >= 1 && r.controlGroup <= 5) groupCounts[r.controlGroup]++;
        }
    }

    // If neither a building nor a regiment is currently selected, default to inspecting the player's Corps HQ
    // so the player always sees their HQ & construction options ready just like in the 2004 game!
    if (!selectedBldg && selectedRegs.empty() && corpsHqId != -1) {
        for (auto& b : engine.getBuildingsMut()) {
            if (b.id == corpsHqId) {
                b.selected = true;
                selectedBldg = &b;
                break;
            }
        }
    }

    auto selectHQById = [&](int hqId) {
        if (hqId == -1) return;
        selectedBldg = nullptr;
        for (auto& r : engine.getRegimentsMut()) r.selected = false;
        for (auto& b : engine.getBuildingsMut()) {
            b.selected = (b.id == hqId);
            if (b.id == hqId) {
                selectedBldg = &b;
            }
        }
        selectedRegs.clear();
        sound.playSfx(SfxId::UI_CLICK, 0.6f);
    };

    // [F] Hotkey: Quick-Select Airfield (or prompt to construct one from Corps HQ)
    if (IsKeyPressed(KEY_F)) {
        if (airfieldId != -1) {
            selectHQById(airfieldId);
        } else if (corpsHqId != -1) {
            selectHQById(corpsHqId);
            engine.setPendingBuildType(static_cast<int>(BuildingType::AIRFIELD));
            engine.pushLog("DEPLOY AIRFIELD: Left-click ground to place or 2x-click to auto-build Forward Airfield ($220).");
        }
    }

    // =========================================================================
    // 2. CONSOLE RIBBON TAB BAR (y = sh - 196 .. sh - 168):
    //    Left: HQ Quick-Select Tabs (Corps HQ, Infantry HQ, Mech HQ, Armor HQ, Airfield, All Army)
    //    Right: Squad Control Group Tabs (Group 1 .. Group 5)
    // =========================================================================
    float consoleY = static_cast<float>(sh - 168);
    float ribbonY  = consoleY - 28.0f;
    DrawRectangleGradientH(0, static_cast<int>(ribbonY), sw, 28, {18, 24, 21, 242}, {24, 30, 26, 242});
    DrawLineEx({0.0f, ribbonY}, {static_cast<float>(sw), ribbonY}, 1.5f, {120, 112, 78, 220});

    struct HQTab { const char* label; int bldgId; BuildingType type; };
    HQTab hqTabs[5] = {
        {"[H] CORPS HQ", corpsHqId, BuildingType::CORPS_HQ},
        {"INFANTRY HQ",   infHqId,   BuildingType::INFANTRY_HQ},
        {"MECH HQ",       mechHqId,  BuildingType::MECH_HQ},
        {"ARMOR HQ",      armorHqId, BuildingType::ARMOR_HQ},
        {"AIRFIELD [F]",  airfieldId, BuildingType::AIRFIELD}
    };
    float tabX = 12.0f;
    for (int t = 0; t < 5; ++t) {
        float tw = (t == 0) ? 122.0f : (t == 4 ? 104.0f : 96.0f);
        bool exists = (hqTabs[t].bldgId != -1);
        bool isSel  = (selectedBldg && selectedBldg->id == hqTabs[t].bldgId);
        if (DrawHUDButton({tabX, ribbonY + 3.0f, tw, 22.0f}, hqTabs[t].label,
                          exists ? Color{38, 52, 44, 255} : Color{26, 30, 28, 200},
                          isSel ? GOLD : (exists ? SKYBLUE : GRAY), 10, isSel)) {
            if (exists) {
                if (isSel) {
                    cameraTarget_ = selectedBldg->pos;
                }
                selectHQById(hqTabs[t].bldgId);
            } else if (corpsHqId != -1) {
                selectHQById(corpsHqId);
                engine.setPendingBuildType(static_cast<int>(hqTabs[t].type));
            }
        }
        tabX += tw + 5.0f;
    }

    if (DrawHUDButton({tabX, ribbonY + 3.0f, 114.0f, 22.0f}, "[X] ALL FORCES",
                      {44, 62, 46, 255}, GOLD, 11, !selectedRegs.empty() && static_cast<int>(selectedRegs.size()) == pRegs)) {
        engine.selectAllPlayerCombatUnits();
    }

    // Right side of Ribbon: Interactive Control Group Tabs (GROUP 1 .. GROUP 5)
    ui.drawText("CONTROL GROUPS (1-5):", static_cast<float>(sw - 585), ribbonY + 7.0f, 11.0f, {210, 200, 155, 255}, true);
    for (int g = 1; g <= 5; ++g) {
        Rectangle gr = {static_cast<float>(sw - 442 + (g - 1) * 86), ribbonY + 3.0f, 80.0f, 22.0f};
        char gLbl[32];
        if (groupCounts[g] > 0) {
            std::snprintf(gLbl, sizeof(gLbl), "GRP %d (%d)", g, groupCounts[g]);
        } else {
            std::snprintf(gLbl, sizeof(gLbl), "GRP %d [+]", g);
        }
        bool modDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                       IsKeyDown(KEY_LEFT_SUPER)   || IsKeyDown(KEY_RIGHT_SUPER);
        if (DrawHUDButton(gr, gLbl, {36, 46, 40, 255}, groupCounts[g] > 0 ? GOLD : GRAY, 11, groupCounts[g] > 0)) {
            if (modDown || (groupCounts[g] == 0 && !selectedRegs.empty())) {
                engine.assignControlGroup(g);
                sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
            } else {
                Vector3 center{0, 0, 0};
                if (engine.selectControlGroup(g, center)) {
                    sound.playSfx(SfxId::UI_CLICK, 0.6f);
                }
            }
        }
        if (CheckCollisionPointRec(GetMousePosition(), gr) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !selectedRegs.empty()) {
            engine.assignControlGroup(g);
            sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
        }
    }

    // =========================================================================
    // 3. AUTHENTIC 2004 3-PANEL MILITARY COMMAND CONSOLE (y = sh - 168 .. sh)
    //    Left: Minimap & Map Toggles | Center: Selection Inspector & Queue | Right: Contextual Command Card
    // =========================================================================
    float barH = 168.0f;
    float barY = consoleY;
    DrawRectangleGradientV(0, static_cast<int>(barY), sw, static_cast<int>(barH), {26, 33, 29, 252}, {12, 16, 14, 255});
    DrawLineEx({0.0f, barY}, {static_cast<float>(sw), barY}, 2.5f, {178, 154, 96, 255});

    // -------------------------------------------------------------------------
    // 3A. LEFT PANEL: Tactical Minimap + Overlay/Speed Toggles
    // -------------------------------------------------------------------------
    Rectangle miniRect = {12.0f, barY + 10.0f, 186.0f, 122.0f};
    drawMinimap(engine, miniRect);

    if (DrawHUDButton({12.0f, barY + 136.0f, 90.0f, 24.0f},
                      engine.showSupplyOverlay() ? "SUPPLY: ON" : "SUPPLY: OFF",
                      {36, 54, 42, 255}, GOLD, 10, engine.showSupplyOverlay())) {
        engine.toggleSupplyOverlay();
        sound.playSfx(SfxId::UI_CLICK);
    }
    if (DrawHUDButton({108.0f, barY + 136.0f, 90.0f, 24.0f},
                      TextFormat("SPEED: %.0fx", engine.getGameSpeed()), {36, 48, 56, 255}, SKYBLUE, 10)) {
        float nextSpd = (engine.getGameSpeed() < 1.5f) ? 2.0f : (engine.getGameSpeed() < 2.5f ? 3.0f : 1.0f);
        engine.setGameSpeed(nextSpd);
        sound.playSfx(SfxId::UI_CLICK);
    }

    // -------------------------------------------------------------------------
    // 3B. CENTER PANEL: Context-Sensitive Selection Inspector & Production Queue
    // -------------------------------------------------------------------------
    Rectangle centerPanel = {210.0f, barY + 10.0f, 520.0f, 150.0f};
    DrawRectangleRec(centerPanel, {16, 21, 18, 245});
    DrawRectangleLinesEx(centerPanel, 1.5f, {118, 110, 78, 220});

    float cx = centerPanel.x + 14.0f;
    float cy = centerPanel.y + 10.0f;

    if (selectedBldg) {
        const auto& bDef = gd.getBuilding(selectedBldg->type);
        ui.drawText(TextFormat("%s  (%s)", bDef.name.c_str(), nat.name.c_str()), cx, cy, 16.0f, GOLD, true);
        ui.drawText(TextFormat("Tech Tier: %d   |   Supply Radius: %.0fm   |   Cities Held: %d/6",
                               p0.techTier, selectedBldg->supplyRadius, pCities),
                    cx, cy + 20.0f, 12.0f, LIGHTGRAY, false);

        // Pack HQ button for mobile HQs
        if (bDef.canPackMove) {
            Rectangle packBtn = {centerPanel.x + centerPanel.width - 128.0f, cy, 116.0f, 22.0f};
            if (DrawHUDButton(packBtn, "Pack HQ [P]", {74, 58, 34, 255}, GOLD, 11)) {
                engine.togglePackBuilding(selectedBldg->id);
            }
        }

        // Quick Tech Upgrade button right in Center Inspector for Corps HQ
        if (selectedBldg->type == BuildingType::CORPS_HQ && p0.techTier < 3) {
            float upCost = (p0.techTier == 1) ? 260.0f : 380.0f;
            Rectangle upQuick = {centerPanel.x + 290.0f, cy + 18.0f, 145.0f, 20.0f};
            if (DrawHUDButton(upQuick, TextFormat("Upgrade Tier %d ($%.0f) [U]", p0.techTier + 1, upCost), {44, 64, 42, 255}, GOLD, 10)) {
                engine.upgradeCorpsHQTech(0);
            }
        }

        // Building Health Bar
        float hpPct = std::clamp(selectedBldg->health / std::max(1.0f, selectedBldg->maxHealth), 0.0f, 1.0f);
        Rectangle hpBox = {cx, cy + 40.0f, 260.0f, 14.0f};
        DrawRectangleRec(hpBox, {32, 38, 34, 255});
        DrawRectangle(static_cast<int>(hpBox.x), static_cast<int>(hpBox.y),
                      static_cast<int>(hpBox.width * hpPct), static_cast<int>(hpBox.height), LIME);
        DrawRectangleLinesEx(hpBox, 1.0f, GRAY);
        ui.drawText(TextFormat("STRUCTURAL INTEGRITY: %.0f / %.0f HP", selectedBldg->health, selectedBldg->maxHealth),
                    hpBox.x + 270.0f, cy + 40.0f, 12.0f, LIME, true);

        if (selectedBldg->type == BuildingType::AIRFIELD) {
            ui.drawText("TACTICAL AIR OPERATIONS & COMBAT FLIGHT WING:", cx, cy + 56.0f, 12.0f, GOLD, true);
            int activeAirborne = 0;
            for (const auto& as : engine.getAirStrikes()) {
                if (as.teamId == 0 && !as.isShotDown) ++activeAirborne;
            }
            ui.drawText(TextFormat("AIRBORNE MISSIONS: %d active flight(s) in sector", activeAirborne),
                        cx, cy + 74.0f, 12.0f, activeAirborne > 0 ? LIME : SKYBLUE, false);
            ui.drawText("* AIR RECON [R]: Clears Fog of War along entire flight corridor and exposes target sector.",
                        cx, cy + 92.0f, 11.0f, RAYWHITE, false);
            ui.drawText("* BOMBING RUN [B]: Tactical bomber strike delivering heavy 260 HE explosive damage.",
                        cx, cy + 107.0f, 11.0f, RAYWHITE, false);
            ui.drawText("CAUTION: Enemy Quad-Flak AA Half-Tracks & 88mm Flak Batteries will shoot down your planes!",
                        cx, cy + 124.0f, 10.0f, ORANGE, true);
        } else {
            // 10-Slot Production Queue & Active Progress Bar (2 rows of 5 slots)
            ui.drawText("PRODUCTION QUEUE (10 SLOTS - RIGHT-CLICK SLOT OR UNIT TO CANCEL):",
                        cx, cy + 54.0f, 11.0f, {250, 218, 110, 255}, true);
            for (int q = 0; q < 10; ++q) {
                int col = q % 5;
                int row = q / 5;
                Rectangle slot = {cx + col * 49.0f, cy + 70.0f + row * 27.0f, 45.0f, 24.0f};
                DrawRectangleRec(slot, {26, 32, 28, 255});
                if (q < static_cast<int>(selectedBldg->prodQueue.size())) {
                    RegimentType rt = selectedBldg->prodQueue[q];
                    const Texture2D& uTex = ui.getUnitCard(rt);
                    if (uTex.id != 0) {
                        DrawTexturePro(uTex, {0, 0, static_cast<float>(uTex.width), static_cast<float>(uTex.height)},
                                       slot, {0, 0}, 0.0f, WHITE);
                    }
                    bool hovered = CheckCollisionPointRec(GetMousePosition(), slot);
                    if (hovered) {
                        DrawRectangleRec(slot, {220, 40, 40, 110});
                        DrawRectangleLinesEx(slot, 1.5f, RED);
                        ui.drawText("X", slot.x + slot.width - 10.0f, slot.y + 2.0f, 10.0f, RED, true);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            engine.cancelProductionQueueSlot(selectedBldg->id, q);
                        }
                    } else {
                        DrawRectangleLinesEx(slot, 1.5f, q == 0 ? GOLD : SKYBLUE);
                    }
                } else {
                    DrawRectangleLinesEx(slot, 1.0f, {65, 72, 65, 255});
                    ui.drawText(TextFormat("%d", q + 1), slot.x + 19.0f, slot.y + 6.0f, 10.0f, {80, 90, 80, 255}, false);
                }
            }

            if (!selectedBldg->prodQueue.empty()) {
                float pct = std::clamp(selectedBldg->prodProgress / std::max(0.1f, selectedBldg->prodTotalTime), 0.0f, 1.0f);
                Rectangle pBar = {cx + 252.0f, cy + 70.0f, 238.0f, 22.0f};
                DrawRectangleRec(pBar, {24, 30, 26, 255});
                DrawRectangle(static_cast<int>(pBar.x), static_cast<int>(pBar.y),
                              static_cast<int>(pBar.width * pct), static_cast<int>(pBar.height), {55, 160, 75, 255});
                DrawRectangleLinesEx(pBar, 1.2f, GOLD);
                ui.drawTextCentered(TextFormat("TRAINING: %.0f%%", pct * 100.0f), pBar, 11.0f, RAYWHITE, true);
                RegimentType activeRt = selectedBldg->prodQueue.front();
                ui.drawText(TextFormat("ACTIVE: %s", gd.getRegimentDisplayName(selectedBldg->nation, activeRt).c_str()),
                            cx + 252.0f, cy + 96.0f, 11.0f, GOLD, true);
                ui.drawText(TextFormat("Queue: %zu/10  |  Right-click cancels", selectedBldg->prodQueue.size()),
                            cx + 252.0f, cy + 110.0f, 10.0f, LIGHTGRAY, false);
            } else {
                ui.drawText("Queue Idle (0/10 Slots)", cx + 252.0f, cy + 72.0f, 12.0f, GOLD, true);
                ui.drawText("Click cards on right to train units.", cx + 252.0f, cy + 90.0f, 11.0f, LIGHTGRAY, false);
                ui.drawText("Right-click card/slot to cancel & refund.", cx + 252.0f, cy + 107.0f, 10.0f, SKYBLUE, false);
            }

            ui.drawText("TIP: Right-Click unit card or slot to cancel & refund  |  Right-Click battlefield to rally  |  [P] Pack HQ",
                        cx, cy + 128.0f, 10.0f, SKYBLUE, false);
        }
    } else if (selectedRegs.size() == 1) {
        const RTSRegiment* r = selectedRegs[0];
        const auto& rDef = gd.getRegiment(r->type);
        std::string dispName = gd.getRegimentDisplayName(r->nation, r->type);

        // Large Unit Silhouette Card on Left of Inspector
        Rectangle cardBox = {cx, cy + 4.0f, 108.0f, 68.0f};
        const Texture2D& uTex = ui.getUnitCard(r->type);
        if (uTex.id != 0) {
            DrawTexturePro(uTex, {0, 0, static_cast<float>(uTex.width), static_cast<float>(uTex.height)},
                           cardBox, {0, 0}, 0.0f, WHITE);
        }
        DrawRectangleLinesEx(cardBox, 2.0f, GOLD);

        float ix = cx + 122.0f;
        ui.drawText(dispName.c_str(), ix, cy, 15.0f, GOLD, true);
        float dnw = ui.measureWidth(dispName.c_str(), 15.0f, true);

        // Veterancy Rank & Title
        const char* rankName = (r->veterancy == 1) ? "VETERAN (*)" :
                               (r->veterancy == 2) ? "ELITE (**)" :
                               (r->veterancy >= 3 ? "HEROIC (***)" : "RECRUIT");
        Color rankCol = (r->veterancy == 1) ? Color{120, 215, 255, 255} :
                        (r->veterancy == 2) ? Color{255, 210, 60, 255} :
                        (r->veterancy >= 3 ? Color{255, 140, 40, 255} : LIGHTGRAY);
        ui.drawText(TextFormat("[%s]", rankName), ix + dnw + 8.0f, cy, 12.0f, rankCol, true);

        const char* stanceStr = (r->stance == FormationStance::LINE) ? "LINE FORMATION" :
                                (r->stance == FormationStance::COLUMN ? "ROAD COLUMN (+25% SPD)" : "ENTRENCHED (+35% DEF)");
        const char* hospitalStr = engine.isWaterAt(r->pos.x, r->pos.z) ? "WADING RIVER: -72% SPEED & MORALE DRAIN" :
                                  (r->atTownHealingStation ? "TOWN HOSPITAL: HEALING CASUALTIES" :
                                  (r->health < r->maxHealth ? "NO HOSPITAL (CONTROLLED TOWN REQUIRED TO HEAL)" : "COMBAT READY"));
        Color hospCol = engine.isWaterAt(r->pos.x, r->pos.z) ? SKYBLUE :
                        (r->atTownHealingStation ? LIME : (r->health < r->maxHealth ? ORANGE : LIGHTGRAY));
        std::string supplyStr;
        Color supplyCol = LIME;
        if (r->supplySourceId != -1) {
            supplyStr = "SUPPLY: SECURED (BASE/DEPOT)";
            supplyCol = LIME;
        } else if (r->supplyBuffer > 0.0f) {
            supplyStr = TextFormat("SUPPLY: EXPEDITION RATIONS (%.0fs)", r->supplyBuffer);
            supplyCol = (r->supplyBuffer > 20.0f) ? Color{255, 230, 110, 255} : ORANGE;
        } else {
            supplyStr = "SUPPLY: SEVERED (OUT OF SUPPLY)";
            supplyCol = RED;
        }

        ui.drawText(TextFormat("%s   |   %s   |   %s",
                               stanceStr,
                               supplyStr.c_str(),
                               r->controlGroup > 0 ? TextFormat("GROUP #%d", r->controlGroup) : "UNGROUPED"),
                    ix, cy + 16.0f, 10.0f, supplyCol, true);
        ui.drawText(TextFormat("STATUS: %s", hospitalStr), ix, cy + 28.0f, 10.0f, hospCol, true);

        // Regiment Health & Organization Bars
        float hpPct  = std::clamp(r->health / std::max(1.0f, r->maxHealth), 0.0f, 1.0f);
        float orgPct = std::clamp(r->organization / std::max(1.0f, r->maxOrganization), 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 41), 210, 9, {32, 36, 34, 255});
        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 41), static_cast<int>(210 * hpPct), 9, LIME);
        ui.drawText(TextFormat("HP: %.0f/%.0f", r->health, r->maxHealth), ix + 218.0f, cy + 40.0f, 10.0f, RAYWHITE, false);

        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 53), 210, 9, {32, 36, 34, 255});
        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 53), static_cast<int>(210 * orgPct), 9, SKYBLUE);
        ui.drawText(TextFormat("MORALE/ORG: %.0f%%", orgPct * 100.0f), ix + 218.0f, cy + 52.0f, 10.0f, SKYBLUE, false);

        // Experience & Veterancy Progression Bar
        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 65), 210, 8, {32, 36, 34, 255});
        float xpPct = (r->veterancy >= 3) ? 1.0f : std::clamp(r->xp / 100.0f, 0.0f, 1.0f);
        Color xpCol = (r->veterancy >= 3) ? Color{255, 150, 40, 255} : Color{235, 195, 50, 255};
        DrawRectangle(static_cast<int>(ix), static_cast<int>(cy + 65), static_cast<int>(210 * xpPct), 8, xpCol);
        if (r->veterancy >= 3) {
            ui.drawText("XP: MAX RANK (+45% ATK, +42% DEF)", ix + 218.0f, cy + 64.0f, 10.0f, xpCol, true);
        } else {
            ui.drawText(TextFormat("XP: %.0f/100 (+%d%% ATK, +%d%% DEF)", r->xp, r->veterancy * 15, r->veterancy * 14),
                        ix + 218.0f, cy + 64.0f, 10.0f, xpCol, true);
        }

        // Sub-Unit Squad Status Indicators
        ui.drawText("REGIMENT ELEMENTS:", cx, cy + 84.0f, 11.0f, {250, 218, 110, 255}, true);
        for (size_t s = 0; s < r->subUnits.size(); ++s) {
            Rectangle sb = {cx + 138.0f + s * 36.0f, cy + 81.0f, 30.0f, 18.0f};
            DrawRectangleRec(sb, r->subUnits[s].alive ? Color{52, 148, 68, 255} : Color{68, 32, 32, 255});
            DrawRectangleLinesEx(sb, 1.0f, r->subUnits[s].alive ? LIME : RED);
        }

        if (r->type == RegimentType::PACKED_HQ_TRUCK) {
            if (DrawHUDButton({cx, cy + 106.0f, 360.0f, 30.0f}, "UNPACK CONVOY TRUCK INTO HEADQUARTERS HERE [P]",
                              {42, 86, 48, 255}, LIME, 12, true)) {
                engine.unpackTruckRegiment(r->id);
            }
        } else {
            float atkBonus = 1.0f + r->veterancy * 0.15f;
            float defBonus = r->veterancy * 14.0f;
            ui.drawText(TextFormat("Firepower: Inf %.0f (x%.2f) | Arm %.0f | Def Res: +%.0f%% | Rng: %.0fm",
                                   rDef.dmgVsInfantry * atkBonus, atkBonus, rDef.dmgVsVehicle * atkBonus, defBonus, rDef.attackRange),
                        cx, cy + 114.0f, 10.0f, LIGHTGRAY, false);
        }
    } else if (selectedRegs.size() > 1) {
        ui.drawText(TextFormat("SELECTED TASK FORCE (%d REGIMENTS)  --  CLICK OR RIGHT-CLICK MAP TO ADVANCE:",
                               static_cast<int>(selectedRegs.size())),
                    cx, cy, 14.0f, GOLD, true);

        // Multi-Unit Selection Grid (Up to 12 clickable Regiment Cards with Health & Org bars)
        int showCount = std::min(12, static_cast<int>(selectedRegs.size()));
        for (int i = 0; i < showCount; ++i) {
            const RTSRegiment* r = selectedRegs[i];
            int row = i / 6;
            int col = i % 6;
            Rectangle uBox = {cx + col * 82.0f, cy + 24.0f + row * 56.0f, 76.0f, 50.0f};
            const Texture2D& uTex = ui.getUnitCard(r->type);
            DrawRectangleRec(uBox, {26, 34, 30, 255});
            if (uTex.id != 0) {
                DrawTexturePro(uTex, {0, 0, static_cast<float>(uTex.width), static_cast<float>(uTex.height)},
                               {uBox.x, uBox.y, uBox.width, 38.0f}, {0, 0}, 0.0f, WHITE);
            }
            float hpPct  = std::clamp(r->health / std::max(1.0f, r->maxHealth), 0.0f, 1.0f);
            float orgPct = std::clamp(r->organization / std::max(1.0f, r->maxOrganization), 0.0f, 1.0f);
            DrawRectangle(static_cast<int>(uBox.x + 2), static_cast<int>(uBox.y + 39), static_cast<int>((uBox.width - 4) * hpPct), 5, LIME);
            DrawRectangle(static_cast<int>(uBox.x + 2), static_cast<int>(uBox.y + 44), static_cast<int>((uBox.width - 4) * orgPct), 4, SKYBLUE);
            DrawRectangleLinesEx(uBox, 1.5f, GOLD);

            if (r->veterancy > 0) {
                const char* stars = (r->veterancy == 1) ? "[*]" : (r->veterancy == 2 ? "[**]" : "[***]");
                float sw_w = ui.measureWidth(stars, 9.0f, true);
                DrawRectangle(static_cast<int>(uBox.x + uBox.width - sw_w - 6), static_cast<int>(uBox.y + 2), static_cast<int>(sw_w + 4), 12, {0, 0, 0, 200});
                ui.drawText(stars, uBox.x + uBox.width - sw_w - 4, uBox.y + 2.0f, 9.0f, GOLD, true);
            }

            if (r->controlGroup > 0) {
                DrawRectangle(static_cast<int>(uBox.x + 2), static_cast<int>(uBox.y + 2), 14, 13, GOLD);
                ui.drawText(TextFormat("%d", r->controlGroup), uBox.x + 5.0f, uBox.y + 2.0f, 10.0f, BLACK, true, false);
            }

            if (CheckCollisionPointRec(GetMousePosition(), uBox) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int focusId = r->id;
                for (auto& reg : engine.getRegimentsMut()) reg.selected = (reg.id == focusId);
                sound.playSfx(SfxId::UI_CLICK, 0.6f);
            }
        }
    }

    // -------------------------------------------------------------------------
    // 3C. RIGHT PANEL: Authentic 2004 Context-Sensitive Command Card (4x2 Grid of Large Action Buttons!)
    // -------------------------------------------------------------------------
    Rectangle rightPanel = {742.0f, barY + 10.0f, static_cast<float>(sw - 754), 150.0f};
    DrawRectangleRec(rightPanel, {16, 21, 18, 245});
    DrawRectangleLinesEx(rightPanel, 1.5f, {118, 110, 78, 220});

    float rx = rightPanel.x + 12.0f;
    float ry = rightPanel.y + 8.0f;
    float btnW = (rightPanel.width - 42.0f) * 0.25f;
    float btnH = 48.0f;

    auto autoBuildOrPlace = [&](BuildingType bt) {
        bool alreadyPending = (engine.pendingBuildType() == static_cast<int>(bt));
        if (alreadyPending) {
            Vector3 basePos = {0.0f, 0.0f, 460.0f};
            int friendlyBldgs = 0;
            for (const auto& b : engine.getBuildings()) {
                if (b.teamId == 0) {
                    ++friendlyBldgs;
                    if (b.type == BuildingType::CORPS_HQ) basePos = b.pos;
                }
            }
            float angle = friendlyBldgs * 1.15f;
            Vector3 autoSpot = {
                std::clamp(basePos.x + std::cos(angle) * 26.0f, -520.0f, 520.0f),
                0.0f,
                std::clamp(basePos.z - 16.0f + std::sin(angle) * 18.0f, -520.0f, 520.0f)
            };
            if (engine.constructSupportBuilding(bt, autoSpot, 0)) {
                for (auto& r : engine.getRegimentsMut()) r.selected = false;
                for (auto& b : engine.getBuildingsMut()) b.selected = false;
                if (!engine.getBuildingsMut().empty()) {
                    engine.getBuildingsMut().back().selected = true;
                    cameraTarget_ = engine.getBuildingsMut().back().pos;
                }
                engine.setPendingBuildType(-1);
            }
        } else {
            engine.setPendingBuildType(static_cast<int>(bt));
            sound.playSfx(SfxId::UI_CLICK);
        }
    };

    auto queueAtHQ = [&](RegimentType rt) {
        const auto& rDef = gd.getRegiment(rt);
        int targetBldgId = -1;
        if (selectedBldg && (selectedBldg->type == rDef.producedBy || selectedBldg->type == BuildingType::CORPS_HQ ||
                             (selectedBldg->type == BuildingType::AIRFIELD && rt == RegimentType::PARATROOP_REGIMENT))) {
            targetBldgId = selectedBldg->id;
        }
        if (targetBldgId == -1) {
            for (const auto& b : engine.getBuildings()) {
                if (b.teamId == 0 && (b.type == rDef.producedBy ||
                                      (b.type == BuildingType::AIRFIELD && rt == RegimentType::PARATROOP_REGIMENT))) {
                    targetBldgId = b.id;
                    break;
                }
            }
        }
        if (targetBldgId == -1 && corpsHqId != -1) {
            targetBldgId = corpsHqId;
        }
        if (targetBldgId != -1) {
            engine.queueUnitProduction(targetBldgId, rt);
        }
    };

    auto cancelAtHQ = [&](RegimentType rt) {
        const auto& rDef = gd.getRegiment(rt);
        int targetBldgId = -1;
        if (selectedBldg && (selectedBldg->type == rDef.producedBy || selectedBldg->type == BuildingType::CORPS_HQ ||
                             (selectedBldg->type == BuildingType::AIRFIELD && rt == RegimentType::PARATROOP_REGIMENT))) {
            targetBldgId = selectedBldg->id;
        }
        if (targetBldgId == -1) {
            for (const auto& b : engine.getBuildings()) {
                if (b.teamId == 0 && (b.type == rDef.producedBy ||
                                      (b.type == BuildingType::AIRFIELD && rt == RegimentType::PARATROOP_REGIMENT))) {
                    targetBldgId = b.id;
                    break;
                }
            }
        }
        if (targetBldgId == -1 && corpsHqId != -1) {
            targetBldgId = corpsHqId;
        }
        if (targetBldgId != -1) {
            engine.cancelUnitTypeProduction(targetBldgId, rt);
        }
    };

    if (selectedBldg && selectedBldg->isUnderConstruction) {
        // UNDER CONSTRUCTION COMMAND CARD: Progress bar, time remaining, cancel & refund
        float remTime = (1.0f - selectedBldg->constructProgress) * selectedBldg->constructTotalTime;
        ui.drawText(TextFormat("%s  --  UNDER CONSTRUCTION: %d%% (%.1fs REMAINING)",
                               gd.getBuilding(selectedBldg->type).name.c_str(),
                               static_cast<int>(selectedBldg->constructProgress * 100.0f),
                               std::max(0.0f, remTime)),
                    rx, ry, 12.0f, GOLD, true);

        // Large high-contrast progress bar across HUD
        float barW = 4 * (btnW + 6.0f) - 6.0f;
        float barH = 26.0f;
        Rectangle pBar = {rx, ry + 24.0f, barW, barH};
        DrawRectangleRec(pBar, {24, 28, 26, 255});
        DrawRectangleLinesEx(pBar, 1.5f, {80, 75, 55, 255});

        float fillW = barW * std::clamp(selectedBldg->constructProgress, 0.0f, 1.0f);
        if (fillW > 0.0f) {
            DrawRectangleGradientH(static_cast<int>(pBar.x), static_cast<int>(pBar.y),
                                   static_cast<int>(fillW), static_cast<int>(barH),
                                   {180, 140, 40, 255}, {240, 190, 60, 255});
        }
        ui.drawTextCentered(TextFormat("CREWS ERECTING STRUCTURE  --  %d%% (%.1fs)",
                                       static_cast<int>(selectedBldg->constructProgress * 100.0f),
                                       std::max(0.0f, remTime)),
                            pBar, 11.0f, WHITE, true);

        // Cancel button: 100% Refund
        Rectangle cancelBtn = {rx, ry + 24.0f + barH + 10.0f, btnW * 1.5f, btnH};
        if (DrawHUDButton(cancelBtn, "Cancel & 100% Refund", {68, 36, 36, 255}, RED, 11)) {
            engine.cancelConstruction(selectedBldg->id);
            selectedBldg = nullptr;
        }

        // Return to Corps HQ
        Rectangle hqBtn = {rx + cancelBtn.width + 12.0f, ry + 24.0f + barH + 10.0f, btnW * 1.2f, btnH};
        if (DrawHUDButton(hqBtn, "Corps HQ [H]", {38, 52, 62, 255}, SKYBLUE, 11)) {
            selectHQById(corpsHqId);
        }
    } else if (selectedBldg && selectedBldg->type == BuildingType::SUPPLY_DEPOT) {
        // DEDICATED SUPPLY DEPOT / LOGISTICS HUB COMMAND CARD
        ui.drawText("FORWARD SUPPLY DEPOT  --  LOGISTICS, RESOURCE EXTRACTION & REPAIR HUB", rx, ry, 12.0f, GOLD, true);

        float cardW = 4 * (btnW + 6.0f) - 6.0f;
        Rectangle statBox = {rx, ry + 22.0f, cardW, 36.0f};
        DrawRectangleRec(statBox, {22, 28, 25, 230});
        DrawRectangleLinesEx(statBox, 1.2f, {75, 110, 85, 255});

        ui.drawText("SUPPLY REACH: 160m Logistics Projection  |  STATUS: ONLINE & CONNECTED TO CORPS HQ", rx + 10.0f, ry + 25.0f, 10.5f, LIME, true);
        ui.drawText("RESOURCE YIELD: +$12 Money/s   +$20 Ammo/s   +$16 Oil/s   |  Maintains Unit Org (+9.0/s) & Halts Starvation", rx + 10.0f, ry + 40.0f, 10.5f, RAYWHITE, false);

        // Action Buttons
        Rectangle b0 = {rx + 0 * (btnW + 6.0f), ry + 64.0f, btnW, btnH};
        Rectangle b1 = {rx + 1 * (btnW + 6.0f), ry + 64.0f, btnW, btnH};
        Rectangle b2 = {rx + 2 * (btnW + 6.0f), ry + 64.0f, btnW, btnH};

        if (DrawHUDButton(b0, "Pack Convoy [P]", {58, 48, 32, 255}, GOLD, 11)) {
            engine.togglePackBuilding(selectedBldg->id);
        }
        if (DrawHUDButton(b1, "Supply Overlay [Tab]", {38, 58, 48, 255}, LIME, 10, engine.showSupplyOverlay())) {
            engine.toggleSupplyOverlay();
        }
        if (DrawHUDButton(b2, "Corps HQ [H]", {42, 56, 64, 255}, SKYBLUE, 11)) {
            selectHQById(corpsHqId);
        }
    } else if (selectedBldg && (selectedBldg->type == BuildingType::INFANTRY_HQ ||
                         selectedBldg->type == BuildingType::MECH_HQ ||
                         selectedBldg->type == BuildingType::ARMOR_HQ)) {
        // DIVISION HQ COMMAND CARD: Train Specialized Regiments + Pack HQ
        ui.drawText(TextFormat("%s COMMAND CARD  --  LEFT-CLICK TO MUSTER | RIGHT-CLICK TO CANCEL:",
                               gd.getBuilding(selectedBldg->type).name.c_str()),
                    rx, ry, 12.0f, GOLD, true);

        struct DivItem { RegimentType type; const char* label; };
        std::vector<DivItem> items;
        if (selectedBldg->type == BuildingType::INFANTRY_HQ) {
            items = {
                {RegimentType::RIFLE_REGIMENT,          "Rifle Regt ($75)"},
                {RegimentType::MG_REGIMENT,             "MG Regt ($95)"},
                {RegimentType::MORTAR_REGIMENT,         "Mortar Regt ($110)"},
                {RegimentType::AT_BAZOOKA_REGIMENT,     "Bazooka AT ($115)"},
                {RegimentType::ENGINEER_FLAME_REGIMENT, "Engineers ($125)"},
                {RegimentType::SNIPER_REGIMENT,         "Snipers ($140 T2)"},
                {RegimentType::PARATROOP_REGIMENT,      "Paratroops ($165 T2)"}
            };
        } else if (selectedBldg->type == BuildingType::MECH_HQ) {
            items = {
                {RegimentType::RECON_HALFTRACK,  "Half-Track ($140)"},
                {RegimentType::AA_HALFTRACK,     "AA Half-Track ($165)"},
                {RegimentType::MOBILE_ARTILLERY, "Artillery ($220 T2)"},
                {RegimentType::KATYUSHA_ROCKET,  "Katyusha ($250 T2)"}
            };
        } else {
            items = {
                {RegimentType::LIGHT_TANK,           "Light Tank ($180)"},
                {RegimentType::MEDIUM_TANK,          "Med Tank ($250 T2)"},
                {RegimentType::TANK_DESTROYER,       "Tank Destr ($265 T2)"},
                {RegimentType::HEAVY_TANK,           "Heavy Tank ($340 T3)"},
                {RegimentType::ELITE_SIGNATURE_TANK, "Elite Tank ($430 T3)"}
            };
        }

        for (size_t i = 0; i < items.size() && i < 7; ++i) {
            int row = static_cast<int>(i) / 4;
            int col = static_cast<int>(i) % 4;
            Rectangle br = {rx + col * (btnW + 6.0f), ry + 24.0f + row * (btnH + 8.0f), btnW, btnH};
            const Texture2D* uIcon = &ui.getUnitCard(items[i].type);
            bool rightClicked = CheckCollisionPointRec(GetMousePosition(), br) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
            if (DrawHUDButton(br, items[i].label, {34, 48, 52, 255}, SKYBLUE, 11, false, uIcon)) {
                queueAtHQ(items[i].type);
            } else if (rightClicked) {
                cancelAtHQ(items[i].type);
            }
        }

        // Slot 8 (Bottom-Right): Upgrade Tech Tier [U] (or Pack HQ if max tier)
        Rectangle slot8Rect = {rx + 3 * (btnW + 6.0f), ry + 24.0f + (btnH + 8.0f), btnW, btnH};
        if (p0.techTier < 3) {
            float upCost = (p0.techTier == 1) ? 260.0f : 380.0f;
            const char* upLbl = TextFormat("Upgrade Tier %d ($%.0f) [U]", p0.techTier + 1, upCost);
            if (DrawHUDButton(slot8Rect, upLbl, {46, 66, 48, 255}, GOLD, 11)) {
                engine.upgradeCorpsHQTech(0);
            }
        } else {
            if (DrawHUDButton(slot8Rect, "Pack HQ Truck [P]", {74, 58, 34, 255}, GOLD, 11)) {
                engine.togglePackBuilding(selectedBldg->id);
            }
        }
    } else if (selectedBldg && selectedBldg->type == BuildingType::AIRFIELD) {
        // FORWARD AIRFIELD COMMAND CARD: Aerial Recon, Fighter CAP, Bombing Runs, Airborne Drops, Emergency Supply
        int opAirfields = engine.getOperationalAirfieldCount(0);
        int activeSorties = engine.getActiveAirSortiesCount(0);
        int maxSorties = engine.getMaxAirSorties(0);
        ui.drawText(TextFormat("FORWARD AIRFIELD -- AIR WING: %d/%d SORTIES (%d AIRFIELD%s) | [E] RECON [J] CAP [B] BOMB [P] PARA [R] SUPPLY",
                               activeSorties, maxSorties, opAirfields, opAirfields == 1 ? "" : "S"),
                    rx, ry, 11.5f, (activeSorties >= maxSorties) ? ORANGE : GOLD, true);

        Rectangle b0 = {rx + 0 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b1 = {rx + 1 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b2 = {rx + 2 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b3 = {rx + 3 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b4 = {rx + 0 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b5 = {rx + 1 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b6 = {rx + 2 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b7 = {rx + 3 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};

        bool capFull = (activeSorties >= maxSorties);
        bool reconActive = (engine.pendingAirMissionType() == 1);
        const char* reconLbl = capFull ? "Recon (Cap Full)" : "Air Recon [E] ($75)";
        if (DrawHUDButton(b0, reconLbl, {38, 54, 72, 255}, capFull ? GRAY : (reconActive ? GOLD : SKYBLUE), 11, reconActive)) {
            if (capFull && !reconActive) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Construct another Airfield to expand flight capacity.", activeSorties, maxSorties));
            } else {
                engine.setPendingAirMissionType(reconActive ? -1 : 1);
                sound.playSfx(SfxId::UI_CLICK);
            }
        }

        bool ftrActive = (engine.pendingAirMissionType() == 5);
        const char* ftrLbl = capFull ? "CAP (Cap Full)" : "Fighter CAP [J] ($140)";
        if (DrawHUDButton(b1, ftrLbl, {40, 62, 82, 255}, capFull ? GRAY : (ftrActive ? GOLD : SKYBLUE), 11, ftrActive)) {
            if (capFull && !ftrActive) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Construct another Airfield to expand flight capacity.", activeSorties, maxSorties));
            } else {
                engine.setPendingAirMissionType(ftrActive ? -1 : 5);
                sound.playSfx(SfxId::UI_CLICK);
            }
        }

        bool bombActive = (engine.pendingAirMissionType() == 2);
        const char* bombLbl = capFull ? "Bombing (Cap Full)" : "Bombing Run [B] ($165)";
        if (DrawHUDButton(b2, bombLbl, {72, 38, 38, 255}, capFull ? GRAY : (bombActive ? GOLD : ORANGE), 11, bombActive)) {
            if (capFull && !bombActive) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Construct another Airfield to expand flight capacity.", activeSorties, maxSorties));
            } else {
                engine.setPendingAirMissionType(bombActive ? -1 : 2);
                sound.playSfx(SfxId::UI_CLICK);
            }
        }

        bool paraActive = (engine.pendingAirMissionType() == 3);
        const char* paraLbl = capFull ? "Airborne (Cap Full)" : "Airborne Drop [P] ($180)";
        if (DrawHUDButton(b3, paraLbl, {38, 62, 44, 255}, capFull ? GRAY : (paraActive ? GOLD : LIME), 11, paraActive)) {
            if (capFull && !paraActive) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Construct another Airfield to expand flight capacity.", activeSorties, maxSorties));
            } else {
                engine.setPendingAirMissionType(paraActive ? -1 : 3);
                sound.playSfx(SfxId::UI_CLICK);
            }
        }

        bool supplyActive = (engine.pendingAirMissionType() == 4);
        const char* supplyLbl = capFull ? "Supply (Cap Full)" : "Supply Drop [R] ($120)";
        if (DrawHUDButton(b4, supplyLbl, {62, 54, 34, 255}, capFull ? GRAY : (supplyActive ? GOLD : GOLD), 11, supplyActive)) {
            if (capFull && !supplyActive) {
                engine.pushLog(TextFormat("AIRFIELD CAPACITY REACHED (%d/%d sorties airborne): Construct another Airfield to expand flight capacity.", activeSorties, maxSorties));
            } else {
                engine.setPendingAirMissionType(supplyActive ? -1 : 4);
                sound.playSfx(SfxId::UI_CLICK);
            }
        }

        // Slot 5: Train Airborne Paratrooper Squad directly on runway!
        const Texture2D* pIcon = &ui.getUnitCard(RegimentType::PARATROOP_REGIMENT);
        bool rightClickedP = CheckCollisionPointRec(GetMousePosition(), b5) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        if (DrawHUDButton(b5, "Muster Paratroops [T] ($165)", {38, 58, 48, 255}, LIME, 10, false, pIcon)) {
            queueAtHQ(RegimentType::PARATROOP_REGIMENT);
        } else if (rightClickedP) {
            cancelAtHQ(RegimentType::PARATROOP_REGIMENT);
        }

        if (IsKeyPressed(KEY_T)) {
            queueAtHQ(RegimentType::PARATROOP_REGIMENT);
        }

        // Slot 6: Airfield Repair & Fortify Runway
        if (DrawHUDButton(b6, "Field Repair ($60)", {44, 56, 52, 255}, SKYBLUE, 11)) {
            if (p0.money >= 60.0f) {
                engine.getPlayerMut(0).money -= 60.0f;
                for (auto& b : engine.getBuildingsMut()) {
                    if (b.id == selectedBldg->id) {
                        b.health = std::min(b.maxHealth, b.health + 350.0f);
                        break;
                    }
                }
                engine.pushLog("AIRFIELD REPAIRED: Maintenance crews repaired +350 HP.");
                sound.playSfx(SfxId::UI_CONFIRM, 0.7f);
            }
        }

        // Slot 7: Base Build [H]
        if (DrawHUDButton(b7, "Base Build [H]", {42, 56, 64, 255}, SKYBLUE, 12)) {
            selectHQById(corpsHqId);
        }
    } else if (!selectedRegs.empty()) {
        // COMBAT REGIMENTS COMMAND CARD: Stances, Abilities, Orders & Quick-Return to Corps HQ
        ui.drawText("TACTICAL REGIMENT ORDERS  --  CLICK OR RIGHT-CLICK MAP TO MOVE / ATTACK:", rx, ry, 13.0f, GOLD, true);
        FormationStance activeStance = engine.getSelectedFormationStance();

        Rectangle b0 = {rx + 0 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b1 = {rx + 1 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b2 = {rx + 2 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b3 = {rx + 3 * (btnW + 6.0f), ry + 24.0f, btnW, btnH};
        Rectangle b4 = {rx + 0 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b5 = {rx + 1 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b6 = {rx + 2 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};
        Rectangle b7 = {rx + 3 * (btnW + 6.0f), ry + 24.0f + btnH + 8.0f, btnW, btnH};

        if (DrawHUDButton(b0, "Attack-Move [M]", {46, 56, 70, 255}, SKYBLUE, 12, attackMoveMode_)) {
            attackMoveMode_ = !attackMoveMode_;
            sound.playSfx(SfxId::UI_CLICK);
        }
        if (DrawHUDButton(b1, "Halt / Hold", {62, 42, 42, 255}, ORANGE, 12)) {
            engine.stopSelectedUnits();
            sound.playSfx(SfxId::UI_CLICK);
        }
        if (DrawHUDButton(b2, "Line Stance [Z]", {36, 54, 42, 255}, LIME, 12, activeStance == FormationStance::LINE)) {
            engine.setSelectedFormationStance(FormationStance::LINE);
        }
        if (DrawHUDButton(b3, "Road Column [V]", {36, 48, 58, 255}, SKYBLUE, 12, activeStance == FormationStance::COLUMN)) {
            engine.setSelectedFormationStance(FormationStance::COLUMN);
        }
        if (DrawHUDButton(b4, "Entrench +35% [C]", {56, 50, 34, 255}, GOLD, 12, activeStance == FormationStance::ENTRENCHED)) {
            engine.setSelectedFormationStance(FormationStance::ENTRENCHED);
        }

        bool hasArmor = false;
        bool hasInfOrArt = false;
        for (const auto* r : selectedRegs) {
            if (r->type == RegimentType::LIGHT_TANK || r->type == RegimentType::MEDIUM_TANK ||
                r->type == RegimentType::HEAVY_TANK || r->type == RegimentType::TANK_DESTROYER ||
                r->type == RegimentType::ELITE_SIGNATURE_TANK) {
                hasArmor = true;
            } else if (r->type == RegimentType::RIFLE_REGIMENT || r->type == RegimentType::MG_REGIMENT ||
                       r->type == RegimentType::SNIPER_REGIMENT || r->type == RegimentType::PARATROOP_REGIMENT ||
                       r->type == RegimentType::MORTAR_REGIMENT || r->type == RegimentType::MOBILE_ARTILLERY ||
                       r->type == RegimentType::KATYUSHA_ROCKET) {
                hasInfOrArt = true;
            }
        }

        const char* b5Text = hasArmor ? "Smoke Screen [K]" : "Set Group 1 [1]";
        Color b5Color = hasArmor ? Color{180, 220, 240, 255} : GOLD;
        if (DrawHUDButton(b5, b5Text, {42, 52, 60, 255}, b5Color, 12)) {
            if (hasArmor) {
                bool fired = false;
                for (const auto* r : selectedRegs) {
                    if (engine.triggerRegimentAbility(r->id, 0)) fired = true;
                }
                if (fired) sound.playSfx(SfxId::MORTAR_THUMP, 0.7f);
            } else {
                engine.assignControlGroup(1);
                sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
            }
        }

        const char* b6Text = hasInfOrArt ? "Rapid Volley [L]" : "Set Group 2 [2]";
        Color b6Color = hasInfOrArt ? Color{255, 185, 75, 255} : GOLD;
        if (DrawHUDButton(b6, b6Text, {56, 46, 36, 255}, b6Color, 12)) {
            if (hasInfOrArt) {
                bool fired = false;
                for (const auto* r : selectedRegs) {
                    if (engine.triggerRegimentAbility(r->id, 1)) fired = true;
                }
                if (fired) sound.playSfx(SfxId::RIFLE_VOLLEY, 0.8f);
            } else {
                engine.assignControlGroup(2);
                sound.playSfx(SfxId::UI_CONFIRM, 0.65f);
            }
        }

        if (DrawHUDButton(b7, "Base Build [H]", {42, 56, 64, 255}, SKYBLUE, 12)) {
            selectHQById(corpsHqId);
        }
    } else {
        bool hasPlayerHQ = (corpsHqId != -1);
        if (!hasPlayerHQ) {
            for (const auto& r : engine.getRegiments()) {
                if (r.teamId == 0 && r.type == RegimentType::PACKED_HQ_TRUCK && r.health > 0.0f) {
                    hasPlayerHQ = true;
                    break;
                }
            }
        }

        // CORPS HQ / BASE CONSTRUCTION COMMAND CARD (Default when Corps HQ or Support Building is selected)
        if (hasPlayerHQ) {
            ui.drawText("CORPS HQ CONSTRUCTION & TECH  --  CLICK TO PLACE ON MAP (OR 2x-CLICK AUTO-BUILD):", rx, ry, 12.0f, GOLD, true);
        } else {
            ui.drawText("CORPS HQ DESTROYED  --  BASE CONSTRUCTION & TECH SUSPENDED (DEPLOY MOBILE HQ):", rx, ry, 12.0f, RED, true);
        }

        struct BuildBtnInfo { BuildingType type; const char* label; };
        BuildBtnInfo buildBtns[7] = {
            {BuildingType::INFANTRY_HQ,  "Infantry HQ ($180)"},
            {BuildingType::MECH_HQ,      "Mech HQ ($240)"},
            {BuildingType::ARMOR_HQ,     "Armor HQ ($320 T2)"},
            {BuildingType::AIRFIELD,     "Airfield ($220)"},
            {BuildingType::SUPPLY_DEPOT, "Supply Depot ($130) [+$12M/+20A/+16O]"},
            {BuildingType::BUNKER,       "Pillbox Bunker ($140)"},
            {BuildingType::AA_BATTERY,   "88mm Flak/AT ($170)"}
        };

        for (int i = 0; i < 7; ++i) {
            int row = i / 4;
            int col = i % 4;
            Rectangle br = {rx + col * (btnW + 6.0f), ry + 24.0f + row * (btnH + 8.0f), btnW, btnH};
            bool active = (engine.pendingBuildType() == static_cast<int>(buildBtns[i].type));
            Color btnBorder = !hasPlayerHQ ? Color{160, 60, 60, 255} : (active ? GOLD : Color{140, 155, 125, 255});
            Color btnBg = !hasPlayerHQ ? Color{32, 22, 22, 255} : Color{36, 50, 42, 255};
            if (DrawHUDButton(br, buildBtns[i].label, btnBg, btnBorder, 11, active || !hasPlayerHQ)) {
                autoBuildOrPlace(buildBtns[i].type);
            }
        }

        // Slot 8 (Row 1, Col 3): Upgrade Corps HQ Tech Tier [U]
        Rectangle techRect = {rx + 3 * (btnW + 6.0f), ry + 24.0f + (btnH + 8.0f), btnW, btnH};
        const char* techLbl = (p0.techTier == 1) ? "Upgrade Tier 2 ($260) [U]" :
                              (p0.techTier == 2 ? "Upgrade Tier 3 ($380) [U]" : "Max Tech Tier 3 (Mastered)");
        bool techDisabled = (p0.techTier >= 3 || !hasPlayerHQ);
        Color techBorder = !hasPlayerHQ ? Color{160, 60, 60, 255} : GOLD;
        Color techBg = !hasPlayerHQ ? Color{32, 22, 22, 255} : Color{46, 66, 48, 255};
        if (DrawHUDButton(techRect, techLbl, techBg, techBorder, 11, techDisabled)) {
            engine.upgradeCorpsHQTech(0);
        }
    }

    // 4. BATTLEFIELD TACTICAL RADIO DISPATCH TICKER (Live audio/combat event messages)
    drawTacticalRadioLog(engine);

    // 5. INTERACTIVE CONTROLS QUICK-REFERENCE OVERLAY (Toggled by CONTROLS [?] button)
    if (showControlsHelp_) {
        Rectangle card = {(sw - 820.0f) * 0.5f, 62.0f, 820.0f, 440.0f};
        ui.drawCommandPanel(card, "2004 AXIS & ALLIES -- BATTLEFIELD CONTROLS & COMMAND GUIDE");
        float hx = card.x + 28.0f;
        float hy = card.y + 44.0f;

        ui.drawText("1. SELECT FORCES (LEFT-CLICK ONLY):", hx, hy, 14.0f, GOLD, true); hy += 18.0f;
        ui.drawText("   * Left-Click a soldier/tank or Left-Drag a Green Box to select army units. Press [SPACE] to pan to battle alert!", hx, hy, 12.0f, RAYWHITE, false); hy += 15.0f;
        ui.drawText("   * Left-Clicking on empty ground DESELECTS. Left-Click NEVER moves or attacks!", hx, hy, 12.0f, LIME, true); hy += 20.0f;

        ui.drawText("2. MOVE & TARGET (RIGHT-CLICK):", hx, hy, 14.0f, GOLD, true); hy += 18.0f;
        ui.drawText("   * RIGHT-CLICK ground to move in formation (or press [M] for Attack-Move).", hx, hy, 12.0f, RAYWHITE, false); hy += 15.0f;
        ui.drawText("   * RIGHT-CLICK enemy regiment or structure to TARGET & FOCUS FIRE (shows red target reticle).", hx, hy, 12.0f, LIME, true); hy += 20.0f;

        ui.drawText("3. FORMATIONS, DYNAMIC COVER & ABILITIES:", hx, hy, 14.0f, GOLD, true); hy += 18.0f;
        ui.drawText("   * Formations: [Z] Line (standard), [V] Road Column (+25% Speed), [C] Entrench (+35% Defense).", hx, hy, 12.0f, RAYWHITE, false); hy += 15.0f;
        ui.drawText("   * Dynamic Cover: Craters, Boulders & Wrecks grant Heavy (-40%) and Light (-25%) incoming damage protection.", hx, hy, 12.0f, SKYBLUE, false); hy += 15.0f;
        ui.drawText("   * Unit Abilities: [K] Smoke Screen Canister (Armor -55% evasion), [L] Rapid Suppression Volley (+100% ROF).", hx, hy, 12.0f, LIME, true); hy += 20.0f;

        ui.drawText("4. MUSTER REGIMENTS & 10-SLOT ARMY BUILDER:", hx, hy, 14.0f, GOLD, true); hy += 18.0f;
        ui.drawText("   * Click [INFANTRY/MECH/ARMOR HQ] to queue up to 10 regiments in the Army Builder.", hx, hy, 12.0f, RAYWHITE, false); hy += 15.0f;
        ui.drawText("   * RIGHT-CLICK any unit card or queued slot to CANCEL & REFUND 100% of resources!", hx, hy, 12.0f, LIME, true); hy += 20.0f;

        ui.drawText("5. AIR FORCE MISSIONS, CAP DOGFIGHTS & AA FLAK:", hx, hy, 14.0f, GOLD, true); hy += 18.0f;
        ui.drawText("   * Shortcuts: Fighter CAP [J] ($140), Bombing [B] ($165), Recon [E] ($75), Paratroops [P] ($180), Supply [R] ($120).", hx, hy, 12.0f, RAYWHITE, false); hy += 15.0f;
        ui.drawText("   * Fighter Interceptors actively patrol airspace, engage hostile bombers in 3D dogfights with 20mm tracer fire!", hx, hy, 12.0f, LIME, true);

        if (DrawHUDButton({card.x + card.width - 150.0f, card.y + card.height - 38.0f, 126.0f, 28.0f}, "CLOSE [ESC]", {46, 76, 48, 255}, GOLD, 12, true)) {
            showControlsHelp_ = false;
        }
    }

    drawTelemetryHUD();

    if (qualityToastTimer_ > 0.0f) {
        float alpha = std::min(1.0f, qualityToastTimer_ * 1.5f);
        int tw = MeasureText(qualityToastMsg_.c_str(), 14) + 40;
        float tx = (sw - tw) * 0.5f;
        float ty = 52.0f;
        DrawRectangleRec({tx, ty, static_cast<float>(tw), 32.0f}, ColorAlpha(Color{20, 24, 26, 240}, alpha));
        DrawRectangleLinesEx({tx, ty, static_cast<float>(tw), 32.0f}, 1.5f, ColorAlpha(GOLD, alpha));
        ui.drawText(qualityToastMsg_.c_str(), tx + 20.0f, ty + 8.0f, 13.0f, ColorAlpha(WHITE, alpha), true);
    }
}

void RTSRenderer::drawTelemetryHUD() const {
    if (!telemetry_.showOverlay) return;

    int sw = GetScreenWidth();
    const auto& ui = UITheme::instance();
    Rectangle r = {static_cast<float>(sw - 336), 50.0f, 328.0f, 156.0f};

    // Semi-transparent brushed slate box with brass border
    DrawRectangleRec(r, {16, 20, 24, 235});
    DrawRectangleLinesEx(r, 1.5f, {178, 154, 96, 255});

    ui.drawText("HARDWARE & PERFORMANCE ENGINE", r.x + 10.0f, r.y + 8.0f, 12.0f, GOLD, true);

    // FPS with dynamic color coding (Green >= 58, Yellow >= 28, Red < 28)
    Color fpsCol = (telemetry_.fps >= 58.0f) ? LIME : ((telemetry_.fps >= 28.0f) ? YELLOW : RED);
    ui.drawText(TextFormat("FPS: %.0f  (%.1f ms)", telemetry_.fps, telemetry_.frameTimeMs),
                r.x + 10.0f, r.y + 28.0f, 15.0f, fpsCol, true);

    // Culling Efficiency
    int totalEntities = telemetry_.renderedEntities + telemetry_.culledEntities;
    float cullingPct = totalEntities > 0 ? (100.0f * telemetry_.culledEntities / totalEntities) : 0.0f;
    ui.drawText(TextFormat("Frustum Culling: %.0f%% Entities Saved", cullingPct),
                r.x + 10.0f, r.y + 49.0f, 12.0f, SKYBLUE, true);
    ui.drawText(TextFormat("3D Visible: %d | Culled: %d", telemetry_.renderedEntities, telemetry_.culledEntities),
                r.x + 10.0f, r.y + 67.0f, 11.0f, LIGHTGRAY, false);

    // Graphics Preset
    const char* qStr = "HIGH";
    if (quality_ == GraphicQuality::LOW) qStr = "LOW (iGPU / Iris 60+ FPS)";
    else if (quality_ == GraphicQuality::MEDIUM) qStr = "MEDIUM (Balanced)";
    else if (quality_ == GraphicQuality::HIGH) qStr = "HIGH (PBR + Dynamic VFX)";
    else if (quality_ == GraphicQuality::ULTRA) qStr = "ULTRA (120-170+ FPS)";
    ui.drawText(TextFormat("Quality: %s", qStr), r.x + 10.0f, r.y + 88.0f, 11.0f, {245, 220, 140, 255}, true);

    // Renderer Hardware
    ui.drawText("GPU: AMD Radeon Pro 5300M (OpenGL 4.1)", r.x + 10.0f, r.y + 108.0f, 10.0f, {170, 180, 190, 230}, false);
    ui.drawText("[F10] Cycle Quality  |  [F11] Toggle Telemetry", r.x + 10.0f, r.y + 128.0f, 10.0f, GRAY, false);
}

void RTSRenderer::drawTacticalRadioLog(const RTSEngine& engine) {
    const auto& log = engine.getCombatLog();
    if (log.empty()) return;

    const auto& ui = UITheme::instance();
    float rx = 12.0f;
    float ry = 50.0f;
    float rw = 450.0f;
    int displayCount = std::min(4, static_cast<int>(log.size()));
    float rh = 18.0f + displayCount * 16.0f;

    DrawRectangleRec({rx, ry, rw, rh}, {12, 16, 14, 225});
    DrawRectangleLinesEx({rx, ry, rw, rh}, 1.0f, {80, 110, 95, 200});

    // Header badge
    DrawRectangle(static_cast<int>(rx), static_cast<int>(ry), static_cast<int>(rw), 16, {22, 32, 26, 250});
    ui.drawText("RADIO DISPATCH TICKER", rx + 6.0f, ry + 2.0f, 10.0f, GOLD, true, false);

    float ly = ry + 18.0f;
    int startIdx = std::max(0, static_cast<int>(log.size()) - displayCount);
    for (int i = startIdx; i < static_cast<int>(log.size()); ++i) {
        const std::string& msg = log[i];
        Color msgCol = RAYWHITE;
        if (msg.find("TOWN CAPTURED") != std::string::npos || msg.find("DEPLOYED") != std::string::npos ||
            msg.find("CONSTRUCTED") != std::string::npos || msg.find("CASUALTY") != std::string::npos) {
            msgCol = LIME;
        } else if (msg.find("AA ") != std::string::npos || msg.find("WARNING") != std::string::npos ||
                   msg.find("DESTROYED") != std::string::npos || msg.find("SIREN") != std::string::npos ||
                   msg.find("INTERCEPT") != std::string::npos) {
            msgCol = Color{255, 115, 105, 255};
        } else if (msg.find("TECH") != std::string::npos || msg.find("AIR ") != std::string::npos ||
                   msg.find("SUPPLY") != std::string::npos || msg.find("PHOTO") != std::string::npos) {
            msgCol = SKYBLUE;
        } else if (msg.find("SPECIAL") != std::string::npos || msg.find("HQ") != std::string::npos) {
            msgCol = GOLD;
        }
        ui.drawText(msg.c_str(), rx + 6.0f, ly, 10.0f, msgCol, false, false);
        ly += 16.0f;
    }
}

void RTSRenderer::drawMinimap(RTSEngine& engine, Rectangle rect) {
    DrawRectangleRec(rect, {22, 34, 26, 252});

    BeginScissorMode(static_cast<int>(rect.x + 1), static_cast<int>(rect.y + 1),
                     static_cast<int>(rect.width - 2), static_cast<int>(rect.height - 2));

    float mw = kMapFullSpan;
    float md = kMapFullSpan;

    auto toMini = [&](Vector3 wp) -> Vector2 {
        return {
            rect.x + ((wp.x + mw * 0.5f) / mw) * rect.width,
            rect.y + ((wp.z + md * 0.5f) / md) * rect.height
        };
    };

    if (engine.hasRiver()) {
        float riverY = rect.y + rect.height * 0.5f;
        DrawRectangle(static_cast<int>(rect.x), static_cast<int>(riverY - 1), static_cast<int>(rect.width), 3, {45, 95, 145, 210});

        // 3 Fortified Bridge crossing icons on the minimap
        for (float bx : {-240.0f, 0.0f, 240.0f}) {
            Vector2 bmp = toMini({bx, 0.0f, 0.0f});
            DrawRectangle(static_cast<int>(bmp.x - 3), static_cast<int>(bmp.y - 3), 6, 6, {235, 210, 110, 255});
            DrawRectangleLines(static_cast<int>(bmp.x - 3), static_cast<int>(bmp.y - 3), 6, 6, BLACK);
        }
    }

    // 1. Render 64x64 Fog of War Shroud on the Minimap
    float cellW = rect.width / RTSEngine::kFogGridSize;
    float cellH = rect.height / RTSEngine::kFogGridSize;
    for (int gz = 0; gz < RTSEngine::kFogGridSize; ++gz) {
        for (int gx = 0; gx < RTSEngine::kFogGridSize; ++gx) {
            uint8_t state = engine.getFogCellState(gx, gz);
            if (state == 2) continue; // Currently inside live friendly Vision Radius
            Color fogCol = (state == 0) ? Color{5, 8, 7, 232} : Color{10, 15, 12, 145};
            DrawRectangleRec({rect.x + gx * cellW, rect.y + gz * cellH, cellW + 0.8f, cellH + 0.8f}, fogCol);
        }
    }

    // 2. Draw Buildings & Towns (Enemy buildings and unexplored neutral towns are HIDDEN on the minimap until scouted!)
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f) continue;
        if (b.teamId != 0 && !engine.isPosVisibleToTeam(b.pos, 0) && !engine.isPosExploredByPlayer(b.pos)) {
            continue;
        }
        Vector2 mp = toMini(b.pos);
        if (engine.showSupplyOverlay() && b.teamId == 0) {
            float vPx = (engine.getBuildingVisionRadius(b.type) / mw) * rect.width;
            DrawCircleLines(static_cast<int>(mp.x), static_cast<int>(mp.y), vPx, {90, 210, 240, 85});
        }
        Color bc = (b.teamId == 0) ? LIME :
                   (engine.isAllied(0, b.teamId) ? SKYBLUE :
                   (b.teamId == 1 ? RED :
                   (b.teamId == 2 ? Color{245, 155, 45, 255} :
                   (b.teamId >= 3 ? Color{195, 110, 245, 255} :
                   (engine.isPosExploredByPlayer(b.pos) ? GOLD : Color{125, 115, 65, 135})))));
        DrawRectangle(static_cast<int>(mp.x - 3), static_cast<int>(mp.y - 3), 6, 6, bc);

        // Contested town expanding animated radar ping
        if (b.type == BuildingType::CITY_DEPOT && b.captureProgress != 0.0f && std::fabs(b.captureProgress) < 95.0f) {
            float ping = std::fmod(waterAnimTime_ * 14.0f, 15.0f) + 3.0f;
            float pingAlpha = std::max(0.0f, 1.0f - (ping - 3.0f) / 15.0f);
            DrawCircleLines(static_cast<int>(mp.x), static_cast<int>(mp.y), ping, ColorAlpha(GOLD, pingAlpha));
        }
    }

    // 2B. Smoldering Wrecks on the Minimap
    for (const auto& w : engine.getWrecks()) {
        Vector2 wmp = toMini(w.pos);
        DrawCircle(static_cast<int>(wmp.x), static_cast<int>(wmp.y), 1.5f, {45, 42, 40, 200});
    }

    // 3. Draw Regiments (Hostile units are STRICTLY HIDDEN on the minimap unless inside live Vision / Recon!)
    for (const auto& r : engine.getRegiments()) {
        if (r.health <= 0.0f) continue;
        if (r.teamId != 0 && !engine.isAllied(0, r.teamId) && !engine.isPosVisibleToTeam(r.pos, 0)) {
            continue;
        }
        Vector2 mp = toMini(r.pos);
        Color rc = (r.teamId == 0) ? (r.selected ? WHITE : LIME) :
                   (engine.isAllied(0, r.teamId) ? SKYBLUE :
                   (r.teamId == 1 ? RED :
                   (r.teamId == 2 ? Color{245, 155, 45, 255} : Color{195, 110, 245, 255})));
        DrawCircle(static_cast<int>(mp.x), static_cast<int>(mp.y), 2.5f, rc);
    }

    // 4. Draw Selected HQ Assembly Point (Rally Point) line and marker on the Minimap
    for (const auto& b : engine.getBuildings()) {
        if (b.health <= 0.0f || b.teamId != 0 || !b.selected) continue;
        if (b.type != BuildingType::CORPS_HQ && b.type != BuildingType::INFANTRY_HQ &&
            b.type != BuildingType::MECH_HQ && b.type != BuildingType::ARMOR_HQ &&
            b.type != BuildingType::AIRFIELD) continue;
        Vector2 bmp = toMini(b.pos);
        Vector2 rmp = toMini(b.rallyPoint);
        DrawLine(static_cast<int>(bmp.x), static_cast<int>(bmp.y), static_cast<int>(rmp.x), static_cast<int>(rmp.y), SKYBLUE);
        DrawCircle(static_cast<int>(rmp.x), static_cast<int>(rmp.y), 3.5f, SKYBLUE);
        DrawCircleLines(static_cast<int>(rmp.x), static_cast<int>(rmp.y), 4.5f, GOLD);
    }

    if (moveOrderIndicatorTimer_ > 0.0f) {
        Vector2 omp = toMini(moveOrderIndicatorPos_);
        DrawCircle(static_cast<int>(omp.x), static_cast<int>(omp.y), 3.5f, moveOrderIndicatorIsAttack_ ? RED : LIME);
        DrawCircleLines(static_cast<int>(omp.x), static_cast<int>(omp.y), 5.0f, GOLD);
    }

    // 5. True Rotated Camera Perspective Frustum on Minimap
    int curSw = GetScreenWidth();
    int curSh = GetScreenHeight();
    Vector3 gTL = screenToGround({0.0f, 0.0f}, engine);
    Vector3 gTR = screenToGround({static_cast<float>(curSw), 0.0f}, engine);
    Vector3 gBR = screenToGround({static_cast<float>(curSw), static_cast<float>(curSh)}, engine);
    Vector3 gBL = screenToGround({0.0f, static_cast<float>(curSh)}, engine);

    auto clampGround = [](Vector3 v) -> Vector3 {
        return {
            std::clamp(v.x, -kMapHalfExtent, kMapHalfExtent),
            0.0f,
            std::clamp(v.z, -kMapHalfExtent, kMapHalfExtent)
        };
    };

    Vector2 mTL = toMini(clampGround(gTL));
    Vector2 mTR = toMini(clampGround(gTR));
    Vector2 mBR = toMini(clampGround(gBR));
    Vector2 mBL = toMini(clampGround(gBL));

    // Draw crisp yellow boundary lines
    DrawLineV(mTL, mTR, Color{255, 230, 80, 240});
    DrawLineV(mTR, mBR, Color{255, 230, 80, 240});
    DrawLineV(mBR, mBL, Color{255, 230, 80, 240});
    DrawLineV(mBL, mTL, Color{255, 230, 80, 240});

    // Camera Center Crosshair on Minimap
    Vector2 camMini = toMini(cameraTarget_);
    DrawLine(static_cast<int>(camMini.x - 3), static_cast<int>(camMini.y), static_cast<int>(camMini.x + 4), static_cast<int>(camMini.y), GOLD);
    DrawLine(static_cast<int>(camMini.x), static_cast<int>(camMini.y - 3), static_cast<int>(camMini.x), static_cast<int>(camMini.y + 4), GOLD);

    EndScissorMode();
    DrawRectangleLinesEx(rect, 2.0f, {172, 150, 94, 255});

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, rect)) {
        float wx = ((mouse.x - rect.x) / rect.width) * mw - mw * 0.5f;
        float wz = ((mouse.y - rect.y) / rect.height) * md - md * 0.5f;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Left-Click on Minimap pans the camera focus
            cameraTarget_.x = std::clamp(wx, -kMapHalfExtent, kMapHalfExtent);
            cameraTarget_.z = std::clamp(wz, -kMapHalfExtent, kMapHalfExtent);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            // Right-Click on Minimap orders selected forces to target that sector!
            Vector3 mTarget = {std::clamp(wx, -kMapHalfExtent, kMapHalfExtent), 0.0f, std::clamp(wz, -kMapHalfExtent, kMapHalfExtent)};
            engine.issueRightClickOrder(mTarget);
            moveOrderIndicatorPos_   = mTarget;
            moveOrderIndicatorTimer_ = 1.25f;
        }
    }
}

} // namespace aa2004
