// ============================================================================
// AXIS & ALLIES (2004) — HIGH-DETAIL 3D MILITARY MODEL PROCEDURAL RENDERER
// Modern AAA 3D Models: Multi-Tiered WWII Tanks (Roadwheels, Return Rollers,
// Idler/Drive Sprockets, Sloped Glacis with Weld Beads, Pioneer Tools, Tow Cables,
// Turret Stowage Baskets, Cupolas), Katyusha Rocket Launchers, Multi-Part Squads,
// Gabled European Cities with Clocktowers, Industrial Sawtooth Factories with
// Gantry Cranes, Petrochemical Refineries with Distillation Columns & Catwalks,
// Flight Operations Airfields with Control Towers & Windsocks, and Concrete Bunkers.
// ============================================================================

#include "rts/rts_models.hpp"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

namespace aa2004 {
namespace models {

namespace {

inline Color Shade(Color c, float mult) {
    return {
        static_cast<unsigned char>(std::clamp(c.r * mult, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(c.g * mult, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(c.b * mult, 0.0f, 255.0f)),
        c.a
    };
}

// Modern Solid Beveled Structural Box with ambient occlusion and bevel highlights (NO wireframes!)
void DrawBeveledBox(Vector3 center, float w, float h, float d, Color baseCol) {
    DrawCube(center, w, h, d, baseCol);
    // Subtle beveled top highlight
    DrawCube({center.x, center.y + h * 0.49f, center.z}, w * 0.96f, h * 0.035f, d * 0.96f, Shade(baseCol, 1.15f));
    // Ground / underside ambient occlusion contact shadow
    DrawCube({center.x, center.y - h * 0.48f, center.z}, w * 1.01f, h * 0.04f, d * 1.01f, Shade(baseCol, 0.76f));
}

// 3D Pitched / Gabled Architectural Roof with overhang eaves, bargeboards and ridge cap
void DrawPitchedGabledRoof(Vector3 center, float w, float h, float d, Color roofCol, Color trimCol) {
    int steps = 6;
    for (int i = 0; i < steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float layerW = w * (1.0f - t * 0.88f);
        float layerH = (h / steps) * 1.12f;
        float layerY = center.y - h * 0.5f + (i + 0.5f) * (h / steps);
        DrawCube({center.x, layerY, center.z}, layerW, layerH, d + 0.25f, (i % 2 == 0) ? roofCol : Shade(roofCol, 0.94f));
    }
    // Ridge beam cap along the peak
    DrawCube({center.x, center.y + h * 0.52f, center.z}, w * 0.14f, 0.22f, d + 0.35f, trimCol);
    // Eaves drip edge overhang
    DrawCube({center.x, center.y - h * 0.48f, center.z}, w + 0.35f, 0.16f, d + 0.42f, trimCol);
}

// 3D Recessed Architectural Window with frame, stone lintel, sill & illuminated mullions
void DrawRecessedWindow(Vector3 center, float w, float h, float depth, Color frameCol, Color glassCol) {
    // Stone sill below window
    DrawCube({center.x, center.y - h * 0.52f, center.z}, w + 0.18f, 0.12f, depth + 0.12f, {185, 180, 172, 255});
    // Stone lintel above window
    DrawCube({center.x, center.y + h * 0.52f, center.z}, w + 0.18f, 0.14f, depth + 0.12f, {172, 168, 160, 255});
    // Recessed dark window cavity
    DrawCube(center, w, h, depth, {22, 26, 32, 255});
    // Glass pane with reflection / warm illumination
    DrawCube({center.x, center.y, center.z + depth * 0.1f}, w * 0.88f, h * 0.88f, depth * 0.2f, glassCol);
    // Crossed window mullions (window panes grid)
    DrawCube({center.x, center.y, center.z + depth * 0.2f}, 0.05f, h * 0.88f, depth * 0.22f, frameCol);
    DrawCube({center.x, center.y, center.z + depth * 0.2f}, w * 0.88f, 0.05f, depth * 0.22f, frameCol);
}

void DrawSoftShadowDisc(Vector3 pos, float rx, float rz) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 0.06f, pos.z);
    rlScalef(rx, 1.0f, rz);
    DrawCylinder({0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 0.02f, 14, {8, 10, 8, 135});
    rlPopMatrix();
}

} // namespace

void drawNationalInsignia3D(Vector3 center, float radius, NationId nation) {
    Color c = WHITE;
    if (nation == NationId::USSR) c = {235, 45, 35, 255};
    else if (nation == NationId::GER) c = {25, 28, 30, 255};
    else if (nation == NationId::JPN) c = {215, 40, 35, 255};
    else if (nation == NationId::UK)  c = {240, 210, 120, 255};

    DrawCylinder({center.x, center.y, center.z}, radius, radius, 0.05f, 10, WHITE);
    DrawCylinder({center.x, center.y + 0.02f, center.z}, radius * 0.68f, radius * 0.68f, 0.05f, 10, c);
}

void drawEntrenchmentRing3D(Vector3 pos, float radius) {
    const int numBags = 16;
    Color bagTan1 = {176, 156, 114, 255};
    Color bagTan2 = {152, 134, 96, 255};
    Color timber  = {78, 54, 32, 255};

    for (int i = 0; i < numBags; ++i) {
        float ang = (static_cast<float>(i) / numBags) * 2.0f * PI;
        float bx = pos.x + std::cos(ang) * radius;
        float bz = pos.z + std::sin(ang) * radius;
        rlPushMatrix();
        rlTranslatef(bx, pos.y + 0.32f, bz);
        rlRotatef(-ang * RAD2DEG, 0.0f, 1.0f, 0.0f);
        // Bottom row of staggered sandbags
        DrawBeveledBox({0.0f, 0.0f, 0.0f}, 0.65f, 0.36f, 1.40f, (i % 2 == 0) ? bagTan1 : bagTan2);
        // Upper staggered sandbag
        DrawBeveledBox({0.0f, 0.32f, 0.0f}, 0.56f, 0.30f, 1.20f, (i % 2 == 0) ? bagTan2 : bagTan1);
        // Timber reinforcement stake driven into earth
        DrawCube({-0.28f, 0.12f, 0.0f}, 0.12f, 0.72f, 0.12f, timber);
        rlPopMatrix();
    }
}

void drawSoldier3D(Vector3 pos, float facingDeg, float walkPhase, bool isWalking, float muzzleFlashTimer,
                   const NationDef& nat, RegimentType type, float timeSec, int memberIdx,
                   bool isPinned, bool isCrouched) {
    (void)timeSec;
    float shadowRadiusX = isPinned ? 0.75f : 0.58f;
    float shadowRadiusZ = isPinned ? 1.15f : 0.58f;
    DrawSoftShadowDisc(pos, shadowRadiusX, shadowRadiusZ);

    float stride = isWalking ? std::sin(walkPhase) * 0.22f : 0.0f;
    Color tunicCol  = nat.primaryColor;
    Color pantsCol  = Shade(nat.primaryColor, 0.76f);
    Color helmetCol = Shade(nat.primaryColor, 0.68f);
    Color skinCol   = {228, 188, 152, 255};
    Color packCol   = {122, 106, 74, 255};
    Color gunMetal  = {38, 42, 45, 255};
    Color woodStock = {112, 72, 38, 255};

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facingDeg, 0.0f, 1.0f, 0.0f);

    if (isPinned) {
        // Authentic WWII combat prone dive & combat crawl!
        // Squad hits the dirt and crawls low when under suppressing fire
        rlTranslatef(0.0f, 0.14f, 0.22f);
        rlRotatef(-76.0f, 1.0f, 0.0f, 0.0f);
        if (isWalking) {
            stride = std::sin(walkPhase * 1.5f) * 0.15f;
        }
    } else if (isCrouched) {
        // Fortified / entrenched foxhole crouch behind cover
        rlTranslatef(0.0f, -0.32f, 0.0f);
    }

    // Left & Right Boots + Legs with knee articulation
    DrawCube({-0.16f, 0.32f,  stride}, 0.18f, 0.62f, 0.20f, pantsCol);
    DrawCube({ 0.16f, 0.32f, -stride}, 0.18f, 0.62f, 0.20f, pantsCol);
    DrawCube({-0.16f, 0.08f,  stride + 0.04f}, 0.20f, 0.16f, 0.28f, {28, 26, 24, 255});
    DrawCube({ 0.16f, 0.08f, -stride + 0.04f}, 0.20f, 0.16f, 0.28f, {28, 26, 24, 255});

    // Tunic Torso + Webbing Belt + Ammunition Pouches + Field Canteen + Backpack
    DrawBeveledBox({0.0f, 0.92f, 0.0f}, 0.56f, 0.68f, 0.34f, tunicCol);
    DrawCube({0.0f, 0.66f, 0.0f}, 0.58f, 0.10f, 0.36f, {58, 48, 34, 255}); // Webbing belt
    // Ammo pouches on belt
    DrawCube({-0.18f, 0.66f, 0.19f}, 0.14f, 0.12f, 0.08f, {88, 64, 42, 255});
    DrawCube({ 0.18f, 0.66f, 0.19f}, 0.14f, 0.12f, 0.08f, {88, 64, 42, 255});
    // Water canteen on hip
    DrawCylinder({0.26f, 0.68f, -0.06f}, 0.08f, 0.08f, 0.16f, 6, {75, 82, 68, 255});

    DrawBeveledBox({0.0f, 0.96f, -0.24f}, 0.42f, 0.46f, 0.22f,
                   (type == RegimentType::ENGINEER_FLAME_REGIMENT) ? Color{145, 58, 38, 255} : packCol);

    // Head & Distinct WWII Steel Helmet with rolled rim
    DrawSphere({0.0f, 1.44f, 0.02f}, 0.21f, skinCol);
    DrawSphere({0.0f, 1.54f, 0.0f}, 0.26f, helmetCol);
    DrawCylinder({0.0f, 1.48f, 0.0f}, 0.29f, 0.27f, 0.06f, 10, helmetCol); // Helmet brim

    // Arms & Specialized Weapon Systems
    DrawCube({-0.32f, 0.98f, 0.16f}, 0.15f, 0.42f, 0.36f, tunicCol);
    DrawCube({ 0.32f, 0.98f, 0.18f}, 0.15f, 0.42f, 0.38f, tunicCol);

