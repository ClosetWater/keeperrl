#include "stdafx.h"
#include "game_config.h"
#include "pretty_printing.h"

const char* GameConfig::getConfigName(GameConfigId id) {
  switch (id) {
    case GameConfigId::CAMPAIGN_VILLAINS:
      return "campaign_villains";
    case GameConfigId::KEEPER_CREATURES:
      return "keeper_creatures";
    case GameConfigId::ADVENTURER_CREATURES:
      return "adventurer_creatures";
    case GameConfigId::BUILD_MENU:
      return "build_menu";
    case GameConfigId::WORKSHOPS_MENU:
      return "workshops_menu";
    case GameConfigId::IMMIGRATION:
      return "immigration";
    case GameConfigId::TECHNOLOGY:
      return "technology";
    case GameConfigId::CREATURE_ATTRIBUTES:
      return "creatures";
    case GameConfigId::Z_LEVELS:
      return "zlevels";
    case GameConfigId::RESOURCE_COUNTS:
      return "resources";
    case GameConfigId::GAME_INTRO_TEXT:
      return "intro_text";
    case GameConfigId::SPELL_SCHOOLS:
      return "spell_schools";
    case GameConfigId::SPELLS:
      return "spells";
    case GameConfigId::TILES:
      return "tiles";
    case GameConfigId::FURNITURE:
      return "furniture";
    case GameConfigId::FURNITURE_LISTS:
      return "furniture_lists";
    case GameConfigId::ENEMIES:
      return "enemies";
    case GameConfigId::ITEM_LISTS:
      return "item_lists";
    case GameConfigId::EXTERNAL_ENEMIES:
      return "endless_enemies";
    case GameConfigId::ITEMS:
      return "items";
    case GameConfigId::BUILDING_INFO:
      return "building_info";
    case GameConfigId::NAMES:
      return "names";
    case GameConfigId::BIOMES:
      return "biomes";
    case GameConfigId::CAMPAIGN_INFO:
      return "campaign_info";
  }
}

GameConfig::GameConfig(vector<DirectoryPath> modDirs) : dirs(std::move(modDirs)) {
}
