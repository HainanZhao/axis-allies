// ============================================================================
// AXIS & ALLIES (2004) — COMPLETE GAME DATABASE IMPLEMENTATION
// 5 Nations, 20 Historical Generals, 60 Special Operations, 17 Regiments,
// 10 Buildings/Mobile HQs, 8 Skirmish Theaters, 12 Campaigns, 26 Territories
// ============================================================================

#include "data/game_data.hpp"

namespace aa2004 {

const GameData& GameData::instance() {
    static GameData inst;
    return inst;
}

GameData::GameData() {
    initNationsAndGenerals();
    initBuildings();
    initRegiments();
    initMapsAndCampaigns();
    initWorldTerritories();
}

const NationDef& GameData::getNation(NationId id) const {
    size_t idx = static_cast<size_t>(id);
    if (idx >= nations_.size()) idx = static_cast<size_t>(NationId::USA);
    return nations_[idx];
}

const GeneralDef& GameData::getGeneral(NationId nation, int generalIdx) const {
    const auto& n = getNation(nation);
    if (generalIdx < 0 || generalIdx >= static_cast<int>(n.generals.size())) {
        return n.generals[0];
    }
    return n.generals[generalIdx];
}

const BuildingDef& GameData::getBuilding(BuildingType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx >= buildings_.size()) idx = 0;
    return buildings_[idx];
}

const RegimentDef& GameData::getRegiment(RegimentType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx >= regiments_.size()) idx = 0;
    return regiments_[idx];
}

std::string GameData::getRegimentDisplayName(NationId nation, RegimentType type) const {
    const auto& n = getNation(nation);
    switch (type) {
        case RegimentType::LIGHT_TANK:           return n.lightTankName;
        case RegimentType::MEDIUM_TANK:          return n.mediumTankName;
        case RegimentType::TANK_DESTROYER:       return n.tankDestroyerName;
        case RegimentType::HEAVY_TANK:           return n.heavyTankName;
        case RegimentType::ELITE_SIGNATURE_TANK: return n.eliteTankName;
        case RegimentType::MOBILE_ARTILLERY:     return n.spgName;
        default:                                 return getRegiment(type).name;
    }
}

