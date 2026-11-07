#!/usr/bin/env python3
"""
AXIS & ALLIES (2004) — AAA Procedural Texture, Satellite World Map,
National Medallion & Tactical Unit Card Generator (NumPy + Pillow)
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "textures")
os.makedirs(OUT_DIR, exist_ok=True)


def fbm_noise(h, w, octaves=5, base_freq=4.0, seed=42):
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    amp = 0.5
    freq = base_freq
    for _ in range(octaves):
        gh = max(2, int(freq))
        gw = max(2, int(freq * (w / h)))
        grid = rng.random((gh, gw), dtype=np.float32)
        img = Image.fromarray((grid * 255).astype(np.uint8), mode="L").resize((w, h), Image.Resampling.BICUBIC)
        acc += (np.asarray(img, dtype=np.float32) / 255.0) * amp
        amp *= 0.5
        freq *= 2.1
    acc = (acc - acc.min()) / max(1e-5, (acc.max() - acc.min()))
    return acc


def generate_tileable_textures():
    w, h = 1024, 1024
    n1 = fbm_noise(h, w, octaves=7, base_freq=6.0, seed=101)
    n2 = fbm_noise(h, w, octaves=7, base_freq=24.0, seed=202)
    n3 = fbm_noise(h, w, octaves=5, base_freq=72.0, seed=303)
    macro = 0.55 * n1 + 0.35 * n2 + 0.10 * n3

    def save_texture(name, gen_func):
        arr = gen_func()
        arr = np.clip(arr, 0, 255).astype(np.uint8)
        Image.fromarray(arr, "RGBA").save(os.path.join(OUT_DIR, name))

    # 1. Lush Normandy Bocage Grass (Layered blades, rich olive undertones, clover tufts)
    def make_grass():
        rng = np.random.default_rng(404)
        blades = rng.random((h, w), dtype=np.float32) * 0.18
        tufts = (n2 > 0.62).astype(np.float32) * 0.12
        var = macro * 0.7 + blades + tufts
        r = 38.0 + var * 58.0
        g = 72.0 + var * 82.0
        b = 28.0 + var * 44.0
        # Dark soil under-crevices
        crevices = (n3 < 0.28).astype(np.float32) * 0.25
        r = r * (1.0 - crevices) + 42.0 * crevices
        g = g * (1.0 - crevices) + 38.0 * crevices
        b = b * (1.0 - crevices) + 26.0 * crevices
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    # 2. Rich Battlefield Mud & Road Dirt (Pebbles, dried wheel ruts, coarse sand)
    def make_dirt():
        rng = np.random.default_rng(505)
        pebbles = (rng.random((h, w), dtype=np.float32) > 0.985).astype(np.float32) * 55.0
        var = macro * 0.75 + n3 * 0.25
        r = 85.0 + var * 65.0 + pebbles
        g = 68.0 + var * 52.0 + pebbles
        b = 45.0 + var * 38.0 + pebbles * 0.9
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    # 3. Craggy Granite Cliff & Riverbank Rock (Layered fracture strata & moss)
    def make_rock():
        strata = np.sin(macro * 18.0) * 0.12
        var = np.clip(macro + strata, 0.0, 1.0)
        moss = (n2 > 0.70).astype(np.float32) * 0.22
        r = 65.0 + var * 75.0 - moss * 20.0
        g = 68.0 + var * 76.0 + moss * 18.0
        b = 70.0 + var * 78.0 - moss * 15.0
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    # 4. Wind-Swept Desert Sand (Fine ripples & golden specular grains)
    def make_sand():
        ripples = np.sin(np.linspace(0, 32.0 * np.pi, w)[None, :] + n1 * 4.0) * 0.12
        var = np.clip(macro * 0.75 + ripples, 0.0, 1.0)
        r = 175.0 + var * 65.0
        g = 148.0 + var * 56.0
        b = 98.0  + var * 48.0
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    # 5. Crystalline Snow (Soft blue shadows & glistening ice flecks)
    def make_snow():
        rng = np.random.default_rng(606)
        sparkle = (rng.random((h, w), dtype=np.float32) > 0.992).astype(np.float32) * 35.0
        var = macro * 0.65 + n3 * 0.35
        r = 205.0 + var * 48.0 + sparkle
        g = 216.0 + var * 38.0 + sparkle
        b = 230.0 + var * 25.0 + sparkle
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    # 6. Industrial Rolled Armor Steel (Brushed mill scale & matte metallic finish)
    def make_steel():
        brush = np.sin(np.linspace(0, 64.0 * np.pi, h)[:, None] + n2 * 2.0) * 0.08
        var = np.clip(macro * 0.85 + brush, 0.0, 1.0)
        r = 64.0 + var * 68.0
        g = 70.0 + var * 72.0
        b = 74.0 + var * 76.0
        return np.stack([r, g, b, np.full_like(r, 255)], axis=-1)

    save_texture("tex_grass.png", make_grass)
    save_texture("tex_dirt.png",  make_dirt)
    save_texture("tex_rock.png",  make_rock)
    save_texture("tex_sand.png",  make_sand)
    save_texture("tex_snow.png",  make_snow)
    save_texture("tex_steel.png", make_steel)


def generate_world_map_hd():
    w, h = 2048, 1152
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    nx = xx / w
    ny = yy / h

    # Draw accurate multi-vertex geographic continent & island polygons aligned 1-to-1
    # with the 26 WWII territories in game_data.cpp
    mask_img = Image.new("L", (w, h), 0)
    mdraw = ImageDraw.Draw(mask_img)

    def poly(pts, val=220):
        px = [(int(x * w), int(y * h)) for x, y in pts]
        mdraw.polygon(px, fill=val)

    # 1. North America (Alaska -> Canada -> Western/Eastern USA -> Florida -> Mexico/Central America)
    poly([
        (0.02, 0.30), (0.04, 0.22), (0.09, 0.17), (0.15, 0.16), (0.17, 0.23),
        (0.20, 0.17), (0.25, 0.21), (0.26, 0.28), (0.22, 0.35), (0.22, 0.44),
        (0.20, 0.50), (0.18, 0.47), (0.15, 0.49), (0.14, 0.55), (0.18, 0.62),
        (0.15, 0.63), (0.10, 0.54), (0.06, 0.46), (0.05, 0.36), (0.06, 0.31)
    ], 225)
    # Greenland & Baffin Island
    poly([(0.23, 0.06), (0.30, 0.05), (0.31, 0.14), (0.26, 0.17), (0.23, 0.12)], 205)
    # Cuba & Caribbean
    poly([(0.18, 0.53), (0.22, 0.54), (0.22, 0.56), (0.18, 0.55)], 190)

    # 2. South America
    poly([
        (0.18, 0.62), (0.24, 0.64), (0.29, 0.71), (0.28, 0.81),
        (0.23, 0.92), (0.20, 0.95), (0.19, 0.84), (0.17, 0.71)
    ], 215)

    # 3. United Kingdom & Ireland (centered around 0.33, 0.26)
    poly([(0.315, 0.21), (0.345, 0.21), (0.352, 0.29), (0.315, 0.30)], 225)
    poly([(0.292, 0.24), (0.310, 0.24), (0.310, 0.28), (0.292, 0.28)], 205)

    # 4. Scandinavia (Norway, Sweden, Finland - centered around 0.42, 0.14)
    poly([
        (0.39, 0.19), (0.40, 0.10), (0.45, 0.07), (0.50, 0.10),
        (0.49, 0.20), (0.46, 0.20), (0.44, 0.23), (0.41, 0.22)
    ], 225)

    # 5. Continental Europe (Iberia, France, Germany, Italy, Balkans) & Eurasia (USSR, Middle East, India, China, SE Asia)
    poly([
        # Iberia & France
        (0.28, 0.49), (0.28, 0.43), (0.32, 0.41), (0.32, 0.34), (0.37, 0.32),
        # Germany, Poland, Baltic, Karelia, Arctic USSR, Siberia, Far East, Kamchatka
        (0.42, 0.27), (0.48, 0.25), (0.50, 0.16), (0.56, 0.12), (0.68, 0.10),
        (0.80, 0.11), (0.89, 0.14), (0.91, 0.22), (0.88, 0.26), (0.85, 0.21),
        # Manchuria, Korea, Coastal China, Indochina, Malaya
        (0.83, 0.28), (0.85, 0.35), (0.83, 0.38), (0.81, 0.34), (0.82, 0.43),
        (0.79, 0.51), (0.78, 0.59), (0.76, 0.68), (0.74, 0.67), (0.73, 0.59),
        # Burma, Bay of Bengal, India Subcontinent
        (0.69, 0.56), (0.65, 0.68), (0.62, 0.67), (0.59, 0.56),
        # Persia & Arabian Peninsula
        (0.55, 0.55), (0.56, 0.63), (0.51, 0.65), (0.49, 0.56),
        # Anatolia, Greece/Balkans, Italy Boot, Southern France
        (0.48, 0.50), (0.45, 0.52), (0.43, 0.46), (0.42, 0.52), (0.39, 0.46),
        (0.35, 0.46), (0.32, 0.50)
    ], 235)

    # Carve Black Sea & Caspian Sea
    poly([(0.49, 0.42), (0.55, 0.41), (0.55, 0.46), (0.49, 0.46)], 0)
    poly([(0.58, 0.39), (0.60, 0.39), (0.61, 0.47), (0.58, 0.47)], 0)

    # 6. Africa (Algeria/Tunisia 0.37,0.62 & Egypt 0.49,0.64 -> South Africa) & Madagascar
    poly([
        (0.30, 0.58), (0.38, 0.55), (0.43, 0.58), (0.49, 0.58),
        (0.52, 0.67), (0.55, 0.70), (0.50, 0.80), (0.45, 0.92),
        (0.40, 0.92), (0.37, 0.78), (0.35, 0.71), (0.29, 0.67)
    ], 230)
    poly([(0.52, 0.82), (0.54, 0.81), (0.54, 0.88), (0.52, 0.88)], 205)

    # 7. Imperial Japan Archipelago (Hokkaido, Honshu, Shikoku, Kyushu around 0.90, 0.35)
    poly([(0.895, 0.27), (0.920, 0.28), (0.915, 0.31), (0.890, 0.30)], 225)
    poly([(0.885, 0.31), (0.918, 0.32), (0.912, 0.39), (0.875, 0.41), (0.872, 0.37)], 235)

    # 8. Philippines & Iwo Jima (around 0.85, 0.56)
    poly([(0.835, 0.51), (0.865, 0.52), (0.870, 0.60), (0.835, 0.59)], 225)

    # 9. Borneo, Sumatra, Java & New Guinea (Dutch East Indies around 0.77, 0.76)
    poly([(0.70, 0.70), (0.74, 0.75), (0.79, 0.78), (0.73, 0.78), (0.69, 0.72)], 215)
    poly([(0.75, 0.71), (0.79, 0.71), (0.79, 0.77), (0.75, 0.77)], 230)
    poly([(0.82, 0.73), (0.88, 0.75), (0.87, 0.78), (0.81, 0.77)], 220)

    # 10. Australia & Guadalcanal (around 0.86, 0.84)
    poly([
        (0.80, 0.81), (0.85, 0.79), (0.90, 0.80), (0.92, 0.86),
        (0.89, 0.92), (0.83, 0.91), (0.79, 0.87)
    ], 235)

    # 11. Pearl Harbor & Midway (around 0.94, 0.52)
    poly([(0.925, 0.50), (0.955, 0.51), (0.955, 0.54), (0.925, 0.53)], 220)

    # Smooth polygon distance field + multi-octave FBM coastline displacement
    base_mask = mask_img.filter(ImageFilter.GaussianBlur(radius=18))
    field = np.asarray(base_mask, dtype=np.float32) / 255.0

    coast_noise = fbm_noise(h, w, octaves=6, base_freq=9.0, seed=777)
    mtn_noise   = fbm_noise(h, w, octaves=6, base_freq=22.0, seed=888)

    # Add major WWII mountain ranges (Rockies, Andes, Alps, Caucasus, Urals, Himalayas/Tibet)
    mtn_chains = np.zeros((h, w), dtype=np.float32)
    ridges = [
        (0.08, 0.34, 0.03, 0.14, 0.55),  # Rockies
        (0.20, 0.78, 0.02, 0.14, 0.55),  # Andes
        (0.39, 0.41, 0.04, 0.03, 0.50),  # Alps
        (0.58, 0.43, 0.04, 0.02, 0.50),  # Caucasus
        (0.66, 0.22, 0.02, 0.10, 0.45),  # Urals
        (0.68, 0.47, 0.07, 0.05, 0.75),  # Himalayas & Tibet
    ]
    for cx, cy, rx, ry, amp in ridges:
        d2 = ((nx - cx) / rx) ** 2 + ((ny - cy) / ry) ** 2
        mtn_chains += amp * np.exp(-d2 * 1.6)

    elev = field * 0.78 + (coast_noise - 0.48) * 0.34 + mtn_chains * field * (0.4 + 0.6 * mtn_noise)

    sea_level = 0.36
    is_land = elev > sea_level

    # Relief normal shading from elevation gradient
    dy, dx = np.gradient(elev + mtn_noise * 0.16)
    shade = np.clip(0.62 - dx * 32.0 - dy * 26.0, 0.28, 1.38)

    # Bathymetric ocean coloration (deep abyssal blue -> continental shelf turquoise)
    ocean_depth = np.clip((sea_level - elev) / 0.36, 0.0, 1.0)
    or_c = (32.0 * (1.0 - ocean_depth) + 9.0 * ocean_depth)
    og_c = (76.0 * (1.0 - ocean_depth) + 24.0 * ocean_depth)
    ob_c = (114.0 * (1.0 - ocean_depth) + 48.0 * ocean_depth)

    # Topographic land coloration (lush forest -> steppe olive -> Sahara/Arabia desert -> alpine snow)
    land_h = np.clip((elev - sea_level) / 0.75, 0.0, 1.0)
    sahara_mask = np.exp(-(((nx - 0.43) / 0.13) ** 2 + ((ny - 0.62) / 0.09) ** 2))
    aussie_mask = np.exp(-(((nx - 0.85) / 0.06) ** 2 + ((ny - 0.85) / 0.05) ** 2)) * 0.75
    gobi_mask   = np.exp(-(((nx - 0.73) / 0.07) ** 2 + ((ny - 0.34) / 0.05) ** 2)) * 0.55
    desert_mask = np.clip(sahara_mask + aussie_mask + gobi_mask, 0.0, 1.0)

    snow_mask = np.clip((0.17 - ny) / 0.09, 0.0, 1.0) + np.clip((land_h - 0.58) * 2.4, 0.0, 1.0)
    snow_mask = np.clip(snow_mask, 0.0, 0.88)

    lr = 50.0 + land_h * 74.0 + desert_mask * 92.0
    lg = 86.0 + land_h * 44.0 + desert_mask * 48.0
    lb = 44.0 + land_h * 26.0 + desert_mask * 10.0

    lr = (lr * (1.0 - snow_mask) + 220.0 * snow_mask) * shade
    lg = (lg * (1.0 - snow_mask) + 226.0 * snow_mask) * shade
    lb = (lb * (1.0 - snow_mask) + 235.0 * snow_mask) * shade

    # Crisp dark coastline rim + shallow coastal water glow
    coast_dist = np.abs(elev - sea_level)
    coast_line = np.exp(-((coast_dist / 0.011) ** 2))

    r = np.where(is_land, lr, or_c)
    g = np.where(is_land, lg, og_c)
    b = np.where(is_land, lb, ob_c)

    r = r * (1.0 - coast_line * 0.52) + 18.0 * coast_line * 0.52
    g = g * (1.0 - coast_line * 0.52) + 28.0 * coast_line * 0.52
    b = b * (1.0 - coast_line * 0.52) + 24.0 * coast_line * 0.52

    arr = np.stack([
        np.clip(r, 0, 255).astype(np.uint8),
        np.clip(g, 0, 255).astype(np.uint8),
        np.clip(b, 0, 255).astype(np.uint8),
        np.full((h, w), 255, dtype=np.uint8)
    ], axis=-1)

    img = Image.fromarray(arr, "RGBA")
    draw = ImageDraw.Draw(img)

    # Subtle nautical graticule grid
    for gx in range(0, w, 128):
        draw.line([(gx, 0), (gx, h)], fill=(120, 160, 185, 32), width=1)
    for gy in range(0, h, 128):
        draw.line([(0, gy), (w, gy)], fill=(120, 160, 185, 32), width=1)

    img.save(os.path.join(OUT_DIR, "world_map_hd.png"))


def generate_menu_backdrop_hd():
    w, h = 2048, 1152
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    nx = xx / w
    ny = yy / h

    clouds = fbm_noise(h, w, octaves=6, base_freq=5.0, seed=333)

    # Dramatic twilight storm sky + warm horizon fire glow
    horizon_glow = np.exp(-((ny - 0.68) / 0.14) ** 2) * (0.6 + 0.4 * clouds)
    beam1 = np.exp(-(((nx - (0.28 + (0.68 - ny) * 0.35)) / 0.035) ** 2)) * (ny < 0.70)
    beam2 = np.exp(-(((nx - (0.72 - (0.68 - ny) * 0.28)) / 0.040) ** 2)) * (ny < 0.70)
    beams = (beam1 + beam2) * np.clip(1.0 - ny * 1.1, 0.0, 1.0) * 0.45

    r = 18.0 + clouds * 28.0 + horizon_glow * 145.0 + beams * 110.0
    g = 24.0 + clouds * 34.0 + horizon_glow * 78.0  + beams * 125.0
    b = 22.0 + clouds * 30.0 + horizon_glow * 28.0  + beams * 95.0

    # Ground silhouette below ny = 0.70
    hill = 0.70 + 0.03 * np.sin(nx * 14.0) + 0.015 * np.cos(nx * 37.0)
    ground_mask = ny > hill
    r = np.where(ground_mask, r * 0.22, r)
    g = np.where(ground_mask, g * 0.24, g)
    b = np.where(ground_mask, b * 0.22, b)

    # Vignette
    vig = 1.0 - 0.48 * (((nx - 0.5) * 1.35) ** 2 + ((ny - 0.5) * 1.35) ** 2)
    vig = np.clip(vig, 0.35, 1.0)

    arr = np.stack([
        np.clip(r * vig, 0, 255).astype(np.uint8),
        np.clip(g * vig, 0, 255).astype(np.uint8),
        np.clip(b * vig, 0, 255).astype(np.uint8),
        np.full((h, w), 255, dtype=np.uint8)
    ], axis=-1)

    img = Image.fromarray(arr, "RGBA")
    img.save(os.path.join(OUT_DIR, "menu_backdrop_hd.png"))


def star_points(cx, cy, r_outer, r_inner, n=5, angle_offset=-math.pi / 2):
    pts = []
    for i in range(n * 2):
        r = r_outer if i % 2 == 0 else r_inner
        a = angle_offset + i * math.pi / n
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return pts


def generate_nation_flags():
    configs = [
        # 0: USSR
        ((165, 32, 28), (245, 198, 72), "ussr"),
        # 1: GER
        ((76, 86, 96), (225, 230, 235), "ger"),
        # 2: UK
        ((36, 62, 118), (218, 52, 48), "uk"),
        # 3: JPN
        ((235, 228, 212), (196, 36, 36), "jpn"),
        # 4: USA
        ((52, 102, 56), (242, 245, 240), "usa"),
    ]
    size = 128
    for idx, (bg, fg, kind) in enumerate(configs):
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        # Brass outer ring
        d.ellipse([4, 4, size - 4, size - 4], fill=(188, 162, 92, 255), outline=(245, 222, 138, 255), width=3)
        d.ellipse([12, 12, size - 12, size - 12], fill=bg + (255,), outline=(30, 34, 32, 255), width=2)

        cx, cy = size / 2, size / 2
        if kind == "ussr":
            d.polygon(star_points(cx, cy, 38, 15, 5), fill=(248, 208, 70, 255), outline=(120, 20, 15, 255))
        elif kind == "ger":
            # Balkenkreuz
            d.rectangle([cx - 34, cy - 10, cx + 34, cy + 10], fill=(240, 240, 240, 255))
            d.rectangle([cx - 10, cy - 34, cx + 10, cy + 34], fill=(240, 240, 240, 255))
            d.rectangle([cx - 34, cy - 6, cx + 34, cy + 6], fill=(22, 24, 26, 255))
            d.rectangle([cx - 6, cy - 34, cx + 6, cy + 34], fill=(22, 24, 26, 255))
        elif kind == "uk":
            # RAF Roundel
            d.ellipse([cx - 34, cy - 34, cx + 34, cy + 34], fill=(28, 56, 122, 255), outline=(240, 212, 125, 255), width=2)
            d.ellipse([cx - 22, cy - 22, cx + 22, cy + 22], fill=(242, 242, 242, 255))
            d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], fill=(208, 42, 42, 255))
        elif kind == "jpn":
            # Rising Sun rays
            for r_i in range(16):
                a1 = r_i * (2 * math.pi / 16) - 0.06
                a2 = r_i * (2 * math.pi / 16) + 0.06
                d.polygon([
                    (cx, cy),
                    (cx + math.cos(a1) * 48, cy + math.sin(a1) * 48),
                    (cx + math.cos(a2) * 48, cy + math.sin(a2) * 48)
                ], fill=(198, 38, 34, 255))
            d.ellipse([cx - 18, cy - 18, cx + 18, cy + 18], fill=(198, 38, 34, 255))
            d.ellipse([4, 4, size - 4, size - 4], outline=(218, 190, 108, 255), width=6)
        elif kind == "usa":
            d.ellipse([cx - 36, cy - 36, cx + 36, cy + 36], fill=(42, 82, 46, 255), outline=(240, 240, 240, 255), width=3)
            d.polygon(star_points(cx, cy, 32, 13, 5), fill=(248, 248, 248, 255))

        img.save(os.path.join(OUT_DIR, f"flag_{idx}.png"))


def generate_unit_cards():
    w, h = 128, 80
    for i in range(17):
        img = Image.new("RGBA", (w, h), (22, 28, 25, 255))
        d = ImageDraw.Draw(img)

        # Subtle dark tactical crosshatch grid background
        for gy in range(8, h - 8, 12):
            d.line([(4, gy), (w - 4, gy)], fill=(28, 36, 32, 255), width=1)
        for gx in range(8, w - 8, 16):
            d.line([(gx, 4), (gx, h - 4)], fill=(28, 36, 32, 255), width=1)

        # Tactical card beveled frame
        d.rectangle([1, 1, w - 2, h - 2], outline=(175, 155, 95, 255), width=2)
        d.rectangle([3, 3, w - 4, h - 4], outline=(42, 50, 46, 255), width=1)

        fg = (230, 236, 222, 255)       # Soldier / Hull base
        acc = (250, 212, 98, 255)       # Gold equipment / weapon highlight
        steel = (145, 155, 165, 255)    # Gunmetal / Armor plating
        dark = (34, 38, 42, 255)        # Deep recesses / treads
        fire = (255, 125, 30, 255)      # Flamethrower / Rocket flame
        cyan = (85, 215, 245, 255)      # Sniper scope optic / crosshairs

        if i == 0:
            # 0: RIFLE_REGIMENT (Infantry Squad: Helmet, webbing & M1/Kar98k rifle)
            d.ellipse([54, 15, 74, 33], fill=fg)  # Helmet
            d.arc([52, 22, 76, 35], 0, 180, fill=steel, width=2)  # Helmet rim
            d.polygon([(46, 68), (52, 36), (76, 36), (82, 68)], fill=fg)  # Torso
            # Crossed ammo bandolier
            d.line([(52, 38), (80, 64)], fill=(125, 98, 55, 255), width=3)
            # Service Rifle with wooden stock & barrel
            d.line([(32, 58), (96, 26)], fill=(135, 88, 48, 255), width=5) # Wood stock
            d.line([(46, 51), (104, 22)], fill=acc, width=3)               # Steel barrel
            d.line([(102, 23), (110, 19)], fill=steel, width=2)             # Bayonet tip

        elif i == 1:
            # 1: MG_REGIMENT (Heavy Machine Gun Section: Tripod, cooling jacket & ammo belt)
            # Gunner crouched
            d.ellipse([34, 26, 50, 42], fill=fg)  # Helmet
            d.polygon([(30, 68), (36, 44), (54, 44), (58, 68)], fill=fg)
            # Heavy Machine Gun Tripod & Receiver
            d.line([(62, 52), (48, 68)], fill=steel, width=3)  # Rear leg
            d.line([(62, 52), (78, 68)], fill=steel, width=3)  # Front leg
            d.rectangle([54, 46, 76, 54], fill=dark, outline=acc, width=1) # Receiver
            # Ventilated barrel cooling jacket with flash hider
            d.line([(76, 49), (114, 47)], fill=steel, width=5)
            d.line([(114, 46), (120, 46)], fill=acc, width=3) # Flash hider
            # Ammo can and brass linked ammunition belt
            d.rectangle([48, 56, 60, 68], fill=(95, 108, 75, 255), outline=acc, width=1)
            d.arc([54, 48, 68, 62], 45, 225, fill=acc, width=3) # Belt feed

        elif i == 2:
            # 2: MORTAR_REGIMENT (81mm Mortar Battery: Angled tube, baseplate & bomb)
            # Circular ground baseplate
            d.ellipse([32, 62, 58, 70], fill=dark, outline=steel, width=2)
            # A-frame bipod support
            d.line([(64, 44), (56, 68)], fill=steel, width=3)
            d.line([(64, 44), (74, 68)], fill=steel, width=3)
            # Heavy 81mm Mortar Tube at 55-degree elevation
            d.line([(44, 64), (88, 28)], fill=fg, width=8)
            d.line([(44, 64), (88, 28)], fill=acc, width=4)
            # 81mm finned mortar shell positioned above muzzle
            d.ellipse([94, 18, 106, 28], fill=acc) # Shell body
            d.line([(100, 26), (106, 32)], fill=steel, width=4) # Tail fins
            # Ballistic trajectory dotted arc
            d.arc([85, 8, 122, 42], 180, 270, fill=acc, width=2)

        elif i == 3:
            # 3: AT_BAZOOKA_REGIMENT (Anti-Tank Squad: Shoulder launcher with blast shield)
            # Soldier kneeling
            d.ellipse([38, 24, 54, 40], fill=fg)  # Helmet
            d.polygon([(32, 68), (38, 42), (58, 42), (62, 68)], fill=fg)
            # Shoulder-mounted rocket launch tube
            d.line([(22, 34), (106, 34)], fill=steel, width=8)
            d.line([(22, 34), (106, 34)], fill=dark, width=4)
            # Square blast shield with viewport
            d.rectangle([72, 22, 82, 46], fill=acc, outline=dark, width=1)
            d.rectangle([75, 28, 79, 34], fill=dark) # Viewport
            # Conical shaped-charge rocket warhead tip
            d.polygon([(106, 30), (120, 34), (106, 38)], fill=fire)

        elif i == 4:
            # 4: ENGINEER_FLAME_REGIMENT (Combat Engineer: Twin backpack tanks & roaring fire jet)
            # Soldier silhouette
            d.ellipse([36, 20, 52, 36], fill=fg)  # Helmet
            d.polygon([(32, 68), (38, 38), (56, 38), (62, 68)], fill=fg)
            # Twin vertical backpack fuel tanks with manifold
            d.rounded_rectangle([20, 26, 30, 58], radius=3, fill=(185, 52, 38, 255), outline=acc, width=1)
            d.rounded_rectangle([28, 26, 38, 58], radius=3, fill=(185, 52, 38, 255), outline=acc, width=1)
            # Fuel lance projector wand
            d.line([(48, 46), (82, 44)], fill=steel, width=4)
            d.ellipse([80, 41, 86, 47], fill=acc) # Nozzle
            # Roaring swirling fire jet burst
            d.polygon([(86, 44), (104, 32), (124, 38), (112, 44), (126, 52), (102, 54)], fill=fire)
            d.polygon([(88, 44), (102, 36), (114, 42), (104, 46), (116, 50), (98, 48)], fill=(255, 235, 110, 255))

        elif i == 5:
            # 5: SNIPER_REGIMENT (Scout Sniper: Scoped precision rifle & glowing tactical optic reticle)
            # Prone/kneeling ghillie sniper
            d.ellipse([34, 32, 50, 48], fill=(95, 115, 82, 255)) # Ghillie hood
            d.polygon([(26, 68), (34, 48), (64, 48), (72, 68)], fill=(75, 95, 62, 255))
            # Precision heavy-barrel sniper rifle with bipod
            d.line([(60, 58), (54, 68)], fill=steel, width=2) # Bipod
            d.line([(60, 58), (66, 68)], fill=steel, width=2)
            d.line([(42, 53), (112, 49)], fill=(125, 85, 45, 255), width=5) # Stock
            d.line([(56, 51), (122, 48)], fill=steel, width=3)              # Barrel
            # Large telescopic optical scope
            d.rectangle([68, 44, 88, 49], fill=dark, outline=acc, width=1)
            # Glowing tactical sniper crosshairs reticle
            cx, cy, cr = 98, 28, 18
            d.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], outline=cyan, width=2)
            d.ellipse([cx - cr / 2, cy - cr / 2, cx + cr / 2, cy + cr / 2], outline=cyan, width=1)
            d.line([(cx - cr - 4, cy), (cx + cr + 4, cy)], fill=cyan, width=1)
            d.line([(cx, cy - cr - 4), (cx, cy + cr + 4)], fill=cyan, width=1)
            d.point([(cx, cy)], fill=acc)

        elif i == 6:
            # 6: PARATROOP_REGIMENT (Airborne Infantry: Billowing parachute & descending soldier)
            # Billowing parachute canopy
            d.pieslice([36, 8, 92, 44], 180, 360, fill=(235, 238, 242, 255), outline=acc, width=2)
            d.line([(38, 26), (90, 26)], fill=(195, 205, 215, 255), width=1)
            # Shroud suspension lines
            d.line([(38, 26), (64, 52)], fill=steel, width=1)
            d.line([(52, 26), (64, 52)], fill=steel, width=1)
            d.line([(76, 26), (64, 52)], fill=steel, width=1)
            d.line([(90, 26), (64, 52)], fill=steel, width=1)
            # Airborne soldier silhouette with submachine gun
            d.ellipse([58, 48, 70, 60], fill=fg)
            d.polygon([(56, 74), (60, 58), (68, 58), (72, 74)], fill=fg)
            d.line([(58, 62), (84, 60)], fill=acc, width=3) # SMG

        elif i == 7:
            # 7: RECON_HALFTRACK (Light Armored Scout: Front wheels, rear tracks & scout antenna)
            d.polygon([(24, 56), (32, 42), (94, 42), (106, 56)], fill=fg) # Lower hull
            d.polygon([(46, 42), (54, 30), (84, 30), (90, 42)], fill=steel) # Cab
            d.rectangle([22, 54, 108, 60], fill=dark)
            # Front steerable pneumatic wheel
            d.ellipse([92, 52, 108, 68], fill=dark, outline=acc, width=2)
            # Rear caterpillar track assembly with 3 roadwheels
            d.rounded_rectangle([24, 56, 82, 68], radius=5, fill=dark, outline=steel, width=2)
            d.ellipse([30, 58, 42, 66], fill=steel)
            d.ellipse([46, 58, 58, 66], fill=steel)
            d.ellipse([62, 58, 74, 66], fill=steel)
            # Radio whip antenna
            d.line([(54, 30), (46, 12)], fill=acc, width=2)

        elif i == 8:
            # 8: AA_HALFTRACK (Self-Propelled Anti-Air: Quad-20mm Flak barrels at high angle)
            # Vehicle hull & wheels/tracks
            d.polygon([(22, 58), (30, 46), (94, 46), (106, 58)], fill=fg)
            d.ellipse([92, 54, 108, 70], fill=dark, outline=acc, width=2)
            d.rounded_rectangle([22, 58, 80, 70], radius=5, fill=dark, outline=steel, width=2)
            # Armored quad-flak swivel mount
            d.ellipse([46, 38, 68, 48], fill=steel, outline=dark, width=1)
            # 4 Synchronized 20mm Flak barrels elevated at 65 degrees
            for offset in (-3, 0, 3):
                d.line([(54 + offset, 42), (86 + offset, 16)], fill=steel, width=3)
                d.line([(84 + offset, 18), (89 + offset, 14)], fill=acc, width=2) # Flash hider
            # Antiaircraft reticle symbol in corner
            d.ellipse([102, 12, 118, 28], outline=acc, width=2)
            d.line([(100, 20), (120, 20)], fill=acc, width=1)
            d.line([(110, 10), (110, 30)], fill=acc, width=1)

        elif i == 9:
            # 9: MOBILE_ARTILLERY (Self-Propelled Howitzer: Heavy casemate & 150mm gun)
            # Heavy tracked chassis
            d.polygon([(20, 58), (28, 42), (96, 42), (106, 58)], fill=fg)
            d.rounded_rectangle([18, 56, 108, 70], radius=6, fill=dark, outline=steel, width=2)
            for wx in range(26, 102, 14):
                d.ellipse([wx, 58, wx + 12, 68], fill=steel)
            # Tall armored casemate superstructure
            d.polygon([(36, 42), (44, 26), (78, 26), (84, 42)], fill=steel, outline=acc, width=1)
            # Massive elevated 150mm Howitzer cannon with muzzle brake
            d.line([(62, 34), (114, 16)], fill=steel, width=7)
            d.line([(62, 34), (114, 16)], fill=dark, width=3)
            d.rectangle([110, 13, 118, 19], fill=acc) # Muzzle brake

        elif i == 10:
            # 10: KATYUSHA_ROCKET (BM-13 Multi-Rail Rocket Artillery: Gantry & rockets)
            # Truck cab & dual rear axles
            d.rectangle([24, 48, 104, 62], fill=fg)
            d.polygon([(82, 48), (88, 36), (102, 36), (104, 48)], fill=steel) # Cab
            for wx in (32, 50, 92):
                d.ellipse([wx, 56, wx + 14, 70], fill=dark, outline=acc, width=2)
            # Elevated rocket launch gantry truss
            d.line([(32, 48), (84, 24)], fill=steel, width=4)
            d.line([(48, 48), (84, 24)], fill=steel, width=3)
            # 4 visible RS-132 rockets with fiery exhaust trails
            for ry in (-4, 0, 4):
                d.line([(36, 46 + ry), (96, 18 + ry)], fill=acc, width=3)
                d.polygon([(94, 17 + ry), (100, 15 + ry), (96, 21 + ry)], fill=fire) # Warhead
                # Fiery exhaust trail
                d.line([(36, 46 + ry), (22, 52 + ry)], fill=fire, width=3)

        elif i == 11:
            # 11: LIGHT_TANK (Fast Scout: Stuart / Panzer II: Compact hull, quick turret)
            d.polygon([(26, 56), (36, 44), (88, 44), (96, 56)], fill=fg)
            d.rounded_rectangle([22, 54, 98, 66], radius=5, fill=dark, outline=acc, width=2)
            for wx in range(28, 92, 14):
                d.ellipse([wx, 56, wx + 10, 64], fill=steel)
            # Compact rounded turret & 37mm quick-firing cannon
            d.polygon([(46, 44), (52, 32), (74, 32), (78, 44)], fill=steel)
            d.line([(74, 38), (112, 36)], fill=acc, width=4)
            # Scout chevron speed stripe in corner
            d.line([(102, 16), (110, 22), (102, 28)], fill=acc, width=3)

        elif i == 12:
            # 12: MEDIUM_TANK (Workhorse Battle Tank: T-34 / Sherman: Sloped glacis & 76mm gun)
            d.polygon([(18, 56), (32, 40), (96, 40), (108, 56)], fill=fg)
            d.rounded_rectangle([16, 54, 110, 68], radius=6, fill=dark, outline=steel, width=2)
            for wx in range(24, 102, 15):
                d.ellipse([wx, 56, wx + 12, 66], fill=steel)
            # Cast curved turret + commander cupola
            d.polygon([(44, 40), (52, 26), (82, 26), (88, 40)], fill=steel, outline=acc, width=1)
            d.ellipse([54, 23, 64, 28], fill=dark) # Cupola
            # High-velocity 76mm gun with double-baffle muzzle brake
            d.line([(82, 33), (120, 31)], fill=steel, width=6)
            d.line([(82, 33), (120, 31)], fill=acc, width=2)
            d.rectangle([116, 28, 122, 34], fill=acc) # Muzzle brake

        elif i == 13:
            # 13: TANK_DESTROYER (Ambush Hunter: StuG III / SU-85: Low-profile casemate & long AT gun)
            # Low-profile sleek casemate hull without turret
            d.polygon([(16, 56), (28, 42), (98, 42), (110, 56)], fill=fg)
            d.polygon([(36, 42), (48, 30), (84, 30), (92, 42)], fill=steel, outline=acc, width=1)
            d.rounded_rectangle([14, 54, 112, 68], radius=6, fill=dark, outline=steel, width=2)
            for wx in range(22, 104, 15):
                d.ellipse([wx, 56, wx + 12, 66], fill=steel)
            # Extra-long high-velocity anti-tank gun barrel protruding forward
            d.line([(78, 36), (126, 35)], fill=steel, width=6)
            d.line([(78, 36), (126, 35)], fill=acc, width=2)
            d.ellipse([74, 32, 84, 40], fill=dark) # Mantlet collar

        elif i == 14:
            # 14: HEAVY_TANK (Breakthrough Behemoth: Tiger I / IS-2: Massive armor & 88mm/122mm gun)
            # Wide heavy tracks & massive hull
            d.polygon([(14, 56), (22, 38), (102, 38), (114, 56)], fill=fg)
            d.rounded_rectangle([10, 52, 118, 68], radius=7, fill=dark, outline=acc, width=2)
            for wx in range(18, 110, 15):
                d.ellipse([wx, 54, wx + 14, 66], fill=steel)
            # Massive armored turret with side stowage bin
            d.polygon([(40, 38), (48, 22), (86, 22), (92, 38)], fill=steel, outline=acc, width=2)
            d.rectangle([34, 26, 42, 36], fill=dark) # Bustle bin
            # Long 88mm heavy cannon with perforated muzzle brake
            d.line([(86, 30), (126, 28)], fill=steel, width=7)
            d.line([(86, 30), (126, 28)], fill=acc, width=3)
            d.rectangle([122, 25, 128, 31], fill=acc) # Muzzle brake

        elif i == 15:
            # 15: ELITE_SIGNATURE_TANK (Super-Heavy Flagship: King Tiger / IS-3: Triple gold stars)
            # Sovereign super-heavy sloped armor
            d.polygon([(12, 56), (26, 36), (104, 36), (116, 56)], fill=fg)
            d.rounded_rectangle([8, 52, 120, 68], radius=7, fill=dark, outline=acc, width=3)
            for wx in range(16, 112, 16):
                d.ellipse([wx, 54, wx + 14, 66], fill=steel)
            # Sloped heavy turret + cupola
            d.polygon([(42, 36), (50, 20), (88, 20), (96, 36)], fill=steel, outline=acc, width=2)
            d.ellipse([54, 16, 66, 22], fill=acc) # Cupola
            # Massive high-velocity KwK 43 88mm / 122mm cannon
            d.line([(92, 28), (126, 26)], fill=steel, width=8)
            d.line([(92, 28), (126, 26)], fill=acc, width=4)
            d.rectangle([122, 22, 128, 30], fill=acc) # Muzzle brake
            # Triple Gold Stars (★★★) Emblem in top corner
            for s_idx in range(3):
                sx = 20 + s_idx * 14
                d.polygon(star_points(sx, 16, 6, 2.5, 5), fill=acc)

        elif i == 16:
            # 16: PACKED_HQ_TRUCK (Mobile Command HQ Convoy: Heavy prime mover with canvas canopy)
            d.rectangle([18, 48, 108, 62], fill=fg)
            d.polygon([(84, 48), (90, 34), (106, 34), (108, 48)], fill=steel) # Cab
            for wx in (26, 44, 62, 96):
                d.ellipse([wx, 56, wx + 14, 70], fill=dark, outline=acc, width=2)
            # Heavy arched canvas canopy with gold trim
            d.rounded_rectangle([18, 24, 84, 48], radius=4, fill=(162, 148, 108, 255), outline=acc, width=2)
            d.line([(34, 24), (34, 48)], fill=acc, width=1)
            d.line([(52, 24), (52, 48)], fill=acc, width=1)
            d.line([(68, 24), (68, 48)], fill=acc, width=1)
            # Mobile command antenna mast
            d.line([(88, 34), (88, 12)], fill=acc, width=2)
            d.ellipse([85, 9, 91, 15], fill=acc)

        img.save(os.path.join(OUT_DIR, f"unit_{i}.png"))


if __name__ == "__main__":
    generate_tileable_textures()
    generate_world_map_hd()
    generate_menu_backdrop_hd()
    generate_nation_flags()
    generate_unit_cards()
    print("Successfully generated all AAA high-resolution textures and maps in:", OUT_DIR)