    if (type == RegimentType::MG_REGIMENT && memberIdx == 0) {
        // Tripod Heavy Machine Gun (Browning M1919 / MG-42) with cooling jacket & ammo can
        DrawCylinderEx({0.0f, 0.72f, 0.32f}, {0.0f, 0.78f, 1.35f}, 0.08f, 0.06f, 8, gunMetal);
        DrawCube({-0.18f, 0.74f, 0.55f}, 0.20f, 0.22f, 0.24f, {85, 95, 68, 255}); // Ammo can
        DrawLine3D({0.0f, 0.72f, 0.55f}, {-0.32f, 0.0f, 0.85f}, gunMetal);
        DrawLine3D({0.0f, 0.72f, 0.55f}, { 0.32f, 0.0f, 0.85f}, gunMetal);
    } else if (type == RegimentType::MORTAR_REGIMENT && memberIdx <= 1) {
        // Angled 81mm Mortar Tube + Baseplate + Bipod
        DrawCylinder({0.0f, 0.06f, 0.55f}, 0.32f, 0.32f, 0.08f, 8, gunMetal);
        DrawCylinderEx({0.0f, 0.10f, 0.55f}, {0.0f, 1.38f, 1.05f}, 0.12f, 0.12f, 10, {56, 74, 52, 255});
    } else if (type == RegimentType::AT_BAZOOKA_REGIMENT) {
        // Shoulder-Fired Rocket Launcher (Bazooka / Panzerschreck) with blast shield
        DrawCylinderEx({0.26f, 1.25f, -0.65f}, {0.26f, 1.25f, 1.15f}, 0.08f, 0.08f, 8, {62, 74, 55, 255});
        DrawCube({0.26f, 1.25f, 0.42f}, 0.28f, 0.28f, 0.04f, gunMetal); // Blast shield
    } else if (type == RegimentType::ENGINEER_FLAME_REGIMENT) {
        // Flamethrower Wand + Fuel Lance
        DrawCylinderEx({0.15f, 0.95f, 0.25f}, {0.15f, 1.05f, 1.35f}, 0.06f, 0.05f, 6, gunMetal);
        DrawCylinder({0.15f, 1.05f, 1.35f}, 0.09f, 0.07f, 0.15f, 6, {65, 42, 35, 255});
    } else if (type == RegimentType::SNIPER_REGIMENT) {
        // Scoped Bolt-Action Sniper Rifle with Optical Scope Tube
        DrawCube({0.12f, 1.05f, 0.55f}, 0.08f, 0.14f, 1.25f, woodStock);
        DrawCylinderEx({0.12f, 1.12f, 0.25f}, {0.12f, 1.12f, 1.45f}, 0.04f, 0.035f, 6, gunMetal);
        DrawCylinderEx({0.12f, 1.22f, 0.45f}, {0.12f, 1.22f, 0.85f}, 0.045f, 0.045f, 6, gunMetal); // Scope
    } else {
        // Standard Infantry Service Rifle (M1 Garand / Kar98k / Mosin-Nagant) with sling
        DrawCube({0.10f, 1.02f, 0.45f}, 0.08f, 0.12f, 0.95f, woodStock);
        DrawCylinderEx({0.10f, 1.08f, 0.15f}, {0.10f, 1.08f, 1.25f}, 0.035f, 0.03f, 6, gunMetal);
    }

    if (muzzleFlashTimer > 0.0f) {
        DrawSphere({0.10f, 1.08f, 1.38f}, 0.32f, {255, 240, 150, 255});
        DrawSphere({0.10f, 1.08f, 1.62f}, 0.20f, {255, 160, 40, 240});
    }

    rlPopMatrix();
}

void drawTank3D(Vector3 pos, float hullFacingDeg, float turretFacingDeg, float recoil, float muzzleFlashTimer,
                const NationDef& nat, RegimentType type, float timeSec,
                float pitchDeg, float rollDeg) {
    float s = 1.0f;
    if (type == RegimentType::HEAVY_TANK) s = 1.25f;
    else if (type == RegimentType::ELITE_SIGNATURE_TANK) s = 1.38f;
    else if (type == RegimentType::TANK_DESTROYER) s = 1.08f;

    DrawSoftShadowDisc(pos, 2.75f * s, 3.85f * s);

    Color armorBase  = nat.primaryColor;
    Color armorLight = Shade(armorBase, 1.14f);
    Color armorDark  = Shade(armorBase, 0.78f);
    Color trackSteel = {42, 45, 48, 255};
    Color wheelSteel = {68, 72, 75, 255};
    Color barrelSteel = {38, 42, 45, 255};
    Color toolWood   = {115, 78, 44, 255};

    if (muzzleFlashTimer > 0.0f) {
        float flashPulse = muzzleFlashTimer * 3.0f;
        float yaw = (type == RegimentType::TANK_DESTROYER) ? hullFacingDeg : turretFacingDeg;
        float yawRad = yaw * DEG2RAD;
        float barrelZ = 4.8f * s;
        Vector3 dustPos = {
            pos.x + std::sin(yawRad) * barrelZ,
            pos.y + 0.09f,
            pos.z + std::cos(yawRad) * barrelZ
        };
        float dustRadius = (1.0f - muzzleFlashTimer * 1.5f) * 5.2f * s;
        DrawCircle3D(dustPos, dustRadius, {1.0f, 0.0f, 0.0f}, 90.0f,
                     ColorAlpha(Color{192, 176, 142, 255}, 0.35f * flashPulse));

        // Hot ejected brass shell casing tumbling from turret
        DrawCylinderEx({pos.x, pos.y + 1.85f * s, pos.z},
                       {pos.x + 0.25f * s, pos.y + 1.95f * s, pos.z + 0.15f * s},
                       0.05f * s, 0.05f * s, 6, GOLD);
    }

    float hullRockDeg = recoil * 3.8f;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(hullFacingDeg, 0.0f, 1.0f, 0.0f);
    rlRotatef(pitchDeg - hullRockDeg, 1.0f, 0.0f, 0.0f); // Suspension terrain pitch + gun recoil rock
    rlRotatef(rollDeg, 0.0f, 0.0f, 1.0f);                 // Suspension terrain roll along slope bank

    // 1. Heavy Caterpillar Track Assemblies, 5 Roadwheels, 3 Upper Return Rollers, Idler & Drive Sprocket
    float trackX = 1.44f * s;
    float trackLen = 4.75f * s;
    float trackH = 0.88f * s;

    // Track rubber/steel continuous treads
    DrawBeveledBox({-trackX, trackH * 0.5f, 0.0f}, 0.64f * s, trackH, trackLen, trackSteel);
    DrawBeveledBox({ trackX, trackH * 0.5f, 0.0f}, 0.64f * s, trackH, trackLen, trackSteel);

    // Front Idler with tensioner bracket and Rear Drive Sprocket with dual gear rings
    for (float side : {-trackX, trackX}) {
        // Front idler wheel
        DrawCylinderEx({side - 0.32f * s, 0.52f * s,  trackLen * 0.44f},
                       {side + 0.32f * s, 0.52f * s,  trackLen * 0.44f}, 0.40f * s, 0.40f * s, 12, barrelSteel);
        // Rear drive sprocket with gear hub
        DrawCylinderEx({side - 0.32f * s, 0.52f * s, -trackLen * 0.44f},
                       {side + 0.32f * s, 0.52f * s, -trackLen * 0.44f}, 0.42f * s, 0.42f * s, 12, barrelSteel);
        DrawCylinderEx({side - 0.34f * s, 0.52f * s, -trackLen * 0.44f},
                       {side + 0.34f * s, 0.52f * s, -trackLen * 0.44f}, 0.22f * s, 0.22f * s, 8, {85, 90, 94, 255});
    }

    // 5 Large Heavy Roadwheels with rubber rims and center grease hubs
    for (int w = -2; w <= 2; ++w) {
        float wz = w * 0.88f * s;
        // Left roadwheel
        DrawCylinderEx({-trackX - 0.33f * s, 0.42f * s, wz}, {-trackX + 0.33f * s, 0.42f * s, wz},
                       0.35f * s, 0.35f * s, 12, wheelSteel);
        DrawSphere({-trackX - 0.34f * s, 0.42f * s, wz}, 0.12f * s, barrelSteel); // Hub cap
        // Right roadwheel
        DrawCylinderEx({ trackX - 0.33f * s, 0.42f * s, wz}, { trackX + 0.33f * s, 0.42f * s, wz},
                       0.35f * s, 0.35f * s, 12, wheelSteel);
        DrawSphere({ trackX + 0.34f * s, 0.42f * s, wz}, 0.12f * s, barrelSteel); // Hub cap
    }

    // 3 Upper Return Rollers supporting top track run
    for (int r = -1; r <= 1; ++r) {
        float rz = r * 1.35f * s;
        DrawCylinderEx({-trackX - 0.28f * s, 0.78f * s, rz}, {-trackX + 0.28f * s, 0.78f * s, rz},
                       0.14f * s, 0.14f * s, 8, barrelSteel);
        DrawCylinderEx({ trackX - 0.28f * s, 0.78f * s, rz}, { trackX + 0.28f * s, 0.78f * s, rz},
                       0.14f * s, 0.14f * s, 8, barrelSteel);
    }

    // 2. Armored Side Skirts (Schürzen) & Lower Hull Frame
    DrawBeveledBox({-trackX, 0.82f * s, 0.0f}, 0.68f * s, 0.30f * s, trackLen * 0.98f, armorDark);
    DrawBeveledBox({ trackX, 0.82f * s, 0.0f}, 0.68f * s, 0.30f * s, trackLen * 0.98f, armorDark);
    DrawBeveledBox({0.0f, 0.86f * s, 0.0f}, 2.45f * s, 0.88f * s, 4.35f * s, armorBase);

    // 3. Welded Sloped Front Glacis Plate, Driver Armored Visor & Ball Mount MG
    rlPushMatrix();
    rlTranslatef(0.0f, 1.05f * s, 1.85f * s);
    rlRotatef(26.0f, 1.0f, 0.0f, 0.0f);
    DrawBeveledBox({0.0f, 0.0f, 0.0f}, 2.38f * s, 0.42f * s, 1.18f * s, armorLight);
    // Welded joint seams
    DrawCube({0.0f, 0.21f * s, 0.55f * s}, 2.40f * s, 0.05f * s, 0.06f * s, {28, 30, 32, 255});
    // Driver armored visor with prism slits
    DrawCube({-0.55f * s, 0.22f * s, 0.10f * s}, 0.44f * s, 0.10f * s, 0.18f * s, {22, 26, 28, 255});
    DrawCube({-0.55f * s, 0.22f * s, 0.18f * s}, 0.32f * s, 0.03f * s, 0.04f * s, {85, 145, 185, 240});
    // Ball-mount hull machine gun with canvas boot
    DrawSphere({0.48f * s, 0.16f * s, 0.12f * s}, 0.14f * s, {62, 58, 48, 255});
    DrawCylinderEx({0.48f * s, 0.16f * s, 0.12f * s}, {0.48f * s, 0.18f * s, 0.52f * s}, 0.035f * s, 0.03f * s, 6, barrelSteel);
    rlPopMatrix();

    // Bolted Spare Track Shoe Links across lower front glacis (Field Improvised Armor)
    for (int t = -2; t <= 2; ++t) {
        DrawCube({t * 0.38f * s, 0.55f * s, 2.22f * s}, 0.32f * s, 0.22f * s, 0.14f * s, trackSteel);
    }

    // Front Fender Armored Headlights with wire brush guards & warm lenses
    DrawCube({-trackX, 1.02f * s, 2.05f * s}, 0.26f * s, 0.22f * s, 0.22f * s, barrelSteel);
    DrawSphere({-trackX, 1.02f * s, 2.16f * s}, 0.11f * s, {255, 245, 205, 240});
    DrawCube({ trackX, 1.02f * s, 2.05f * s}, 0.26f * s, 0.22f * s, 0.22f * s, barrelSteel);
    DrawSphere({ trackX, 1.02f * s, 2.16f * s}, 0.11f * s, {255, 245, 205, 240});

    // Pioneer Tools & Steel Tow Cables strapped to fenders
    // Left fender pioneer shovel & pickaxe
    DrawCube({-trackX + 0.12f * s, 1.04f * s, 0.4f * s}, 0.08f * s, 0.06f * s, 1.6f * s, toolWood);
    DrawCube({-trackX + 0.12f * s, 1.04f * s, 1.25f * s}, 0.20f * s, 0.08f * s, 0.28f * s, barrelSteel); // Shovel blade
    // Right fender steel tow cable with cast eyelet loops
    DrawCylinderEx({trackX - 0.10f * s, 1.04f * s, -1.2f * s}, {trackX - 0.10f * s, 1.04f * s, 1.4f * s},
                   0.035f * s, 0.035f * s, 6, {75, 78, 80, 255});

    // 4. Rear Engine Deck Radiator Grilles & Cast Exhaust Mufflers
    DrawCube({-0.52f * s, 1.32f * s, -1.25f * s}, 0.72f * s, 0.06f * s, 1.10f * s, {28, 32, 34, 255});
    DrawCube({ 0.52f * s, 1.32f * s, -1.25f * s}, 0.72f * s, 0.06f * s, 1.10f * s, {28, 32, 34, 255});
    // Exhaust mufflers with curved heat shields and dark soot
    DrawCylinder({-0.55f * s, 0.95f * s, -2.22f * s}, 0.15f * s, 0.15f * s, 0.68f * s, 8, {42, 36, 34, 255});
    DrawCylinder({ 0.55f * s, 0.95f * s, -2.22f * s}, 0.15f * s, 0.15f * s, 0.68f * s, 8, {42, 36, 34, 255});
    DrawSphere({-0.55f * s, 1.29f * s, -2.22f * s}, 0.10f * s, {20, 20, 22, 255}); // Soot opening
    DrawSphere({ 0.55f * s, 1.29f * s, -2.22f * s}, 0.10f * s, {20, 20, 22, 255});

    // National Emblem on Front Glacis
    drawNationalInsignia3D({0.65f * s, 1.31f * s, 1.45f * s}, 0.34f * s, nat.id);

    rlPopMatrix();

    // 5. Rotating Turret, Gun Mantlet, Cupola, Stowage Bustle & Main Cannon
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 1.56f * s, pos.z);
    float yaw = (type == RegimentType::TANK_DESTROYER) ? hullFacingDeg : turretFacingDeg;
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);

