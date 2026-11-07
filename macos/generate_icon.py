import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H = 2048, 2048
img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

# -------------------------------------------------------------
# 1. Base Squircle & Drop Shadow
# -------------------------------------------------------------
rect_box = [160, 160, 1888, 1888]
radius = 380

# Drop shadow
shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
sdraw = ImageDraw.Draw(shadow)
sdraw.rounded_rectangle([160, 180, 1888, 1908], radius=radius, fill=(0, 0, 0, 175))
shadow = shadow.filter(ImageFilter.GaussianBlur(38))
img.paste(shadow, (0, 0), shadow)

# Base Squircle Mask
base_mask = Image.new("L", (W, H), 0)
bmdraw = ImageDraw.Draw(base_mask)
bmdraw.rounded_rectangle(rect_box, radius=radius, fill=255)

# Gradient background: dark military steel/olive
bg_arr = np.zeros((H, W, 4), dtype=np.uint8)
yy, xx = np.ogrid[:H, :W]

# Radial lighting center (top left-ish)
dist = np.sqrt((xx - 900)**2 + (yy - 700)**2)
light = np.clip(1.0 - dist / 1400.0, 0, 1)

# Diagonal gradient from olive steel to dark iron
t_diag = ((xx - 160) + (yy - 160)) / ((1888 - 160) * 2.0)
t_diag = np.clip(t_diag, 0.0, 1.0)

r_chan = (42 * (1 - 0.65 * t_diag) + 35 * light).clip(14, 255).astype(np.uint8)
g_chan = (56 * (1 - 0.62 * t_diag) + 42 * light).clip(18, 255).astype(np.uint8)
b_chan = (46 * (1 - 0.65 * t_diag) + 32 * light).clip(15, 255).astype(np.uint8)

bg_arr[:, :, 0] = r_chan
bg_arr[:, :, 1] = g_chan
bg_arr[:, :, 2] = b_chan
bg_arr[:, :, 3] = 255

# Subtle brushed metal noise
np.random.seed(42)
noise = (np.random.randn(H, W) * 2.5).astype(int)
for c in range(3):
    bg_arr[:, :, c] = np.clip(bg_arr[:, :, c].astype(int) + noise, 0, 255).astype(np.uint8)

bg_img = Image.fromarray(bg_arr, mode="RGBA")
img.paste(bg_img, (0, 0), base_mask)

draw = ImageDraw.Draw(img)

# Squircle outer beveled gold border (multi-line)
border_layers = 24
for i in range(border_layers):
    t = i / float(border_layers)
    # Metallic gold blend
    gold_r = int(245 * (1 - 0.3 * t))
    gold_g = int(205 * (1 - 0.35 * t))
    gold_b = int(85 * (1 - 0.45 * t))
    cur_box = [160 + i, 160 + i, 1888 - i, 1888 - i]
    draw.rounded_rectangle(cur_box, radius=max(10, radius - i), outline=(gold_r, gold_g, gold_b, 255), width=1)

# Inner groove & thin inner gold wire
draw.rounded_rectangle([186, 186, 1862, 1862], radius=radius - 26, outline=(25, 20, 10, 200), width=3)
draw.rounded_rectangle([198, 198, 1850, 1850], radius=radius - 38, outline=(180, 145, 55, 140), width=2)

# Corner heavy brass rivets
rivets = [(280, 280), (1768, 280), (280, 1768), (1768, 1768)]
for rx, ry in rivets:
    draw.ellipse([rx - 24, ry - 22, rx + 28, ry + 30], fill=(0, 0, 0, 130))
    draw.ellipse([rx - 22, ry - 22, rx + 22, ry + 22], fill=(195, 155, 60, 255), outline=(70, 50, 15, 255), width=3)
    draw.ellipse([rx - 16, ry - 16, rx + 16, ry + 16], fill=(225, 185, 80, 255))
    draw.ellipse([rx - 10, ry - 10, rx + 2, ry + 2], fill=(255, 245, 180, 200))
    # Screwdriver slot (slanted)
    draw.line([rx - 11, ry - 11, rx + 11, ry + 11], fill=(55, 38, 12, 255), width=4)

# -------------------------------------------------------------
# 2. Heraldic Shield
# -------------------------------------------------------------
shield_w = 440
shield_top = 400
shield_waist = 940
shield_bottom = 1580

