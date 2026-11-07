// ============================================================================
// AXIS & ALLIES (2004) — AAA STUDIO UI THEME, TRUETYPE FONTS & TEXTURE CACHE
// ============================================================================

#include "ui/ui_theme.hpp"
#include <cmath>
#include <cstdio>

namespace aa2004 {

UITheme& UITheme::instance() {
    static UITheme inst;
    return inst;
}

Texture2D UITheme::loadTexSafe(const char* path, Color fallbackColor, int w, int h) {
    if (FileExists(path)) {
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) {
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
            return tex;
        }
    }
    Image img = GenImageColor(w, h, fallbackColor);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

void UITheme::init(bool headless) {
    if (loaded_) return;
    headless_ = headless && !IsWindowReady();
    if (headless_) {
        loaded_ = true;
        return;
    }

    // Load Anti-Aliased TrueType Fonts at High Resolution for Retina Clarity
    if (FileExists("assets/fonts/font_title.ttf") && FileExists("assets/fonts/font_bold.ttf")) {
        fontTitle_   = LoadFontEx("assets/fonts/font_title.ttf", 72, nullptr, 0);
        fontBold_    = LoadFontEx("assets/fonts/font_bold.ttf", 44, nullptr, 0);
        fontRegular_ = LoadFontEx("assets/fonts/font_regular.ttf", 36, nullptr, 0);
        fontMono_    = LoadFontEx("assets/fonts/font_mono.ttf", 32, nullptr, 0);

        SetTextureFilter(fontTitle_.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(fontBold_.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(fontRegular_.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(fontMono_.texture, TEXTURE_FILTER_BILINEAR);
        hasCustomFonts_ = true;
    }

    // Load AAA Textures & Icons
    worldMapTex_     = loadTexSafe("assets/textures/world_map_hd.png", {22, 48, 72, 255}, 512, 256);
    menuBackdropTex_ = loadTexSafe("assets/textures/menu_backdrop_hd.png", {18, 24, 22, 255}, 512, 256);

    for (int i = 0; i < 5; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "assets/textures/flag_%d.png", i);
        flagTextures_[i] = loadTexSafe(buf, {80, 90, 80, 255}, 64, 64);
    }

    for (int i = 0; i < 17; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "assets/textures/unit_%d.png", i);
        unitTextures_[i] = loadTexSafe(buf, {45, 55, 48, 255}, 96, 60);
    }

    texGrass_ = loadTexSafe("assets/textures/tex_grass.png", {68, 96, 52, 255}, 256, 256);
    texDirt_  = loadTexSafe("assets/textures/tex_dirt.png", {112, 92, 68, 255}, 256, 256);
    texRock_  = loadTexSafe("assets/textures/tex_rock.png", {96, 98, 102, 255}, 256, 256);
    texSand_  = loadTexSafe("assets/textures/tex_sand.png", {198, 172, 120, 255}, 256, 256);
    texSnow_  = loadTexSafe("assets/textures/tex_snow.png", {220, 228, 236, 255}, 256, 256);
    texSteel_ = loadTexSafe("assets/textures/tex_steel.png", {88, 94, 98, 255}, 256, 256);

    loaded_ = true;
}

void UITheme::shutdown() {
    if (!loaded_ || headless_) return;

    if (hasCustomFonts_) {
        UnloadFont(fontTitle_);
        UnloadFont(fontBold_);
        UnloadFont(fontRegular_);
        UnloadFont(fontMono_);
        hasCustomFonts_ = false;
    }

    UnloadTexture(worldMapTex_);
    UnloadTexture(menuBackdropTex_);
    for (auto& t : flagTextures_) UnloadTexture(t);
    for (auto& t : unitTextures_) UnloadTexture(t);
    UnloadTexture(texGrass_);
    UnloadTexture(texDirt_);
    UnloadTexture(texRock_);
    UnloadTexture(texSand_);
    UnloadTexture(texSnow_);
    UnloadTexture(texSteel_);

    loaded_ = false;
}

void UITheme::drawText(const char* text, float x, float y, float fontSize, Color color, bool bold, bool shadow) const {
    if (!text || text[0] == '\0') return;
    if (!hasCustomFonts_) {
        if (shadow) DrawText(text, static_cast<int>(x + 1), static_cast<int>(y + 1), static_cast<int>(fontSize), {0, 0, 0, 190});
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(fontSize), color);
        return;
    }
    const Font& f = bold ? fontBold_ : fontRegular_;
    float spacing = std::max(0.6f, fontSize * 0.04f);
    if (shadow) {
        DrawTextEx(f, text, {std::round(x + 1.5f), std::round(y + 1.5f)}, fontSize, spacing, {0, 0, 0, 210});
    }
    DrawTextEx(f, text, {std::round(x), std::round(y)}, fontSize, spacing, color);
}

void UITheme::drawTextTitle(const char* text, float x, float y, float fontSize, Color color, bool shadow) const {
    if (!text || text[0] == '\0') return;
    if (!hasCustomFonts_) {
        drawText(text, x, y, fontSize, color, true, shadow);
        return;
    }
    float spacing = std::max(1.2f, fontSize * 0.05f);
    if (shadow) {
        DrawTextEx(fontTitle_, text, {std::round(x + 2.5f), std::round(y + 2.5f)}, fontSize, spacing, {0, 0, 0, 230});
    }
    DrawTextEx(fontTitle_, text, {std::round(x), std::round(y)}, fontSize, spacing, color);
}

float UITheme::measureWidth(const char* text, float fontSize, bool bold) const {
    if (!text || text[0] == '\0') return 0.0f;
    if (!hasCustomFonts_) {
        return static_cast<float>(MeasureText(text, static_cast<int>(fontSize)));
    }
    if (fontSize >= 46.0f) {
        float spacing = std::max(1.2f, fontSize * 0.05f);
        return MeasureTextEx(fontTitle_, text, fontSize, spacing).x;
    }
    const Font& f = bold ? fontBold_ : fontRegular_;
    float spacing = std::max(0.6f, fontSize * 0.04f);
    return MeasureTextEx(f, text, fontSize, spacing).x;
}

void UITheme::drawTextCentered(const char* text, Rectangle box, float fontSize, Color color, bool bold) const {
    float tw = measureWidth(text, fontSize, bold);
    float tx = box.x + (box.width - tw) * 0.5f;
    float ty = box.y + (box.height - fontSize) * 0.5f;
    drawText(text, tx, ty, fontSize, color, bold, true);
}

void UITheme::drawCommandPanel(Rectangle rect, const char* title, Color topCol, Color botCol, Color brassBorder) const {
    // Drop shadow behind panel
    DrawRectangleRec({rect.x + 5.0f, rect.y + 5.0f, rect.width, rect.height}, {0, 0, 0, 145});

    // Brushed metallic vertical gradient body
    DrawRectangleGradientV(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rect.width), static_cast<int>(rect.height),
                           topCol, botCol);