    // Main turret housing with beveled chamfers
    DrawBeveledBox({0.0f, 0.12f * s, -0.10f * s}, 1.92f * s, 0.84f * s, 2.32f * s, armorLight);

    // Rear turret bustle stowage bin with rolled canvas tarps & wooden crates
    DrawBeveledBox({0.0f, 0.14f * s, -1.38f * s}, 1.48f * s, 0.58f * s, 0.56f * s, armorDark);
    DrawCylinderEx({-0.55f * s, 0.52f * s, -1.38f * s}, {0.55f * s, 0.52f * s, -1.38f * s},
                   0.16f * s, 0.16f * s, 8, {138, 122, 88, 255}); // Rolled canvas bedroll
    DrawCube({0.45f * s, 0.48f * s, -1.15f * s}, 0.32f * s, 0.28f * s, 0.28f * s, {142, 115, 68, 255}); // Ammo crate

    // Commander Cupola with 6 all-around armored periscope vision blocks
    DrawCylinder({-0.42f * s, 0.52f * s, -0.25f * s}, 0.38f * s, 0.40f * s, 0.30f * s, 12, armorDark);
    for (int p = 0; p < 6; ++p) {
        float pAng = p * (2.0f * PI / 6.0f);
        DrawCube({-0.42f * s + std::sin(pAng) * 0.38f * s, 0.62f * s, -0.25f * s + std::cos(pAng) * 0.38f * s},
                 0.08f * s, 0.06f * s, 0.08f * s, {85, 145, 185, 240});
    }

    // Roof pintle-mounted anti-aircraft machine gun (.50 cal / MG-42)
    DrawCylinderEx({0.42f * s, 0.68f * s, -0.10f * s}, {0.42f * s, 0.76f * s, 0.90f * s}, 0.05f * s, 0.04f * s, 6, barrelSteel);
    DrawCube({0.30f * s, 0.72f * s, 0.25f * s}, 0.14f * s, 0.16f * s, 0.16f * s, {75, 85, 62, 255}); // Ammo box

    // Flexible radio whip antenna swaying with time and motion
    float antSwayX = std::sin(timeSec * 4.2f + pos.x * 0.1f) * 0.18f * s;
    float antSwayZ = std::cos(timeSec * 3.6f + pos.z * 0.1f) * 0.18f * s;
    DrawCylinderEx({-0.55f * s, 0.54f * s, -1.25f * s},
                   {-0.55f * s + antSwayX, 2.75f * s, -1.25f * s + antSwayZ}, 0.025f * s, 0.012f * s, 6, barrelSteel);
    DrawSphere({-0.55f * s + antSwayX, 2.78f * s, -1.25f * s + antSwayZ}, 0.05f * s, GOLD);

    // Heavy Gun Mantlet with canvas dust boot
    DrawBeveledBox({0.0f, 0.12f * s, 1.15f * s}, 1.20f * s, 0.58f * s, 0.52f * s, armorDark);

    // High-Velocity Main Cannon Barrel + Steered Double-Baffle Muzzle Brake
    float barrelLen = (type == RegimentType::ELITE_SIGNATURE_TANK || type == RegimentType::HEAVY_TANK) ? 3.95f * s : 3.35f * s;
    float zStart = 1.15f * s - recoil * 0.68f;
    float zEnd   = zStart + barrelLen;
    DrawCylinderEx({0.0f, 0.14f * s, zStart}, {0.0f, 0.16f * s, zEnd}, 0.16f * s, 0.13f * s, 12, barrelSteel);
    // Double-baffle Muzzle Brake at tip with hollow rifled bore
    DrawCylinderEx({0.0f, 0.16f * s, zEnd - 0.35f * s}, {0.0f, 0.16f * s, zEnd + 0.14f * s},
                   0.24f * s, 0.24f * s, 10, Shade(barrelSteel, 0.85f));
    DrawSphere({0.0f, 0.16f * s, zEnd + 0.15f * s}, 0.07f * s, BLACK); // Hollow rifled muzzle opening

    if (muzzleFlashTimer > 0.0f) {
        // Multi-stage incandescent muzzle blast starburst
        DrawSphere({0.0f, 0.16f * s, zEnd + 0.55f * s}, 0.72f * s, {255, 250, 220, 255});
        DrawSphere({0.0f, 0.16f * s, zEnd + 1.05f * s}, 0.48f * s, {255, 160, 45, 235});
        // Side baffle exhaust gas vents
        DrawSphere({-0.35f * s, 0.16f * s, zEnd - 0.10f * s}, 0.32f * s, {255, 190, 75, 220});
        DrawSphere({ 0.35f * s, 0.16f * s, zEnd - 0.10f * s}, 0.32f * s, {255, 190, 75, 220});
    }

    rlPopMatrix();
}

void drawVehicle3D(Vector3 pos, float facingDeg, float recoil, float muzzleFlashTimer,
                   const NationDef& nat, RegimentType type, BuildingType packedBldg, float timeSec,
                   float pitchDeg, float rollDeg) {
    (void)packedBldg;
    DrawSoftShadowDisc(pos, 2.2f, 3.1f);

    Color bodyCol   = nat.primaryColor;
    Color cabCol    = Shade(bodyCol, 1.12f);
    Color chassis   = {42, 46, 48, 255};
    Color tireCol   = {26, 28, 30, 255};
    Color glassCol  = {85, 130, 155, 235};

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facingDeg, 0.0f, 1.0f, 0.0f);
    rlRotatef(pitchDeg, 1.0f, 0.0f, 0.0f); // Suspension terrain pitch
    rlRotatef(rollDeg, 0.0f, 0.0f, 1.0f);  // Suspension terrain roll

    // Chassis frame + Front steerable wheels with leaf springs
    DrawBeveledBox({0.0f, 0.55f, 0.0f}, 2.15f, 0.42f, 4.45f, chassis);
    // Front wheels with hub nuts
    DrawCylinderEx({-1.20f, 0.46f, 1.45f}, {-0.88f, 0.46f, 1.45f}, 0.45f, 0.45f, 12, tireCol);
    DrawSphere({-1.22f, 0.46f, 1.45f}, 0.12f, chassis);
    DrawCylinderEx({ 0.88f, 0.46f, 1.45f}, { 1.20f, 0.46f, 1.45f}, 0.45f, 0.45f, 12, tireCol);
    DrawSphere({ 1.22f, 0.46f, 1.45f}, 0.12f, chassis);

    // Sturdy Front Bumper with Mounted Recovery Winch
    DrawCube({0.0f, 0.45f, 2.38f}, 2.45f, 0.22f, 0.25f, chassis);
    DrawCylinderEx({-0.45f, 0.58f, 2.38f}, {0.45f, 0.58f, 2.38f}, 0.14f, 0.14f, 8, {85, 90, 94, 255}); // Winch cable drum

    if (type == RegimentType::RECON_HALFTRACK || type == RegimentType::AA_HALFTRACK || type == RegimentType::MOBILE_ARTILLERY) {
        // Rear half-track caterpillar treads with roadwheels & drive sprockets
        DrawBeveledBox({-1.05f, 0.46f, -0.85f}, 0.48f, 0.70f, 2.40f, tireCol);
        DrawBeveledBox({ 1.05f, 0.46f, -0.85f}, 0.48f, 0.70f, 2.40f, tireCol);
        for (float rz : {-1.6f, -1.0f, -0.4f}) {
            DrawCylinderEx({-1.25f, 0.38f, rz}, {-0.85f, 0.38f, rz}, 0.28f, 0.28f, 8, {65, 70, 72, 255});
            DrawCylinderEx({ 0.85f, 0.38f, rz}, { 1.25f, 0.38f, rz}, 0.28f, 0.28f, 8, {65, 70, 72, 255});
        }
    } else {
        // Rear dual truck axles
        for (float rz : {-0.55f, -1.55f}) {
            DrawCylinderEx({-1.24f, 0.46f, rz}, {-0.88f, 0.46f, rz}, 0.45f, 0.45f, 12, tireCol);
            DrawCylinderEx({ 0.88f, 0.46f, rz}, { 1.24f, 0.46f, rz}, 0.45f, 0.45f, 12, tireCol);
        }
    }

    // Engine Hood with stamped airflow louvers & Armored Cab with split windshield
    DrawBeveledBox({0.0f, 1.05f, 1.55f}, 1.85f, 0.75f, 1.35f, bodyCol);
    // Vertical radiator airflow louvers on front
    for (int l = -3; l <= 3; ++l) {
        DrawCube({l * 0.20f, 1.05f, 2.24f}, 0.08f, 0.55f, 0.04f, {25, 28, 30, 255});
    }

    DrawBeveledBox({0.0f, 1.38f, 0.55f}, 1.95f, 0.95f, 1.10f, cabCol);
    // Split glass windshield with center divider
    DrawCube({-0.45f, 1.55f, 1.11f}, 0.75f, 0.42f, 0.06f, glassCol);
    DrawCube({ 0.45f, 1.55f, 1.11f}, 0.75f, 0.42f, 0.06f, glassCol);
    DrawCube({0.0f, 1.55f, 1.12f}, 0.08f, 0.44f, 0.08f, bodyCol);

    // Jerry cans in side running-board racks
    DrawCube({-1.12f, 0.85f, 0.35f}, 0.18f, 0.38f, 0.28f, {75, 85, 62, 255});
    DrawCube({ 1.12f, 0.85f, 0.35f}, 0.18f, 0.38f, 0.28f, {75, 85, 62, 255});

    if (type == RegimentType::KATYUSHA_ROCKET) {
        // Iconic BM-13 Katyusha Elevated Multi-Rail Rocket Launcher + 8 Detailed RS-132 Rockets
        rlPushMatrix();
        rlTranslatef(0.0f, 1.45f, -0.95f);
        rlRotatef(-28.0f, 1.0f, 0.0f, 0.0f);
        DrawBeveledBox({0.0f, 0.0f, 0.0f}, 1.95f, 0.18f, 2.65f, {68, 76, 64, 255});
        // 8 Parallel I-Beam Rocket Launching Rails
        for (int r = -3; r <= 3; ++r) {
            float rx = r * 0.25f;
            // Guide rail I-beam
            DrawCube({rx, 0.14f, 0.0f}, 0.08f, 0.10f, 2.65f, chassis);
            // RS-132 Rocket with propellant motor & warhead
            DrawCylinderEx({rx, 0.24f, -1.25f}, {rx, 0.24f, 1.25f}, 0.08f, 0.08f, 8, {225, 228, 215, 255});
            DrawSphere({rx, 0.24f, 1.32f}, 0.09f, {195, 42, 32, 255}); // Red warhead nosecone
            // Tail stabilizing fins
            DrawCube({rx, 0.24f, -1.22f}, 0.22f, 0.02f, 0.18f, {85, 90, 94, 255});
            DrawCube({rx, 0.24f, -1.22f}, 0.02f, 0.22f, 0.18f, {85, 90, 94, 255});
        }
        rlPopMatrix();
    } else if (type == RegimentType::MOBILE_ARTILLERY) {
        // Self-Propelled 105mm/150mm Howitzer Casemate & Elevated Cannon
        DrawBeveledBox({0.0f, 1.35f, -0.65f}, 2.12f, 1.05f, 2.35f, bodyCol);
        DrawCylinderEx({0.0f, 1.65f, -0.25f - recoil * 0.5f}, {0.0f, 2.45f, 2.95f - recoil * 0.5f},
                       0.20f, 0.17f, 10, {48, 54, 56, 255});
        DrawSphere({0.0f, 2.45f, 2.95f - recoil * 0.5f}, 0.09f, BLACK); // Muzzle opening
    } else if (type == RegimentType::AA_HALFTRACK) {
        // Quad-Flak Anti-Aircraft Turret Mount on Rear Flatbed
        DrawCylinder({0.0f, 1.05f, -0.95f}, 0.75f, 0.80f, 0.55f, 12, Shade(bodyCol, 0.85f));
        DrawCylinderEx({-0.22f, 1.45f, -0.95f}, {-0.22f, 2.30f, 0.75f}, 0.07f, 0.06f, 6, chassis);
        DrawCylinderEx({ 0.22f, 1.45f, -0.95f}, { 0.22f, 2.30f, 0.75f}, 0.07f, 0.06f, 6, chassis);
        DrawCylinderEx({-0.22f, 1.65f, -0.95f}, {-0.22f, 2.50f, 0.75f}, 0.07f, 0.06f, 6, chassis);
        DrawCylinderEx({ 0.22f, 1.65f, -0.95f}, { 0.22f, 2.50f, 0.75f}, 0.07f, 0.06f, 6, chassis);
    } else if (type == RegimentType::PACKED_HQ_TRUCK) {
        // Heavy Mobile HQ Convoy Canopy with National Emblem & Gold Trim
        DrawBeveledBox({0.0f, 1.68f, -0.95f}, 2.18f, 1.55f, 2.65f, {162, 148, 108, 255});
        drawNationalInsignia3D({0.0f, 2.48f, -0.95f}, 0.55f, nat.id);
    } else {
        // Armored Half-Track Troop Compartment with interior wooden benches & roof MG
        DrawBeveledBox({0.0f, 1.18f, -0.95f}, 2.05f, 0.85f, 2.45f, bodyCol);
        // Troop bench interior
        DrawCube({-0.65f, 1.25f, -0.95f}, 0.35f, 0.12f, 2.1f, {125, 95, 55, 255});
        DrawCube({ 0.65f, 1.25f, -0.95f}, 0.35f, 0.12f, 2.1f, {125, 95, 55, 255});
        DrawCylinderEx({0.0f, 1.72f, 0.15f}, {0.0f, 1.78f, 1.15f}, 0.06f, 0.05f, 6, chassis);
    }

    if (muzzleFlashTimer > 0.0f) {
        DrawSphere({0.0f, 2.15f, 2.65f}, 0.48f, {255, 220, 100, 240});
    }

    rlPopMatrix();
}

