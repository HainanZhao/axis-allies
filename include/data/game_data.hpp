#pragma once
// ============================================================================
// AXIS & ALLIES (2004) — GAME DATA REGISTRY HEADER
// ============================================================================

#include "core/types.hpp"

namespace aa2004 {

class GameData {
public:
    static const GameData& instance();

    const NationDef& getNation(NationId id) const;
    const GeneralDef& getGeneral(NationId nation, int generalIdx) const;
    const BuildingDef& getBuilding(BuildingType type) const;
    const RegimentDef& getRegiment(RegimentType type) const;
    std::string getRegimentDisplayName(NationId nation, RegimentType type) const;

    const std::vector<SkirmishMapDef>& getSkirmishMaps() const { return skirmishMaps_; }
    const std::vector<CampaignMissionDef>& getCampaignMissions() const { return campaignMissions_; }
    const std::vector<TerritoryDef>& getTerritories() const { return territories_; }
    const TerritoryDef& getTerritory(int id) const { return territories_.at(id); }

private:
    GameData();
    void initNationsAndGenerals();
    void initBuildings();
    void initRegiments();
    void initMapsAndCampaigns();
    void initWorldTerritories();

    std::array<NationDef, static_cast<size_t>(NationId::COUNT)> nations_;
    std::array<BuildingDef, static_cast<size_t>(BuildingType::COUNT)> buildings_;
    std::array<RegimentDef, static_cast<size_t>(RegimentType::COUNT)> regiments_;
    std::vector<SkirmishMapDef> skirmishMaps_;
    std::vector<CampaignMissionDef> campaignMissions_;
    std::vector<TerritoryDef> territories_;
};

} // namespace aa2004