def get_shield_pts(inset=0):
    w = shield_w - inset
    top = shield_top + inset
    waist = shield_waist + (inset * 0.25)
    bottom = shield_bottom - inset
    
    pts = []
    # top rim with gentle arch & center notch
    pts.append((1024 - w, top + 25))
    pts.append((1024 - w * 0.5, top + 5))
    pts.append((1024, top + 15))
    pts.append((1024 + w * 0.5, top + 5))
    pts.append((1024 + w, top + 25))
    
    steps = 24
    # right curve to bottom
    for i in range(steps + 1):
        t = i / float(steps)
        y = waist + t * (bottom - waist)
        x = 1024 + w * (1.0 - t**1.85)
        pts.append((x, y))
        
    pts.append((1024, bottom))
    
    # left curve back up
    for i in range(steps, -1, -1):
        t = i / float(steps)
        y = waist + t * (bottom - waist)
        x = 1024 - w * (1.0 - t**1.85)
        pts.append((x, y))
        
    return pts

# Shield drop shadow
shield_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ssdraw = ImageDraw.Draw(shield_shadow)
ssdraw.polygon([(x, y + 26) for (x, y) in get_shield_pts(-10)], fill=(0, 0, 0, 190))
shield_shadow = shield_shadow.filter(ImageFilter.GaussianBlur(32))
img.paste(shield_shadow, (0, 0), shield_shadow)

# Shield outer rim
shield_rim_pts = get_shield_pts(0)
draw.polygon(shield_rim_pts, fill=(215, 175, 65, 255), outline=(90, 65, 20, 255))
shield_rim_mid = get_shield_pts(14)
draw.polygon(shield_rim_mid, fill=(145, 110, 35, 255))
shield_inner_pts = get_shield_pts(24)

# Shield interior mask
shield_mask = Image.new("L", (W, H), 0)
smdraw = ImageDraw.Draw(shield_mask)
smdraw.polygon(shield_inner_pts, fill=255)

shield_img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
sidraw = ImageDraw.Draw(shield_img)
# Left half: Allied olive drab
sidraw.rectangle([0, 0, 1024, H], fill=(28, 42, 32, 255))
# Right half: Axis dark panzer grey
sidraw.rectangle([1024, 0, W, H], fill=(22, 24, 28, 255))
# Dividing center groove
sidraw.line([1024, shield_top, 1024, shield_bottom], fill=(160, 130, 50, 180), width=4)

# Shield internal radial vignette
s_light = np.clip(1.0 - np.sqrt((xx - 1024)**2 + (yy - 850)**2) / 600.0, 0, 1) * 30
sh_arr = np.array(shield_img)
for c in range(3):
    sh_arr[:, :, c] = np.clip(sh_arr[:, :, c].astype(int) + s_light.astype(int), 0, 255).astype(np.uint8)
shield_img = Image.fromarray(sh_arr, mode="RGBA")

img.paste(shield_img, (0, 0), shield_mask)

# -------------------------------------------------------------
# 3. Axis Iron Cross Motif
# -------------------------------------------------------------
ic_x, ic_y = 1024, 880

def draw_iron_cross(d, cx, cy, span, inner_w, outer_w, fill, outline=None, outline_w=1):
    pts = []
    # Up arm
    pts.append((cx - inner_w, cy - inner_w))
    pts.append((cx - outer_w, cy - span))
    pts.append((cx, cy - span * 0.94))
    pts.append((cx + outer_w, cy - span))
    pts.append((cx + inner_w, cy - inner_w))
    
    # Right arm
    pts.append((cx + span, cy - outer_w))
    pts.append((cx + span * 0.94, cy))
    pts.append((cx + span, cy + outer_w))
    pts.append((cx + inner_w, cy + inner_w))
    
    # Down arm
    pts.append((cx + outer_w, cy + span))
    pts.append((cx, cy + span * 0.94))
    pts.append((cx - outer_w, cy + span))
    pts.append((cx - inner_w, cy + inner_w))
    
    # Left arm
    pts.append((cx - span, cy + outer_w))
    pts.append((cx - span * 0.94, cy))
    pts.append((cx - span, cy - outer_w))
    
    if outline:
        d.polygon(pts, fill=fill, outline=outline, width=outline_w)
    else:
        d.polygon(pts, fill=fill)