void drawAircraft3D(Vector3 pos, float facingDeg, const NationDef& nat, float propAngleDeg) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facingDeg, 0.0f, 1.0f, 0.0f);

    Color fuseCol = nat.primaryColor;
    Color wingCol = Shade(fuseCol, 1.15f);
    Color canopy  = {110, 175, 215, 230};

    // Fuselage + Nose Engine Cowling with radiator scoop
    DrawCylinderEx({0.0f, 0.0f, -3.2f}, {0.0f, 0.0f, 2.8f}, 0.42f, 0.62f, 12, fuseCol);
    DrawSphere({0.0f, 0.0f, 2.85f}, 0.45f, Shade(fuseCol, 0.82f)); // Prop hub spinner
    // Main Swept Wings with National Roundels
    DrawBeveledBox({0.0f, 0.05f, 0.35f}, 7.6f, 0.16f, 1.45f, wingCol);
    drawNationalInsignia3D({-2.6f, 0.15f, 0.35f}, 0.48f, nat.id);
    drawNationalInsignia3D({ 2.6f, 0.15f, 0.35f}, 0.48f, nat.id);
    // Multi-pane Glass Cockpit Canopy
    DrawSphere({0.0f, 0.48f, 0.55f}, 0.42f, canopy);
    // Tail Horizontal & Vertical Stabilizers
    DrawBeveledBox({0.0f, 0.08f, -2.85f}, 2.6f, 0.12f, 0.72f, wingCol);
    DrawBeveledBox({0.0f, 0.65f, -2.85f}, 0.14f, 1.15f, 0.85f, nat.accentColor);

    // Spinning Front Propeller Blades
    rlPushMatrix();
    rlTranslatef(0.0f, 0.0f, 2.92f);
    rlRotatef(propAngleDeg, 0.0f, 0.0f, 1.0f);
    DrawCube({0.0f, 0.0f, 0.0f}, 1.85f, 0.16f, 0.06f, {30, 32, 34, 255});
    DrawCube({0.0f, 0.0f, 0.0f}, 0.16f, 1.85f, 0.06f, {30, 32, 34, 255});
    rlPopMatrix();

    rlPopMatrix();
}

void drawFighterAircraft3D(Vector3 pos, float facingDeg, float rollDeg, const NationDef& nat, float propAngleDeg, bool firingGuns) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facingDeg, 0.0f, 1.0f, 0.0f);
    rlRotatef(rollDeg, 0.0f, 0.0f, 1.0f); // Banking maneuver roll

    Color fuseCol = nat.primaryColor;
    Color wingCol = Shade(fuseCol, 1.18f);
    Color canopy  = {95, 185, 235, 240};

    // Sleek streamlined fighter fuselage with tapered nose and supercharger scoop
    DrawCylinderEx({0.0f, 0.0f, -2.6f}, {0.0f, 0.0f, 2.4f}, 0.35f, 0.52f, 12, fuseCol);
    DrawSphere({0.0f, 0.0f, 2.45f}, 0.38f, nat.accentColor); // Aerodynamic nose cone spinner
    // Chin oil-cooler radiator scoop
    DrawBeveledBox({0.0f, -0.28f, 1.1f}, 0.42f, 0.22f, 0.95f, Shade(fuseCol, 0.75f));

    // Elliptical High-Speed Fighter Wings (P-51 / Spitfire / Bf 109 aerodynamic profile)
    DrawBeveledBox({0.0f, 0.04f, 0.45f}, 6.4f, 0.14f, 1.25f, wingCol);
    drawNationalInsignia3D({-2.1f, 0.12f, 0.45f}, 0.42f, nat.id);
    drawNationalInsignia3D({ 2.1f, 0.12f, 0.45f}, 0.42f, nat.id);

    // Twin Wing-Mounted 20mm Autocannons
    DrawCylinderEx({-1.55f, 0.04f, 0.2f}, {-1.55f, 0.04f, 1.45f}, 0.045f, 0.040f, 6, {28, 28, 30, 255});
    DrawCylinderEx({ 1.55f, 0.04f, 0.2f}, { 1.55f, 0.04f, 1.45f}, 0.045f, 0.040f, 6, {28, 28, 30, 255});

    if (firingGuns) {
        // High-velocity 20mm muzzle flashes and forward tracer ejection
        DrawSphere({-1.55f, 0.04f, 1.52f}, 0.24f, Color{255, 230, 110, 245});
        DrawSphere({ 1.55f, 0.04f, 1.52f}, 0.24f, Color{255, 230, 110, 245});
    }

    // Teardrop Bubble Cockpit Canopy
    DrawSphere({0.0f, 0.38f, 0.42f}, 0.36f, canopy);
    // Tail Empennage
    DrawBeveledBox({0.0f, 0.06f, -2.35f}, 2.2f, 0.10f, 0.65f, wingCol);
    DrawBeveledBox({0.0f, 0.52f, -2.35f}, 0.12f, 0.95f, 0.75f, nat.accentColor);

    // High-RPM Propeller with yellow warning tips
    rlPushMatrix();
    rlTranslatef(0.0f, 0.0f, 2.50f);
    rlRotatef(propAngleDeg, 0.0f, 0.0f, 1.0f);
    DrawCube({0.0f, 0.0f, 0.0f}, 1.65f, 0.14f, 0.05f, {25, 28, 30, 255});
    DrawCube({0.0f, 0.0f, 0.0f}, 0.14f, 1.65f, 0.05f, {25, 28, 30, 255});
    rlPopMatrix();

    rlPopMatrix();
}

void drawDetailedRegiment3D(const RegimentInstance& reg, const NationDef& nat, const RegimentDef& rDef, float timeSec) {
    if (reg.stance == FormationStance::ENTRENCHED) {
        drawEntrenchmentRing3D(reg.pos, (rDef.category == UnitCategory::INFANTRY) ? 4.3f : 5.6f);
    }

    bool isWalking = (reg.order == OrderType::MOVE || reg.order == OrderType::ATTACK_MOVE);
    bool isPinned = (rDef.category == UnitCategory::INFANTRY && reg.organization < 30.0f);
    bool isCrouched = (reg.stance == FormationStance::ENTRENCHED);
    for (size_t i = 0; i < reg.members.size(); ++i) {
        const auto& m = reg.members[i];
        if (!m.alive) continue;

        if (rDef.category == UnitCategory::INFANTRY) {
            drawSoldier3D(m.worldPos, reg.facing, m.walkPhase, isWalking, m.muzzleFlashTimer,
                          nat, reg.type, timeSec, static_cast<int>(i), isPinned, isCrouched);
        } else if (rDef.category == UnitCategory::ARMOR) {
            drawTank3D(m.worldPos, reg.facing, m.turretAngleRad * RAD2DEG, m.recoil, m.muzzleFlashTimer,
                       nat, reg.type, timeSec);
        } else {
            drawVehicle3D(m.worldPos, reg.facing, m.recoil, m.muzzleFlashTimer,
                          nat, reg.type, reg.packedBuildingType, timeSec);
        }
    }
}

