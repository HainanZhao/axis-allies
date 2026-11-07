#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — AAA STUDIO UI THEME, TRUETYPE FONTS & TEXTURE CACHE
// Crisp High-DPI Anti-Aliased Typography, Brushed-Gunmetal & Brass Panels,
// National Medallions, Unit Silhouette Cards & Satellite World Map Cache
// ============================================================================

#include "core/types.hpp"

namespace aa2004 {

class UITheme {
public:
    static UITheme& instance();

    void init(bool headless = false);
    void shutdown();

    bool isLoaded() const { return loaded_; }

    // Crisp Anti-Aliased TrueType Text Rendering
    void drawText(const char* text, float x, float y, float fontSize, Color color, bool bold = true, bool shadow = true) const;
    void drawTextTitle(const char* text, float x, float y, float fontSize, Color color, bool shadow = true) const;
    void drawTextCentered(const char* text, Rectangle box, float fontSize, Color color, bool bold = true) const;
    float measureWidth(const char* text, float fontSize, bool bold = true) const;

    // Studio-Grade Military Command Panels & Buttons
    void drawCommandPanel(Rectangle rect, const char* title = nullptr,
                          Color topCol = {26, 33, 30, 244},
                          Color botCol = {14, 18, 16, 248},
                          Color brassBorder = {168, 148, 92, 255}) const;

    bool drawMilitaryButton(Rectangle rect, const char* label,
                            Color baseCol = {38, 54, 42, 255},
                            Color borderCol = {168, 148, 92, 255},
                            float fontSize = 16.0f,
                            bool highlighted = false,
                            const Texture2D* iconTex = nullptr) const;

    // Texture Accessors
    const Texture2D& getWorldMapTexture() const { return worldMapTex_; }
    const Texture2D& getMenuBackdropTexture() const { return menuBackdropTex_; }
    const Texture2D& getNationFlag(NationId nation) const;
    const Texture2D& getUnitCard(RegimentType type) const;

    const Texture2D& getGrassTex() const { return texGrass_; }
    const Texture2D& getDirtTex() const { return texDirt_; }
    const Texture2D& getRockTex() const { return texRock_; }
    const Texture2D& getSandTex() const { return texSand_; }
    const Texture2D& getSnowTex() const { return texSnow_; }
    const Texture2D& getSteelTex() const { return texSteel_; }

private:
    UITheme() = default;
    Texture2D loadTexSafe(const char* path, Color fallbackColor, int w = 64, int h = 64);

    bool loaded_ = false;
    bool headless_ = false;

    Font fontTitle_{};
    Font fontBold_{};
    Font fontRegular_{};
    Font fontMono_{};
    bool hasCustomFonts_ = false;

    Texture2D worldMapTex_{};
    Texture2D menuBackdropTex_{};
    std::array<Texture2D, 5> flagTextures_{};
    std::array<Texture2D, 17> unitTextures_{};

    Texture2D texGrass_{};
    Texture2D texDirt_{};
    Texture2D texRock_{};
    Texture2D texSand_{};
    Texture2D texSnow_{};
    Texture2D texSteel_{};
};

} // namespace aa2004