void GameData::initNationsAndGenerals() {
    // ------------------------------------------------------------------------
    // 0. SOVIET UNION (USSR)
    // ------------------------------------------------------------------------
    {
        NationDef n{};
        n.id = NationId::USSR;
        n.code = "USSR";
        n.name = "Soviet Union";
        n.shortName = "U.S.S.R.";
        n.motto = "Not One Step Back!";
        n.description = "Massed Red Army infantry reserves, T-34/85 & IS-2 tank waves, elite Snipers, and screaming BM-13 Katyusha rocket salvos.";
        n.signatureUnitName = "BM-13 Katyusha Rocket Battery & IS-3 Heavy Tank";
        n.alliance = Alliance::ALLIES;
        n.primaryColor = {175, 42, 34, 255};
        n.accentColor = {255, 138, 120, 255};
        n.mapColor = {180, 48, 38, 255};
        n.startingIPC = 24;
        n.capitalTerritoryId = 8; // Moscow / Russia
        n.lightTankName = "BT-7 Fast Tank Regiment";
        n.mediumTankName = "T-34/85 Medium Tank Regiment";
        n.tankDestroyerName = "SU-100 Tank Destroyer Regiment";
        n.heavyTankName = "IS-2 Guards Heavy Tank Regiment";
        n.eliteTankName = "IS-3 Shock Armor Regiment";
        n.spgName = "SU-122 Howitzer Battery";

        // 4 Historical Soviet Generals
        GeneralDef zhukov{};
        zhukov.id = "zhukov";
        zhukov.name = "Marshal Georgy Zhukov";
        zhukov.nickname = "Savior of Moscow, Stalingrad & Berlin";
        zhukov.specialty = "Deep Battle Doctrine & Shock Offensives";
        zhukov.bio = "Deputy Supreme Commander of the Red Army. Orchestrated the decisive victories at Moscow, Stalingrad, Kursk, and Berlin.";
        zhukov.passiveDesc = "+18% Infantry & Tank Attack, +20% Organization Recovery";
        zhukov.nation = NationId::USSR;
        zhukov.portraitColor = {160, 38, 30, 255};
        zhukov.infAttackMult = 1.18f;
        zhukov.armorAttackMult = 1.15f;
        zhukov.orgRegenMult = 1.20f;
        zhukov.specialOps = {{
            {"communist_zeal", "Communist Zeal (Urrah!)", "+30% Atk/Spd & Full Org", "Fires up the Red Army: +30% Attack, +25% Speed, and full Organization recovery.", 90, 38.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"katyusha_salvo", "Stalin's Organ Katyusha Salvo", "Rocket Saturation Barrage", "Unleashes screaming M-13 rockets across the target zone, shattering enemy armor and morale.", 150, 50.0f, 0.0f, 32.0f, SpecOpTargetType::GROUND_AREA},
            {"guards_reserve", "Red Banner Guards Reserve", "Spawn T-34/85 + Rifle Regt", "Deploys a veteran T-34/85 Tank Regiment and Rifle Regiment at your Corps HQ.", 140, 52.0f, 0.0f, 0.0f, SpecOpTargetType::HQ_SPAWN}
        }};

        GeneralDef konev{};
        konev.id = "konev";
        konev.name = "Marshal Ivan Konev";
        konev.nickname = "Master of Red Army Artillery";
        konev.specialty = "Massed Artillery & IL-2 Sturmovik Support";
        konev.bio = "Commander of the 1st Ukrainian Front. Famous for massing hundreds of artillery pieces per kilometer before unleashing armor.";
        konev.passiveDesc = "+28% Artillery & Katyusha Damage, +25% Ammo Income";
        konev.nation = NationId::USSR;
        konev.portraitColor = {145, 42, 34, 255};
        konev.artilleryAttackMult = 1.28f;
        konev.ammoRateMult = 1.25f;
        konev.specialOps = {{
            {"artillery_prep", "203mm B-4 Howitzer Barrage", "Heavy Siege Bombardment", "Stalin's Sledgehammer 203mm howitzers pulverize enemy bunkers, HQs, and armor.", 135, 46.0f, 0.0f, 28.0f, SpecOpTargetType::GROUND_AREA},
            {"sturmovik_attack", "IL-2 Sturmovik Flying Tank Run", "23mm Cannon & Rocket Sweep", "Armored IL-2 Sturmoviks strafe enemy vehicles and depots with rockets.", 130, 44.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA},
            {"ural_gear_up", "Ural Factory Surge", "+200 All Resources & +20% Defense", "Grants +200 Money/Ammo/Oil immediately and boosts unit defense for 18s.", 100, 45.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL}
        }};

        GeneralDef rokossovsky{};
        rokossovsky.id = "rokossovsky";
        rokossovsky.name = "Marshal K. Rokossovsky";
        rokossovsky.nickname = "Architect of Operation Bagration";
        rokossovsky.specialty = "Double Envelopment & Tank Army Blitz";
        rokossovsky.bio = "Commanded the encirclement at Stalingrad and the destruction of Army Group Centre in Operation Bagration.";
        rokossovsky.passiveDesc = "+20% Tank Speed, -15% Armor Cost";
        rokossovsky.nation = NationId::USSR;
        rokossovsky.portraitColor = {152, 48, 36, 255};
        rokossovsky.armorSpeedMult = 1.20f;
        rokossovsky.armorCostMult = 0.85f;
        rokossovsky.specialOps = {{
            {"bagration_blitz", "Bagration Tank Breakthrough", "+35% Tank Speed & +25% Attack", "All Soviet tanks and vehicles surge forward with rapid speed and firepower.", 110, 42.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"partisan_uprising", "Partisan Behind-Lines Raid", "Spawn Partisans at Target", "Spawns two Soviet Rifle & Anti-Tank regiments at target coordinates to cut enemy supply.", 135, 52.0f, 0.0f, 18.0f, SpecOpTargetType::GROUND_AREA},
            {"katyusha_salvo", "Guards Mortar Rocket Strike", "Area Rocket Barrage", "Fires a dense salvo of high-explosive Katyusha rockets onto target coordinates.", 150, 50.0f, 0.0f, 30.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef chuikov{};
        chuikov.id = "chuikov";
        chuikov.name = "Gen. Vasily Chuikov";
        chuikov.nickname = "Defender of Stalingrad (62nd Army)";
        chuikov.specialty = "Urban Combat, Snipers & Fanatical Defense";
        chuikov.bio = "Held the ruins of Stalingrad against Paulus's Sixth Army by hugging enemy lines and deploying legendary snipers.";
        chuikov.passiveDesc = "+25% Unit Defense, -22% Infantry Cost";
        chuikov.nation = NationId::USSR;
        chuikov.portraitColor = {130, 34, 28, 255};
        chuikov.defenseMult = 1.25f;
        chuikov.infCostMult = 0.78f;
        chuikov.specialOps = {{
            {"motherland_calls", "Order 227: Motherland Calls", "Deploy 2 Rifle + 1 Sniper Regt", "Mobilizes two Rifle Regiments and an Elite Sniper Regiment at your Corps HQ.", 115, 42.0f, 0.0f, 0.0f, SpecOpTargetType::HQ_SPAWN},
            {"urban_shock", "Stalingrad Shock Doctrine", "+35% Inf Attack & Defense", "Friendly infantry and engineers deal +35% damage and take 30% less damage.", 90, 38.0f, 20.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"volga_rockets", "Volga Bank Rocket Barrage", "Heavy Rocket Bombardment", "Launches a devastating Katyusha barrage across the frontline.", 150, 52.0f, 0.0f, 30.0f, SpecOpTargetType::GROUND_AREA}
        }};

        n.generals = {zhukov, konev, rokossovsky, chuikov};
        nations_[static_cast<size_t>(NationId::USSR)] = n;
    }

    // ------------------------------------------------------------------------
    // 1. GERMANY (GER)
    // ------------------------------------------------------------------------
    {
        NationDef n{};
        n.id = NationId::GER;
        n.code = "GER";
        n.name = "Germany";
        n.shortName = "Germany";
        n.motto = "Blitzkrieg Doctrine";
        n.description = "Formidable Panzer divisions, deadly 88mm Flak batteries, Stuka dive bombers, and the terrifying King Tiger heavy tank.";
        n.signatureUnitName = "Panzer VI Ausf. B King Tiger (Tiger II)";
        n.alliance = Alliance::AXIS;
        n.primaryColor = {88, 98, 108, 255};
        n.accentColor = {185, 205, 220, 255};
        n.mapColor = {92, 104, 114, 255};
        n.startingIPC = 40;
        n.capitalTerritoryId = 5; // Berlin / Germany
        n.lightTankName = "Panzer III Ausf. L Regiment";
        n.mediumTankName = "Panzer IV Ausf. H Regiment";
        n.tankDestroyerName = "Jagdpanther TD Regiment";
        n.heavyTankName = "Panzer VI Tiger I Regiment";
        n.eliteTankName = "King Tiger (Tiger II) Heavy Regiment";
        n.spgName = "Hummel 150mm Artillery Battery";

        GeneralDef rommel{};
        rommel.id = "rommel";
        rommel.name = "Field Marshal Erwin Rommel";
        rommel.nickname = "The Desert Fox";
        rommel.specialty = "Lightning Blitzkrieg & Outflanking";
        rommel.bio = "Commander of the 7th Panzer Ghost Division and Afrika Korps. Master of speed, deception, and mobile 88mm ambushes.";
        rommel.passiveDesc = "+22% Tank Speed, -50% Out-of-Supply Penalty";
        rommel.nation = NationId::GER;
        rommel.portraitColor = {110, 100, 82, 255};
        rommel.armorSpeedMult = 1.22f;
        rommel.outOfSupplyPenaltyMult = 0.50f;
        rommel.specialOps = {{
            {"blitzkrieg", "Ghost Division Blitzkrieg", "+40% Speed, +25% Atk, No Supply Loss", "All German units gain +40% speed, +25% firepower, and ignore supply attrition for 18s!", 100, 40.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"stuka_dive", "Ju-87 Stuka Siren Dive Bomb", "Pinpoint Armor-Buster Strike", "Screaming Jericho-trumpet Stukas dive-bomb target armor and HQs.", 130, 44.0f, 0.0f, 22.0f, SpecOpTargetType::GROUND_AREA},
            {"flak_ambush", "88mm Flak Ambush Battery", "Deploy 88mm Battery + Inf", "Constructs a forward 88mm AA/AT Battery and deploys an Anti-Tank squad.", 135, 50.0f, 0.0f, 16.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef manstein{};
        manstein.id = "manstein";
        manstein.name = "Field Marshal E. von Manstein";
        manstein.nickname = "Master of the Backhand Blow";
        manstein.specialty = "Elastic Defense & Panzer Counterstrokes";
        manstein.bio = "Author of the Ardennes Sickle Cut plan and Kharkov counterstroke. Crushes overextended foes with heavy Panzers.";
        manstein.passiveDesc = "+20% Panzer Firepower, +22% Organization Recovery";
        manstein.nation = NationId::GER;
        manstein.portraitColor = {82, 92, 102, 255};
        manstein.armorAttackMult = 1.20f;
        manstein.orgRegenMult = 1.22f;
        manstein.specialOps = {{
            {"backhand_blow", "Kharkov Counterstroke", "Full Org + 30% Armor Attack", "Restores 100% Organization to all German forces and grants +30% attack.", 105, 42.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"nebelwerfer", "Nebelwerfer Screaming Mimi", "150mm Rocket Mortar Barrage", "Saturates the target zone with high-explosive Nebelwerfer rockets.", 145, 50.0f, 0.0f, 28.0f, SpecOpTargetType::GROUND_AREA},
            {"tiger_callin", "Schwere Panzer-Abteilung", "Deploy Tiger I Heavy Regiment", "Deploys an elite Tiger I Heavy Tank Regiment at your Corps HQ.", 180, 65.0f, 0.0f, 0.0f, SpecOpTargetType::HQ_SPAWN}
        }};

        GeneralDef guderian{};
        guderian.id = "guderian";
        guderian.name = "Gen. Heinz Guderian";
        guderian.nickname = "Father of the Panzerwaffe";
        guderian.specialty = "Massed Panzer Spearheads & Radio Logistics";
        guderian.bio = "Pioneer of modern mechanized warfare who insisted tanks fight in concentrated divisions linked by radio and mobile supply.";
        guderian.passiveDesc = "-15% Tank Cost, +20% Synthetic Oil Income";
        guderian.nation = NationId::GER;
        guderian.portraitColor = {72, 82, 92, 255};
        guderian.armorCostMult = 0.85f;
        guderian.oilRateMult = 1.20f;
        guderian.specialOps = {{
            {"achtung_panzer", "Achtung - Panzer! Surge", "+35% Speed & +25% Firepower", "Supercharges all tanks and half-tracks with speed and reload buffs.", 95, 38.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"field_workshop", "Panzer Field Workshop", "Repair All Units +45% & +150 Oil", "Repairs all friendly tanks, vehicles, and HQs across the battlefield.", 90, 38.0f, 0.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"stuka_dive", "Ju-87G Kanonenvogel Strike", "Twin-37mm Anti-Tank Stuka", "Tank-busting Stukas shred enemy armored columns in the target zone.", 140, 46.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef kesselring{};
        kesselring.id = "kesselring";
        kesselring.name = "Field Marshal A. Kesselring";
        kesselring.nickname = "Commander South & Luftwaffe";
        kesselring.specialty = "Fallschirmjager, Bunkers & V-Weapons";
        kesselring.bio = "Luftwaffe Field Marshal and master of the Gustav Line defense in Italy. Commands Fallschirmjager drops and V-2 strikes.";
        kesselring.passiveDesc = "+35% Bunker & HQ Health, +18% Unit Defense";
        kesselring.nation = NationId::GER;
        kesselring.portraitColor = {90, 104, 114, 255};
        kesselring.bunkerHealthMult = 1.35f;
        kesselring.defenseMult = 1.18f;
        kesselring.specialOps = {{
            {"fallschirmjager", "Fallschirmjager Green Devils", "Paradrop 2 Elite Regiments", "Drops Paratrooper and Panzerschreck regiments directly onto target coordinates.", 155, 58.0f, 0.0f, 18.0f, SpecOpTargetType::GROUND_AREA},
            {"v2_strike", "V-2 Vengeance Missile Strike", "Ballistic Warhead Impact", "Launches a supersonic V-2 ballistic warhead that devastates enemy bases and armor.", 200, 72.0f, 0.0f, 34.0f, SpecOpTargetType::GROUND_AREA},
            {"luftwaffe_sweep", "Fw-190 Luftwaffe Strafe", "Recon + Cluster Bomb Run", "Reveals the battlefield and strafes the target zone with 20mm cannons.", 120, 42.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA}
        }};

        n.generals = {rommel, manstein, guderian, kesselring};
        nations_[static_cast<size_t>(NationId::GER)] = n;
    }

    // ------------------------------------------------------------------------
    // 2. GREAT BRITAIN (UK)
    // ------------------------------------------------------------------------
    {
        NationDef n{};
        n.id = NationId::UK;
        n.code = "UK";
        n.name = "Great Britain";
        n.shortName = "Britain";
        n.motto = "Never Surrender";
        n.description = "Master of fortified defense, Royal Engineers, SAS depot sabotage, and Churchill Crocodile flamethrower tanks.";
        n.signatureUnitName = "Churchill Crocodile Flamethrower Tank";
        n.alliance = Alliance::ALLIES;
        n.primaryColor = {178, 136, 74, 255};
        n.accentColor = {245, 212, 152, 255};
        n.mapColor = {188, 144, 80, 255};
        n.startingIPC = 30;
        n.capitalTerritoryId = 3; // United Kingdom
        n.lightTankName = "Crusader Mk III Regiment";
        n.mediumTankName = "Cromwell Cruiser Regiment";
        n.tankDestroyerName = "Sherman Firefly 17-pdr Regiment";
        n.heavyTankName = "Churchill Mk VII Heavy Regiment";
        n.eliteTankName = "Churchill Crocodile Flame Regiment";
        n.spgName = "Sexton 25-pdr Artillery Battery";

        GeneralDef montgomery{};
        montgomery.id = "montgomery";
        montgomery.name = "Field Marshal B. Montgomery";
        montgomery.nickname = "Monty — Hero of El Alamein";
        montgomery.specialty = "Methodical Artillery & Steel Defense";
        montgomery.bio = "Commander of the British Eighth Army. Defeated Rommel at El Alamein through massed 25-pounders and steel resolve.";
        montgomery.passiveDesc = "+22% Unit Defense, +20% Artillery Firepower";
        montgomery.nation = NationId::UK;
        montgomery.portraitColor = {158, 122, 66, 255};
        montgomery.defenseMult = 1.22f;
        montgomery.artilleryAttackMult = 1.20f;
        montgomery.specialOps = {{
            {"creeping_barrage", "El Alamein Creeping Barrage", "25-Pounder Artillery Saturation", "Royal Artillery unleashes a concentrated 25-pounder barrage onto target area.", 130, 46.0f, 0.0f, 28.0f, SpecOpTargetType::GROUND_AREA},
            {"desert_rats", "Eighth Army Desert Rats Surge", "+30% Defense & +20% Firepower", "All British regiments gain +30% armor resilience and +20% attack for 18s.", 95, 40.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"royal_engineers", "Royal Engineers Overhaul", "Repair All HQs/Bunkers +50%", "Instantly repairs all friendly HQs, Depots, and Bunkers and grants +150 Ammo.", 90, 38.0f, 0.0f, 0.0f, SpecOpTargetType::GLOBAL}
        }};

        GeneralDef wingate{};
        wingate.id = "wingate";
        wingate.name = "Maj. Gen. Orde Wingate";
        wingate.nickname = "Founder of the Chindits";
        wingate.specialty = "SAS Sabotage & Deep Penetration";
        wingate.bio = "Unorthodox pioneer of long-range penetration commandos. Specializes in cutting enemy supply lines and blowing up depots.";
        wingate.passiveDesc = "+20% Infantry Speed, -60% Out-of-Supply Penalty";
        wingate.nation = NationId::UK;
        wingate.portraitColor = {108, 122, 66, 255};
        wingate.infSpeedMult = 1.20f;
        wingate.outOfSupplyPenaltyMult = 0.40f;
        wingate.specialOps = {{
            {"sas_sabotage", "SAS Depot Sabotage", "Demolish Enemy Depots & Drain Oil", "SAS Commandos detonate satchel charges at target location, crippling enemy structures.", 125, 45.0f, 0.0f, 20.0f, SpecOpTargetType::GROUND_AREA},
            {"chindit_drop", "Chindit Commando Insertion", "Drop Paratroop + AT Behind Lines", "Inserts an elite Airborne Commando & Anti-Tank task force at target point.", 150, 56.0f, 0.0f, 18.0f, SpecOpTargetType::GROUND_AREA},
            {"soe_propaganda", "SOE Psychological Warfare", "Drain 70% Enemy Organization", "Broadcasts demoralizing intelligence that drains enemy Organization in target area.", 85, 36.0f, 0.0f, 26.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef wavell{};
        wavell.id = "wavell";
        wavell.name = "Field Marshal A. Wavell";
        wavell.nickname = "Middle East & India Commander";
        wavell.specialty = "Commonwealth Reserves & Fortifications";
        wavell.bio = "Held the Middle East and India against overwhelming odds through clever deception and Commonwealth mobilization.";
        wavell.passiveDesc = "-18% Infantry Cost, +15% Money Income";
        wavell.nation = NationId::UK;
        wavell.portraitColor = {140, 110, 62, 255};
        wavell.infCostMult = 0.82f;
        wavell.moneyRateMult = 1.15f;
        wavell.specialOps = {{
            {"commonwealth_call", "Commonwealth Mobilization", "Deploy 2 Veteran Rifle Regts", "Deploys two Commonwealth Rifle Regiments at your Corps HQ and +150 Ammo.", 110, 42.0f, 0.0f, 0.0f, SpecOpTargetType::HQ_SPAWN},
            {"lancaster_raid", "RAF Lancaster Tallboy Raid", "Earthquake Bomb Carpet Raid", "RAF Bomber Command drops 12,000-lb Tallboy bombs across the target zone.", 195, 70.0f, 0.0f, 32.0f, SpecOpTargetType::GROUND_AREA},
            {"imperial_stand", "Imperial Resolve", "+25% Defense & Full Org", "Restores full Organization and bolsters unit defense across the map.", 90, 38.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL}
        }};

        GeneralDef mountbatten{};
        mountbatten.id = "mountbatten";
        mountbatten.name = "Adm. Lord Louis Mountbatten";
        mountbatten.nickname = "Chief of Combined Operations";
        mountbatten.specialty = "Radar, Typhoon Rockets & Naval Salvos";
        mountbatten.bio = "Head of British Combined Operations. Combines Royal Navy broadsides with RAF Hawker Typhoon rocket strikes.";
        mountbatten.passiveDesc = "+20% Supply Radius, +15% Oil Income";
        mountbatten.nation = NationId::UK;
        mountbatten.portraitColor = {64, 98, 120, 255};
        mountbatten.supplyRadiusMult = 1.20f;
        mountbatten.oilRateMult = 1.15f;
        mountbatten.specialOps = {{
            {"chain_home_radar", "Chain Home Radar Sweep", "Reveal Map & +25% Accuracy", "Reveals all enemy forces and boosts friendly firepower for 22 seconds.", 75, 32.0f, 22.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"typhoon_rockets", "Hawker Typhoon RP-3 Rockets", "Rocket Strike vs Armor", "RAF Hawker Typhoons fire salvos of 60-lb rockets onto enemy armor.", 140, 48.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA},
            {"royal_navy_salvo", "HMS King George V Salvo", "14-Inch Naval Bombardment", "Royal Navy battleship shells rain down on the target coordinates.", 175, 62.0f, 0.0f, 30.0f, SpecOpTargetType::GROUND_AREA}
        }};

        n.generals = {montgomery, wingate, wavell, mountbatten};
        nations_[static_cast<size_t>(NationId::UK)] = n;
    }

    // ------------------------------------------------------------------------
    // 3. IMPERIAL JAPAN (JPN)
    // ------------------------------------------------------------------------
    {
        NationDef n{};
        n.id = NationId::JPN;
        n.code = "JPN";
        n.name = "Imperial Japan";
        n.shortName = "Japan";
        n.motto = "Bushido & Imperial Will";
        n.description = "Unbreakable Bushido organization, swift Banzai charges, carrier Zero air raids, Yamato 18-inch salvos, and Kamikaze strikes.";
        n.signatureUnitName = "Type 5 Ho-Ri Heavy Tank Destroyer";
        n.alliance = Alliance::AXIS;
        n.primaryColor = {198, 118, 42, 255};
        n.accentColor = {255, 208, 128, 255};
        n.mapColor = {210, 128, 48, 255};
        n.startingIPC = 30;
        n.capitalTerritoryId = 19; // Tokyo / Japan
        n.lightTankName = "Type 95 Ha-Go Light Regiment";
        n.mediumTankName = "Type 97 ShinHoTo Chi-Ha Regiment";
        n.tankDestroyerName = "Type 3 Ho-Ni III TD Regiment";
        n.heavyTankName = "Type 4 Chi-To Heavy Regiment";
        n.eliteTankName = "Type 5 Ho-Ri Heavy Assault Regiment";
        n.spgName = "Type 4 Ho-Ro 150mm Battery";

        GeneralDef yamamoto{};
        yamamoto.id = "yamamoto";
        yamamoto.name = "Adm. Isoroku Yamamoto";
        yamamoto.nickname = "Commander-in-Chief, Combined Fleet";
        yamamoto.specialty = "Combined Fleet Air & Naval Salvos";
        yamamoto.bio = "Strategist of the Combined Fleet. Unleashes Mitsubishi Zero sweeps, Val dive bombers, and Yamato 18.1-inch salvos.";
        yamamoto.passiveDesc = "+25% Organization Recovery, +15% Money Income";
        yamamoto.nation = NationId::JPN;
        yamamoto.portraitColor = {180, 105, 36, 255};
        yamamoto.orgRegenMult = 1.25f;
        yamamoto.moneyRateMult = 1.15f;
        yamamoto.specialOps = {{
            {"imperial_will", "Imperial Will", "+30% Attack & Full Organization", "Inspires all Imperial forces with +30% firepower and full Organization!", 95, 38.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"yamato_salvo", "Battleship Yamato 18.1\" Salvo", "Super-Battleship Bombardment", "Yamato fires the largest naval guns in history onto target coordinates.", 185, 64.0f, 0.0f, 32.0f, SpecOpTargetType::GROUND_AREA},
            {"kamikaze_strike", "Divine Wind Kamikaze Strike", "Pinpoint High-Explosive Dive", "Explosive-laden aircraft dive directly into enemy armor or HQs.", 160, 52.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef yamashita{};
        yamashita.id = "yamashita";
        yamashita.name = "Gen. Tomoyuki Yamashita";
        yamashita.nickname = "The Tiger of Malaya";
        yamashita.specialty = "Banzai Shock Charges & Jungle Blitz";
        yamashita.bio = "Conquered Malaya and Singapore in 70 days through rapid flanking maneuvers and relentless night assaults.";
        yamashita.passiveDesc = "+25% Infantry Speed, +22% Infantry Attack";
        yamashita.nation = NationId::JPN;
        yamashita.portraitColor = {158, 92, 32, 255};
        yamashita.infSpeedMult = 1.25f;
        yamashita.infAttackMult = 1.22f;
        yamashita.specialOps = {{
            {"banzai_charge", "Tenno Heika Banzai! Charge", "+45% Speed & +40% Attack", "All infantry and tanks surge forward at blistering speed and ferocity!", 90, 36.0f, 16.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"knee_mortar", "Type 89 Knee Mortar Barrage", "Rapid Infantry Mortar Rain", "Blankets the target zone with high-explosive shells and shrapnel.", 115, 40.0f, 0.0f, 25.0f, SpecOpTargetType::GROUND_AREA},
            {"imperial_flank", "Imperial Guards Flanking Force", "Spawn Rifle + Ha-Go Light Tank", "Deploys a veteran Rifle Regiment and Type 95 Ha-Go Tank Regiment at target.", 145, 52.0f, 0.0f, 16.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef nagumo{};
        nagumo.id = "nagumo";
        nagumo.name = "Vice Adm. Chuichi Nagumo";
        nagumo.nickname = "Commander, Kido Butai Carrier Fleet";
        nagumo.specialty = "Six-Carrier Air Raids & Torpedo Bombers";
        nagumo.bio = "Led the First Air Fleet from Pearl Harbor to the Indian Ocean raids. Overwhelms enemies from the skies.";
        nagumo.passiveDesc = "+20% Oil Income, +15% Tank Speed";
        nagumo.nation = NationId::JPN;
        nagumo.portraitColor = {140, 82, 38, 255};
        nagumo.oilRateMult = 1.20f;
        nagumo.armorSpeedMult = 1.15f;
        nagumo.specialOps = {{
            {"zero_val_raid", "A6M Zero & Val Dive Bomb Run", "Carrier Fighter-Bomber Strike", "Kido Butai carrier aircraft strafe and dive-bomb enemy vehicles and depots.", 130, 42.0f, 0.0f, 25.0f, SpecOpTargetType::GROUND_AREA},
            {"kate_carpet", "B5N Kate Heavy Bombing Raid", "Formation Carpet Bombing", "Carrier bombers drop heavy armor-piercing bombs across the target zone.", 180, 62.0f, 0.0f, 30.0f, SpecOpTargetType::GROUND_AREA},
            {"ohka_strike", "Ohka Rocket Kamikaze Strike", "Rocket-Bomber Impact", "Rocket-propelled Ohka slams into the target area with catastrophic force.", 165, 54.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef kuribayashi{};
        kuribayashi.id = "kuribayashi";
        kuribayashi.name = "Lt. Gen. T. Kuribayashi";
        kuribayashi.nickname = "Commander of Fortress Iwo Jima";
        kuribayashi.specialty = "Interlocking Pillboxes & Tunnels";
        kuribayashi.bio = "Transformed Iwo Jima into an underground honeycomb of mutually supporting bunkers and concealed mortar batteries.";
        kuribayashi.passiveDesc = "+45% Bunker Health, +28% Unit Defense";
        kuribayashi.nation = NationId::JPN;
        kuribayashi.portraitColor = {120, 72, 32, 255};
        kuribayashi.bunkerHealthMult = 1.45f;
        kuribayashi.defenseMult = 1.28f;
        kuribayashi.specialOps = {{
            {"tunnel_network", "Underground Tunnel Ambush", "Build Bunker + AT Squad at Target", "Constructs an instant Pillbox Bunker and deploys an Anti-Tank Regiment.", 130, 48.0f, 0.0f, 16.0f, SpecOpTargetType::GROUND_AREA},
            {"spigot_mortar", "Type 98 320mm Spigot Mortar", "Massive Crater Bombardment", "Fires 675-lb spigot mortar shells that obliterate enemy formations.", 145, 50.0f, 0.0f, 28.0f, SpecOpTargetType::GROUND_AREA},
            {"gyokusai_stand", "Gyokusai Fortress Stand", "+40% Defense & +25% Attack", "All friendly units and structures take far less damage and fire faster.", 95, 40.0f, 20.0f, 0.0f, SpecOpTargetType::GLOBAL}
        }};

        n.generals = {yamamoto, yamashita, nagumo, kuribayashi};
        nations_[static_cast<size_t>(NationId::JPN)] = n;
    }

    // ------------------------------------------------------------------------
    // 4. UNITED STATES (USA)
    // ------------------------------------------------------------------------
    {
        NationDef n{};
        n.id = NationId::USA;
        n.code = "USA";
        n.name = "United States";
        n.shortName = "U.S.A.";
        n.motto = "Arsenal of Democracy";
        n.description = "Unmatched industrial output, superior combined-arms logistics, 101st Airborne drops, and B-17 strategic air power.";
        n.signatureUnitName = "T26E4 Super Pershing Heavy Tank";
        n.alliance = Alliance::ALLIES;
        n.primaryColor = {58, 122, 62, 255};
        n.accentColor = {152, 218, 156, 255};
        n.mapColor = {62, 130, 68, 255};
        n.startingIPC = 40;
        n.capitalTerritoryId = 0; // Eastern USA
        n.lightTankName = "M5A1 Stuart Light Regiment";
        n.mediumTankName = "M4A3E8 Sherman Easy Eight Regiment";
        n.tankDestroyerName = "M10 Wolverine TD Regiment";
        n.heavyTankName = "M26 Pershing Heavy Regiment";
        n.eliteTankName = "T26E4 Super Pershing Regiment";
        n.spgName = "M7 Priest 105mm Artillery Battery";

        GeneralDef patton{};
        patton.id = "patton";
        patton.name = "Gen. George S. Patton";
        patton.nickname = "Old Blood and Guts";
        patton.specialty = "Armored Spearhead & Rapid Pursuit";
        patton.bio = "Commander of the U.S. Third Army. Master of aggressive armored thrusts across France, Bastogne, and the Rhine.";
        patton.passiveDesc = "+18% Tank Speed, +15% Armor Firepower";
        patton.nation = NationId::USA;
        patton.portraitColor = {76, 124, 64, 255};
        patton.armorSpeedMult = 1.18f;
        patton.armorAttackMult = 1.15f;
        patton.specialOps = {{
            {"third_army_blitz", "Third Army Armored Blitz", "+35% Speed & +25% Attack", "All friendly tanks and units surge forward with +35% speed and +25% firepower.", 100, 42.0f, 18.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"tot_barrage", "Time-on-Target 155mm Barrage", "Synchronized Long Tom Strike", "Calls in synchronized 155mm artillery shells onto target coordinates.", 135, 48.0f, 0.0f, 26.0f, SpecOpTargetType::GROUND_AREA},
            {"blood_and_guts", "Blood & Guts Rally", "Restore 100% Organization", "Instantly restores full Organization to all friendly regiments and boosts defense.", 80, 34.0f, 15.0f, 0.0f, SpecOpTargetType::GLOBAL}
        }};

        GeneralDef eisenhower{};
        eisenhower.id = "eisenhower";
        eisenhower.name = "Gen. Dwight D. Eisenhower";
        eisenhower.nickname = "Supreme Allied Commander";
        eisenhower.specialty = "Supreme Logistics, 101st Airborne & B-17s";
        eisenhower.bio = "Architect of Operation Overlord. Excels at supply network expansion, economic mobilization, and combined air-ground power.";
        eisenhower.passiveDesc = "+25% Supply Radius, +18% Money Income";
        eisenhower.nation = NationId::USA;
        eisenhower.portraitColor = {60, 108, 82, 255};
        eisenhower.supplyRadiusMult = 1.25f;
        eisenhower.moneyRateMult = 1.18f;
        eisenhower.specialOps = {{
            {"c47_supply", "C-47 Skytrain Supply Drop", "+220 Money/Ammo/Oil & Heal", "Drops emergency supplies (+220 all resources) and heals friendly units in zone.", 90, 38.0f, 0.0f, 22.0f, SpecOpTargetType::GROUND_AREA},
            {"airborne_101", "101st Airborne Screaming Eagles", "Paradrop 2 Paratroop Regiments", "Drops two veteran 101st Airborne Paratrooper regiments onto the target zone.", 155, 58.0f, 0.0f, 18.0f, SpecOpTargetType::GROUND_AREA},
            {"b17_carpet", "B-17 Flying Fortress Carpet Raid", "Heavy Strategic Bombing Run", "Eighth Air Force B-17s carpet-bomb a wide corridor, leveling enemy HQs.", 205, 74.0f, 0.0f, 34.0f, SpecOpTargetType::GROUND_AREA}
        }};

        GeneralDef nimitz{};
        nimitz.id = "nimitz";
        nimitz.name = "Adm. Chester W. Nimitz";
        nimitz.nickname = "CINCPAC — U.S. Pacific Fleet";
        nimitz.specialty = "Battleship Iowa Salvos & Marine Landings";
        nimitz.bio = "Commander of the U.S. Pacific Fleet. Commands 16-inch battleship salvos, USMC assaults, and Corsair strikes.";
        nimitz.passiveDesc = "+20% Oil Income, +15% Infantry Attack";
        nimitz.nation = NationId::USA;
        nimitz.portraitColor = {44, 90, 122, 255};
        nimitz.oilRateMult = 1.20f;
        nimitz.infAttackMult = 1.15f;
        nimitz.specialOps = {{
            {"iowa_salvo", "USS Iowa 16-Inch Naval Salvo", "Heavy Battleship Bombardment", "USS Iowa fires nine 16-inch armor-piercing shells onto target coordinates.", 175, 62.0f, 0.0f, 30.0f, SpecOpTargetType::GROUND_AREA},
            {"corsair_napalm", "F4U Corsair Rocket & Napalm Run", "Close Air Support Strike", "Marine Corsairs strafe and rocket the target zone, burning enemy organization.", 130, 45.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA},
            {"usmc_wave", "USMC Semper Fi Assault Wave", "Deploy Rifle + Engineer at HQ", "Instantly deploys a Marine Rifle Regiment and Combat Engineer Regiment at HQ.", 135, 50.0f, 0.0f, 0.0f, SpecOpTargetType::HQ_SPAWN}
        }};

        GeneralDef arnold{};
        arnold.id = "arnold";
        arnold.name = "Gen. Henry 'Hap' Arnold";
        arnold.nickname = "Commander, U.S. Army Air Forces";
        arnold.specialty = "Total Air Supremacy & Tank Busters";
        arnold.bio = "Five-star General of the Air Force. Turns the skies into an Allied highway with P-38 recon, P-47 Jugs, and B-29 raids.";
        arnold.passiveDesc = "+20% Ammo Income, +15% Artillery & Air Attack";
        arnold.nation = NationId::USA;
        arnold.portraitColor = {72, 102, 132, 255};
        arnold.ammoRateMult = 1.20f;
        arnold.artilleryAttackMult = 1.15f;
        arnold.specialOps = {{
            {"p38_recon", "P-38 Lightning Photo Recon", "Reveal Map & +20% Firepower", "Reveals all enemy positions and boosts friendly accuracy for 24 seconds.", 65, 30.0f, 24.0f, 0.0f, SpecOpTargetType::GLOBAL},
            {"p47_jugs", "P-47 Thunderbolt Tank Busters", "HVAR Rocket Strike vs Armor", "P-47 Thunderbolts launch armor-piercing rockets at enemy tanks in zone.", 145, 48.0f, 0.0f, 24.0f, SpecOpTargetType::GROUND_AREA},
            {"b29_firestorm", "B-29 Superfortress Firestorm", "Devastating Strategic Bombing", "High-altitude B-29 raid levels enemy bases, depots, and massed divisions.", 215, 78.0f, 0.0f, 36.0f, SpecOpTargetType::GROUND_AREA}
        }};

        n.generals = {patton, eisenhower, nimitz, arnold};
        nations_[static_cast<size_t>(NationId::USA)] = n;
    }
}

void GameData::initBuildings() {
    buildings_[static_cast<size_t>(BuildingType::CORPS_HQ)] = {
        BuildingType::CORPS_HQ, "corps_hq", "Corps HQ",
        "Primary Mobile Command Center. Anchors supply lines, advances Tech Tiers 1-3, and generates baseline Money/Ammo/Oil.",
        400, 100, 100, 1600.0f, 220.0f, true, 1, 28.0f, 12.0f, 10.0f, 32.0f, 18.0f, 1.2f, 14.0f
    };
    buildings_[static_cast<size_t>(BuildingType::SUPPLY_DEPOT)] = {
        BuildingType::SUPPLY_DEPOT, "supply_depot", "Supply Depot",
        "Mobile Logistics Hub. Extends your Supply Line radius and generates Money, Ammo, and Oil for frontline regiments.",
        130, 0, 0, 650.0f, 160.0f, true, 1, 12.0f, 20.0f, 16.0f, 0.0f, 0.0f, 1.5f, 6.0f
    };
    buildings_[static_cast<size_t>(BuildingType::INFANTRY_HQ)] = {
        BuildingType::INFANTRY_HQ, "infantry_hq", "Infantry Division HQ",
        "Mobile Division Headquarters. Deploys Rifle, MG, Mortar, Anti-Tank, Engineer, Sniper, and Paratrooper regiments.",
        180, 40, 0, 950.0f, 115.0f, true, 1, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 1.5f, 8.0f
    };
    buildings_[static_cast<size_t>(BuildingType::MECH_HQ)] = {
        BuildingType::MECH_HQ, "mech_hq", "Mechanized Division HQ",
        "Mobile Division Headquarters. Deploys Recon Half-Tracks, AA Flak Half-Tracks, Self-Propelled Artillery, and Katyusha Batteries.",
        240, 60, 50, 1050.0f, 115.0f, true, 1, 0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 1.5f, 10.0f
    };
    buildings_[static_cast<size_t>(BuildingType::ARMOR_HQ)] = {
        BuildingType::ARMOR_HQ, "armor_hq", "Armored Division HQ",
        "Mobile Heavy Factory. Produces Light Tanks, Medium Tanks, Tank Destroyers, Heavy Tanks, and Nation Signature Elite Armor.",
        320, 80, 100, 1250.0f, 125.0f, true, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 12.0f
    };
    buildings_[static_cast<size_t>(BuildingType::AIRFIELD)] = {
        BuildingType::AIRFIELD, "airfield", "Forward Airfield",
        "Tactical Airbase. Dispatches high-speed Air Reconnaissance flights (clearing Fog of War) and Tactical Bomber raids.",
        220, 40, 50, 1050.0f, 125.0f, false, 1, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 12.0f
    };
    buildings_[static_cast<size_t>(BuildingType::BUNKER)] = {
        BuildingType::BUNKER, "bunker", "Fortified Pillbox Bunker",
        "Reinforced concrete fortification armed with interlocking heavy machine guns and an anti-tank cannon.",
        140, 60, 0, 850.0f, 55.0f, false, 1, 0.0f, 0.0f, 0.0f, 38.0f, 26.0f, 0.9f, 5.0f
    };
    buildings_[static_cast<size_t>(BuildingType::AA_BATTERY)] = {
        BuildingType::AA_BATTERY, "aa_battery", "88mm / 40mm Flak & AT Battery",
        "Dual-purpose anti-aircraft and long-range armor-piercing battery. Lethal against enemy tanks and air strikes.",
        170, 80, 20, 750.0f, 55.0f, false, 1, 0.0f, 0.0f, 0.0f, 46.0f, 38.0f, 1.3f, 6.0f
    };
    buildings_[static_cast<size_t>(BuildingType::CITY_DEPOT)] = {
        BuildingType::CITY_DEPOT, "city_depot", "Strategic Town & Healing Station",
        "Capturable urban center & Field Hospital. Wounded soldiers can ONLY heal and reinforce at controlled towns! Also grants +82m Vision & Supply.",
        0, 0, 0, 1500.0f, 180.0f, false, 1, 22.0f, 14.0f, 6.0f, 0.0f, 0.0f, 1.5f, 0.0f
    };
    buildings_[static_cast<size_t>(BuildingType::OIL_DERRICK)] = {
        BuildingType::OIL_DERRICK, "oil_derrick", "Oil Refinery & Fuel Depot",
        "Capturable industrial refinery. Projects local supply and fuels your Mechanized and Armored divisions.",
        0, 0, 0, 1200.0f, 110.0f, false, 1, 10.0f, 0.0f, 26.0f, 0.0f, 0.0f, 1.5f, 0.0f
    };
}

void GameData::initRegiments() {
    regiments_[static_cast<size_t>(RegimentType::RIFLE_REGIMENT)] = {
        RegimentType::RIFLE_REGIMENT, "rifle", "Rifle Infantry Regiment",
        "6-soldier frontline infantry squad. Captures Strategic Cities and Oil Refineries.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 1,
        75, 25, 0, 0.4f, 0.0f,
        6, 360.0f, 100.0f, 11.5f, 26.0f, 45.0f, 1.0f,
        18.0f, 7.0f, 18.0f, 10.0f, 0.0f, false, true, false,
        3, 1, 2
    };
    regiments_[static_cast<size_t>(RegimentType::MG_REGIMENT)] = {
        RegimentType::MG_REGIMENT, "mg", "Heavy Machine Gun Regiment",
        "5-soldier suppression squad. Shreds enemy infantry and rapidly drains enemy Organization.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 1,
        95, 45, 0, 0.7f, 0.0f,
        5, 340.0f, 105.0f, 10.5f, 30.0f, 46.0f, 0.55f,
        24.0f, 6.0f, 12.0f, 20.0f, 0.0f, false, true, false,
        3, 1, 2
    };
    regiments_[static_cast<size_t>(RegimentType::MORTAR_REGIMENT)] = {
        RegimentType::MORTAR_REGIMENT, "mortar", "Heavy Mortar Regiment",
        "4-soldier indirect-fire support squad. Lobbing high-explosive shells over cover.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 1,
        110, 60, 0, 0.9f, 0.0f,
        4, 300.0f, 100.0f, 10.0f, 44.0f, 45.0f, 1.8f,
        28.0f, 14.0f, 42.0f, 22.0f, 5.5f, true, false, false,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::AT_BAZOOKA_REGIMENT)] = {
        RegimentType::AT_BAZOOKA_REGIMENT, "bazooka", "Bazooka / Panzerschreck Regiment",
        "5-soldier anti-armor squad armed with shaped-charge rocket launchers.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 1,
        115, 55, 0, 0.7f, 0.0f,
        5, 330.0f, 100.0f, 11.0f, 27.0f, 44.0f, 1.5f,
        10.0f, 38.0f, 55.0f, 14.0f, 2.5f, false, true, false,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::ENGINEER_FLAME_REGIMENT)] = {
        RegimentType::ENGINEER_FLAME_REGIMENT, "engineer", "Combat Engineer & Flame Regiment",
        "5-soldier assault pioneers with flamethrowers and repair kits. Melts bunkers and repairs HQs/tanks.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 1,
        125, 50, 25, 0.6f, 0.3f,
        5, 380.0f, 115.0f, 11.5f, 20.0f, 44.0f, 0.8f,
        30.0f, 20.0f, 70.0f, 26.0f, 4.0f, false, true, true,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::SNIPER_REGIMENT)] = {
        RegimentType::SNIPER_REGIMENT, "sniper", "Elite Sniper & Recon Regiment",
        "3-soldier marksman team with long sight range and lethal precision against enemy infantry and officers.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 2,
        140, 60, 0, 0.5f, 0.0f,
        3, 260.0f, 120.0f, 12.8f, 42.0f, 62.0f, 1.4f,
        46.0f, 6.0f, 8.0f, 34.0f, 0.0f, false, true, false,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::PARATROOP_REGIMENT)] = {
        RegimentType::PARATROOP_REGIMENT, "paratroop", "Airborne Paratrooper Regiment",
        "6-soldier elite airborne infantry. High Organization and reduced penalty when fighting behind enemy lines.",
        UnitCategory::INFANTRY, BuildingType::INFANTRY_HQ, 2,
        150, 65, 0, 0.5f, 0.0f,
        6, 440.0f, 130.0f, 12.5f, 28.0f, 50.0f, 0.85f,
        25.0f, 18.0f, 25.0f, 18.0f, 0.0f, false, true, false,
        4, 2, 3
    };

    // Mechanized Division HQ Regiments
    regiments_[static_cast<size_t>(RegimentType::RECON_HALFTRACK)] = {
        RegimentType::RECON_HALFTRACK, "recon_ht", "Armored Recon Half-Track Regiment",
        "3 fast armored half-tracks. Captures territory, shreds infantry, and spots targets for artillery.",
        UnitCategory::MECHANIZED, BuildingType::MECH_HQ, 1,
        140, 40, 45, 0.6f, 0.7f,
        3, 480.0f, 105.0f, 18.5f, 30.0f, 58.0f, 0.7f,
        24.0f, 14.0f, 20.0f, 15.0f, 0.0f, false, true, false,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::AA_HALFTRACK)] = {
        RegimentType::AA_HALFTRACK, "aa_ht", "Quad-Flak AA Half-Track Regiment",
        "3 mobile anti-aircraft half-tracks armed with rapid-fire autocannons.",
        UnitCategory::MECHANIZED, BuildingType::MECH_HQ, 1,
        160, 65, 55, 0.8f, 0.8f,
        3, 460.0f, 105.0f, 17.0f, 36.0f, 55.0f, 0.5f,
        28.0f, 18.0f, 22.0f, 22.0f, 2.0f, false, false, false,
        5, 2, 3
    };
    regiments_[static_cast<size_t>(RegimentType::MOBILE_ARTILLERY)] = {
        RegimentType::MOBILE_ARTILLERY, "spg_artillery", "Self-Propelled Artillery Battery",
        "2 long-range 105mm/150mm armored howitzers. Crushes enemy fortifications and massed formations.",
        UnitCategory::ARTILLERY, BuildingType::MECH_HQ, 2,
        220, 110, 80, 1.4f, 1.0f,
        2, 440.0f, 100.0f, 12.5f, 64.0f, 50.0f, 2.6f,
        42.0f, 36.0f, 90.0f, 35.0f, 8.0f, true, false, false,
        4, 2, 2
    };
    regiments_[static_cast<size_t>(RegimentType::KATYUSHA_ROCKET)] = {
        RegimentType::KATYUSHA_ROCKET, "katyusha", "BM-13 Katyusha Rocket Battery",
        "2 multiple-rocket launcher trucks. Fires screaming salvos of high-explosive rockets over huge range.",
        UnitCategory::ARTILLERY, BuildingType::MECH_HQ, 2,
        250, 140, 90, 1.8f, 1.1f,
        2, 380.0f, 100.0f, 14.0f, 70.0f, 50.0f, 3.2f,
        56.0f, 38.0f, 100.0f, 48.0f, 10.5f, true, false, false,
        5, 3, 2
    };

    // Armored Division HQ Regiments
    regiments_[static_cast<size_t>(RegimentType::LIGHT_TANK)] = {
        RegimentType::LIGHT_TANK, "light_tank", "Light Tank Regiment",
        "3 fast light tanks designed for flanking enemy supply lines and hunting infantry/artillery.",
        UnitCategory::ARMOR, BuildingType::ARMOR_HQ, 1,
        180, 50, 75, 0.7f, 1.0f,
        3, 620.0f, 110.0f, 17.5f, 32.0f, 50.0f, 1.2f,
        26.0f, 28.0f, 35.0f, 18.0f, 2.5f, false, false, false,
        5, 3, 2
    };
    regiments_[static_cast<size_t>(RegimentType::MEDIUM_TANK)] = {
        RegimentType::MEDIUM_TANK, "medium_tank", "Medium Battle Tank Regiment",
        "3 main battle tanks (Sherman / T-34 / Panzer IV / Cromwell / Chi-Ha). Backbone of armored spearheads.",
        UnitCategory::ARMOR, BuildingType::ARMOR_HQ, 2,
        250, 80, 110, 1.0f, 1.4f,
        3, 880.0f, 115.0f, 14.5f, 36.0f, 50.0f, 1.45f,
        32.0f, 44.0f, 60.0f, 24.0f, 3.5f, false, false, false,
        5, 3, 3
    };
    regiments_[static_cast<size_t>(RegimentType::TANK_DESTROYER)] = {
        RegimentType::TANK_DESTROYER, "tank_destroyer", "Tank Destroyer Regiment",
        "3 high-velocity armor hunters (Wolverine / SU-100 / Jagdpanther / Firefly). Lethal vs heavy armor.",
        UnitCategory::ARMOR, BuildingType::ARMOR_HQ, 2,
        265, 95, 115, 1.1f, 1.4f,
        3, 780.0f, 110.0f, 14.0f, 42.0f, 54.0f, 1.55f,
        18.0f, 64.0f, 65.0f, 26.0f, 2.0f, false, false, false,
        6, 3, 3
    };
    regiments_[static_cast<size_t>(RegimentType::HEAVY_TANK)] = {
        RegimentType::HEAVY_TANK, "heavy_tank", "Heavy Tank Regiment",
        "2 heavily armored breakthrough tanks (Pershing / IS-2 / Tiger I / Churchill / Chi-To).",
        UnitCategory::ARMOR, BuildingType::ARMOR_HQ, 3,
        340, 120, 150, 1.4f, 1.9f,
        2, 1180.0f, 125.0f, 11.5f, 38.0f, 50.0f, 1.65f,
        40.0f, 62.0f, 90.0f, 32.0f, 4.5f, false, false, false,
        7, 4, 4
    };
    regiments_[static_cast<size_t>(RegimentType::ELITE_SIGNATURE_TANK)] = {
        RegimentType::ELITE_SIGNATURE_TANK, "elite_tank", "Signature Elite Heavy Armor",
        "2 nation-specific juggernauts (King Tiger / Super Pershing / Churchill Crocodile / IS-3 / Ho-Ri).",
        UnitCategory::ARMOR, BuildingType::ARMOR_HQ, 3,
        430, 150, 190, 1.8f, 2.4f,
        2, 1550.0f, 140.0f, 10.8f, 42.0f, 54.0f, 1.6f,
        54.0f, 82.0f, 120.0f, 42.0f, 5.5f, false, false, false,
        8, 4, 4
    };
    regiments_[static_cast<size_t>(RegimentType::PACKED_HQ_TRUCK)] = {
        RegimentType::PACKED_HQ_TRUCK, "packed_hq_truck", "Mobile HQ Supply Convoy",
        "Packed Headquarters or Supply Depot in transit. Drive to a new strategic sector and click [UNPACK HQ].",
        UnitCategory::SUPPLY_TRUCK, BuildingType::CORPS_HQ, 1,
        150, 0, 20, 0.0f, 0.3f,
        1, 800.0f, 100.0f, 14.5f, 0.0f, 42.0f, 2.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false, false,
        0, 0, 0
    };
}

void GameData::initMapsAndCampaigns() {
    skirmishMaps_ = {
        {0, "Normandy Bocage Breakthrough", "Western Europe (1944)", "Hedgerows, stone bridges, and strategic French crossroads.", BiomeType::NORMANDY_BOCAGE, 340.0f, true, 5, 3},
        {1, "El Alamein Ridge", "North Africa (1942)", "Sun-baked desert dunes, oil dumps, and open tank killing grounds.", BiomeType::EL_ALAMEIN_DESERT, 340.0f, false, 4, 4},
        {2, "Stalingrad Volga Ruins", "Eastern Front (1942)", "Frozen river crossings and shattered industrial tractor factories.", BiomeType::STALINGRAD_WINTER, 340.0f, true, 6, 2},
        {3, "Guadalcanal & Pacific Atoll", "South Pacific (1942)", "Tropical island causeways, Henderson airfield, and coastal palm groves.", BiomeType::PACIFIC_ATOLL, 340.0f, true, 4, 3},
        {4, "Kursk Armor Salient", "Eastern Steppe (1943)", "Rolling steppe plains built for the largest tank battle in history.", BiomeType::KURSK_STEPPE, 360.0f, false, 5, 4},
        {5, "Ardennes Bastogne Crossroads", "Belgium Winter (1944)", "Snow-covered pine forests and vital road junctions.", BiomeType::ARDENNES_WINTER, 340.0f, true, 5, 3},
        {6, "Remagen Rhine Crossing", "Western Germany (1945)", "Fortified riverbridges leading into the industrial Ruhr valley.", BiomeType::RHINE_CROSSING, 340.0f, true, 5, 3},
        {7, "Fall of Berlin", "Heart of the Reich (1945)", "Dense urban fortifications, flak towers, and central government avenues.", BiomeType::BERLIN_RUINS, 340.0f, true, 6, 3}
    };

    campaignMissions_ = {
        // Allied Historical Campaign (6 Missions)
        {0, false, 1, "Operation Lightfoot: El Alamein", "October 1942", "Egypt, North Africa",
         "Montgomery's Eighth Army must shatter Rommel's Afrika Korps supply lines and seize the desert ridges.",
         "Capture the central oasis depots and destroy Rommel's Corps HQ.",
         NationId::UK, 0, NationId::GER, 0, BiomeType::EL_ALAMEIN_DESERT, 700, 550, 500, 1},
        {1, false, 2, "Not One Step Back: Stalingrad", "November 1942", "Volga River, USSR",
         "Chuikov and Zhukov launch Operation Uranus to encircle German forces in the frozen ruins of Stalingrad.",
         "Hold the Volga supply depots and crush the Axis HQ with Katyusha & T-34 waves.",
         NationId::USSR, 0, NationId::GER, 1, BiomeType::STALINGRAD_WINTER, 750, 650, 500, 1},
        {2, false, 3, "Operation Overlord: D-Day Breakout", "June 1944", "Normandy, France",
         "Eisenhower and Patton push inland through the Normandy bocage against fortified German Panzer divisions.",
         "Establish forward Supply Depots, call in 101st Airborne drops, and take the French rail hubs.",
         NationId::USA, 1, NationId::GER, 3, BiomeType::NORMANDY_BOCAGE, 850, 650, 600, 1},
        {3, false, 4, "Operation Bagration", "July 1944", "Byelorussia, Eastern Front",
         "Rokossovsky unleashes a massive double-envelopment blitz that shatters Army Group Centre.",
         "Seize the river crossings and overwhelm the German armored reserves.",
         NationId::USSR, 2, NationId::GER, 2, BiomeType::KURSK_STEPPE, 900, 700, 650, 1},
        {4, false, 5, "Relief of Bastogne: Battle of the Bulge", "December 1944", "Ardennes Forest, Belgium",
         "Patton's Third Army pivots north through winter blizzards to relieve the besieged 101st Airborne.",
         "Break the German encirclement and destroy the enemy Armor & Corps HQs.",
         NationId::USA, 0, NationId::GER, 0, BiomeType::ARDENNES_WINTER, 950, 750, 700, 2},
        {5, false, 6, "Fortress Iwo Jima & Pacific Triumph", "February 1945", "Volcano Islands, Pacific",
         "Admiral Nimitz backs USMC assault regiments with 16-inch USS Iowa salvos against Kuribayashi's bunkers.",
         "Clear the interlocking pillboxes and capture the island command complex.",
         NationId::USA, 2, NationId::JPN, 3, BiomeType::PACIFIC_ATOLL, 1000, 850, 750, 2},

        // Axis Conquest Campaign (6 Missions)
        {6, true, 1, "Fall of Tobruk: Afrika Korps Blitz", "June 1942", "Libya, North Africa",
         "Rommel leads the Ghost Division around the British flank to sever Eighth Army supply lines.",
         "Use Blitzkrieg mobility to cut off British depots and capture Tobruk.",
         NationId::GER, 0, NationId::UK, 2, BiomeType::EL_ALAMEIN_DESERT, 750, 550, 550, 1},
        {7, true, 2, "Tiger of Malaya: Fall of Singapore", "February 1942", "Malaya & Singapore",
         "General Yamashita's bicycle infantry and Ha-Go tanks outflank Commonwealth defenders through the jungle.",
         "Overrun the British defensive line and seize the fortress HQ.",
         NationId::JPN, 1, NationId::UK, 1, BiomeType::PACIFIC_ATOLL, 750, 600, 500, 1},
        {8, true, 3, "Case Blue: Drive on the Caucasus Oilfields", "August 1942", "Southern Russia",
         "Guderian and Manstein spearhead a Panzer offensive to capture the vital Soviet oil refineries.",
         "Capture all Oil Derricks and crush the defending Red Army HQ.",
         NationId::GER, 2, NationId::USSR, 1, BiomeType::KURSK_STEPPE, 850, 650, 650, 1},
        {9, true, 4, "Showdown at Kursk: Operation Citadel", "July 1943", "Kursk Salient, USSR",
         "Manstein deploys heavy Tiger and Jagdpanther regiments to punch through deep Soviet anti-tank belts.",
         "Shatter the Red Army salient and destroy Zhukov's Corps HQ.",
         NationId::GER, 1, NationId::USSR, 0, BiomeType::KURSK_STEPPE, 950, 750, 750, 2},
        {10, true, 5, "Combined Fleet Decisive Battle", "October 1944", "Philippine Sea & Leyte",
         "Admiral Yamamoto unleashes the Yamato's 18.1-inch guns and Kido Butai air wings against Allied landings.",
         "Destroy the American beachhead HQ and hold the island airfields.",
         NationId::JPN, 0, NationId::USA, 2, BiomeType::PACIFIC_ATOLL, 950, 750, 750, 2},
        {11, true, 6, "Operation Sea Lion: Invasion of Britain", "Alternate 1945", "Southern England",
         "With the Continent secured, Kesselring's Fallschirmjager and King Tigers cross the Channel.",
         "Defeat Montgomery's home defense forces and capture the Allied Supreme HQ.",
         NationId::GER, 3, NationId::UK, 0, BiomeType::NORMANDY_BOCAGE, 1100, 850, 850, 2}
    };
}

void GameData::initWorldTerritories() {
    // 26 Interconnected Global Territories (1942 Revised Setup, well-spaced on map)
    territories_ = {
        {0, "eastern_usa", "Eastern United States", "Americas", NationId::USA, 12, true, true, true, BiomeType::NORMANDY_BOCAGE,
         {0.19f, 0.42f}, {1, 2, 3}, {4, 2, 2, 3, 1, 2, 1, 1}},
        {1, "western_usa", "Western United States", "Americas", NationId::USA, 10, false, true, true, BiomeType::NORMANDY_BOCAGE,
         {0.08f, 0.44f}, {0, 2, 24}, {3, 1, 1, 2, 0, 2, 1, 1}},
        {2, "eastern_canada", "Canada & North Atlantic", "Americas", NationId::UK, 4, false, true, true, BiomeType::ARDENNES_WINTER,
         {0.19f, 0.25f}, {0, 1, 3}, {2, 1, 1, 1, 0, 1, 0, 0}},
        {3, "united_kingdom", "United Kingdom", "Western Europe", NationId::UK, 8, true, true, true, BiomeType::NORMANDY_BOCAGE,
         {0.33f, 0.26f}, {0, 2, 4, 6, 7}, {4, 2, 2, 2, 1, 3, 1, 1}},
        {4, "western_europe", "France & Low Countries", "Western Europe", NationId::GER, 6, false, true, true, BiomeType::NORMANDY_BOCAGE,
         {0.34f, 0.41f}, {3, 5, 6, 11}, {3, 2, 1, 2, 0, 1, 0, 1}},
        {5, "germany", "Greater Germany (Berlin)", "Central Europe", NationId::GER, 10, true, true, true, BiomeType::BERLIN_RUINS,
         {0.43f, 0.32f}, {4, 6, 7, 9, 10}, {5, 2, 2, 3, 1, 2, 1, 1}},
        {6, "southern_europe", "Italy & Balkans", "Southern Europe", NationId::GER, 6, false, true, true, BiomeType::RHINE_CROSSING,
         {0.43f, 0.47f}, {3, 4, 5, 10, 11, 12}, {3, 1, 1, 2, 0, 1, 0, 1}},
        {7, "norway_finland", "Norway & Finland", "Northern Europe", NationId::GER, 3, false, false, true, BiomeType::ARDENNES_WINTER,
         {0.42f, 0.14f}, {3, 5, 8, 9}, {3, 1, 0, 1, 0, 1, 0, 0}},
        {8, "russia", "Moscow & Central Russia", "USSR", NationId::USSR, 8, true, true, false, BiomeType::STALINGRAD_WINTER,
         {0.61f, 0.25f}, {7, 9, 10, 13, 14, 15}, {6, 2, 2, 3, 1, 2, 0, 1}},
        {9, "karelia", "Leningrad & Karelia", "USSR", NationId::USSR, 3, false, true, true, BiomeType::STALINGRAD_WINTER,
         {0.52f, 0.18f}, {5, 7, 8, 10}, {4, 1, 1, 1, 0, 1, 0, 1}},
        {10, "ukraine", "Ukraine & Kursk", "Eastern Europe", NationId::GER, 4, false, false, true, BiomeType::KURSK_STEPPE,
         {0.52f, 0.36f}, {5, 6, 8, 9, 13}, {3, 2, 1, 3, 1, 1, 0, 0}},
        {11, "algeria_tunisia", "Algeria & Tunisia", "North Africa", NationId::GER, 2, false, false, true, BiomeType::EL_ALAMEIN_DESERT,
         {0.37f, 0.62f}, {4, 6, 12}, {3, 1, 1, 2, 0, 1, 0, 0}},
        {12, "egypt", "Egypt & El Alamein (Suez)", "North Africa", NationId::UK, 3, false, false, true, BiomeType::EL_ALAMEIN_DESERT,
         {0.49f, 0.64f}, {6, 11, 13, 16}, {3, 2, 1, 2, 0, 1, 0, 0}},
        {13, "caucasus", "Stalingrad & Caucasus", "USSR", NationId::USSR, 5, false, true, true, BiomeType::STALINGRAD_WINTER,
         {0.61f, 0.43f}, {8, 10, 12, 16}, {4, 2, 1, 2, 0, 1, 0, 1}},
        {14, "ural_siberia", "Urals & Novosibirsk", "USSR", NationId::USSR, 3, false, false, false, BiomeType::KURSK_STEPPE,
         {0.71f, 0.20f}, {8, 15, 17}, {3, 1, 1, 1, 0, 0, 0, 0}},
        {15, "soviet_far_east", "Vladivostok & Far East", "USSR", NationId::USSR, 2, false, false, true, BiomeType::ARDENNES_WINTER,
         {0.83f, 0.18f}, {8, 14, 18, 19}, {4, 1, 0, 1, 0, 0, 0, 0}},
        {16, "india", "India & Burma Command", "South Asia", NationId::UK, 5, false, true, true, BiomeType::PACIFIC_ATOLL,
         {0.64f, 0.60f}, {12, 13, 17, 20, 22}, {4, 2, 1, 1, 0, 1, 0, 1}},
        {17, "china", "Szechwan & Central China", "East Asia", NationId::USA, 3, false, false, false, BiomeType::KURSK_STEPPE,
         {0.71f, 0.42f}, {14, 16, 18, 20}, {3, 1, 0, 0, 0, 1, 0, 0}},
        {18, "manchuria", "Manchuria & Coastal China", "East Asia", NationId::JPN, 4, false, false, true, BiomeType::KURSK_STEPPE,
         {0.80f, 0.34f}, {15, 17, 19, 20}, {4, 2, 1, 2, 0, 1, 0, 0}},
        {19, "japan", "Imperial Japan (Tokyo)", "Pacific", NationId::JPN, 8, true, true, true, BiomeType::PACIFIC_ATOLL,
         {0.90f, 0.35f}, {15, 18, 20, 21, 24}, {5, 2, 2, 2, 1, 3, 1, 1}},
        {20, "indochina", "French Indochina & Malaya", "Southeast Asia", NationId::JPN, 3, false, false, true, BiomeType::PACIFIC_ATOLL,
         {0.75f, 0.60f}, {16, 17, 18, 19, 21, 22}, {3, 1, 1, 1, 0, 1, 0, 0}},
        {21, "philippines", "Philippines & Iwo Jima", "Pacific", NationId::JPN, 3, false, false, true, BiomeType::PACIFIC_ATOLL,
         {0.85f, 0.56f}, {19, 20, 22, 23, 24}, {3, 1, 0, 1, 0, 1, 0, 0}},
        {22, "east_indies", "Borneo & Dutch East Indies", "Pacific", NationId::JPN, 4, false, false, true, BiomeType::PACIFIC_ATOLL,
         {0.77f, 0.76f}, {16, 20, 21, 23}, {2, 1, 0, 1, 0, 0, 0, 0}},
        {23, "australia", "Australia & Guadalcanal", "South Pacific", NationId::UK, 3, false, false, true, BiomeType::PACIFIC_ATOLL,
         {0.86f, 0.84f}, {21, 22, 24}, {3, 1, 1, 1, 0, 1, 0, 0}},
        {24, "hawaii_midway", "Pearl Harbor & Midway", "Central Pacific", NationId::USA, 3, false, false, true, BiomeType::PACIFIC_ATOLL,
         {0.94f, 0.52f}, {1, 19, 21, 23, 25}, {3, 1, 1, 1, 0, 2, 1, 1}},
        {25, "alaska", "Alaska & Aleutian Islands", "Americas", NationId::USA, 2, false, false, true, BiomeType::ARDENNES_WINTER,
         {0.08f, 0.22f}, {1, 2, 15, 24}, {2, 1, 0, 0, 0, 1, 0, 0}}
    };
}

} // namespace aa2004