void drawDetailedRegiment3D(const RTSRegiment& reg, const NationDef& nat, const RegimentDef& rDef, float timeSec) {
    if (reg.stance == FormationStance::ENTRENCHED) {
        drawEntrenchmentRing3D(reg.pos, (rDef.category == UnitCategory::INFANTRY) ? 4.3f : 5.6f);
    }

    bool isWalking = (Vector3Distance(reg.pos, reg.targetPos) > 1.2f);
    bool isPinned = (rDef.category == UnitCategory::INFANTRY && reg.organization < 30.0f);
    bool isCrouched = (reg.stance == FormationStance::ENTRENCHED);
    for (size_t i = 0; i < reg.subUnits.size(); ++i) {
        const auto& m = reg.subUnits[i];
        if (!m.alive) continue;

        float muzzleFlash = (m.recoil > 0.32f) ? (m.recoil * 0.15f) : 0.0f;
        if (rDef.category == UnitCategory::INFANTRY) {
            drawSoldier3D(m.worldPos, reg.facing, m.animPhase, isWalking, muzzleFlash,
                          nat, reg.type, timeSec, static_cast<int>(i), isPinned, isCrouched);
        } else if (rDef.category == UnitCategory::ARMOR) {
            drawTank3D(m.worldPos, reg.facing, m.turretYaw, m.recoil, muzzleFlash,
                       nat, reg.type, timeSec, m.pitchDeg, m.rollDeg);
        } else {
            drawVehicle3D(m.worldPos, reg.facing, m.recoil, muzzleFlash,
                          nat, reg.type, reg.packedBuildingType, timeSec, m.pitchDeg, m.rollDeg);
        }
    }
}