# Cross shadow
ic_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
icsdraw = ImageDraw.Draw(ic_shadow)
draw_iron_cross(icsdraw, ic_x, ic_y + 16, 320, 72, 220, fill=(0, 0, 0, 190))
ic_shadow = ic_shadow.filter(ImageFilter.GaussianBlur(22))
img.paste(ic_shadow, (0, 0), shield_mask)

# Cross Outer Silver Bevel
cross_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
cldraw = ImageDraw.Draw(cross_layer)
draw_iron_cross(cldraw, ic_x, ic_y, 320, 72, 220, fill=(235, 238, 242, 255), outline=(100, 105, 110, 255), outline_w=4)
# Inner Silver highlight trim
draw_iron_cross(cldraw, ic_x, ic_y, 308, 66, 210, fill=(185, 190, 198, 255))
# Core Enamel Black
draw_iron_cross(cldraw, ic_x, ic_y, 296, 58, 198, fill=(18, 20, 23, 255), outline=(40, 44, 48, 255), outline_w=2)

img.paste(cross_layer, (0, 0), shield_mask)

# -------------------------------------------------------------
# 4. Allied Star Medallion in Center
# -------------------------------------------------------------
star_cx, star_cy = ic_x, ic_y
med_r = 142

# Medallion Shadow
med_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
mdsdraw = ImageDraw.Draw(med_shadow)
mdsdraw.ellipse([star_cx - med_r - 8, star_cy - med_r - 4, star_cx + med_r + 8, star_cy + med_r + 16], fill=(0, 0, 0, 200))
med_shadow = med_shadow.filter(ImageFilter.GaussianBlur(18))
img.paste(med_shadow, (0, 0), med_shadow)

# Outer Gold Rim of Medallion
draw.ellipse([star_cx - med_r, star_cy - med_r, star_cx + med_r, star_cy + med_r], fill=(225, 185, 65, 255), outline=(95, 70, 20, 255), width=6)
# Inner ring
draw.ellipse([star_cx - (med_r - 14), star_cy - (med_r - 14), star_cx + (med_r - 14), star_cy + (med_r - 14)], fill=(145, 110, 35, 255))
# Inner dark forest green disc
draw.ellipse([star_cx - (med_r - 22), star_cy - (med_r - 22), star_cx + (med_r - 22), star_cy + (med_r - 22)], fill=(24, 38, 28, 255), outline=(195, 160, 55, 255), width=3)
# Allied White concentric insignia ring
draw.ellipse([star_cx - (med_r - 36), star_cy - (med_r - 36), star_cx + (med_r - 36), star_cy + (med_r - 36)], outline=(245, 240, 215, 200), width=4)

# 5-pointed Allied Star
def get_star_pts(cx, cy, r_out, r_in, points=5, rot_deg=-90):
    pts = []
    angle_step = math.pi / points
    rot = math.radians(rot_deg)
    for i in range(2 * points):
        r = r_out if i % 2 == 0 else r_in
        a = rot + i * angle_step
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

star_pts = get_star_pts(star_cx, star_cy, 88, 35)

# Star Drop Shadow
draw.polygon([(x + 3, y + 5) for (x, y) in star_pts], fill=(0, 0, 0, 140))
# Star Body
draw.polygon(star_pts, fill=(255, 253, 240, 255), outline=(190, 155, 50, 255), width=3)
# Faceted 3D shading
for i in range(5):
    p_tip = star_pts[i * 2]
    p_next_in = star_pts[(i * 2 + 1) % 10]
    draw.polygon([(star_cx, star_cy), p_tip, p_next_in], fill=(215, 200, 150, 140))

# -------------------------------------------------------------
# 5. "A & A" Emblem Plaque
# -------------------------------------------------------------
plaque_w = 340 # half width = 340 => 680 total width
plaque_top = 1270
plaque_bot = 1450
px1, py1 = 1024 - plaque_w, plaque_top
px2, py2 = 1024 + plaque_w, plaque_bot

# Plaque shadow
plaque_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
psdraw = ImageDraw.Draw(plaque_shadow)
psdraw.rounded_rectangle([px1 - 10, py1 + 12, px2 + 10, py2 + 28], radius=24, fill=(0, 0, 0, 220))
plaque_shadow = plaque_shadow.filter(ImageFilter.GaussianBlur(24))
img.paste(plaque_shadow, (0, 0), plaque_shadow)

