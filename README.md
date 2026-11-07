# Axis & Allies (2004 Collector's Edition) — Native C++20 & Raylib 3D Engine for MacBook Pro

A complete, native **C++20** and **Raylib 5.5 (OpenGL 4.1 3D)** replication of the classic **2004 *Axis & Allies* PC Video Game (TimeGate Studios / Atari Real-Time Strategy & WWII Strategy Simulator)**, hardware-accelerated and optimized specifically for your MacBook Pro (**Intel Core i7-9750H** + **AMD Radeon Pro 5300M 4GB** + **Retina HiDPI Display**).

---

## Quick Start (Play Immediately)

### Option 1: Double-Click the Native macOS Application Bundle
Double-click **`Axis & Allies 2004.app`** in `/Users/hainan/projects/axis-allies` (configured with custom `AppIcon.icns`, Retina HiDPI, and `NSSupportsAutomaticGraphicsSwitching = false` to lock onto your discrete **AMD Radeon Pro 5300M 4GB** GPU), or launch from Terminal:
```bash
open "/Users/hainan/projects/axis-allies/Axis & Allies 2004.app"
```

### Option 2: Run Directly from Terminal
```bash
cd /Users/hainan/projects/axis-allies
./bin/axis_allies_2004
```

### Build & Run Automated Verification Suite
```bash
make -j12       # Compiles native x86_64 C++20 binary with -O3 -march=native
make test       # Runs headless C++20 verification suite (5 nations, 20 generals, 60 spec ops, mobile HQ pack/unpack, stances, RTS AI, WW2 simulator, save/load)
make app        # Packages "Axis & Allies 2004.app" with custom AppIcon.icns
```

---

## Authentic 2004 Video Game Features Implemented End-to-End

1. **5 Playable Superpowers & Nation-Specific 3D Armor**:
   - **United States**, **Great Britain**, **Soviet Union**, **Germany**, and **Imperial Japan**.
   - Unique signature heavy armor & artillery (*King Tiger*, *T26E4 Super Pershing*, *IS-3 Shock Tank*, *Churchill Crocodile Flamethrower Tank*, *Type 5 Ho-Ri*, *BM-13 Katyusha Rocket Battery*).
2. **All 20 Historical Generals & 60 Special Operations**:
   - 4 Historical Commanders per nation (*Patton, Eisenhower, Nimitz, Arnold, Montgomery, Wingate, Wavell, Mountbatten, Zhukov, Konev, Rokossovsky, Chuikov, Rommel, Manstein, Guderian, Kesselring, Yamamoto, Yamashita, Nagumo, Kuribayashi*).
   - Passive doctinal bonuses + 3 active Command Point Special Operations (`F1`, `F2`, `F3`) per General (*101st Airborne Paradrop*, *B-17 Carpet Raid*, *V-2 Missile Strike*, *Katyusha Rocket Barrage*, *Battleship Yamato 18.1" Salvo*, *Ghost Division Blitzkrieg*, *SAS Sabotage*, etc.).
3. **2004 TimeGate RTS Mechanics — Mobile Pack/Unpack HQs, Formations & Supply Lines**:
   - **Mobile Base System**: Select your **Corps HQ**, **Infantry HQ**, **Mechanized HQ**, **Armor HQ**, or **Supply Depot** and press **`P`** (or click `[PACK SELECTED HQ INTO MOBILE TRUCK]`) to pack the building into a wheeled convoy truck, drive it behind your advancing frontlines, and unpack it!
   - **Supply Lines & Organization**: HQs, Supply Depots, and captured Cities project a visual Supply Radius (`TAB`). Regiments inside friendly supply regenerate health and Organization (cyan bar); out-of-supply regiments suffer speed and firepower attrition.
   - **Regimental Formation Stances (`Z` / `V` / `C`)**: Switch selected regiments between **Line Formation (`Z`)**, **Road Column (`V`, +25% move speed)**, and **Entrenched Cover (`C`, +35% defense & 3D sandbag emplacements)**.
   - **3-Resource War Economy**: Real-time **Money ($)**, **Ammo**, and **Oil** rates + **Tech Tiers 1–3**.
4. **World War II Mode (1942 Global Strategy Simulator + 3D RTS / Live Tactical Simulator)**:
   - 26 interconnected global territories across Europe, North Africa, Russia, and the Pacific.
   - R&D Weapons Lab (6 secret WWII technologies), division mobilization, and strategic maneuvers.
   - Resolve any frontline invasion either by **personally commanding your regiments in a 3D Real-Time Strategy Battle** (with surviving regiments returning to the global map) or running the **Live Tactical Operational Simulator** (4 combined-arms engagement waves: *Air Superiority & Flak*, *Artillery Counter-Battery*, *Armored Spearhead*, *Infantry Close Assault*).
5. **12 Historical Campaign Operations & 8 Custom 3D Skirmish Theaters**:
   - 6 Allied Campaigns + 6 Axis Campaigns with persistent completion tracking (`saves/campaign_profile.dat`) + 8 custom skirmish maps with 3D weather effects (falling snow, desert sandstorms, Normandy rain).
6. **Procedural WWII Orchestral Audio & Weapon SFX Engine**:
   - Zero external audio asset dependencies — synthesizes multi-voice brass/snare military marches and 19 combat sound effects in C++ PCM memory via CoreAudio.

---

## Controls & Hotkeys

| Input | Action |
|---|---|
| **Left-Click / Drag Box** | Select Regiment, Building, or Box-Select Multiple Regiments |
| **Double-Click Regiment** | Select all player regiments of the same type |
| **Right-Click Ground / Enemy** | Move Formation / Attack Target / Capture City or Oil Refinery |
| **`Z` / `V` / `C`** | Set Formation Stance: **Line (`Z`)**, **Road Column (`V`)**, **Entrench (`C`)** |
| **`Ctrl + 1..9` / `1..9`** | Assign Control Group / Recall Control Group (Double-tap centers camera) |
| **`WASD` / Arrow Keys / Screen Edge** | Pan 3D Tactical Camera |
| **`Q` / `E` or Middle-Mouse Drag** | Rotate 3D Camera Orbit |
| **`R` / `F` & Mouse Wheel** | Tilt Camera Pitch & Zoom In/Out |
| **`P`** | Pack Selected HQ into Mobile Truck / Unpack Truck into HQ |
| **`F1` / `F2` / `F3`** | Activate General Special Operations 1, 2, or 3 |
| **`F5` / `F8`** | Quick-Save / Quick-Load 3D RTS Battle (`saves/quicksave_rts.dat`) |
| **`TAB`** | Toggle 3D Supply Line Radius Rings |
| **`A` / `H` / `S`** | Select All Combat Units / Center on Corps HQ / Stop Selected Units |
| **`SPACE`** | Pause / Resume Tactical Battle |