template <typename TBuilding>
static void drawDetailedBuildingImpl(const TBuilding& b, const NationDef& nat, const BuildingDef& bDef, float timeSec) {
    (void)bDef;
    Vector3 p = b.pos;
    bool neutral = (b.teamId < 0);
    Color teamCol = neutral ? Color{135, 138, 132, 255} : nat.primaryColor;
    Color accent  = neutral ? LIGHTGRAY : nat.accentColor;
    Color concrete = {152, 154, 148, 255};
    Color slateRoof = {58, 62, 68, 255};
    Color terraRoof = {168, 76, 52, 255};
    Color windowGlass = {245, 235, 185, 240}; // Warm illuminated interior window light

    // Proportional Reinforced Concrete Foundation Pad fitted snugly to building footprint
    float padSize = 10.0f;
    if (b.type == BuildingType::CORPS_HQ || b.type == BuildingType::CITY_DEPOT) padSize = 15.5f;
    else if (b.type == BuildingType::AIRFIELD) padSize = 14.5f;
    else if (b.type == BuildingType::INFANTRY_HQ || b.type == BuildingType::MECH_HQ || b.type == BuildingType::ARMOR_HQ) padSize = 12.0f;
    else if (b.type == BuildingType::BUNKER || b.type == BuildingType::AA_BATTERY) padSize = 5.8f;
    else if (b.type == BuildingType::SUPPLY_DEPOT) padSize = 8.8f;
    else if (b.type == BuildingType::OIL_DERRICK) padSize = 6.4f;

    // Foundation stone/concrete plinth with beveled curbs
    DrawBeveledBox({p.x, p.y + 0.15f, p.z}, padSize, 0.32f, padSize, {76, 80, 75, 255});

    if (b.type == BuildingType::CORPS_HQ) {
        // Multi-Story WWII Supreme Headquarters with Gabled Slate Roof, Stone Quoins, Portico & Willys Jeep
        // Main 2-story headquarters building
        DrawBeveledBox({p.x, p.y + 3.2f, p.z - 1.2f}, 11.5f, 5.8f, 7.5f, teamCol);
        // Architectural stone corner quoins
        for (float qx : {-5.6f, 5.6f}) {
            for (float qz : {-4.8f, 2.4f}) {
                DrawCube({p.x + qx, p.y + 3.2f, p.z + qz}, 0.55f, 5.8f, 0.55f, {195, 190, 182, 255});
            }
        }
        // Gabled Pitched Slate Roof with overhang eaves
        DrawPitchedGabledRoof({p.x, p.y + 6.8f, p.z - 1.2f}, 12.2f, 2.4f, 7.8f, slateRoof, {42, 45, 48, 255});

        // Recessed multi-pane windows on first and second floors
        for (int floor = 0; floor < 2; ++floor) {
            float wy = p.y + 1.8f + floor * 2.5f;
            for (int wx = -2; wx <= 2; ++wx) {
                if (floor == 0 && wx == 0) continue; // Entrance doorway
                DrawRecessedWindow({p.x + wx * 2.2f, wy, p.z + 2.56f}, 1.15f, 1.45f, 0.16f, WHITE, windowGlass);
            }
        }

        // Stone Entrance Portico with steps, timber posts and double entrance doors
        DrawBeveledBox({p.x, p.y + 0.35f, p.z + 3.2f}, 3.4f, 0.40f, 1.6f, concrete);
        DrawCube({p.x - 1.3f, p.y + 1.7f, p.z + 3.8f}, 0.22f, 2.6f, 0.22f, WHITE);
        DrawCube({p.x + 1.3f, p.y + 1.7f, p.z + 3.8f}, 0.22f, 2.6f, 0.22f, WHITE);
        DrawPitchedGabledRoof({p.x, p.y + 3.3f, p.z + 3.5f}, 3.8f, 0.8f, 1.8f, slateRoof, {42, 45, 48, 255});
        // Double entrance doors
        DrawCube({p.x, p.y + 1.6f, p.z + 2.58f}, 1.8f, 2.4f, 0.08f, {92, 58, 34, 255});

        // Forward Command Operations Bunker Wing with Communications Entry Conduits
        DrawBeveledBox({p.x - 2.8f, p.y + 2.1f, p.z + 4.2f}, 5.4f, 3.6f, 4.2f, Shade(teamCol, 0.86f));
        DrawBeveledBox({p.x - 2.8f, p.y + 4.1f, p.z + 4.2f}, 5.8f, 0.5f, 4.6f, slateRoof);
        DrawRecessedWindow({p.x - 2.8f, p.y + 2.2f, p.z + 6.32f}, 1.4f, 1.2f, 0.16f, WHITE, windowGlass);

        // Vehicle Deployment Bay with paved concrete apron
        DrawBeveledBox({p.x + 3.6f, p.y + 2.2f, p.z + 4.0f}, 4.6f, 3.8f, 4.4f, {72, 78, 74, 255});
        DrawCube({p.x + 3.6f, p.y + 1.6f, p.z + 6.22f}, 3.4f, 2.6f, 0.1f, {24, 28, 26, 255}); // Open bay door

        // Parked Reconnaissance Staff Jeep on the apron
        Vector3 jeepPos = {p.x + 6.8f, p.y + 0.5f, p.z + 2.8f};
        DrawBeveledBox({jeepPos.x, jeepPos.y + 0.45f, jeepPos.z}, 1.6f, 0.55f, 3.1f, Shade(teamCol, 0.78f));
        DrawBeveledBox({jeepPos.x, jeepPos.y + 0.85f, jeepPos.z - 0.4f}, 1.5f, 0.45f, 1.4f, {95, 110, 85, 230}); // Canvas top
        // Folded windshield
        DrawCube({jeepPos.x, jeepPos.y + 0.95f, jeepPos.z + 0.45f}, 1.4f, 0.35f, 0.06f, {115, 165, 195, 220});
        // 4 Jeep wheels + spare tire on rear
        for (float side : {-0.80f, 0.80f}) {
            DrawCylinderEx({jeepPos.x + side, jeepPos.y + 0.28f, jeepPos.z - 0.9f},
                           {jeepPos.x + side * 1.15f, jeepPos.y + 0.28f, jeepPos.z - 0.9f}, 0.28f, 0.28f, 8, {28, 30, 32, 255});
            DrawCylinderEx({jeepPos.x + side, jeepPos.y + 0.28f, jeepPos.z + 0.9f},
                           {jeepPos.x + side * 1.15f, jeepPos.y + 0.28f, jeepPos.z + 0.9f}, 0.28f, 0.28f, 8, {28, 30, 32, 255});
        }
        DrawCylinderEx({jeepPos.x, jeepPos.y + 0.55f, jeepPos.z - 1.65f},
                       {jeepPos.x, jeepPos.y + 0.55f, jeepPos.z - 1.50f}, 0.26f, 0.26f, 8, {28, 30, 32, 255});

        // Sandbag Security Revetment at entrance
        Color sandbagCol = {186, 168, 128, 255};
        DrawCube({p.x - 2.8f, p.y + 0.65f, p.z + 6.6f}, 3.8f, 0.75f, 0.85f, sandbagCol);
        // Sentry guardhouse booth with striped roof
        DrawBeveledBox({p.x + 1.2f, p.y + 1.45f, p.z + 6.8f}, 1.2f, 2.5f, 1.2f, WHITE);
        DrawPitchedGabledRoof({p.x + 1.2f, p.y + 2.85f, p.z + 6.8f}, 1.4f, 0.6f, 1.4f, RED, WHITE);

        // Rooftop Communications Center: 4-Leg Structural Steel Lattice Radio Mast
        DrawCylinder({p.x - 3.5f, p.y + 7.8f, p.z - 1.5f}, 0.45f, 0.75f, 4.2f, 8, {55, 62, 65, 255});
        rlPushMatrix();
        rlTranslatef(p.x - 3.5f, p.y + 12.2f, p.z - 1.5f);
        rlRotatef(timeSec * 70.0f, 0.0f, 1.0f, 0.0f);
        DrawBeveledBox({0.0f, 0.0f, 0.0f}, 3.2f, 1.0f, 0.38f, accent);
        rlPopMatrix();
        // Blinking Red Mast Beacon
        float blink = (std::fmod(timeSec, 1.0f) < 0.5f) ? 1.0f : 0.2f;
        DrawSphere({p.x - 3.5f, p.y + 13.0f, p.z - 1.5f}, 0.22f, ColorAlpha(RED, blink));

        // Flagpole & Waving National Banner
        DrawCylinder({p.x + 4.5f, p.y + 7.2f, p.z - 1.5f}, 0.12f, 0.12f, 5.8f, 8, LIGHTGRAY);
        float wave = std::sin(timeSec * 4.5f) * 0.28f;
        DrawCube({p.x + 5.9f, p.y + 12.2f, p.z - 1.5f + wave}, 2.6f, 1.4f, 0.15f, accent);
        drawNationalInsignia3D({p.x, p.y + 7.2f, p.z - 1.2f}, 2.2f, nat.id);
    } else if (b.type == BuildingType::INFANTRY_HQ || b.type == BuildingType::MECH_HQ || b.type == BuildingType::ARMOR_HQ) {
        // Military Division Factory with Sawtooth Roof, Brick Pilasters, Twin Smokestacks & Overhead Gantry Crane
        float w = (b.type == BuildingType::ARMOR_HQ) ? 10.8f : 9.4f;
        float h = (b.type == BuildingType::ARMOR_HQ) ? 5.2f : 4.2f;
        DrawBeveledBox({p.x, p.y + h * 0.5f + 0.5f, p.z}, w, h, 8.6f, teamCol);
        // Architectural brick pilasters along the walls
        for (int pil = -2; pil <= 2; ++pil) {
            DrawCube({p.x + pil * (w * 0.22f), p.y + h * 0.5f + 0.5f, p.z + 4.35f}, 0.45f, h + 0.2f, 0.18f, {172, 85, 58, 255});
        }
        // Sawtooth industrial factory roof
        DrawPitchedGabledRoof({p.x, p.y + h + 1.2f, p.z}, w + 0.6f, 1.6f, 9.2f, slateRoof, {42, 45, 48, 255});
        // Glass clerestory skylight windows on roof
        for (int sky = -2; sky <= 2; ++sky) {
            DrawCube({p.x + sky * 1.8f, p.y + h + 1.2f, p.z}, 1.2f, 0.65f, 8.8f, {135, 185, 215, 220});
        }

        // Factory Smokestacks with iron reinforcing rings and subtle heat vapor
        if (b.type != BuildingType::INFANTRY_HQ) {
            for (float sx : {-w * 0.35f, -w * 0.14f}) {
                DrawCylinder({p.x + sx, p.y + h + 0.8f, p.z - 2.6f}, 0.55f, 0.65f, 4.2f, 12, {62, 54, 52, 255});
                // Iron reinforcement bands
                DrawCylinder({p.x + sx, p.y + h + 2.2f, p.z - 2.6f}, 0.68f, 0.68f, 0.12f, 10, {28, 30, 32, 255});
                DrawCylinder({p.x + sx, p.y + h + 3.8f, p.z - 2.6f}, 0.68f, 0.68f, 0.12f, 10, {28, 30, 32, 255});
                // Subtle heat vapor
                float sTime = timeSec * 1.8f;
                for (int sm = 0; sm < 3; ++sm) {
                    float smY = p.y + h + 4.4f + sm * 1.0f + std::fmod(sTime + sm * 0.6f, 2.6f);
                    float smDrift = (smY - (p.y + h)) * 0.32f;
                    DrawSphere({p.x + sx + smDrift, smY, p.z - 2.6f}, 0.38f + sm * 0.18f,
                               ColorAlpha(Color{220, 222, 225, 255}, 0.18f - sm * 0.05f));
                }
            }
        }

        // Industrial roll-up vehicle hangar doors with yellow/black hazard threshold
        DrawCube({p.x, p.y + 2.0f, p.z + 4.34f}, 4.6f, 3.2f, 0.12f, {45, 48, 52, 255});
        DrawCube({p.x, p.y + 0.42f, p.z + 4.36f}, 4.8f, 0.15f, 0.15f, YELLOW); // Hazard border

        // External Electrical Transformer Cage with high-voltage warning
        Vector3 transPos = {p.x + w * 0.42f + 1.2f, p.y + 0.5f, p.z - 1.5f};
        DrawBeveledBox({transPos.x, transPos.y + 0.85f, transPos.z}, 1.6f, 1.4f, 1.8f, {78, 85, 82, 255});
        DrawCylinder({transPos.x, transPos.y + 1.65f, transPos.z}, 0.18f, 0.18f, 0.65f, 8, {195, 205, 215, 255}); // Bushing

        // Stacks of raw armor plate and roadwheels in yard
        DrawCube({p.x - w * 0.45f - 1.2f, p.y + 0.35f, p.z + 1.5f}, 1.8f, 0.45f, 1.8f, {55, 58, 62, 255});

        drawNationalInsignia3D({p.x, p.y + h + 1.32f, p.z}, 1.8f, nat.id);
    } else if (b.type == BuildingType::SUPPLY_DEPOT) {
        // Logistics Staging Depot: Timber Warehouse + Raised Loading Dock + Stacks of Ammo & Fuel
        DrawBeveledBox({p.x - 2.0f, p.y + 2.2f, p.z - 1.2f}, 6.2f, 3.4f, 5.6f, Shade(teamCol, 0.92f));
        DrawPitchedGabledRoof({p.x - 2.0f, p.y + 4.3f, p.z - 1.2f}, 6.8f, 1.4f, 6.2f, slateRoof, {42, 45, 48, 255});

        // Raised wooden loading dock platform
        DrawBeveledBox({p.x + 2.8f, p.y + 0.85f, p.z + 0.5f}, 3.8f, 0.70f, 4.8f, {125, 95, 55, 255});

        // Stacks of wooden ammo crates with steel straps
        DrawBeveledBox({p.x + 2.8f, p.y + 1.6f, p.z + 1.8f}, 2.2f, 0.85f, 2.2f, {158, 132, 82, 255});
        DrawBeveledBox({p.x + 2.8f, p.y + 2.3f, p.z + 1.8f}, 1.5f, 0.65f, 1.5f, {138, 114, 68, 255});

        // Pyramidal cluster of 55-gallon olive fuel drums with bungs
        for (int d = 0; d < 4; ++d) {
            DrawCylinder({p.x + 2.2f + (d % 2) * 1.1f, p.y + 1.2f, p.z - 1.2f - (d / 2) * 1.1f},
                         0.42f, 0.42f, 1.3f, 10, {68, 88, 62, 255});
        }
    } else if (b.type == BuildingType::AIRFIELD) {
        // Real Flight Operations Airfield: Textured Tarmac Runway, Striping, Octagonal Control Tower & Windsock
        // Tarmac runway with dark asphalt expansion joints
        DrawBeveledBox({p.x + 1.5f, p.y + 0.55f, p.z}, 7.6f, 0.2f, 14.5f, {45, 48, 52, 255});
        // Center dashed flight line
        for (int dash = -3; dash <= 3; ++dash) {
            DrawCube({p.x + 1.5f, p.y + 0.68f, p.z + dash * 2.2f}, 0.38f, 0.05f, 1.2f, RAYWHITE);
        }
        // Runway Piano-Key Threshold Striping & Numerals
        for (int st = -2; st <= 2; ++st) {
            DrawCube({p.x + 1.5f + st * 1.2f, p.y + 0.68f, p.z + 6.4f}, 0.25f, 0.05f, 1.2f, YELLOW);
            DrawCube({p.x + 1.5f + st * 1.2f, p.y + 0.68f, p.z - 6.4f}, 0.25f, 0.05f, 1.2f, YELLOW);
        }

        // Octagonal Flight Control Tower with 360-degree observation cab & balcony catwalk
        DrawCylinder({p.x - 4.2f, p.y + 0.5f, p.z - 2.2f}, 1.8f, 2.2f, 6.2f, 8, concrete);
        // Catwalk balcony
        DrawCylinder({p.x - 4.2f, p.y + 6.6f, p.z - 2.2f}, 2.5f, 2.5f, 0.15f, 12, {48, 52, 55, 255});
        // Glass observation cab
        DrawCylinder({p.x - 4.2f, p.y + 6.8f, p.z - 2.2f}, 2.2f, 2.0f, 1.8f, 8, {110, 185, 215, 230});
        // Roof anemometer with spinning wind cups
        Vector3 anemoPos = {p.x - 4.2f, p.y + 8.8f, p.z - 2.2f};
        DrawCylinder(anemoPos, 0.06f, 0.06f, 0.85f, 6, LIGHTGRAY);
        rlPushMatrix();
        rlTranslatef(anemoPos.x, anemoPos.y + 0.85f, anemoPos.z);
        rlRotatef(timeSec * 240.0f, 0.0f, 1.0f, 0.0f);
        DrawCube({0.0f, 0.0f, 0.0f}, 0.85f, 0.04f, 0.04f, RED);
        DrawSphere({-0.42f, 0.0f, 0.0f}, 0.08f, RED);
        DrawSphere({ 0.42f, 0.0f, 0.0f}, 0.08f, RED);
        rlPopMatrix();

        // Directional orange/white striped aviation windsock
        Vector3 sockPole = {p.x - 3.8f, p.y + 0.5f, p.z + 4.8f};
        DrawCylinder(sockPole, 0.08f, 0.08f, 4.2f, 8, LIGHTGRAY);
        float sockAngle = std::sin(timeSec * 2.0f) * 0.22f;
        DrawCylinderEx({sockPole.x, sockPole.y + 4.0f, sockPole.z},
                       {sockPole.x + 1.4f, sockPole.y + 3.8f + sockAngle, sockPole.z + 0.4f},
                       0.32f, 0.12f, 8, ORANGE);

        // Dispersal hardstand with parked fighter plane
        drawAircraft3D({p.x + 1.5f, p.y + 1.35f, p.z - 1.0f}, 0.0f, nat, timeSec * 720.0f);
    } else if (b.type == BuildingType::CITY_DEPOT) {
        // European Urban Town Center: Town Hall, Terracotta Roofs, Radial Plaza & Gothic Clocktower
        Color brick1 = {168, 152, 132, 255};
        Color brick2 = {142, 128, 112, 255};

        // Main Town Hall Building with gabled terracotta roof
        DrawBeveledBox({p.x - 2.4f, p.y + 3.6f, p.z - 1.8f}, 6.8f, 6.2f, 5.6f, brick1);
        DrawPitchedGabledRoof({p.x - 2.4f, p.y + 7.6f, p.z - 1.8f}, 7.4f, 2.2f, 6.2f, terraRoof, {88, 42, 32, 255});

        // Recessed arched entrance loggia and multi-pane windows
        for (int wRow = 0; wRow < 2; ++wRow) {
            float rY = p.y + 2.5f + wRow * 2.5f;
            DrawRecessedWindow({p.x - 3.8f, rY, p.z + 1.05f}, 1.1f, 1.4f, 0.15f, WHITE, windowGlass);
            DrawRecessedWindow({p.x - 1.0f, rY, p.z + 1.05f}, 1.1f, 1.4f, 0.15f, WHITE, windowGlass);
        }

        // Soaring Gothic Clocktower with 4 Clock Faces, Belfry & Verdigris Spire
        DrawBeveledBox({p.x - 4.2f, p.y + 6.5f, p.z - 2.8f}, 2.6f, 12.0f, 2.6f, brick2);
        // Stone clock dial face
        DrawCircle3D({p.x - 4.2f, p.y + 11.2f, p.z - 1.48f}, 0.68f, {0, 0, 1}, 0.0f, RAYWHITE);
        DrawCircle3D({p.x - 4.2f, p.y + 11.2f, p.z - 1.47f}, 0.65f, {0, 0, 1}, 0.0f, {245, 240, 225, 255});
        // Clock hands
        DrawLine3D({p.x - 4.2f, p.y + 11.2f, p.z - 1.46f}, {p.x - 4.2f, p.y + 11.6f, p.z - 1.46f}, BLACK);
        DrawLine3D({p.x - 4.2f, p.y + 11.2f, p.z - 1.46f}, {p.x - 3.9f, p.y + 11.2f, p.z - 1.46f}, BLACK);

        // Verdigris Copper Spire
        DrawCylinderEx({p.x - 4.2f, p.y + 12.5f, p.z - 2.8f}, {p.x - 4.2f, p.y + 17.5f, p.z - 2.8f},
                       1.6f, 0.08f, 8, {65, 125, 110, 255});

        // Secondary Merchant House with Half-Timbering (Fachwerk)
        DrawBeveledBox({p.x + 3.6f, p.y + 2.8f, p.z + 2.2f}, 5.2f, 4.6f, 5.4f, brick2);
        DrawPitchedGabledRoof({p.x + 3.6f, p.y + 5.8f, p.z + 2.2f}, 5.6f, 1.8f, 5.8f, slateRoof, {42, 45, 48, 255});

        // Town Square Plaza: Carved Stone Fountain with Water
        Vector3 fountPos = {p.x + 1.5f, p.y + 0.5f, p.z - 3.5f};
        DrawCylinder(fountPos, 1.8f, 1.8f, 0.45f, 12, {165, 160, 152, 255});
        DrawCylinder({fountPos.x, fountPos.y + 0.25f, fountPos.z}, 1.5f, 1.5f, 0.25f, 12, {75, 135, 165, 220}); // Fountain pool
        DrawCylinder({fountPos.x, fountPos.y + 0.45f, fountPos.z}, 0.35f, 0.25f, 1.2f, 8, {165, 160, 152, 255});

        // Controlling Nation Banner on Town Square
        DrawCylinder({p.x + 3.8f, p.y + 0.5f, p.z - 4.5f}, 0.14f, 0.14f, 6.5f, 8, LIGHTGRAY);
        DrawCube({p.x + 5.1f, p.y + 6.2f, p.z - 4.5f}, 2.4f, 1.4f, 0.18f, teamCol);
    } else if (b.type == BuildingType::OIL_DERRICK) {
        // Petrochemical Refinery: Fractionating Column with Platform Catwalks, Storage Tanks & Flare Stack
        // Large Cylindrical Crude Storage Tanks with spiral ladders
        DrawCylinder({p.x - 3.2f, p.y + 0.5f, p.z + 1.8f}, 2.6f, 2.6f, 4.2f, 16, {185, 190, 194, 255});
        DrawCylinder({p.x + 2.8f, p.y + 0.5f, p.z + 2.2f}, 2.1f, 2.1f, 3.6f, 16, {172, 178, 182, 255});

        // Tall Fractionating Distillation Column with Exterior Piping & Catwalk Platforms
        DrawCylinder({p.x + 1.2f, p.y + 0.5f, p.z - 2.6f}, 1.15f, 1.35f, 11.5f, 16, {92, 98, 104, 255});
        // 3 Platform Catwalks
        for (float py : {4.5f, 8.0f, 11.5f}) {
            DrawCylinder({p.x + 1.2f, p.y + py, p.z - 2.6f}, 1.85f, 1.85f, 0.12f, 12, {48, 52, 55, 255});
        }
        // Flare stack pilot flame
        DrawSphere({p.x + 1.2f, p.y + 12.4f, p.z - 2.6f}, 0.45f, {255, 140, 25, 240});

        // Pipeline Manifolds with Yellow Pressure Valves
        DrawCylinderEx({p.x - 3.2f, p.y + 1.4f, p.z + 1.8f}, {p.x + 1.2f, p.y + 1.4f, p.z - 2.6f},
                       0.14f, 0.14f, 8, {68, 72, 75, 255});
        DrawSphere({p.x - 1.0f, p.y + 1.4f, p.z - 0.4f}, 0.22f, YELLOW); // Valve handwheel

        DrawBeveledBox({p.x - 2.5f, p.y + 2.0f, p.z - 2.6f}, 4.2f, 3.0f, 3.6f, teamCol);
    } else if (b.type == BuildingType::BUNKER) {
        // Reinforced Concrete Pillbox Casemate + Steered Anti-Ricochet Firing Slits + Periscope Dome
        drawEntrenchmentRing3D(p, 5.2f);
        DrawCylinder({p.x, p.y + 0.5f, p.z}, 3.8f, 4.2f, 3.2f, 12, concrete);
        // Anti-ricochet stepped firing embrasure
        DrawCube({p.x, p.y + 2.2f, p.z + 3.65f}, 2.8f, 0.45f, 0.42f, {18, 20, 18, 255});
        DrawCylinderEx({p.x, p.y + 2.1f, p.z + 2.0f}, {p.x, p.y + 2.1f, p.z + 5.6f}, 0.16f, 0.14f, 8, {38, 42, 44, 255});
        // Roof Armored Periscope Dome
        DrawSphere({p.x, p.y + 3.75f, p.z}, 0.45f, {45, 48, 52, 255});
    } else {
        // Flak 88 Heavy Gun Emplacement with Cruciform Outriggers & Ready Ammunition Racks
        drawEntrenchmentRing3D(p, 4.8f);
        // Cruciform ground firing platform
        DrawCube({p.x, p.y + 0.65f, p.z}, 5.5f, 0.28f, 0.65f, {52, 56, 58, 255});
        DrawCube({p.x, p.y + 0.65f, p.z}, 0.65f, 0.28f, 5.5f, {52, 56, 58, 255});
        // Rotating gun carriage with angled shield & 88mm gun
        rlPushMatrix();
        rlTranslatef(p.x, p.y + 1.4f, p.z);
        rlRotatef(std::sin(timeSec * 0.9f) * 35.0f, 0.0f, 1.0f, 0.0f);
        DrawBeveledBox({0.0f, 0.4f, 0.4f}, 2.2f, 1.6f, 0.35f, teamCol);
        DrawCylinderEx({0.0f, 0.6f, -0.8f}, {0.0f, 2.4f, 4.5f}, 0.18f, 0.14f, 10, {38, 42, 45, 255});
        // Perforated muzzle brake
        DrawCylinderEx({0.0f, 2.4f, 4.3f}, {0.0f, 2.5f, 4.7f}, 0.22f, 0.22f, 8, {28, 30, 32, 255});
        rlPopMatrix();

        // Ready ammunition racks with brass 88mm shell casings
        for (int s = 0; s < 4; ++s) {
            DrawCylinder({p.x + 2.0f + s * 0.25f, p.y + 0.75f, p.z - 1.8f}, 0.08f, 0.08f, 0.85f, 6, GOLD);
        }
    }
}