# Plaque outer gold frame
draw.rounded_rectangle([px1, py1, px2, py2], radius=24, fill=(220, 180, 65, 255), outline=(90, 65, 20, 255), width=4)
# Plaque inner step
draw.rounded_rectangle([px1 + 8, py1 + 8, px2 - 8, py2 - 8], radius=18, fill=(135, 100, 30, 255))
# Plaque dark gunmetal center
draw.rounded_rectangle([px1 + 16, py1 + 16, px2 - 16, py2 - 16], radius=14, fill=(18, 22, 20, 255), outline=(180, 145, 50, 255), width=2)

# Rivets on plaque corners
for rx in [px1 + 26, px2 - 26]:
    for ry in [py1 + 26, py2 - 26]:
        draw.ellipse([rx - 7, ry - 7, rx + 7, ry + 7], fill=(230, 190, 70, 255), outline=(80, 55, 15, 255), width=2)
        draw.ellipse([rx - 3, ry - 3, rx + 1, ry + 1], fill=(255, 245, 180, 220))

# "A & A" Typography in Georgia Bold
font_georgia_bold = "/System/Library/Fonts/Supplemental/Georgia Bold.ttf"
font_aa = ImageFont.truetype(font_georgia_bold, 155)
aa_text = "A & A"
bbox = font_aa.getbbox(aa_text)
aaw = bbox[2] - bbox[0]
aah = bbox[3] - bbox[1]
aax = 1024 - aaw // 2 - bbox[0]
aay = (py1 + py2) // 2 - aah // 2 - bbox[1] - 4

# Deep 3D drop shadow
draw.text((aax + 4, aay + 6), aa_text, font=font_aa, fill=(0, 0, 0, 240))
# Gold 3D bevel layers
for offset in range(6, 0, -1):
    val = int(120 + offset * 8)
    draw.text((aax, aay + offset), aa_text, font=font_aa, fill=(val, int(val * 0.72), int(val * 0.22), 255))
# Face metallic highlight
draw.text((aax, aay), aa_text, font=font_aa, fill=(255, 235, 130, 255))
draw.text((aax, aay - 1), aa_text, font=font_aa, fill=(255, 248, 190, 160))

# -------------------------------------------------------------
# 6. Top Header "AXIS & ALLIES" & Subtitle "2004"
# -------------------------------------------------------------
font_header = ImageFont.truetype(font_georgia_bold, 68)
header_text = "AXIS & ALLIES"
hbbox = font_header.getbbox(header_text)
hw = hbbox[2] - hbbox[0]
hh = hbbox[3] - hbbox[1]
hx = 1024 - hw // 2 - hbbox[0]
hy = 310

# Header mini plaque bar
h_pad = 28
draw.rounded_rectangle([1024 - hw // 2 - h_pad, hy - 14, 1024 + hw // 2 + h_pad, hy + hh + 14], radius=12, fill=(18, 24, 20, 230), outline=(200, 160, 55, 255), width=3)
draw.text((hx + 3, hy + 4), header_text, font=font_header, fill=(0, 0, 0, 200))
draw.text((hx, hy + 2), header_text, font=font_header, fill=(130, 95, 25, 255))
draw.text((hx, hy), header_text, font=font_header, fill=(250, 220, 110, 255))

# Subtitle "2004" at shield apex bottom
font_sub = ImageFont.truetype(font_georgia_bold, 54)
sub_text = "2 0 0 4"
sbbox = font_sub.getbbox(sub_text)
sw = sbbox[2] - sbbox[0]
sh = sbbox[3] - sbbox[1]
sx = 1024 - sw // 2 - sbbox[0]
sy = 1485

# Mini plaque for 2004
draw.rounded_rectangle([1024 - sw // 2 - 24, sy - 8, 1024 + sw // 2 + 24, sy + sh + 10], radius=10, fill=(16, 20, 18, 240), outline=(190, 150, 50, 255), width=3)
draw.text((sx + 2, sy + 3), sub_text, font=font_sub, fill=(0, 0, 0, 220))
draw.text((sx, sy + 1), sub_text, font=font_sub, fill=(130, 95, 25, 255))
draw.text((sx, sy), sub_text, font=font_sub, fill=(255, 225, 110, 255))

# -------------------------------------------------------------
# 7. Resample to 1024x1024 Lanczos & Save
# -------------------------------------------------------------
final_icon = img.resize((1024, 1024), Image.Resampling.LANCZOS)
final_icon.save("/tmp/aa2004_icon_1024.png", "PNG")
print("Successfully generated /tmp/aa2004_icon_1024.png")