    // Subtle inner bevel frame
    Rectangle inner = {rect.x + 3.0f, rect.y + 3.0f, rect.width - 6.0f, rect.height - 6.0f};
    DrawRectangleLinesEx(inner, 1.0f, {70, 82, 72, 140});
    DrawRectangleLinesEx(rect, 2.0f, brassBorder);

    // Brass corner rivets
    float r = 3.0f;
    Color rivetCol = {215, 190, 120, 230};
    DrawCircleV({rect.x + 7.0f, rect.y + 7.0f}, r, rivetCol);
    DrawCircleV({rect.x + rect.width - 7.0f, rect.y + 7.0f}, r, rivetCol);
    DrawCircleV({rect.x + 7.0f, rect.y + rect.height - 7.0f}, r, rivetCol);
    DrawCircleV({rect.x + rect.width - 7.0f, rect.y + rect.height - 7.0f}, r, rivetCol);

    if (title && title[0] != '\0') {
        Rectangle hdr = {rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, 34.0f};
        DrawRectangleGradientH(static_cast<int>(hdr.x), static_cast<int>(hdr.y),
                               static_cast<int>(hdr.width), static_cast<int>(hdr.height),
                               {52, 64, 48, 255}, {28, 36, 30, 255});
        DrawLineEx({rect.x, rect.y + 36.0f}, {rect.x + rect.width, rect.y + 36.0f}, 2.0f, brassBorder);
        drawText(title, rect.x + 16.0f, rect.y + 9.0f, 18.0f, {248, 218, 120, 255}, true, true);
    }
}

bool UITheme::drawMilitaryButton(Rectangle rect, const char* label, Color baseCol, Color borderCol,
                                 float fontSize, bool highlighted, const Texture2D* iconTex) const {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color topFill = baseCol;
    Color botFill = ColorBrightness(baseCol, -0.28f);
    if (highlighted) {
        topFill = ColorBrightness(baseCol, 0.32f);
        botFill = ColorBrightness(baseCol, -0.05f);
    } else if (pressed) {
        topFill = ColorBrightness(baseCol, -0.15f);
        botFill = ColorBrightness(baseCol, 0.10f);
    } else if (hovered) {
        topFill = ColorBrightness(baseCol, 0.22f);
        botFill = ColorBrightness(baseCol, -0.10f);
    }

    // Subtle button drop shadow
    DrawRectangleRec({rect.x + 2.0f, rect.y + 2.0f, rect.width, rect.height}, {0, 0, 0, 120});
    DrawRectangleGradientV(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rect.width), static_cast<int>(rect.height),
                           topFill, botFill);

    // Top specular highlight line
    DrawLineEx({rect.x + 2.0f, rect.y + 2.0f}, {rect.x + rect.width - 2.0f, rect.y + 2.0f},
               1.0f, ColorAlpha(WHITE, hovered || highlighted ? 0.35f : 0.16f));

    Color activeBorder = highlighted ? Color{255, 215, 90, 255} : (hovered ? Color{245, 225, 130, 255} : borderCol);
    DrawRectangleLinesEx(rect, highlighted ? 2.4f : 1.5f, activeBorder);

    float textOffsetX = 0.0f;
    if (iconTex && iconTex->id != 0) {
        float iconH = rect.height - 6.0f;
        float iconW = iconH * (static_cast<float>(iconTex->width) / std::max(1, iconTex->height));
        Rectangle src = {0.0f, 0.0f, static_cast<float>(iconTex->width), static_cast<float>(iconTex->height)};
        Rectangle dst = {rect.x + 4.0f, rect.y + 3.0f, iconW, iconH};
        DrawTexturePro(*iconTex, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
        textOffsetX = iconW + 6.0f;
    }

    if (label && label[0] != '\0') {
        Rectangle textBox = {rect.x + textOffsetX, rect.y, rect.width - textOffsetX, rect.height};
        Color txtCol = highlighted ? Color{255, 232, 135, 255} : (hovered ? WHITE : Color{235, 240, 232, 255});
        drawTextCentered(label, textBox, fontSize, txtCol, true);
    }

    return clicked;
}

const Texture2D& UITheme::getNationFlag(NationId nation) const {
    size_t idx = static_cast<size_t>(nation);
    if (idx >= flagTextures_.size()) idx = 0;
    return flagTextures_[idx];
}

const Texture2D& UITheme::getUnitCard(RegimentType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx >= unitTextures_.size()) idx = 0;
    return unitTextures_[idx];
}

} // namespace aa2004