void drawDetailedBuilding3D(const RTSBuilding& b, const NationDef& nat, const BuildingDef& bDef, float timeSec) {
    drawDetailedBuildingImpl(b, nat, bDef, timeSec);
}

void drawConstructionSite3D(const RTSBuilding& b, const NationDef& nat, const BuildingDef& bDef, float timeSec) {
    Vector3 p = b.pos;
    float progress = std::clamp(b.constructProgress, 0.0f, 1.0f);

    float w = 12.0f;
    float d = 12.0f;
    float h = 6.0f;
    if (b.type == BuildingType::AIRFIELD) {
        w = 18.0f; d = 10.0f; h = 7.0f;
    } else if (b.type == BuildingType::ARMOR_HQ || b.type == BuildingType::MECH_HQ) {
        w = 14.0f; d = 12.0f; h = 6.5f;
    } else if (b.type == BuildingType::SUPPLY_DEPOT) {
        w = 11.0f; d = 11.0f; h = 5.0f;
    } else if (b.type == BuildingType::BUNKER || b.type == BuildingType::AA_BATTERY) {
        w = 8.0f; d = 8.0f; h = 4.0f;
    }

    // 1. Excavation Pad & Gravel Foundation
    DrawCube({p.x, p.y + 0.12f, p.z}, w + 2.0f, 0.24f, d + 2.0f, {62, 58, 52, 255});

    // Concrete Footings at Corners
    float halfW = w * 0.5f;
    float halfD = d * 0.5f;
    Vector3 corners[4] = {
        {p.x - halfW, p.y, p.z - halfD},
        {p.x + halfW, p.y, p.z - halfD},
        {p.x + halfW, p.y, p.z + halfD},
        {p.x - halfW, p.y, p.z + halfD}
    };
    for (int i = 0; i < 4; ++i) {
        DrawCube({corners[i].x, corners[i].y + 0.35f, corners[i].z}, 1.4f, 0.7f, 1.4f, {95, 92, 88, 255});
    }

    // 2. Rising Building Structure (Rises vertically out of the ground)
    rlPushMatrix();
    rlTranslatef(0.0f, (progress - 1.0f) * h * 0.82f, 0.0f);
    rlScalef(1.0f, std::max(0.08f, progress), 1.0f);
    drawDetailedBuildingImpl(b, nat, bDef, timeSec);
    rlPopMatrix();

    // 3. Wooden & Steel Scaffolding Framework
    Color timberCol = {138, 102, 65, 255};
    Color steelCol  = {88, 92, 98, 255};

    for (int i = 0; i < 4; ++i) {
        DrawCylinder({corners[i].x, corners[i].y + h * 0.5f, corners[i].z}, 0.16f, 0.16f, h, 6, timberCol);
        DrawCylinder({corners[i].x, corners[i].y + h * 0.35f, corners[i].z}, 0.22f, 0.22f, 0.12f, 6, steelCol);
        DrawCylinder({corners[i].x, corners[i].y + h * 0.75f, corners[i].z}, 0.22f, 0.22f, 0.12f, 6, steelCol);
    }

    // Horizontal Scaffolding Railings & Cross Braces
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        DrawLine3D({corners[i].x, p.y + h * 0.5f, corners[i].z}, {corners[next].x, p.y + h * 0.5f, corners[next].z}, timberCol);
        DrawLine3D({corners[i].x, p.y + h * 0.95f, corners[i].z}, {corners[next].x, p.y + h * 0.95f, corners[next].z}, timberCol);
        DrawLine3D({corners[i].x, p.y + 0.5f, corners[i].z}, {corners[next].x, p.y + h * 0.9f, corners[next].z}, steelCol);
    }

    // 4. Rotating Hoist Crane on Scaffold Top
    Vector3 craneBase = {p.x - halfW * 0.6f, p.y + h + 0.1f, p.z - halfD * 0.6f};
    DrawCylinder({craneBase.x, craneBase.y + 1.2f, craneBase.z}, 0.18f, 0.18f, 2.4f, 6, {195, 145, 45, 255});
    float boomYaw = std::sin(timeSec * 1.6f + b.id * 0.8f) * 45.0f;
    float boomLen = 5.2f;
    float boomRad = boomYaw * DEG2RAD;
    Vector3 boomTip = {
        craneBase.x + std::sin(boomRad) * boomLen,
        craneBase.y + 2.8f,
        craneBase.z + std::cos(boomRad) * boomLen
    };
    DrawLine3D({craneBase.x, craneBase.y + 2.2f, craneBase.z}, boomTip, {220, 160, 45, 255});
    DrawLine3D({craneBase.x, craneBase.y + 2.4f, craneBase.z}, boomTip, {80, 80, 80, 255});

    float cableHang = 2.4f + std::sin(timeSec * 2.8f) * 0.4f;
    Vector3 hookPos = {boomTip.x, boomTip.y - cableHang, boomTip.z};
    DrawLine3D(boomTip, hookPos, {60, 60, 60, 255});
    DrawCube({hookPos.x, hookPos.y - 0.25f, hookPos.z}, 2.4f, 0.35f, 0.35f, {75, 82, 92, 255});

    // 5. Welding Sparks & Hammering Particle Bursts
    float sparkCycle = std::fmod(timeSec * 3.5f + b.id * 1.25f, 2.5f);
    if (sparkCycle < 0.95f) {
        Vector3 sparkP = {
            corners[0].x + 0.3f,
            p.y + h * (0.35f + 0.4f * std::sin(timeSec * 2.0f)),
            corners[0].z + 0.3f
        };
        DrawSphere(sparkP, 0.32f, Color{220, 245, 255, 255});
        for (int s = 0; s < 4; ++s) {
            float sAngle = s * 1.57f + timeSec * 12.0f;
            Vector3 sEnd = {
                sparkP.x + std::cos(sAngle) * 0.65f,
                sparkP.y - 0.45f,
                sparkP.z + std::sin(sAngle) * 0.65f
            };
            DrawLine3D(sparkP, sEnd, ORANGE);
        }
    }

    // 6. 3D In-World Floating Construction Billboard
    float barY = p.y + h + 3.8f;
    float barW = 8.5f;
    float barH = 0.55f;
    DrawCube({p.x, barY, p.z}, barW + 0.4f, barH + 0.25f, 0.18f, {22, 26, 24, 235});
    float fillW = barW * progress;
    if (fillW > 0.1f) {
        float fillX = (p.x - barW * 0.5f) + fillW * 0.5f;
        Color barCol = (progress < 0.35f) ? ORANGE : (progress < 0.75f ? GOLD : LIME);
        DrawCube({fillX, barY, p.z + 0.05f}, fillW, barH, 0.14f, barCol);
    }
}

void drawDetailedBuilding3D(const BuildingInstance& b, const NationDef& nat, const BuildingDef& bDef, float timeSec) {
    drawDetailedBuildingImpl(b, nat, bDef, timeSec);
}

void drawDetailedTree3D(Vector3 pos, float scale, BiomeType biome, float timeSec) {
    DrawSoftShadowDisc(pos, 2.6f * scale, 2.6f * scale);

    // Subtle atmospheric wind sway propagating across the canopy
    float windFreq = 1.35f;
    float swayPhase = pos.x * 0.075f + pos.z * 0.065f;
    float swayX = std::sin(timeSec * windFreq + swayPhase) * (0.28f * scale);
    float swayZ = std::cos(timeSec * (windFreq * 0.85f) + swayPhase) * (0.22f * scale);

    Color trunkCol = {82, 56, 34, 255};
    DrawCylinderEx(pos, {pos.x + swayX * 0.35f, pos.y + 3.4f * scale, pos.z + swayZ * 0.35f}, 0.48f * scale, 0.30f * scale, 8, trunkCol);

    Vector3 swayTop = {pos.x + swayX, pos.y, pos.z + swayZ};

    if (biome == BiomeType::STALINGRAD_WINTER || biome == BiomeType::ARDENNES_WINTER) {
        // 4-Tiered Majestic Snow-Dusted Conifer Pine
        Color pineDark = {32, 64, 38, 255};
        Color snowCap  = {230, 238, 245, 255};
        DrawCylinderEx({swayTop.x, pos.y + 2.0f * scale, swayTop.z}, {swayTop.x, pos.y + 5.2f * scale, swayTop.z}, 2.6f * scale, 0.45f * scale, 8, pineDark);
        DrawCylinderEx({swayTop.x, pos.y + 4.2f * scale, swayTop.z}, {swayTop.x, pos.y + 7.4f * scale, swayTop.z}, 2.1f * scale, 0.35f * scale, 8, pineDark);
        DrawCylinderEx({swayTop.x, pos.y + 6.2f * scale, swayTop.z}, {swayTop.x, pos.y + 9.2f * scale, swayTop.z}, 1.5f * scale, 0.20f * scale, 8, pineDark);
        DrawCylinderEx({swayTop.x, pos.y + 8.2f * scale, swayTop.z}, {swayTop.x, pos.y + 10.8f * scale, swayTop.z}, 0.85f * scale, 0.05f * scale, 8, snowCap);
    } else if (biome == BiomeType::EL_ALAMEIN_DESERT || biome == BiomeType::PACIFIC_ATOLL) {
        // Multi-Cluster Curved Palm with Fan Fronds
        Color palmGreen = {52, 102, 38, 255};
        DrawSphere({swayTop.x, pos.y + 4.4f * scale, swayTop.z}, 1.95f * scale, palmGreen);
        DrawSphere({swayTop.x + 1.2f * scale, pos.y + 3.8f * scale, swayTop.z + 0.7f * scale}, 1.45f * scale, Shade(palmGreen, 1.15f));
        DrawSphere({swayTop.x - 1.1f * scale, pos.y + 3.9f * scale, swayTop.z - 0.8f * scale}, 1.45f * scale, Shade(palmGreen, 0.88f));
        DrawSphere({swayTop.x + 0.2f * scale, pos.y + 4.8f * scale, swayTop.z - 1.1f * scale}, 1.35f * scale, Shade(palmGreen, 1.20f));
    } else {
        // Lush Multi-Sphere Normandy Bocage Oak / Elm with Layered Canopies
        Color oakBase = {42, 82, 34, 255};
        DrawSphere({swayTop.x, pos.y + 5.0f * scale, swayTop.z}, 2.55f * scale, oakBase);
        DrawSphere({swayTop.x - 1.35f * scale, pos.y + 4.3f * scale, swayTop.z + 0.9f * scale}, 1.85f * scale, Shade(oakBase, 1.18f));
        DrawSphere({swayTop.x + 1.35f * scale, pos.y + 4.4f * scale, swayTop.z - 0.8f * scale}, 1.90f * scale, Shade(oakBase, 0.86f));
        DrawSphere({swayTop.x + 0.4f * scale, pos.y + 6.2f * scale, swayTop.z + 0.5f * scale}, 1.70f * scale, Shade(oakBase, 1.25f));
        DrawSphere({swayTop.x - 0.5f * scale, pos.y + 5.5f * scale, swayTop.z - 1.3f * scale}, 1.65f * scale, Shade(oakBase, 0.94f));
    }
}

void drawBocageHedge3D(Vector3 pos, float scale, float rotation) {
    DrawSoftShadowDisc(pos, 4.2f * scale, 2.2f * scale);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(rotation, 0.0f, 1.0f, 0.0f);

    // Earthen embankment berm
    DrawBeveledBox({0.0f, 0.35f * scale, 0.0f}, 1.8f * scale, 0.65f * scale, 7.2f * scale, {78, 64, 46, 255});

    // Thick bramble and hedge foliage clusters along berm
    Color hedge1 = {38, 76, 32, 255};
    Color hedge2 = {48, 92, 38, 255};
    Color hedge3 = {32, 64, 26, 255};
    for (int b = -3; b <= 3; ++b) {
        float bz = b * 1.05f * scale;
        float bx = ((b % 2 == 0) ? 0.22f : -0.22f) * scale;
        Color hCol = (b % 3 == 0) ? hedge1 : ((b % 3 == 1) ? hedge2 : hedge3);
        DrawSphere({bx, 1.15f * scale, bz}, 1.05f * scale, hCol);
    }
    rlPopMatrix();
}

void drawRuinedStoneWall3D(Vector3 pos, float scale, float rotation) {
    DrawSoftShadowDisc(pos, 3.8f * scale, 1.8f * scale);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(rotation, 0.0f, 1.0f, 0.0f);

    Color stone = {136, 132, 124, 255};
    Color mortar = {88, 85, 80, 255};
    DrawBeveledBox({0.0f, 0.65f * scale, 0.0f}, 0.55f * scale, 1.25f * scale, 6.4f * scale, stone);
    // Jagged top battle damage
    DrawCube({0.0f, 1.35f * scale, -1.5f * scale}, 0.58f * scale, 0.35f * scale, 1.8f * scale, stone);
    DrawCube({0.0f, 1.30f * scale,  1.8f * scale}, 0.58f * scale, 0.30f * scale, 1.4f * scale, stone);
    // Rubble heap at base
    DrawCube({ 0.45f * scale, 0.22f * scale, 0.2f * scale}, 0.55f * scale, 0.40f * scale, 1.2f * scale, mortar);
    DrawCube({-0.45f * scale, 0.22f * scale, -0.8f * scale}, 0.55f * scale, 0.40f * scale, 1.4f * scale, mortar);
    rlPopMatrix();
}

void drawCzechHedgehog3D(Vector3 pos, float scale) {
    DrawSoftShadowDisc(pos, 2.2f * scale, 2.2f * scale);
    Color steel = {54, 58, 62, 255};
    Color rust  = {88, 64, 50, 255};
    Color col = Shade(steel, 0.95f);

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 0.95f * scale, pos.z);
    // Three 60-degree intersecting heavy steel I-beams
    DrawCube({0, 0, 0}, 0.24f * scale, 0.24f * scale, 2.4f * scale, col);
    DrawCube({0, 0, 0}, 2.4f * scale, 0.24f * scale, 0.24f * scale, rust);
    DrawCube({0, 0, 0}, 0.24f * scale, 2.4f * scale, 0.24f * scale, col);
    // Center gusset reinforcement plates
    DrawCube({0, 0, 0}, 0.48f * scale, 0.48f * scale, 0.48f * scale, {42, 45, 48, 255});
    rlPopMatrix();
}

void drawTelephonePole3D(Vector3 pos, float scale) {
    DrawSoftShadowDisc(pos, 1.4f * scale, 1.4f * scale);
    Color poleWood = {74, 52, 34, 255};
    Color insulCol = {225, 230, 235, 255};
    float h = 8.2f * scale;

    // Main creosoted utility pole
    DrawCylinderEx(pos, {pos.x, pos.y + h, pos.z}, 0.22f * scale, 0.14f * scale, 8, poleWood);
    // Horizontal crossbeam
    Vector3 crossCenter = {pos.x, pos.y + h - 0.65f * scale, pos.z};
    DrawCube(crossCenter, 0.16f * scale, 0.16f * scale, 2.6f * scale, poleWood);
    // White ceramic insulators on crossbeam
    for (int ins = -2; ins <= 2; ++ins) {
        DrawCylinder({crossCenter.x, crossCenter.y + 0.16f * scale, crossCenter.z + ins * 0.55f * scale},
                     0.06f * scale, 0.06f * scale, 0.18f * scale, 6, insulCol);
    }
}

void drawSandbagCheckpoint3D(Vector3 pos, float scale) {
    DrawSoftShadowDisc(pos, 3.2f * scale, 3.2f * scale);
    Color sandbag = {186, 168, 128, 255};
    Color darkBag = {142, 126, 92, 255};

    for (int layer = 0; layer < 3; ++layer) {
        float ly = pos.y + (layer + 0.5f) * 0.32f * scale;
        for (int a = -3; a <= 3; ++a) {
            float rad = a * 0.36f;
            float r = (3.2f - layer * 0.15f) * scale;
            Vector3 bPos = {pos.x + std::sin(rad) * r, ly, pos.z + std::cos(rad) * r};
            DrawCube(bPos, 0.95f * scale, 0.30f * scale, 0.55f * scale, (a % 2 == 0) ? sandbag : darkBag);
        }
    }
    // Wooden ammo supply crates in rear
    DrawBeveledBox({pos.x - 0.6f * scale, pos.y + 0.45f * scale, pos.z - 0.4f * scale},
                   1.2f * scale, 0.75f * scale, 1.2f * scale, {152, 126, 78, 255});
}

void drawRockFormation3D(Vector3 pos, float scale, float rotation) {
    DrawSoftShadowDisc(pos, 3.5f * scale, 3.5f * scale);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(rotation, 0.0f, 1.0f, 0.0f);

    Color granite = {105, 108, 112, 255};
    Color moss    = {85, 96, 75, 255};
    DrawBeveledBox({0.0f, 0.85f * scale, 0.0f}, 2.4f * scale, 1.6f * scale, 2.1f * scale, granite);
    DrawBeveledBox({ 1.1f * scale, 0.55f * scale,  0.7f * scale}, 1.6f * scale, 1.0f * scale, 1.4f * scale, moss);
    DrawBeveledBox({-1.0f * scale, 0.60f * scale, -0.6f * scale}, 1.7f * scale, 1.1f * scale, 1.5f * scale, granite);
    rlPopMatrix();
}

} // namespace models
} // namespace aa2004
