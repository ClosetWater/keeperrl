/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "player_control.h"
#include "level.h"
#include "task.h"
#include "player.h"
#include "model.h"
#include "statistics.h"
#include "options.h"
#include "technology.h"
#include "village_control.h"
#include "item.h"
#include "item_factory.h"
#include "creature.h"
#include "square.h"
#include "view_id.h"
#include "collective.h"
#include "effect.h"
#include "trigger.h"
#include "music.h"
#include "location.h"
#include "model_builder.h"
#include "encyclopedia.h"
#include "map_memory.h"
#include "square_factory.h"
#include "item_action.h"
#include "equipment.h"
#include "collective_teams.h"
#include "minion_equipment.h"
#include "task_map.h"
#include "construction_map.h"
#include "minion_task_map.h"
#include "spell.h"
#include "tribe.h"
#include "visibility_map.h"
#include "creature_name.h"
#include "monster_ai.h"
#include "view.h"
#include "view_index.h"
#include "collective_attack.h"
#include "territory.h"
#include "sound.h"
#include "game.h"
#include "collective_name.h"
#include "creature_attributes.h"
#include "collective_config.h"
#include "villain_type.h"
#include "event_proxy.h"
#include "workshops.h"
#include "attack_trigger.h"
#include "view_object.h"
#include "body.h"
#include "furniture.h"
#include "furniture_type.h"
#include "furniture_factory.h"
#include "known_tiles.h"
#include "tile_efficiency.h"
#include "zones.h"
#include "inventory.h"
#include "immigration.h"
#include "scroll_position.h"

template <class Archive> 
void PlayerControl::serialize(Archive& ar, const unsigned int version) {
  ar& SUBCLASS(CollectiveControl);
  serializeAll(ar, memory, showWelcomeMsg, lastControlKeeperQuestion);
  serializeAll(ar, surprises, newAttacks, ransomAttacks, messages, hints, visibleEnemies, knownLocations);
  serializeAll(ar, visibilityMap);
  serializeAll(ar, messageHistory, eventProxy);
}

SERIALIZABLE(PlayerControl);

SERIALIZATION_CONSTRUCTOR_IMPL(PlayerControl);

typedef Collective::ResourceId ResourceId;

struct PlayerControl::BuildInfo {
  struct FurnitureInfo {
    FurnitureType type;
    CostInfo cost;
    bool noCredit;
    optional<int> maxNumber;
  } furnitureInfo;

  struct TrapInfo {
    TrapType type;
    ViewId viewId;
  } trapInfo;

  enum BuildType {
    DIG,
    FURNITURE,
    TRAP,
    DESTROY,
    ZONE,
    DISPATCH,
    CLAIM_TILE,
    FORBID_ZONE,
    TORCH,
  } buildType;

  string name;
  vector<Requirement> requirements;
  string help;
  char hotkey;
  string groupName;
  bool hotkeyOpensGroup = false;
  ZoneId zone;
  ViewId viewId;
  vector<FurnitureLayer> destroyLayers;

  BuildInfo(FurnitureInfo info, const string& n, vector<Requirement> req = {}, const string& h = "", char key = 0,
      string group = "", bool hotkeyOpens = false) : furnitureInfo(info), buildType(FURNITURE), name(n),
      requirements(req), help(h), hotkey(key), groupName(group), hotkeyOpensGroup(hotkeyOpens) {}

  BuildInfo(TrapInfo info, const string& n, vector<Requirement> req = {}, const string& h = "", char key = 0,
      string group = "") : trapInfo(info), buildType(TRAP), name(n), requirements(req), help(h), hotkey(key), groupName(group) {}

  BuildInfo(BuildType type, const string& n, const string& h = "", char key = 0, string group = "")
      : buildType(type), name(n), help(h), hotkey(key), groupName(group) {
    CHECK(contains({DIG, FORBID_ZONE, DISPATCH, CLAIM_TILE, TORCH}, type));
  }
  BuildInfo(const vector<FurnitureLayer>& layers, const string& n, const string& h = "", char key = 0, string group = "")
      : buildType(DESTROY), name(n), help(h), hotkey(key), groupName(group), destroyLayers(layers) {
  }

  BuildInfo(ZoneId zone, ViewId view, const string& n, const string& h = "", char key = 0, string group = "",
      bool hotkeyOpens = false)
      : buildType(ZONE), name(n), help(h), hotkey(key), groupName(group), hotkeyOpensGroup(hotkeyOpens) , zone(zone),
        viewId(view) {}
};

const vector<PlayerControl::BuildInfo>& PlayerControl::getBuildInfo() {
  const string workshop = "Manufactories";
  static optional<vector<BuildInfo>> buildInfo;
  if (!buildInfo) {
    buildInfo = {
      BuildInfo(BuildInfo::DIG, "Dig or cut tree", "", 'd'),
      BuildInfo({FurnitureType::MOUNTAIN, {ResourceId::STONE, 50}}, "Fill up tunnel", {},
          "Fill up one tile at a time. Cutting off an area is not allowed.", 0, "Structure"),
      BuildInfo({FurnitureType::DUNGEON_WALL, {ResourceId::STONE, 10}}, "Reinforce wall", {},
          "Reinforce wall. +" + toString<int>(100 * CollectiveConfig::getEfficiencyBonus(FurnitureType::DUNGEON_WALL)) +
          " efficiency to to surrounding tiles.", 0, "Structure"),
    };
    for (int i : All(CollectiveConfig::getFloors())) {
      auto& floor = CollectiveConfig::getFloors()[i];
      string efficiency = toString<int>(floor.efficiencyBonus * 100);
      buildInfo->push_back(
            BuildInfo({floor.type, floor.cost},
                floor.name + "  (+" + efficiency + ")",
                {}, floor.name + " floor. +" + efficiency + " efficiency to surrounding tiles.", i == 0 ? 'f' : 0,
                      "Floors", i == 0));
    };
    append(*buildInfo, {
             BuildInfo({FurnitureLayer::FLOOR}, "Remove floor", "", 0, "Floors"),
      BuildInfo(ZoneId::STORAGE_RESOURCES, ViewId::STORAGE_RESOURCES, "Resources",
          "Only wood, iron and granite can be stored here.", 's', "Storage", true),
      BuildInfo(ZoneId::STORAGE_EQUIPMENT, ViewId::STORAGE_EQUIPMENT, "Equipment",
          "All equipment for your minions can be stored here.", 0, "Storage"),
      BuildInfo({FurnitureType::BOOK_SHELF, {ResourceId::WOOD, 80}}, "Library", {},
          "Mana is regenerated here.", 'y'),
      BuildInfo({FurnitureType::THRONE, {ResourceId::GOLD, 800}, false, 1}, "Throne",
          {{RequirementId::VILLAGE_CONQUERED}},
          "Increases population limit by " + toString(ModelBuilder::getThronePopulationIncrease())),
      BuildInfo({FurnitureType::TREASURE_CHEST, {ResourceId::WOOD, 20}}, "Treasure chest", {},
          "Stores gold."),
      BuildInfo({FurnitureType::PIGSTY, {ResourceId::WOOD, 20}}, "Pigsty",
          {{RequirementId::TECHNOLOGY, TechId::PIGSTY}},
          "Increases minion population limit by up to " +
          toString(ModelBuilder::getPigstyPopulationIncrease()) + ".", 'p'),
      BuildInfo({FurnitureType::BED, {ResourceId::WOOD, 60}}, "Bed", {},
          "Humanoid minions sleep here.", 'b'),
      BuildInfo({FurnitureType::TRAINING_WOOD, {ResourceId::WOOD, 60}}, "Wooden dummy", {},
          "Train your minions here. Adds up to " +
          toString(*CollectiveConfig::getTrainingMaxLevelIncrease(FurnitureType::TRAINING_WOOD)) + " experience levels.",
          't', "Training room", true),
      BuildInfo({FurnitureType::TRAINING_IRON, {ResourceId::IRON, 60}}, "Iron dummy",
          {{RequirementId::TECHNOLOGY, TechId::IRON_WORKING}},
          "Train your minions here. Adds up to " +
          toString(*CollectiveConfig::getTrainingMaxLevelIncrease(FurnitureType::TRAINING_IRON)) + " experience levels.",
          0, "Training room"),
      BuildInfo({FurnitureType::TRAINING_STEEL, {ResourceId::STEEL, 60}}, "Steel dummy",
          {{RequirementId::TECHNOLOGY, TechId::STEEL_MAKING}},
          "Train your minions here. Adds up to " +
          toString(*CollectiveConfig::getTrainingMaxLevelIncrease(FurnitureType::TRAINING_STEEL)) + " experience levels.",
          0, "Training room"),
      BuildInfo({FurnitureType::WORKSHOP, {ResourceId::WOOD, 80}}, "Workshop",
          {{RequirementId::TECHNOLOGY, TechId::CRAFTING}},
          "Produces leather equipment, traps, first-aid kits and other.", 'm', workshop, true),
      BuildInfo({FurnitureType::FORGE, {ResourceId::IRON, 100}}, "Forge",
          {{RequirementId::TECHNOLOGY, TechId::IRON_WORKING}}, "Produces iron weapons and armor.", 0, workshop),
      BuildInfo({FurnitureType::LABORATORY, {ResourceId::STONE, 50}}, "Laboratory",
          {{RequirementId::TECHNOLOGY, TechId::ALCHEMY}}, "Produces magical potions.", 0, workshop),
      BuildInfo({FurnitureType::JEWELER, {ResourceId::WOOD, 60}}, "Jeweler",
          {{RequirementId::TECHNOLOGY, TechId::JEWELLERY}}, "Produces magical rings and amulets.", 0, workshop),
      BuildInfo({FurnitureType::STEEL_FURNACE, {ResourceId::STONE, 500}}, "Steel furnace",
          {{RequirementId::TECHNOLOGY, TechId::STEEL_MAKING}}, "Turns iron ore into steel.", 0, workshop),
      BuildInfo({FurnitureType::DEMON_SHRINE, {ResourceId::MANA, 60}}, "Demon shrine", {},
          "Summons various demons to your dungeon."),
      BuildInfo({FurnitureType::BEAST_CAGE, {ResourceId::WOOD, 40}}, "Beast cage", {}, "Beasts sleep here."),
      BuildInfo({FurnitureType::GRAVE, {ResourceId::STONE, 80}}, "Graveyard", {},
          "Spot for hauling dead bodies and for undead creatures to sleep in.", 'g'),
      BuildInfo({FurnitureType::PRISON, {ResourceId::IRON, 20}}, "Prison", {}, "Captured enemies are kept here.", 0),
      BuildInfo({FurnitureType::TORTURE_TABLE, {ResourceId::IRON, 100}}, "Torture table", {},
          "Can be used to torture prisoners.", 'u'),
      BuildInfo(BuildInfo::CLAIM_TILE, "Claim tile", "Claim a tile. Building anything has the same effect.", 0, "Orders"),
      BuildInfo(ZoneId::FETCH_ITEMS, ViewId::FETCH_ICON, "Fetch items",
          "Order imps to fetch items from locations outside the dungeon. This is a one-time order.", 0, "Orders"),
      BuildInfo(ZoneId::PERMANENT_FETCH_ITEMS, ViewId::FETCH_ICON, "Fetch items persistently",
          "Order imps to fetch items from locations outside the dungeon. This is a persistent order.", 0,
          "Orders"),
      BuildInfo(BuildInfo::DISPATCH, "Prioritize task", "Click on an existing task to give it a high priority.", 'a',
          "Orders"),
      BuildInfo({FurnitureLayer::CEILING, FurnitureLayer::MIDDLE}, "Remove construction", "", 'e', "Orders"),
      BuildInfo(BuildInfo::FORBID_ZONE, "Forbid zone", "Mark tiles to keep minions from entering.", 0, "Orders"),
      BuildInfo({FurnitureType::DOOR, {ResourceId::WOOD, 20}}, "Door",
          {{RequirementId::TECHNOLOGY, TechId::CRAFTING}}, "Click on a built door to lock it.", 'o', "Installations"),
      BuildInfo({FurnitureType::BRIDGE, {ResourceId::WOOD, 20}}, "Bridge", {},
        "Build it to pass over water or lava.", 0, "Installations"),
      BuildInfo({FurnitureType::BARRICADE, {ResourceId::WOOD, 20}}, "Barricade",
        {{RequirementId::TECHNOLOGY, TechId::CRAFTING}}, "", 0, "Installations"),
      BuildInfo(BuildInfo::TORCH, "Torch", "Place it on tiles next to a wall.", 'c', "Installations"),
      BuildInfo({FurnitureType::KEEPER_BOARD, {ResourceId::WOOD, 80}}, "Message board", {},
          "A board where you can leave a message for other players.", 0, "Installations"),
      BuildInfo({FurnitureType::EYEBALL, {ResourceId::MANA, 10}}, "Eyeball", {},
        "Makes the area around it visible.", 0, "Installations"),
      BuildInfo({FurnitureType::MINION_STATUE, {ResourceId::GOLD, 300}}, "Statue", {},
        "Increases minion population limit by " +
              toString(ModelBuilder::getStatuePopulationIncrease()) + ".", 0, "Installations"),
      BuildInfo({FurnitureType::WHIPPING_POST, {ResourceId::WOOD, 100}}, "Whipping post", {},
          "A place to whip your minions if they need a morale boost.", 0, "Installations"),
      BuildInfo({FurnitureType::IMPALED_HEAD, {ResourceId::PRISONER_HEAD, 1}, true}, "Prisoner head", {},
          "Impaled head of an executed prisoner. Aggravates enemies.", 0, "Installations"),
      BuildInfo({TrapType::TERROR, ViewId::TERROR_TRAP}, "Terror trap", {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Causes the trespasser to panic.", 0, "Traps"),
      BuildInfo({TrapType::POISON_GAS, ViewId::GAS_TRAP}, "Gas trap", {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Releases a cloud of poisonous gas.", 0, "Traps"),
      BuildInfo({TrapType::ALARM, ViewId::ALARM_TRAP}, "Alarm trap", {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Summons all minions", 0, "Traps"),
      BuildInfo({TrapType::WEB, ViewId::WEB_TRAP}, "Web trap", {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Immobilises the trespasser for some time.", 0, "Traps"),
      BuildInfo({TrapType::BOULDER, ViewId::BOULDER}, "Boulder trap", {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Causes a huge boulder to roll towards the enemy.", 0, "Traps"),
      BuildInfo({TrapType::SURPRISE, ViewId::SURPRISE_TRAP}, "Surprise trap",
          {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
          "Teleports nearby minions to deal with the trespasser.", 0, "Traps"),
    });
  }
  return *buildInfo;
}

vector<PlayerControl::RoomInfo> PlayerControl::getRoomInfo() {
  vector<RoomInfo> ret;
  for (auto& bInfo : getBuildInfo())
    if (bInfo.buildType == BuildInfo::FURNITURE)
      ret.push_back({bInfo.name, bInfo.help, bInfo.requirements});
  return ret;
}

static bool seeEverything = false;

const int hintFrequency = 700;
static vector<string> getHints() {
  return {
    "Research geology to uncover ores in the mountain.",
    "Morale affects minion productivity and chances of fleeing from battle.",
 //   "You can turn these hints off in the settings (F2).",
//    "Killing a leader greatly lowers the morale of his tribe and stops immigration.",
    "Your minions' morale is boosted when they are commanded by the Keeper.",
  };
}

PlayerControl::PlayerControl(Collective* col, Level* level) : CollectiveControl(col),
    eventProxy(this, level->getModel()), hints(getHints()) {
  bool hotkeys[128] = {0};
  for (auto& info : getBuildInfo()) {
    if (info.hotkey) {
      CHECK(!hotkeys[int(info.hotkey)]);
      hotkeys[int(info.hotkey)] = true;
    }
  }
  for (TechInfo info : getTechInfo()) {
    if (info.button.hotkey) {
      CHECK(!hotkeys[int(info.button.hotkey)]);
      hotkeys[int(info.button.hotkey)] = true;
    }
  }
  memory.reset(new MapMemory());
  for(const Location* loc : level->getAllLocations())
    if (loc->isMarkedAsSurprise())
      surprises.insert(loc->getMiddle());
}

PlayerControl::~PlayerControl() {
}

Creature* PlayerControl::getControlled() const {
  if (auto team = getCurrentTeam())
    return getTeams().getLeader(*team);
  else
    return nullptr;
}

optional<TeamId> PlayerControl::getCurrentTeam() const {
  for (TeamId team : getTeams().getAllActive())
    if (getTeams().getLeader(team)->isPlayer())
      return team;
  return none;
}

void PlayerControl::onControlledKilled() {
  if (getKeeper()->isPlayer())
    return;
  vector<CreatureInfo> team;
  TeamId currentTeam = *getCurrentTeam();
  for (Creature* c : getTeams().getMembers(currentTeam))
    if (!c->isPlayer())
      team.push_back(c);
  if (team.empty())
    return;
  optional<Creature::Id> newLeader;
  if (team.size() == 1)
    newLeader = team[0].uniqueId;
  else
    newLeader = getView()->chooseTeamLeader("Choose new team leader:", team, "Order team back to base");
  if (newLeader) {
    if (Creature* c = getCreature(*newLeader)) {
      getTeams().setLeader(currentTeam, c);
      commandTeam(currentTeam);
      return;
    }
  }
  leaveControl();
}

bool PlayerControl::swapTeam() {
  if (auto teamId = getCurrentTeam())
    if (getTeams().getMembers(*teamId).size() > 1) {
      vector<CreatureInfo> team;
      TeamId currentTeam = *getCurrentTeam();
      for (Creature* c : getTeams().getMembers(currentTeam))
        if (!c->isPlayer())
          team.push_back(c);
      if (team.empty())
        return false;
      if (auto newLeader = getView()->chooseTeamLeader("Choose new team leader:", team, "Cancel"))
        if (Creature* c = getCreature(*newLeader)) {
          getControlled()->popController();
          getTeams().setLeader(*teamId, c);
          commandTeam(*teamId);
        }
      return true;
    }
  return false;
}

void PlayerControl::leaveControl() {
  Creature* controlled = getControlled();
  if (controlled == getKeeper())
    lastControlKeeperQuestion = getCollective()->getGlobalTime();
  CHECK(controlled);
  if (!controlled->getPosition().isSameLevel(getLevel()))
    getView()->setScrollPos(getPosition());
  if (controlled->isPlayer())
    controlled->popController();
  for (TeamId team : getTeams().getActive(controlled)) {
    for (Creature* c : getTeams().getMembers(team))
//      if (getGame()->canTransferCreature(c, getCollective()->getLevel()->getModel()))
        getGame()->transferCreature(c, getModel());
    if (!getTeams().isPersistent(team)) {
      if (getTeams().getMembers(team).size() == 1)
        getTeams().cancel(team);
      else
        getTeams().deactivate(team);
      break;
    }
  }
  getView()->stopClock();
}

void PlayerControl::render(View* view) {
  if (firstRender) {
    firstRender = false;
    initialize();
  }
  if (!getControlled()) {
    ViewObject::setHallu(false);
    view->updateView(this, false);
  }
  if (showWelcomeMsg && getGame()->getOptions()->getBoolValue(OptionId::HINTS)) {
    view->updateView(this, false);
    showWelcomeMsg = false;
    view->presentText("", "So warlock,\n \nYou were dabbling in the Dark Arts, a tad, I see.\n \n "
        "Welcome to the valley of " + getGame()->getWorldName() + ", where you'll have to do "
        "what you can to KEEP yourself together. Build rooms, storage units and workshops to endorse your "
        "minions. The only way to go forward in this world is to destroy the ones who oppose you.\n \n"
"Use the mouse to dig into the mountain. You can select rectangular areas using the shift key. You will need access to trees, iron, stone and gold ore. Build rooms and traps and prepare for war. You can control a minion at any time by clicking on them in the minions tab or on the map.\n \n You can turn these messages off in the settings (press F2).");
  }
}

bool PlayerControl::isTurnBased() {
  return getControlled();
}

static vector<ItemType> marketItems {
  {ItemId::POTION, EffectId::HEAL},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::SLEEP)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::BLIND)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::INVISIBLE)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::POISON)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::POISON_RESISTANT)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::FLYING)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::SLOWED)},
  {ItemId::POTION, EffectType(EffectId::LASTING, LastingEffect::SPEED)},
  ItemId::WARNING_AMULET,
  ItemId::HEALING_AMULET,
  ItemId::DEFENSE_AMULET,
  {ItemId::RING, LastingEffect::FIRE_RESISTANT},
  {ItemId::RING, LastingEffect::POISON_RESISTANT},
};

void PlayerControl::addConsumableItem(Creature* creature) {
  ScrollPosition scrollPos;
  while (1) {
    Item* chosenItem = chooseEquipmentItem(creature, {}, [&](const Item* it) {
        return !getCollective()->getMinionEquipment().isOwner(it, creature)
            && !it->canEquip()
            && getCollective()->getMinionEquipment().needsItem(creature, it, true); }, &scrollPos);
    if (chosenItem)
      getCollective()->getMinionEquipment().own(creature, chosenItem);
    else
      break;
  }
}

void PlayerControl::addEquipment(Creature* creature, EquipmentSlot slot) {
  vector<Item*> currentItems = creature->getEquipment().getItem(slot);
  Item* chosenItem = chooseEquipmentItem(creature, currentItems, [&](const Item* it) {
      return !getCollective()->getMinionEquipment().isOwner(it, creature)
      && creature->canEquipIfEmptySlot(it, nullptr) && it->getEquipmentSlot() == slot; });
  if (chosenItem) {
    if (auto creatureId = getCollective()->getMinionEquipment().getOwner(chosenItem))
      if (Creature* c = getCreature(*creatureId))
        c->removeEffect(LastingEffect::SLEEP);
    if (chosenItem->getEquipmentSlot() != EquipmentSlot::WEAPON
        || creature->isEquipmentAppropriate(chosenItem)
        || getView()->yesOrNoPrompt(chosenItem->getTheName() + " is too heavy for " +
          creature->getName().the() + ", and will incur an accuracy penalty.\n Do you want to continue?"))
      getCollective()->getMinionEquipment().own(creature, chosenItem);
  }
}

void PlayerControl::minionEquipmentAction(const EquipmentActionInfo& action) {
  Creature* creature = getCreature(action.creature);
  switch (action.action) {
    case ItemAction::DROP:
      for (auto id : action.ids)
        getCollective()->getMinionEquipment().discard(id);
      break;
    case ItemAction::REPLACE:
      if (action.slot)
        addEquipment(creature, *action.slot);
      else
        addConsumableItem(creature);
      break;
    case ItemAction::LOCK:
      for (auto id : action.ids)
        getCollective()->getMinionEquipment().setLocked(creature, id, true);
      break;
    case ItemAction::UNLOCK:
      for (auto id : action.ids)
        getCollective()->getMinionEquipment().setLocked(creature, id, false);
      break;
    default: 
      break;
  }
}

void PlayerControl::minionTaskAction(const TaskActionInfo& action) {
  Creature* c = getCreature(action.creature);
  if (action.switchTo)
    getCollective()->setMinionTask(c, *action.switchTo);
  for (MinionTask task : action.lock)
    c->getAttributes().getMinionTasks().toggleLock(task);
}

static ItemInfo getItemInfo(const vector<Item*>& stack, bool equiped, bool pending, bool locked,
    optional<ItemInfo::Type> type = none) {
  return CONSTRUCT(ItemInfo,
    c.name = stack[0]->getShortName();
    c.fullName = stack[0]->getNameAndModifiers(false);
    c.description = stack[0]->getDescription();
    c.number = stack.size();
    if (stack[0]->canEquip())
      c.slot = stack[0]->getEquipmentSlot();
    c.viewId = stack[0]->getViewObject().id();
    for (auto it : stack)
      c.ids.insert(it->getUniqueId());
    c.actions = {ItemAction::DROP};
    c.equiped = equiped;
    c.locked = locked;
    if (type)
      c.type = *type;
    c.pending = pending;);
}

static ViewId getSlotViewId(EquipmentSlot slot) {
  switch (slot) {
    case EquipmentSlot::BOOTS: return ViewId::LEATHER_BOOTS;
    case EquipmentSlot::WEAPON: return ViewId::SWORD;
    case EquipmentSlot::RINGS: return ViewId::FIRE_RESIST_RING;
    case EquipmentSlot::HELMET: return ViewId::LEATHER_HELM;
    case EquipmentSlot::RANGED_WEAPON: return ViewId::BOW;
    case EquipmentSlot::GLOVES: return ViewId::LEATHER_GLOVES;
    case EquipmentSlot::BODY_ARMOR: return ViewId::LEATHER_ARMOR;
    case EquipmentSlot::AMULET: return ViewId::AMULET1;
  }
}

static ItemInfo getEmptySlotItem(EquipmentSlot slot) {
  return CONSTRUCT(ItemInfo,
    c.name = "";
    c.fullName = "";
    c.description = "";
    c.slot = slot;
    c.number = 1;
    c.viewId = getSlotViewId(slot);
    c.actions = {ItemAction::REPLACE};
    c.equiped = false;
    c.pending = false;);
}

static ItemInfo getTradeItemInfo(const vector<Item*>& stack, int budget) {
  return CONSTRUCT(ItemInfo,
    c.name = stack[0]->getShortName(nullptr, true);
    c.price = make_pair(ViewId::GOLD, stack[0]->getPrice());
    c.fullName = stack[0]->getNameAndModifiers(false);
    c.description = stack[0]->getDescription();
    c.number = stack.size();
    c.viewId = stack[0]->getViewObject().id();
    for (auto it : stack)
      c.ids.insert(it->getUniqueId());
    c.unavailable = c.price->second > budget;);
}

void PlayerControl::fillEquipment(Creature* creature, PlayerInfo& info) const {
  if (!creature->getBody().isHumanoid())
    return;
  int index = 0;
  double scrollPos = 0;
  vector<EquipmentSlot> slots;
  for (auto slot : Equipment::slotTitles)
    slots.push_back(slot.first);
  vector<Item*> ownedItems = getCollective()->getMinionEquipment().getItemsOwnedBy(creature);
  vector<Item*> slotItems;
  vector<EquipmentSlot> slotIndex;
  for (auto slot : slots) {
    vector<Item*> items;
    for (Item* it : ownedItems)
      if (it->canEquip() && it->getEquipmentSlot() == slot)
        items.push_back(it);
    for (int i = creature->getEquipment().getMaxItems(slot); i < items.size(); ++i)
      // a rare occurence that minion owns too many items of the same slot,
      //should happen only when an item leaves the fortress and then is braught back
      if (!getCollective()->getMinionEquipment().isLocked(creature, items[i]->getUniqueId()))
        getCollective()->getMinionEquipment().discard(items[i]);
    append(slotItems, items);
    append(slotIndex, vector<EquipmentSlot>(items.size(), slot));
    for (Item* item : items) {
      removeElement(ownedItems, item);
      bool equiped = creature->getEquipment().isEquipped(item);
      bool locked = getCollective()->getMinionEquipment().isLocked(creature, item->getUniqueId());
      info.inventory.push_back(getItemInfo({item}, equiped, !equiped, locked, ItemInfo::EQUIPMENT));
      info.inventory.back().actions.push_back(locked ? ItemAction::UNLOCK : ItemAction::LOCK);
    }
    if (creature->getEquipment().getMaxItems(slot) > items.size()) {
      info.inventory.push_back(getEmptySlotItem(slot));
      slotIndex.push_back(slot);
      slotItems.push_back(nullptr);
    }
  }
  vector<pair<string, vector<Item*>>> consumables = Item::stackItems(ownedItems,
      [&](const Item* it) { if (!creature->getEquipment().hasItem(it)) return " (pending)"; else return ""; } );
  for (auto elem : consumables)
    info.inventory.push_back(getItemInfo(elem.second, false,
          !creature->getEquipment().hasItem(elem.second.at(0)), false, ItemInfo::CONSUMABLE));
  for (Item* item : creature->getEquipment().getItems())
    if (!getCollective()->getMinionEquipment().isItemUseful(item))
      info.inventory.push_back(getItemInfo({item}, false, false, false, ItemInfo::OTHER));
}

Item* PlayerControl::chooseEquipmentItem(Creature* creature, vector<Item*> currentItems, ItemPredicate predicate,
    ScrollPosition* scrollPos) {
  vector<Item*> availableItems;
  vector<Item*> usedItems;
  vector<Item*> allItems = getCollective()->getAllItems(predicate);
  getCollective()->getMinionEquipment().sortByEquipmentValue(allItems);
  for (Item* item : allItems)
    if (!contains(currentItems, item)) {
      auto owner = getCollective()->getMinionEquipment().getOwner(item);
      if (owner && getCreature(*owner))
        usedItems.push_back(item);
      else
        availableItems.push_back(item);
    }
  if (currentItems.empty() && availableItems.empty() && usedItems.empty())
    return nullptr;
  vector<pair<string, vector<Item*>>> usedStacks = Item::stackItems(usedItems,
      [&](const Item* it) {
        const Creature* c = getCreature(*getCollective()->getMinionEquipment().getOwner(it));
        return " owned by " + c->getName().a() +
            " (level " + toString<int>(c->getAttributes().getVisibleExpLevel()) + ")";});
  vector<Item*> allStacked;
  vector<ItemInfo> options;
  for (Item* it : currentItems)
    options.push_back(getItemInfo({it}, true, false, false));
  for (auto elem : concat(Item::stackItems(availableItems), usedStacks)) {
    options.emplace_back(getItemInfo(elem.second, false, false, false));
    if (auto creatureId = getCollective()->getMinionEquipment().getOwner(elem.second.at(0)))
      if (const Creature* c = getCreature(*creatureId))
        options.back().owner = CreatureInfo(c);
    allStacked.push_back(elem.second.front());
  }
  auto creatureId = creature->getUniqueId();
  auto index = getView()->chooseItem(options, scrollPos);
  if (!index)
    return nullptr;
  return concat(currentItems, allStacked)[*index];
}

static string requires(TechId id) {
  return " (requires: " + Technology::get(id)->getName() + ")";
}

void PlayerControl::handlePersonalSpells(View* view) {
  vector<ListElem> options {
      ListElem("The Keeper can learn spells for use in combat and other situations. ", ListElem::TITLE),
      ListElem("You can cast them with 's' when you are in control of the Keeper.", ListElem::TITLE)};
  vector<Spell*> knownSpells = Technology::getAvailableSpells(getCollective());
  for (Spell* spell : Technology::getAllKeeperSpells()) {
    ListElem::ElemMod mod = ListElem::NORMAL;
    string suff;
    if (!contains(knownSpells, spell)) {
      mod = ListElem::INACTIVE;
      suff = requires(Technology::getNeededTech(spell));
    }
    options.push_back(ListElem(spell->getName() + suff, mod).setTip(spell->getDescription()));
  }
  view->presentList("Sorcery", options);
}

int PlayerControl::getNumMinions() const {
  return getCollective()->getCreatures(MinionTrait::FIGHTER).size();
}

int PlayerControl::getMinLibrarySize() const {
  return getCollective()->getTechnologies().size();
}

void PlayerControl::handleLibrary(View* view) {
  int libraryCount = getCollective()->getConstructions().getBuiltCount(FurnitureType::BOOK_SHELF);
  if (libraryCount == 0) {
    view->presentText("", "You need to build a library to start research.");
    return;
  }
  vector<ListElem> options;
  bool allInactive = false;
  if (libraryCount <= getMinLibrarySize()) {
    allInactive = true;
    options.emplace_back("You need a larger library to continue research.", ListElem::TITLE);
  }
  options.push_back(ListElem("You have " 
        + toString(int(getCollective()->numResource(ResourceId::MANA))) + " mana. ", ListElem::TITLE));
  vector<Technology*> techs = filter(Technology::getNextTechs(getCollective()->getTechnologies()),
      [](const Technology* tech) { return tech->canResearch(); });
  for (Technology* tech : techs) {
    int cost = getCollective()->getTechCost(tech);
    string text = tech->getName() + " (" + toString(cost) + " mana)";
    options.push_back(ListElem(text, cost <= int(getCollective()->numResource(ResourceId::MANA))
        && !allInactive ? ListElem::NORMAL : ListElem::INACTIVE).setTip(tech->getDescription()));
  }
  options.emplace_back("Researched:", ListElem::TITLE);
  for (Technology* tech : getCollective()->getTechnologies())
    options.push_back(ListElem(tech->getName(), ListElem::INACTIVE).setTip(tech->getDescription()));
  auto index = view->chooseFromList("Library", options);
  if (!index)
    return;
  Technology* tech = techs[*index];
  getCollective()->takeResource({ResourceId::MANA, int(getCollective()->getTechCost(tech))});
  getCollective()->acquireTech(tech);
  view->updateView(this, true);
  handleLibrary(view);
}

typedef CollectiveInfo::Button Button;

static optional<pair<ViewId, int>> getCostObj(CostInfo cost) {
  auto& resourceInfo = CollectiveConfig::getResourceInfo(cost.id);
  if (cost.value > 0 && !resourceInfo.dontDisplay)
    return make_pair(resourceInfo.viewId, cost.value);
  else
    return none;
}

static optional<pair<ViewId, int>> getCostObj(const optional<CostInfo>& cost) {
  if (cost)
    return getCostObj(*cost);
  else
    return none;
}

string PlayerControl::getMinionName(CreatureId id) const {
  static map<CreatureId, string> names;
  if (!names.count(id))
    names[id] = CreatureFactory::fromId(id, TribeId::getMonster())->getName().bare();
  return names.at(id);
}

bool PlayerControl::meetsRequirement(Requirement req) const {
  switch (req.getId()) {
    case RequirementId::TECHNOLOGY:
      return getCollective()->hasTech(req.get<TechId>());
    case RequirementId::VILLAGE_CONQUERED:
      for (const Collective* c : getGame()->getVillains(VillainType::MAIN))
        if (c->isConquered())
          return true;
      return false;
  }
}

string PlayerControl::getRequirementText(Requirement req) {
  switch (req.getId()) {
    case RequirementId::TECHNOLOGY:
      return Technology::get(req.get<TechId>())->getName();
    case PlayerControl::RequirementId::VILLAGE_CONQUERED:
      return "that at least one main villain is conquered";
  }
}

static ViewId getSquareViewId(SquareType type) {
  static unordered_map<SquareType, ViewId, CustomHash<SquareType>> ids;
  if (!ids.count(type))
    ids.insert(make_pair(type, SquareFactory::get(type)->getViewObject().id()));
  return ids.at(type);
}

static ViewId getFurnitureViewId(FurnitureType type) {
  static EnumMap<FurnitureType, optional<ViewId>> ids;
  if (!ids[type])
    ids[type] = FurnitureFactory::get(type, TribeId::getMonster())->getViewObject().id();
  return *ids[type];
}

vector<Button> PlayerControl::fillButtons(const vector<BuildInfo>& buildInfo) const {
  vector<Button> buttons;
  EnumMap<ResourceId, int> numResource([this](ResourceId id) { return getCollective()->numResource(id);});
  for (auto& button : buildInfo) {
    switch (button.buildType) {
      case BuildInfo::FURNITURE: {
           auto& elem = button.furnitureInfo;
           ViewId viewId = getFurnitureViewId(elem.type);
           string description;
           if (elem.cost.value > 0)
             if (int num = getCollective()->getConstructions().getBuiltPositions(elem.type).size())
               description = "[" + toString(num) + "]";
           int availableNow = !elem.cost.value ? 1 : numResource[elem.cost.id] / elem.cost.value;
           if (CollectiveConfig::getResourceInfo(elem.cost.id).dontDisplay && availableNow)
             description += " (" + toString(availableNow) + " available)";
           buttons.push_back({viewId, button.name,
               getCostObj(elem.cost),
               description,
               (elem.noCredit && !availableNow) ?
                  CollectiveInfo::Button::GRAY_CLICKABLE : CollectiveInfo::Button::ACTIVE });
           }
           break;
      case BuildInfo::DIG:
           buttons.push_back({ViewId::DIG_ICON, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::ZONE:
           buttons.push_back({button.viewId, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::CLAIM_TILE:
           buttons.push_back({ViewId::KEEPER_FLOOR, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::DISPATCH:
           buttons.push_back({ViewId::IMP, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::TRAP: {
             auto& elem = button.trapInfo;
             buttons.push_back({elem.viewId, button.name, none});
           }
           break;
      case BuildInfo::DESTROY:
           buttons.push_back({ViewId::DESTROY_BUTTON, button.name, none, "",
                   CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::FORBID_ZONE:
           buttons.push_back({ViewId::FORBID_ZONE, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::TORCH:
           buttons.push_back({ViewId::TORCH, button.name, none, "", CollectiveInfo::Button::ACTIVE});
           break;
    }
    vector<string> unmetReqText;
    for (auto& req : button.requirements)
      if (!meetsRequirement(req)) {
        unmetReqText.push_back("Requires " + getRequirementText(req) + ".");
        buttons.back().state = CollectiveInfo::Button::INACTIVE;
      }
    if (unmetReqText.empty())
      buttons.back().help = button.help;
    else
      buttons.back().help = combineSentences(concat({button.help}, unmetReqText));
    buttons.back().hotkey = button.hotkey;
    buttons.back().groupName = button.groupName;
    buttons.back().hotkeyOpensGroup = button.hotkeyOpensGroup;
  }
  return buttons;
}

vector<PlayerControl::TechInfo> PlayerControl::getTechInfo() const {
  vector<TechInfo> ret;
  ret.push_back({{ViewId::MANA, "Sorcery"}, [this](PlayerControl* c, View* view) {c->handlePersonalSpells(view);}});
  ret.push_back({{ViewId::LIBRARY, "Library", 'l'},
      [this](PlayerControl* c, View* view) { c->handleLibrary(view); }});
  ret.push_back({{ViewId::BOOK, "Keeperopedia"},
      [this](PlayerControl* c, View* view) { Encyclopedia().present(view); }});
  return ret;
}

static string getTriggerLabel(const AttackTrigger& trigger) {
  switch (trigger.getId()) {
    case AttackTriggerId::SELF_VICTIMS: return "Killed tribe members";
    case AttackTriggerId::GOLD: return "Your gold";
    case AttackTriggerId::STOLEN_ITEMS: return "Item theft";
    case AttackTriggerId::ROOM_BUILT:
      switch (trigger.get<FurnitureType>()) {
        case FurnitureType::THRONE: return "Your throne";
        case FurnitureType::IMPALED_HEAD: return "Impaled heads";
        default: FATAL << "Unsupported ROOM_BUILT type"; return "";
      }
    case AttackTriggerId::POWER: return "Your power";
    case AttackTriggerId::FINISH_OFF: return "Finishing you off";
    case AttackTriggerId::ENEMY_POPULATION: return "Dungeon population";
    case AttackTriggerId::TIMER: return "Your evilness";
    case AttackTriggerId::NUM_CONQUERED: return "Your aggression";
    case AttackTriggerId::ENTRY: return "Entry";
    case AttackTriggerId::PROXIMITY: return "Proximity";
  }
}

VillageInfo::Village PlayerControl::getVillageInfo(const Collective* col) const {
  VillageInfo::Village info;
  info.name = col->getName().getShort();
  info.tribeName = col->getName().getRace();
  info.triggers.clear();
  if (col->getModel() == getModel()) {
    if (!getCollective()->isKnownVillainLocation(col))
      info.access = VillageInfo::Village::NO_LOCATION;
    else {
      info.access = VillageInfo::Village::LOCATION;
      for (auto& trigger : col->getTriggers(getCollective()))
        info.triggers.push_back({getTriggerLabel(trigger.trigger), trigger.value});
    }
  } else if (!getGame()->isVillainActive(col))
    info.access = VillageInfo::Village::INACTIVE;
  else {
    info.access = VillageInfo::Village::ACTIVE;
    for (auto& trigger : col->getTriggers(getCollective()))
      info.triggers.push_back({getTriggerLabel(trigger.trigger), trigger.value});
  }
  bool hostile = col->getTribe()->isEnemy(getCollective()->getTribe());
  if (col->isConquered()) {
    info.state = info.CONQUERED;
    info.triggers.clear();
    if (col->canPillage())
      info.actions.push_back({VillageAction::PILLAGE, none});
  } else if (hostile)
    info.state = info.HOSTILE;
  else {
    info.state = info.FRIENDLY;
    if (getCollective()->isKnownVillainLocation(col)) {
      if (col->hasTradeItems())
        info.actions.push_back({VillageAction::TRADE, none});
    } else if (getGame()->isVillainActive(col)) {
      if (col->hasTradeItems())
        info.actions.push_back({VillageAction::TRADE, string("You must discover the location of the ally first.")});
    }
  }
  return info;
}

/*void PlayerControl::handleRecruiting(Collective* ally) {
  ScrollPosition scrollPos;
  vector<Creature*> recruited;
  vector<Creature*> transfers;
  while (1) {
    vector<Creature*> recruits = ally->getRecruits();
    if (recruits.empty())
      break;
    vector<CreatureInfo> creatures = transform2<CreatureInfo>(recruits,
        [] (const Creature* c) { return CreatureInfo(c);});
    string warning;
    if (getCollective()->getPopulationSize() >= getCollective()->getMaxPopulation())
      warning = "You have reached minion limit.";
    auto index = getView()->chooseRecruit("Recruit from " + ally->getName().getShort(), warning,
        {ViewId::GOLD, getCollective()->numResource(ResourceId::GOLD)}, creatures, &scrollPos);
    if (!index)
      break;
    for (Creature* c : recruits)
      if (c->getUniqueId() == *index) {
        ally->recruit(c, getCollective());
        recruited.push_back(c);
        if (c->getLevel()->getModel() != getModel())
          transfers.push_back(c);
        getCollective()->takeResource({ResourceId::GOLD, c->getAttributes().getRecruitmentCost()});
        break;
      }
  }
  for (auto& stack : Creature::stack(recruited))
    getCollective()->addNewCreatureMessage(stack);
  if (!transfers.empty())
    for (Creature* c : transfers)
//      if (getGame()->canTransferCreature(c, getCollective()->getLevel()->getModel()))
        getGame()->transferCreature(c, getModel());
}*/

void PlayerControl::handleTrading(Collective* ally) {
  ScrollPosition scrollPos;
  const set<Position>& storage = getCollective()->getZones().getPositions(ZoneId::STORAGE_EQUIPMENT);
  if (storage.empty()) {
    getView()->presentText("Information", "You need a storage room for equipment in order to trade.");
    return;
  }
  while (1) {
    vector<Item*> available = ally->getTradeItems();
    vector<pair<string, vector<Item*>>> items = Item::stackItems(available);
    if (items.empty())
      break;
    int budget = getCollective()->numResource(ResourceId::GOLD);
    vector<ItemInfo> itemInfo = transform2(items,
        [this, budget] (const pair<string, vector<Item*>> it) {
            return getTradeItemInfo(it.second, budget);});
    auto index = getView()->chooseTradeItem("Trade with " + ally->getName().getShort(),
        {ViewId::GOLD, getCollective()->numResource(ResourceId::GOLD)}, itemInfo, &scrollPos);
    if (!index)
      break;
    for (Item* it : available)
      if (it->getUniqueId() == *index && it->getPrice() <= budget) {
        getCollective()->takeResource({ResourceId::GOLD, it->getPrice()});
        Random.choose(storage).dropItem(ally->buyItem(it));
      }
    getView()->updateView(this, true);
  }
}

static ItemInfo getPillageItemInfo(const vector<Item*>& stack, bool noStorage) {
  return CONSTRUCT(ItemInfo,
    c.name = stack[0]->getShortName(nullptr, true);
    c.fullName = stack[0]->getNameAndModifiers(false);
    c.description = stack[0]->getDescription();
    c.number = stack.size();
    c.viewId = stack[0]->getViewObject().id();
    for (auto it : stack)
      c.ids.insert(it->getUniqueId());
    c.unavailable = noStorage;
    c.unavailableReason = noStorage ? "No storage is available for this item." : "";
  );
}

static vector<PItem> retrieveItems(Collective* col, vector<Item*> items) {
  vector<PItem> ret;
  EntitySet<Item> index(items);
  for (auto pos : col->getTerritory().getAll()) {
    for (auto item : copyOf(pos.getInventory().getItems()))
      if (index.contains(item))
        ret.push_back(pos.modInventory().removeItem(item));
  }
  return ret;
}

void PlayerControl::handlePillage(Collective* col) {
  ScrollPosition scrollPos;
  while (1) {
    struct PillageOption {
      vector<Item*> items;
      set<Position> storage;
    };
    vector<PillageOption> options;
    for (auto& elem : Item::stackItems(col->getAllItems(false)))
      if (auto storage = getCollective()->getStorageFor(elem.second.front()))
        options.push_back({elem.second, *storage});
      else
        options.push_back({elem.second, getCollective()->getZones().getPositions(ZoneId::STORAGE_EQUIPMENT)});
    if (options.empty())
      return;
    vector<ItemInfo> itemInfo = transform2(options, [this] (const PillageOption& it) {
            return getPillageItemInfo(it.items, it.storage.empty());});
    auto index = getView()->choosePillageItem("Pillage " + col->getName().getShort(), itemInfo, &scrollPos);
    if (!index)
      break;
    CHECK(!options[*index].storage.empty());
    Random.choose(options[*index].storage).dropItems(retrieveItems(col, options[*index].items));
    getView()->updateView(this, true);
  }
}

void PlayerControl::handleRansom(bool pay) {
  if (ransomAttacks.empty())
    return;
  auto& ransom = ransomAttacks.front();
  int amount = *ransom.getRansom();
  if (pay && getCollective()->hasResource({ResourceId::GOLD, amount})) {
    getCollective()->takeResource({ResourceId::GOLD, amount});
    ransom.getAttacker()->onRansomPaid();
  }
  removeIndex(ransomAttacks, 0);
}

vector<Collective*> PlayerControl::getKnownVillains(VillainType type) const {
  return filter(getGame()->getVillains(type), [this](Collective* c) {
      return seeEverything || getCollective()->isKnownVillain(c);});
}

vector<Creature*> PlayerControl::getMinionsLike(Creature* like) const {
  vector<Creature*> minions;
  for (Creature* c : getCreatures())
    if (c->getName().stack() == like->getName().stack())
      minions.push_back(c);
  return minions;
}

void PlayerControl::sortMinionsForUI(vector<Creature*>& minions) const {
  sort(minions.begin(), minions.end(), [] (const Creature* c1, const Creature* c2) {
      int l1 = (int) c1->getAttributes().getExpLevel();
      int l2 = (int) c2->getAttributes().getExpLevel();
      return l1 > l2 || (l1 == l2 && c1->getUniqueId() > c2->getUniqueId());
      });
}

vector<PlayerInfo> PlayerControl::getPlayerInfos(vector<Creature*> creatures, UniqueEntity<Creature>::Id chosenId) const {
  sortMinionsForUI(creatures);
  vector<PlayerInfo> minions;
  for (Creature* c : creatures) {
    minions.emplace_back();
    minions.back().readFrom(c);
    // only fill equipment for the chosen minion to avoid lag
    if (c->getUniqueId() == chosenId) {
      if (auto requiredDummy = getCollective()->getMissingTrainingDummy(c))
        minions.back().levelInfo.warning = "Requires " + Furniture::getName(*requiredDummy) + ".";
      for (MinionTask t : ENUM_ALL(MinionTask))
        if (c->getAttributes().getMinionTasks().getValue(t, true) > 0) {
          minions.back().minionTasks.push_back({t,
              !getCollective()->isMinionTaskPossible(c, t),
              getCollective()->getMinionTask(c) == t,
              c->getAttributes().getMinionTasks().isLocked(t)});
        }
      if (getCollective()->usesEquipment(c))
        fillEquipment(c, minions.back());
      if (getCollective()->hasTrait(c, MinionTrait::PRISONER))
        minions.back().actions = { PlayerInfo::EXECUTE };
      else {
        minions.back().actions = { PlayerInfo::CONTROL, PlayerInfo::RENAME };
        if (c != getCollective()->getLeader())
          minions.back().actions.push_back(PlayerInfo::BANISH);
      }
      if (c->getAttributes().getSkills().hasDiscrete(SkillId::CONSUMPTION))
        minions.back().actions.push_back(PlayerInfo::CONSUME);
    }
  }
  return minions;
}

vector<CollectiveInfo::CreatureGroup> PlayerControl::getCreatureGroups(vector<Creature*> v) const {
  sort(v.begin(), v.end(), [](const Creature* c1, const Creature* c2) {
        return c1->getAttributes().getExpLevel() > c2->getAttributes().getExpLevel();});
  map<string, CollectiveInfo::CreatureGroup> groups;
  for (Creature* c : v) {
    if (!groups.count(c->getName().stack()))
      groups[c->getName().stack()] = { c->getUniqueId(), c->getName().stack(), c->getViewObject().id(), 0};
    ++groups[c->getName().stack()].count;
    if (chosenCreature == c->getUniqueId() && !getChosenTeam())
      groups[c->getName().stack()].highlight = true;
  }
  return getValues(groups);
}

vector<CollectiveInfo::CreatureGroup> PlayerControl::getEnemyGroups() const {
  vector<Creature*> enemies;
  for (Vec2 v : getVisibleEnemies())
    if (Creature* c = Position(v, getCollective()->getLevel()).getCreature())
      enemies.push_back(c);
  return getCreatureGroups(enemies);
}

void PlayerControl::fillMinions(CollectiveInfo& info) const {
  vector<Creature*> minions;
  for (Creature* c : getCollective()->getCreaturesAnyOf(
        {MinionTrait::FIGHTER, MinionTrait::PRISONER, MinionTrait::WORKER}))
    minions.push_back(c);
  minions.push_back(getCollective()->getLeader());
  info.minionGroups = getCreatureGroups(minions);
  info.minions = transform2(minions, [](const Creature* c) { return CreatureInfo(c) ;});
  info.minionCount = getCollective()->getPopulationSize();
  info.minionLimit = getCollective()->getMaxPopulation();
}

ItemInfo PlayerControl::getWorkshopItem(const WorkshopItem& option) const {
  return CONSTRUCT(ItemInfo,
      c.name = option.name;
      c.viewId = option.viewId;
      c.price = getCostObj(option.cost * option.number);
      if (option.techId && !getCollective()->hasTech(*option.techId)) {
        c.unavailable = true;
        c.unavailableReason = "Requires technology: " + Technology::get(*option.techId)->getName();
      }
      c.description = option.description;
      c.productionState = option.state.get_value_or(0);
      c.actions = LIST(ItemAction::REMOVE, ItemAction::CHANGE_NUMBER);
      c.number = option.number * option.batchSize;
    );
}

static const ViewObject& getConstructionObject(FurnitureType type) {
  static EnumMap<FurnitureType, optional<ViewObject>> objects;
  if (!objects[type]) {
    objects[type] =  FurnitureFactory::get(type, TribeId::getMonster())->getViewObject();
    objects[type]->setModifier(ViewObject::Modifier::PLANNED);
  }
  return *objects[type];
}

void PlayerControl::fillWorkshopInfo(CollectiveInfo& info) const {
  info.workshopButtons.clear();
  int index = 0;
  int i = 0;
  for (auto workshopType : ENUM_ALL(WorkshopType)) {
    auto& workshopInfo = CollectiveConfig::getWorkshopInfo(workshopType);
    bool unavailable = getCollective()->getConstructions().getBuiltPositions(workshopInfo.furniture).empty();
    info.workshopButtons.push_back({capitalFirst(workshopInfo.taskName),
        getConstructionObject(workshopInfo.furniture).id(), false, unavailable});
    if (chosenWorkshop == workshopType) {
      index = i;
      info.workshopButtons.back().active = true;
    }
    ++i;
  }
  if (chosenWorkshop) {
    auto transFun = [this](const WorkshopItem& item) { return getWorkshopItem(item); };
    info.chosenWorkshop = CollectiveInfo::ChosenWorkshopInfo {
        transform2(getCollective()->getWorkshops().get(*chosenWorkshop).getOptions(), transFun),
        transform2(getCollective()->getWorkshops().get(*chosenWorkshop).getQueued(), transFun),
        index
    };
  }
}

void PlayerControl::fillImmigration(CollectiveInfo& info) const {
  info.immigration.clear();
  auto& immigration = getCollective()->getImmigration();
  for (auto& elem : immigration.getAvailable()) {
    const auto& candidate = elem.second.get();
    const int count = candidate.getCreatures().size();
    optional<double> timeRemaining;
    if (auto time = candidate.getEndTime())
      timeRemaining = *time - getGame()->getGlobalTime();
    vector<string> infoLines;
    candidate.getInfo().visitRequirements(makeDefaultVisitor(
        [&](const Pregnancy&) {
          optional<int> maxT;
          for (Creature* c : getCollective()->getCreatures())
            if (c->isAffected(LastingEffect::PREGNANT))
              if (auto remaining = c->getTimeRemaining(LastingEffect::PREGNANT))
                if (!maxT || *remaining > *maxT)
                  maxT = *remaining;
          if (maxT && (!timeRemaining || *maxT > *timeRemaining))
            timeRemaining = maxT;
        },
        [&](const RecruitmentInfo& info) {
          infoLines.push_back(
              toString(info.getAvailableRecruits(getGame(), candidate.getInfo().getId(0)).size()) +
              " recruits available");
        }
    ));
    Creature* c = candidate.getCreatures()[0];
    string name = c->getName().multiple(count);
    if (auto& s = c->getName().stackOnly())
      name += " (" + *s + ")";
    info.immigration.push_back(ImmigrantDataInfo {
        immigration.getMissingRequirements(candidate),
        infoLines,
        getCostObj(candidate.getCost()),
        name,
        c->getViewObject().id(),
        Range::singleElem((int) c->getAttributes().getVisibleExpLevel()),
        count,
        timeRemaining,
        elem.first,
        none,
        candidate.getCreatedTime(),
        candidate.getInfo().getKeybinding()
    });
  }
  sort(info.immigration.begin(), info.immigration.end(),
      [](const ImmigrantDataInfo& i1, const ImmigrantDataInfo& i2) {
        return (i1.timeLeft && (!i2.timeLeft || *i1.timeLeft > *i2.timeLeft)) ||
            (!i1.timeLeft && !i2.timeLeft && i1.id > i2.id);
      });
}

void PlayerControl::fillImmigrationHelp(CollectiveInfo& info) const {
  info.allImmigration.clear();
  struct CreatureStats {
    Range level;
    PCreature creature;
  };
  static EnumMap<CreatureId, optional<CreatureStats>> creatureStats;
  auto getStats = [&](CreatureId id) -> CreatureStats& {
    if (!creatureStats[id]) {
      int minL = 10000;
      int maxL = -10000;
      PCreature c;
      for (int i : Range(100)) {
        auto creature = CreatureFactory::fromId(id, TribeId::getKeeper());
        minL = min(minL, (int)creature->getAttributes().getVisibleExpLevel());
        maxL = max(maxL, (int)creature->getAttributes().getVisibleExpLevel());
        if (!c)
          c = std::move(creature);
      }
      creatureStats[id] = CreatureStats{Range(minL, maxL + 1), std::move(c)};
    }
    return *creatureStats[id];
  };
  for (auto elem : Iter(getCollective()->getConfig().getImmigrantInfo())) {
    auto creatureId = elem->getId(0);
    Creature* c = getStats(creatureId).creature.get();
    optional<pair<ViewId, int>> costObj;
    vector<string> requirements;
    vector<string> infoLines;
    elem->visitRequirements(makeVisitor<void>(
        [&](const AttractionInfo& attraction) {
          int required = attraction.amountClaimed;
          requirements.push_back("Requires " + toString(required) + " " +
              combineWithOr(transform2(attraction.types,
                  [&](const AttractionType& type) { return AttractionInfo::getAttractionName(type, required); })));
        },
        [&](const TechId& techId) {
          requirements.push_back("Requires technology: " + Technology::get(techId)->getName());
        },
        [&](const SunlightState& state) {
          requirements.push_back("Will only join during the "_s + SunlightInfo::getText(state));
        },
        [&](const FurnitureType& type) {
          requirements.push_back("Requires at least one " + Furniture::getName(type));
        },
        [&](const CostInfo& cost) {
          costObj = getCostObj(cost);
        },
        [&](const ExponentialCost& cost) {
          auto& resourceInfo = CollectiveConfig::getResourceInfo(cost.base.id);
          costObj = make_pair(resourceInfo.viewId, cost.base.value);
          infoLines.push_back("Cost doubles for every " + toString(cost.numToDoubleCost) + " "
              + c->getName().plural());
          if (cost.numFree > 0)
            infoLines.push_back("First " + toString(cost.numFree) + " " + c->getName().plural() + " are free");
        },
        [&](const Pregnancy&) {
          requirements.push_back("Requires a pregnant succubus");
        },
        [&](const RecruitmentInfo& info) {
          if (info.findEnemy(getGame()))
            requirements.push_back("Ally must be discovered and have recruits available");
          else
            requirements.push_back("Recruit is not available in this game");
        }
    ));
    if (auto limit = elem->getLimit())
      infoLines.push_back("Limited to " + toString(*limit) + " creatures");
    info.allImmigration.push_back(ImmigrantDataInfo {
        requirements,
        infoLines,
        costObj,
        c->getName().stack(),
        c->getViewObject().id(),
        getStats(creatureId).level,
        0,
        none,
        elem.index(),
        getCollective()->getImmigration().getAutoState(elem.index())
    });
  }
}

void PlayerControl::refreshGameInfo(GameInfo& gameInfo) const {
  gameInfo.singleModel = getGame()->isSingleModel();
  gameInfo.villageInfo.villages.clear();
  gameInfo.villageInfo.totalMain = 0;
  gameInfo.villageInfo.numConquered = 0;
  for (const Collective* col : getGame()->getVillains(VillainType::MAIN)) {
    ++gameInfo.villageInfo.totalMain;
    if (col->isConquered())
      ++gameInfo.villageInfo.numConquered;
  }
  gameInfo.villageInfo.numMainVillains = 0;
  for (const Collective* col : getKnownVillains(VillainType::MAIN)) {
    gameInfo.villageInfo.villages.push_back(getVillageInfo(col));
    ++gameInfo.villageInfo.numMainVillains;
  }
  gameInfo.villageInfo.numLesserVillains = 0;
  for (const Collective* col : getKnownVillains(VillainType::LESSER)) {
    gameInfo.villageInfo.villages.push_back(getVillageInfo(col));
    ++gameInfo.villageInfo.numLesserVillains;
  }
  for (const Collective* col : getKnownVillains(VillainType::ALLY))
    gameInfo.villageInfo.villages.push_back(getVillageInfo(col));
  SunlightInfo sunlightInfo = getGame()->getSunlightInfo();
  gameInfo.sunlightInfo = { sunlightInfo.getText(), (int)sunlightInfo.getTimeRemaining() };
  gameInfo.infoType = GameInfo::InfoType::BAND;
  CollectiveInfo& info = gameInfo.collectiveInfo;
  info.buildings = fillButtons(getBuildInfo());
  fillMinions(info);
  fillImmigration(info);
  fillImmigrationHelp(info);
  info.chosenCreature.reset();
  if (chosenCreature)
    if (Creature* c = getCreature(*chosenCreature)) {
      if (!getChosenTeam())
        info.chosenCreature = {*chosenCreature, getPlayerInfos(getMinionsLike(c), *chosenCreature)};
      else
        info.chosenCreature = {*chosenCreature, getPlayerInfos(getTeams().getMembers(*getChosenTeam()),
            *chosenCreature), *getChosenTeam()};
    }
  fillWorkshopInfo(info);
  info.monsterHeader = "Minions: " + toString(info.minionCount) + " / " + toString(info.minionLimit);
  info.enemyGroups = getEnemyGroups();
  info.numResource.clear();
  for (auto resourceId : ENUM_ALL(CollectiveResourceId)) {
    auto& elem = CollectiveConfig::getResourceInfo(resourceId);
    if (!elem.dontDisplay)
      info.numResource.push_back(
          {elem.viewId, getCollective()->numResourcePlusDebt(resourceId), elem.name});
  }
  info.warning = "";
  gameInfo.time = getCollective()->getGame()->getGlobalTime();
  gameInfo.modifiedSquares = gameInfo.totalSquares = 0;
  for (Collective* col : getCollective()->getGame()->getCollectives()) {
    gameInfo.modifiedSquares += col->getLevel()->getNumGeneratedSquares();
    gameInfo.totalSquares += col->getLevel()->getNumTotalSquares();
  }
  info.teams.clear();
  for (int i : All(getTeams().getAll())) {
    TeamId team = getTeams().getAll()[i];
    info.teams.emplace_back();
    for (Creature* c : getTeams().getMembers(team))
      info.teams.back().members.push_back(c->getUniqueId());
    info.teams.back().active = getTeams().isActive(team);
    info.teams.back().id = team;
    if (getChosenTeam() == team)
      info.teams.back().highlight = true;
  }
  info.techButtons.clear();
  for (TechInfo tech : getTechInfo())
    info.techButtons.push_back(tech.button);
  gameInfo.messageBuffer = messages;
  info.taskMap.clear();
  for (const Task* task : getCollective()->getTaskMap().getAllTasks()) {
    optional<UniqueEntity<Creature>::Id> creature;
    if (const Creature *c = getCollective()->getTaskMap().getOwner(task))
      creature = c->getUniqueId();
    info.taskMap.push_back({task->getDescription(), creature, getCollective()->getTaskMap().isPriorityTask(task)});
  }
  for (auto& elem : ransomAttacks) {
    info.ransom = {make_pair(ViewId::GOLD, *elem.getRansom()), elem.getAttackerName(),
        getCollective()->hasResource({ResourceId::GOLD, *elem.getRansom()})};
    break;
  }
}

void PlayerControl::addMessage(const PlayerMessage& msg) {
  messages.push_back(msg);
  messageHistory.push_back(msg);
  if (msg.getPriority() == MessagePriority::CRITICAL) {
    getView()->stopClock();
    if (Creature* c = getControlled())
      c->playerMessage(msg);
  }
}

void PlayerControl::initialize() {
  for (Creature* c : getCreatures())
    updateMinionVisibility(c);
}

void PlayerControl::updateMinionVisibility(const Creature* c) {
  vector<Position> visibleTiles = c->getVisibleTiles();
  visibilityMap->update(c, visibleTiles);
  for (Position pos : visibleTiles) {
    if (getCollective()->addKnownTile(pos))
      updateKnownLocations(pos);
    addToMemory(pos);
  }
}

void PlayerControl::onEvent(const GameEvent& event) {
  switch (event.getId()) {
    case EventId::POSITION_DISCOVERED: {
      Position pos = event.get<Position>();
      if (getCollective()->addKnownTile(pos))
        updateKnownLocations(pos);
      addToMemory(pos);
      break;
    }
    case EventId::CREATURE_EVENT: {
      auto& info = event.get<EventInfo::CreatureEvent>();
      if (contains(getCollective()->getCreatures(), info.creature))
        addMessage(PlayerMessage(info.message).setCreature(info.creature->getUniqueId()));
      break;
    }
    case EventId::MOVED: {
      Creature* c = event.get<Creature*>();
      if (contains(getCreatures(), c))
        updateMinionVisibility(c);
      break;
    }
    case EventId::WON_GAME:
      CHECK(!getKeeper()->isDead());
      getGame()->conquered(*getKeeper()->getName().first(), getCollective()->getKills().getSize(),
          getCollective()->getDangerLevel() + getCollective()->getPoints());
      getView()->presentText("", "When you are ready, retire your dungeon and share it online. "
        "Other players will be able to invade it as adventurers. To do this, press Escape and choose \'retire\'.");
      break;
    case EventId::TECHBOOK_READ: {
        Technology* tech = event.get<Technology*>();
        vector<Technology*> nextTechs = Technology::getNextTechs(getCollective()->getTechnologies());
        if (tech == nullptr) {
          if (!nextTechs.empty())
            tech = Random.choose(nextTechs);
          else
            tech = Random.choose(Technology::getAll());
        }
        if (!contains(getCollective()->getTechnologies(), tech)) {
          if (!contains(nextTechs, tech))
            getView()->presentText("Information", "The tome describes the knowledge of " + tech->getName()
                + ", but you do not comprehend it.");
          else {
            getView()->presentText("Information", "You have acquired the knowledge of " + tech->getName());
            getCollective()->acquireTech(tech);
          }
        } else {
          getView()->presentText("Information", "The tome describes the knowledge of " + tech->getName()
              + ", which you already possess.");
        }

      }
      break;
    default:
      break;
  }
}

void PlayerControl::updateKnownLocations(const Position& pos) {
  if (pos.getModel() == getModel())
    if (const Location* loc = pos.getLocation())
      if (!knownLocations.count(loc)) {
        knownLocations.insert(loc);
        if (auto name = loc->getName())
          addMessage(PlayerMessage("Your minions discover the location of " + *name, MessagePriority::HIGH)
              .setLocation(loc));
        else if (loc->isMarkedAsSurprise())
          addMessage(PlayerMessage("Your minions discover a new location.").setLocation(loc));
      }
  for (const Collective* col : getGame()->getCollectives())
    if (col != getCollective() && col->getTerritory().contains(pos)) {
      getCollective()->addKnownVillain(col);
      getCollective()->addKnownVillainLocation(col);
    }
}


const MapMemory& PlayerControl::getMemory() const {
  return *memory;
}

ViewObject PlayerControl::getTrapObject(TrapType type, bool armed) {
  for (auto& info : getBuildInfo())
    if (info.buildType == BuildInfo::TRAP && info.trapInfo.type == type) {
      if (!armed)
        return ViewObject(info.trapInfo.viewId, ViewLayer::LARGE_ITEM, "Unarmed " + Item::getTrapName(type) + " trap")
          .setModifier(ViewObject::Modifier::PLANNED);
      else
        return ViewObject(info.trapInfo.viewId, ViewLayer::LARGE_ITEM, Item::getTrapName(type) + " trap");
    }
  FATAL << "trap not found" << int(type);
  return ViewObject(ViewId::EMPTY, ViewLayer::LARGE_ITEM);
}

void PlayerControl::getSquareViewIndex(Position pos, bool canSee, ViewIndex& index) const {
  if (canSee)
    pos.getViewIndex(index, getCollective()->getLeader()); // use the leader as a generic viewer
  else
    index.setHiddenId(pos.getViewObject().id());
  if (const Creature* c = pos.getCreature())
    if (canSee) {
      index.insert(c->getViewObject());
      if (isEnemy(c))
        index.getObject(ViewLayer::CREATURE).setModifier(ViewObject::Modifier::HOSTILE);
    }
}

static bool showEfficiency(FurnitureType type) {
  switch (type) {
    case FurnitureType::BOOK_SHELF:
    case FurnitureType::DEMON_SHRINE:
    case FurnitureType::WORKSHOP:
    case FurnitureType::TRAINING_WOOD:
    case FurnitureType::TRAINING_IRON:
    case FurnitureType::TRAINING_STEEL:
    case FurnitureType::LABORATORY:
    case FurnitureType::JEWELER:
    case FurnitureType::THRONE:
    case FurnitureType::FORGE:
    case FurnitureType::STEEL_FURNACE:
      return true;
    default:
      return false;
  }
}

void PlayerControl::getViewIndex(Vec2 pos, ViewIndex& index) const {
  Position position(pos, getCollective()->getLevel());
  bool canSeePos = canSee(position);
  getSquareViewIndex(position, canSeePos, index);
  if (!canSeePos)
    if (auto memIndex = getMemory().getViewIndex(position))
      index.mergeFromMemory(*memIndex);
  if (getCollective()->getTerritory().contains(position))
    if (auto furniture = position.getFurniture(FurnitureLayer::MIDDLE)) {
      if (furniture->getType() == FurnitureType::BOOK_SHELF || CollectiveConfig::getWorkshopType(furniture->getType()))
        index.setHighlight(HighlightType::CLICKABLE_FURNITURE);
      if (chosenWorkshop && chosenWorkshop == CollectiveConfig::getWorkshopType(furniture->getType()))
        index.setHighlight(HighlightType::CLICKED_FURNITURE);
      if (draggedCreature)
        if (Creature* c = getCreature(*draggedCreature))
          if (auto task = MinionTasks::getTaskFor(c, furniture->getType()))
            if (c->getAttributes().getMinionTasks().getValue(*task) > 0)
              index.setHighlight(HighlightType::CREATURE_DROP);
      if (showEfficiency(furniture->getType()) && index.hasObject(ViewLayer::FLOOR))
        index.getObject(ViewLayer::FLOOR).setAttribute(ViewObject::Attribute::EFFICIENCY,
            getCollective()->getTileEfficiency().getEfficiency(position));
    }
  if (getCollective()->isMarked(position))
    index.setHighlight(getCollective()->getMarkHighlight(position));
  if (getCollective()->hasPriorityTasks(position))
    index.setHighlight(HighlightType::PRIORITY_TASK);
  for (auto task : getCollective()->getTaskMap().getTasks(position))
    if (auto viewId = task->getViewId())
        index.insert(ViewObject(*viewId, ViewLayer::LARGE_ITEM));
  if (position.isTribeForbidden(getTribeId()))
    index.setHighlight(HighlightType::FORBIDDEN_ZONE);
  getCollective()->getZones().setHighlights(position, index);
  if (rectSelection
      && pos.inRectangle(Rectangle::boundingBox({rectSelection->corner1, rectSelection->corner2})))
    index.setHighlight(rectSelection->deselect ? HighlightType::RECT_DESELECTION : HighlightType::RECT_SELECTION);
  const ConstructionMap& constructions = getCollective()->getConstructions();
  if (!index.hasObject(ViewLayer::LARGE_ITEM)) {
    if (constructions.containsTrap(position))
      index.insert(getTrapObject(constructions.getTrap(position).getType(),
            constructions.getTrap(position).isArmed()));
    for (auto layer : ENUM_ALL(FurnitureLayer))
      if (constructions.containsFurniture(position, layer) && !constructions.getFurniture(position, layer).isBuilt())
        index.insert(getConstructionObject(constructions.getFurniture(position, layer).getFurnitureType()));
  }
  if (constructions.containsTorch(position) && !constructions.getTorch(position).isBuilt())
    index.insert(copyOf(Trigger::getTorchViewObject(constructions.getTorch(position).getAttachmentDir()))
        .setModifier(ViewObject::Modifier::PLANNED));
  if (surprises.count(position) && !getCollective()->getKnownTiles().isKnown(position))
    index.insert(ViewObject(ViewId::UNKNOWN_MONSTER, ViewLayer::CREATURE, "Surprise"));
}

Vec2 PlayerControl::getPosition() const {
  if (const Creature* keeper = getKeeper())
    if (!keeper->isDead() && keeper->getLevel() == getLevel())
      return keeper->getPosition().getCoord();
  if (!getCollective()->getTerritory().isEmpty())
    return getCollective()->getTerritory().getAll().front().getCoord();
  return Vec2(0, 0);
}

optional<CreatureView::MovementInfo> PlayerControl::getMovementInfo() const {
  return none;
}

static enum Selection { SELECT, DESELECT, NONE } selection = NONE;

class MinionController : public Player {
  public:
  MinionController(Creature* c, Model* m, MapMemory* memory, PlayerControl* ctrl)
      : Player(c, false, memory), control(ctrl) {}

  virtual vector<CommandInfo> getCommands() const override {
    return concat(Player::getCommands(), {
      {PlayerInfo::CommandInfo{"Leave minion", 'u', "Leave minion and order team back to base.", true},
       [] (Player* player) { dynamic_cast<MinionController*>(player)->unpossess(); }, true},
      {PlayerInfo::CommandInfo{"Switch control", 's', "Switch control to a different team member.", true},
       [] (Player* player) { dynamic_cast<MinionController*>(player)->swapTeam(); }, getTeam().size() > 1},
      {PlayerInfo::CommandInfo{"Absorb", 'a',
          "Absorb a friendly creature and inherit its attributes. Requires the absorbtion skill.",
          getCreature()->getAttributes().getSkills().hasDiscrete(SkillId::CONSUMPTION)},
       [] (Player* player) { dynamic_cast<MinionController*>(player)->consumeAction();}, false},
    });
  }

  void consumeAction() {
    vector<Creature*> targets = control->getCollective()->getConsumptionTargets(getCreature());
    vector<Creature*> actions;
    for (auto target : targets)
      if (auto action = getCreature()->consume(target))
        actions.push_back(target);
    if (actions.size() == 1 && getView()->yesOrNoPrompt("Really absorb " + actions[0]->getName().the() + "?")) {
      tryToPerform(getCreature()->consume(actions[0]));
    } else
    if (actions.size() > 1) {
      auto dir = getView()->chooseDirection("Which direction?");
      if (!dir)
        return;
      if (Creature* c = getCreature()->getPosition().plus(*dir).getCreature())
        if (contains(targets, c) && getView()->yesOrNoPrompt("Really absorb " + c->getName().the() + "?"))
          tryToPerform(getCreature()->consume(c));
    }
  }

  void unpossess() {
    control->leaveControl();
  }

  bool swapTeam() {
    return control->swapTeam();
  }

  virtual void onFellAsleep() override {
    getGame()->getView()->presentText("Important!", "You fall asleep. You loose control of your minion.");
    unpossess();
  }

  virtual vector<Creature*> getTeam() const override {
    return control->getTeam(getCreature());
  }

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& SUBCLASS(Player)
      & SVAR(control);
  }

  SERIALIZATION_CONSTRUCTOR(MinionController);

  private:
  PlayerControl* SERIAL(control);
};

void PlayerControl::controlSingle(const Creature* cr) {
  CHECK(contains(getCreatures(), cr));
  CHECK(!cr->isDead());
  Creature* c = const_cast<Creature*>(cr);
  commandTeam(getTeams().create({c}));
}



Creature* PlayerControl::getCreature(UniqueEntity<Creature>::Id id) const {
  for (Creature* c : getCreatures())
    if (c->getUniqueId() == id)
      return c;
  return nullptr;
}

vector<Creature*> PlayerControl::getTeam(const Creature* c) {
  vector<Creature*> ret;
  for (auto team : getTeams().getActive(c))
    append(ret, getTeams().getMembers(team));
  return ret;
}

void PlayerControl::commandTeam(TeamId team) {
  if (getControlled())
    leaveControl();
  Creature* c = getTeams().getLeader(team);
  c->pushController(SController(new MinionController(c, getModel(), memory.get(), this)));
  getTeams().activate(team);
  getCollective()->freeTeamMembers(team);
  getView()->resetCenter();
}

optional<PlayerMessage> PlayerControl::findMessage(PlayerMessage::Id id){
  for (auto& elem : messages)
    if (elem.getUniqueId() == id)
      return elem;
  return none;
}

CollectiveTeams& PlayerControl::getTeams() {
  return getCollective()->getTeams();
}

const CollectiveTeams& PlayerControl::getTeams() const {
  return getCollective()->getTeams();
}

void PlayerControl::setScrollPos(Position pos) {
  if (pos.isSameLevel(getLevel()))
    getView()->setScrollPos(pos.getCoord());
  else if (auto stairs = getLevel()->getStairsTo(pos.getLevel()))
    getView()->setScrollPos(stairs->getCoord());
}

void PlayerControl::scrollToMiddle(const vector<Position>& pos) {
  vector<Vec2> visible;
  for (Position v : pos)
    if (getCollective()->getKnownTiles().isKnown(v))
      visible.push_back(v.getCoord());
  CHECK(!visible.empty());
  getView()->setScrollPos(Rectangle::boundingBox(visible).middle());
}

Collective* PlayerControl::getVillain(int num) {
  return concat(getKnownVillains(VillainType::MAIN),
                getKnownVillains(VillainType::LESSER),
                getKnownVillains(VillainType::ALLY))[num];
}

optional<TeamId> PlayerControl::getChosenTeam() const {
  if (chosenTeam && getTeams().exists(*chosenTeam))
    return chosenTeam;
  else
    return none;
}

void PlayerControl::setChosenCreature(optional<UniqueEntity<Creature>::Id> id) {
  clearChosenInfo();
  chosenCreature = id;
}

void PlayerControl::setChosenTeam(optional<TeamId> team, optional<UniqueEntity<Creature>::Id> creature) {
  clearChosenInfo();
  chosenTeam = team;
  chosenCreature = creature;
}

void PlayerControl::clearChosenInfo() {
  setChosenWorkshop(none);
  chosenCreature = none;
  chosenTeam = none;
}

void PlayerControl::setChosenWorkshop(optional<WorkshopType> type) {
  auto refreshHighlights = [&] {
    if (chosenWorkshop)
      for (auto pos : getCollective()->getConstructions().getBuiltPositions(
             CollectiveConfig::getWorkshopInfo(*chosenWorkshop).furniture))
        pos.setNeedsRenderUpdate(true);
  };
  refreshHighlights();
  if (type)
    clearChosenInfo();
  chosenWorkshop = type;
  refreshHighlights();
}

void PlayerControl::minionDragAndDrop(const CreatureDropInfo& info) {
  Position pos(info.pos, getLevel());
  if (Creature* c = getCreature(info.creatureId)) {
    c->removeEffect(LastingEffect::TIED_UP);
    c->removeEffect(LastingEffect::SLEEP);
    if (getCollective()->getConstructions().containsFurniture(pos, FurnitureLayer::MIDDLE)) {
      auto& furniture = getCollective()->getConstructions().getFurniture(pos, FurnitureLayer::MIDDLE);
      if (auto task = MinionTasks::getTaskFor(c, furniture.getFurnitureType())) {
        if (getCollective()->isMinionTaskPossible(c, *task)) {
          getCollective()->setMinionTask(c, *task);
          getCollective()->setTask(c, Task::goTo(pos));
          return;
        }
      }
    }
    PTask task = Task::goToAndWait(pos, 15);
    task->setViewId(ViewId::GUARD_POST);
    getCollective()->setTask(c, std::move(task));
    pos.setNeedsRenderUpdate(true);
  }
}

bool PlayerControl::canSelectRectangle(const BuildInfo& info) {
  switch (info.buildType) {
    case BuildInfo::ZONE:
    case BuildInfo::FORBID_ZONE:
    case BuildInfo::FURNITURE:
    case BuildInfo::DIG:
    case BuildInfo::DESTROY:
    case BuildInfo::DISPATCH:
    case BuildInfo::CLAIM_TILE:
      return true;
    default:
      return false;
  }
}

void PlayerControl::processInput(View* view, UserInput input) {
  switch (input.getId()) {
    case UserInputId::MESSAGE_INFO:
        if (auto message = findMessage(input.get<PlayerMessage::Id>())) {
          if (auto pos = message->getPosition())
            setScrollPos(*pos);
          else if (auto id = message->getCreature()) {
            if (const Creature* c = getCreature(*id))
              setScrollPos(c->getPosition());
          } else if (auto loc = message->getLocation()) {
            if (loc->getMiddle().isSameLevel(getLevel()))
              scrollToMiddle(loc->getAllSquares());
            else
              setScrollPos(loc->getMiddle());
          }
        }
        break;
    case UserInputId::GO_TO_VILLAGE: 
        if (Collective* col = getVillain(input.get<int>())) {
          if (col->getLevel() != getLevel())
            setScrollPos(col->getTerritory().getAll().at(0));
          else
            scrollToMiddle(col->getTerritory().getAll());
        }
        break;
    case UserInputId::CREATE_TEAM:
        if (Creature* c = getCreature(input.get<Creature::Id>()))
          if (getCollective()->hasTrait(c, MinionTrait::FIGHTER) || c == getCollective()->getLeader())
            getTeams().create({c});
        break;
    case UserInputId::CREATE_TEAM_FROM_GROUP:
        if (Creature* creature = getCreature(input.get<Creature::Id>())) {
          vector<Creature*> group = getMinionsLike(creature);
          optional<TeamId> team;
          for (Creature* c : group)
            if (getCollective()->hasTrait(c, MinionTrait::FIGHTER) || c == getCollective()->getLeader()) {
              if (!team)
                team = getTeams().create({c});
              else
                getTeams().add(*team, c);
            }
        }
        break;   
    case UserInputId::CREATURE_DRAG:
        draggedCreature = input.get<Creature::Id>();
        for (auto task : ENUM_ALL(MinionTask))
          for (auto& pos : MinionTasks::getAllPositions(getCollective(), nullptr, task))
            pos.setNeedsRenderUpdate(true);
        break;
    case UserInputId::CREATURE_DRAG_DROP:
        minionDragAndDrop(input.get<CreatureDropInfo>());
        draggedCreature = none;
        break;
    case UserInputId::TEAM_DRAG_DROP: {
        auto& info = input.get<TeamDropInfo>();
        Position pos = Position(info.pos, getLevel());
        if (getTeams().exists(info.teamId))
          for (Creature* c : getTeams().getMembers(info.teamId)) {
            PTask task = Task::goToAndWait(pos, 15);
            task->setViewId(ViewId::GUARD_POST);
            getCollective()->setTask(c, std::move(task));
            pos.setNeedsRenderUpdate(true);
          }
        break;
    }
    case UserInputId::CANCEL_TEAM:
        if (getChosenTeam() == input.get<TeamId>()) {
          setChosenTeam(none);
          chosenCreature = none;
        }
        getTeams().cancel(input.get<TeamId>());
        break;
    case UserInputId::SELECT_TEAM: {
        auto teamId = input.get<TeamId>();
        if (getChosenTeam() == teamId) {
          setChosenTeam(none);
          chosenCreature = none;
        } else
          setChosenTeam(teamId, getTeams().getLeader(teamId)->getUniqueId());
        break;
    }
    case UserInputId::ACTIVATE_TEAM:
        if (!getTeams().isActive(input.get<TeamId>()))
          getTeams().activate(input.get<TeamId>());
        else
          getTeams().deactivate(input.get<TeamId>());
        break;
    case UserInputId::TILE_CLICK: {
        Vec2 pos = input.get<Vec2>();
        if (pos.inRectangle(getLevel()->getBounds()))
          onSquareClick(Position(pos, getLevel()));
        break;
        }

/*    case UserInputId::MOVE_TO:
        if (getCurrentTeam() && getTeams().isActive(*getCurrentTeam()) &&
            getCollective()->getKnownTiles().isKnown(Position(input.get<Vec2>(), getLevel()))) {
          getCollective()->freeTeamMembers(*getCurrentTeam());
          getCollective()->setTask(getTeams().getLeader(*getCurrentTeam()),
              Task::goTo(Position(input.get<Vec2>(), getLevel())), true);
 //         view->continueClock();
        }
        break;*/
    case UserInputId::DRAW_LEVEL_MAP: view->drawLevelMap(this); break;
    case UserInputId::DRAW_WORLD_MAP: getGame()->presentWorldmap(); break;
    case UserInputId::TECHNOLOGY: getTechInfo()[input.get<int>()].butFun(this, view); break;
    case UserInputId::WORKSHOP: {
          int index = input.get<int>();
          if (index < 0 || index >= EnumInfo<WorkshopType>::size)
            setChosenWorkshop(none);
          else {
            WorkshopType type = (WorkshopType) index;
            if (chosenWorkshop == type)
              setChosenWorkshop(none);
            else
              setChosenWorkshop(type);
          }
        }
        break;
    case UserInputId::WORKSHOP_ADD: 
        if (chosenWorkshop) {
          getCollective()->getWorkshops().get(*chosenWorkshop).queue(input.get<int>());
          getCollective()->getWorkshops().scheduleItems(getCollective());
          getCollective()->updateResourceProduction();
        }
        break;
    case UserInputId::WORKSHOP_ITEM_ACTION: {
        auto& info = input.get<WorkshopQueuedActionInfo>();
        if (chosenWorkshop) {
          auto& workshop = getCollective()->getWorkshops().get(*chosenWorkshop);
          switch (info.action) {
            case ItemAction::REMOVE:
              workshop.unqueue(info.itemIndex);
              break;
            case ItemAction::CHANGE_NUMBER: {
              int batchSize = workshop.getQueued().at(info.itemIndex).batchSize;
              if (auto number = getView()->getNumber("Change the number of items:", 0, 50 * batchSize, batchSize)) {
                if (*number > 0)
                  workshop.changeNumber(info.itemIndex, *number / batchSize);
                else
                  workshop.unqueue(info.itemIndex);
              }
              break;
            }
            default:
              break;
          }
          getCollective()->getWorkshops().scheduleItems(getCollective());
          getCollective()->updateResourceProduction();
        }
      }
      break;
    case UserInputId::CREATURE_GROUP_BUTTON: 
        if (Creature* c = getCreature(input.get<Creature::Id>()))
          if (!chosenCreature || getChosenTeam() || !getCreature(*chosenCreature) ||
              getCreature(*chosenCreature)->getName().stack() != c->getName().stack()) {
            setChosenTeam(none);
            setChosenCreature(input.get<Creature::Id>());
            break;
          }
        setChosenTeam(none);
        chosenCreature = none;
        break;
    case UserInputId::CREATURE_BUTTON: {
        auto chosenId = input.get<Creature::Id>();
        if (Creature* c = getCreature(chosenId)) {
          if (!getChosenTeam() || !getTeams().contains(*getChosenTeam(), c))
            setChosenCreature(chosenId);
          else
            setChosenTeam(*chosenTeam, chosenId);
        }
        else
          setChosenTeam(none);
      }
      break;
    case UserInputId::CREATURE_TASK_ACTION:
        minionTaskAction(input.get<TaskActionInfo>());
        break;
    case UserInputId::CREATURE_EQUIPMENT_ACTION:
        minionEquipmentAction(input.get<EquipmentActionInfo>());
        break;
    case UserInputId::CREATURE_CONTROL:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          if (getChosenTeam() && getTeams().exists(*getChosenTeam())) {
            getTeams().setLeader(*getChosenTeam(), c);
            commandTeam(*getChosenTeam());
          } else
            controlSingle(c);
          chosenCreature = none;
          setChosenTeam(none);
        }
        break;
    case UserInputId::CREATURE_RENAME:
        if (Creature* c = getCreature(input.get<RenameActionInfo>().creature))
          c->getName().setFirst(input.get<RenameActionInfo>().name);
        break;
    case UserInputId::CREATURE_CONSUME:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          if (auto creatureId = getView()->chooseTeamLeader("Choose minion to absorb",
              transform2(getCollective()->getConsumptionTargets(c),
                  [] (const Creature* c) { return CreatureInfo(c);}), "cancel"))
            if (Creature* consumed = getCreature(*creatureId))
              getCollective()->orderConsumption(c, consumed);
        }
        break;
    case UserInputId::CREATURE_BANISH:
        if (Creature* c = getCreature(input.get<Creature::Id>()))
          if (getView()->yesOrNoPrompt("Do you want to banish " + c->getName().the() + " forever? "
              "Banishing has a negative impact on morale of other minions.")) {
            vector<Creature*> like = getMinionsLike(c);
            sortMinionsForUI(like);
            if (like.size() > 1)
              for (int i : All(like))
                if (like[i] == c) {
                  if (i < like.size() - 1)
                    setChosenCreature(like[i + 1]->getUniqueId());
                  else
                    setChosenCreature(like[like.size() - 2]->getUniqueId());
                  break;
                }
            getCollective()->banishCreature(c);
          }
        break;
    case UserInputId::CREATURE_EXECUTE:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          getCollective()->orderExecution(c);
          chosenCreature = none;
        }
        break;
    case UserInputId::GO_TO_ENEMY:
        for (Vec2 v : getVisibleEnemies())
          if (Creature* c = Position(v, getCollective()->getLevel()).getCreature())
            setScrollPos(c->getPosition());
        break;
    case UserInputId::ADD_GROUP_TO_TEAM: {
        auto info = input.get<TeamCreatureInfo>();
        if (Creature* creature = getCreature(info.creatureId)) {
          vector<Creature*> group = getMinionsLike(creature);
          for (Creature* c : group)
            if (getTeams().exists(info.team) && !getTeams().contains(info.team, c) &&
                (getCollective()->hasTrait(c, MinionTrait::FIGHTER) || c == getCollective()->getLeader()))
              getTeams().add(info.team, c);
        }
        break; }
    case UserInputId::ADD_TO_TEAM: {
        auto info = input.get<TeamCreatureInfo>();
        if (Creature* c = getCreature(info.creatureId))
          if (getTeams().exists(info.team) && !getTeams().contains(info.team, c) &&
              (getCollective()->hasTrait(c, MinionTrait::FIGHTER) || c == getCollective()->getLeader()))
            getTeams().add(info.team, c);
        break; }
    case UserInputId::REMOVE_FROM_TEAM: {
        auto info = input.get<TeamCreatureInfo>();
        if (Creature* c = getCreature(info.creatureId))
          if (getTeams().exists(info.team) && getTeams().contains(info.team, c)) {
            getTeams().remove(info.team, c);
            if (getTeams().exists(info.team)) {
              if (chosenCreature == info.creatureId)
                setChosenTeam(info.team, getTeams().getLeader(info.team)->getUniqueId());
            } else
              chosenCreature = none;
          }
        break; }
    case UserInputId::IMMIGRANT_ACCEPT: {
        auto available = getCollective()->getImmigration().getAvailable();
        if (auto info = getReferenceMaybe(available, input.get<int>()))
          if (auto sound = info->get().getInfo().getSound())
            getView()->addSound(*sound);
        getCollective()->getImmigration().accept(input.get<int>());
        break; }
    case UserInputId::IMMIGRANT_REJECT:
        getCollective()->getImmigration().rejectIfNonPersistent(input.get<int>());
        break;
    case UserInputId::IMMIGRANT_AUTO_ACCEPT: {
        int id = input.get<int>();
        if (!!getCollective()->getImmigration().getAutoState(id))
          getCollective()->getImmigration().setAutoState(id, none);
        else
          getCollective()->getImmigration().setAutoState(id, ImmigrantAutoState::AUTO_ACCEPT);
        }
        break;
    case UserInputId::IMMIGRANT_AUTO_REJECT: {
        int id = input.get<int>();
        if (!!getCollective()->getImmigration().getAutoState(id))
          getCollective()->getImmigration().setAutoState(id, none);
        else
          getCollective()->getImmigration().setAutoState(id, ImmigrantAutoState::AUTO_REJECT);
        }
        break;
    case UserInputId::RECT_SELECTION: {
        auto& info = input.get<BuildingInfo>();
        if (canSelectRectangle(getBuildInfo()[info.building])) {
          updateSelectionSquares();
          if (rectSelection) {
            rectSelection->corner2 = info.pos;
          } else
            rectSelection = CONSTRUCT(SelectionInfo, c.corner1 = c.corner2 = info.pos;);
          updateSelectionSquares();
        } else
          handleSelection(info.pos, getBuildInfo()[info.building], false);
        break; }
    case UserInputId::RECT_DESELECTION:
        updateSelectionSquares();
        if (rectSelection) {
          rectSelection->corner2 = input.get<Vec2>();
        } else
          rectSelection = CONSTRUCT(SelectionInfo, c.corner1 = c.corner2 = input.get<Vec2>(); c.deselect = true;);
        updateSelectionSquares();
        break;
    case UserInputId::BUILD: {
        auto& info = input.get<BuildingInfo>();
        handleSelection(info.pos, getBuildInfo()[info.building], false);
        break; }
    case UserInputId::VILLAGE_ACTION: 
        if (Collective* village = getVillain(input.get<VillageActionInfo>().villageIndex))
          switch (input.get<VillageActionInfo>().action) {
            case VillageAction::TRADE: 
              handleTrading(village);
              break;
            case VillageAction::PILLAGE:
              handlePillage(village);
              break;
          }
        break;
    case UserInputId::PAY_RANSOM:
        handleRansom(true);
        break;
    case UserInputId::IGNORE_RANSOM:
        handleRansom(false);
        break;
    case UserInputId::SHOW_HISTORY:
        PlayerMessage::presentMessages(getView(), messageHistory);
        break;
    case UserInputId::CHEAT_ATTRIBUTES:
        for (auto resource : ENUM_ALL(CollectiveResourceId))
          getCollective()->returnResource(CostInfo(resource, 1000));
        break;
    case UserInputId::RECT_CONFIRM:
        if (rectSelection) {
          selection = rectSelection->deselect ? DESELECT : SELECT;
          for (Vec2 v : Rectangle::boundingBox({rectSelection->corner1, rectSelection->corner2}))
            handleSelection(v, getBuildInfo()[input.get<BuildingInfo>().building], true, rectSelection->deselect);
        }
        FALLTHROUGH;
    case UserInputId::RECT_CANCEL:
        updateSelectionSquares();
        rectSelection = none;
        selection = NONE;
        break;
    case UserInputId::EXIT: getGame()->exitAction(); return;
    case UserInputId::IDLE: break;
    default: break;
  }
}

void PlayerControl::updateSelectionSquares() {
  if (rectSelection)
    for (Vec2 v : Rectangle::boundingBox({rectSelection->corner1, rectSelection->corner2}))
      Position(v, getLevel()).setNeedsRenderUpdate(true);
}

void PlayerControl::handleSelection(Vec2 pos, const BuildInfo& building, bool rectangle, bool deselectOnly) {
  Position position(pos, getLevel());
  for (auto& req : building.requirements)
    if (!meetsRequirement(req))
      return;
  if (position.isUnavailable())
    return;
  if (!deselectOnly && rectangle && !canSelectRectangle(building))
    return;
  switch (building.buildType) {
    case BuildInfo::TRAP:
        if (getCollective()->getConstructions().containsTrap(position) && selection != SELECT) {
          getCollective()->removeTrap(position);
          getView()->addSound(SoundId::DIG_UNMARK);
          selection = DESELECT;
          // Does this mean I can remove the order if the trap physically exists?
        } else
        if (position.canEnterEmpty({MovementTrait::WALK}) &&
            getCollective()->getTerritory().contains(position) &&
            !getCollective()->getConstructions().containsTrap(position) &&
            selection != DESELECT) {
          getCollective()->addTrap(position, building.trapInfo.type);
          getView()->addSound(SoundId::ADD_CONSTRUCTION);
          selection = SELECT;
        }
      break;
    case BuildInfo::DESTROY:
        for (auto layer : building.destroyLayers)
          if (getCollective()->getConstructions().containsFurniture(position, layer) &&
              !getCollective()->getConstructions().getFurniture(position, layer).isBuilt()) {
            getCollective()->removeFurniture(position, layer);
            getView()->addSound(SoundId::DIG_UNMARK);
            selection = SELECT;
          } else
          if (getCollective()->getKnownTiles().isKnown(position) && !position.isBurning()) {
            selection = SELECT;
            getCollective()->destroySquare(position, layer);
            getView()->addSound(SoundId::REMOVE_CONSTRUCTION);
            updateSquareMemory(position);
          }
        break;
    case BuildInfo::FORBID_ZONE:
        if (position.isTribeForbidden(getTribeId()) && selection != SELECT) {
          position.allowMovementForTribe(getTribeId());
          selection = DESELECT;
        }
        else if (!position.isTribeForbidden(getTribeId()) && selection != DESELECT) {
          position.forbidMovementForTribe(getTribeId());
          selection = SELECT;
        }
        break;
    case BuildInfo::TORCH:
        if (getCollective()->isPlannedTorch(position) && selection != SELECT) {
          getCollective()->removeTorch(position);
          getView()->addSound(SoundId::DIG_UNMARK);
          selection = DESELECT;
        }
        else if (getCollective()->canPlaceTorch(position) && selection != DESELECT) {
          getCollective()->addTorch(position);
          selection = SELECT;
          getView()->addSound(SoundId::ADD_CONSTRUCTION);
        }
        break;
    case BuildInfo::DIG: {
        bool markedToDig = getCollective()->isMarked(position) &&
            (getCollective()->getMarkHighlight(position) == HighlightType::DIG ||
             getCollective()->getMarkHighlight(position) == HighlightType::CUT_TREE);
        if (markedToDig && selection != SELECT) {
          getCollective()->cancelMarkedTask(position);
          getView()->addSound(SoundId::DIG_UNMARK);
          selection = DESELECT;
        } else
        if (!markedToDig && selection != DESELECT) {
          if (auto furniture = position.getFurniture(FurnitureLayer::MIDDLE))
            for (auto type : {DestroyAction::Type::CUT, DestroyAction::Type::DIG})
              if (furniture->canDestroy(type)) {
                getCollective()->orderDestruction(position, type);
                getView()->addSound(SoundId::DIG_MARK);
                selection = SELECT;
                break;
              }
        }
        break;
        }
    case BuildInfo::ZONE:
        if (getCollective()->getZones().isZone(position, building.zone) && selection != SELECT) {
          getCollective()->getZones().eraseZone(position, building.zone);
          selection = DESELECT;
        } else if (selection != DESELECT && !getCollective()->getZones().isZone(position, building.zone) &&
            getCollective()->getKnownTiles().isKnown(position)) {
          getCollective()->getZones().setZone(position, building.zone);
          selection = SELECT;
        }
        break;
    case BuildInfo::CLAIM_TILE:
        if (getCollective()->canClaimSquare(position))
          getCollective()->claimSquare(position);
        break;
    case BuildInfo::DISPATCH:
        getCollective()->setPriorityTasks(position);
        break;
    case BuildInfo::FURNITURE: {
        auto& info = building.furnitureInfo;
        auto layer = Furniture::getLayer(info.type);
        if (getCollective()->getConstructions().containsFurniture(position, layer) &&
            !getCollective()->getConstructions().getFurniture(position, layer).isBuilt()) {
          if (selection != SELECT) {
            getCollective()->removeFurniture(position, layer);
            selection = DESELECT;
            getView()->addSound(SoundId::DIG_UNMARK);
          }
        } else
        if (getCollective()->canAddFurniture(position, info.type) && selection != DESELECT
            && (!info.maxNumber || *info.maxNumber >
                getCollective()->getConstructions().getTotalCount(info.type))) {
          CostInfo cost = info.cost;
          getCollective()->addFurniture(position, info.type, cost, info.noCredit);
          getCollective()->updateResourceProduction();
          selection = SELECT;
          getView()->addSound(SoundId::ADD_CONSTRUCTION);
        }
      }
      break;
  }
}

void PlayerControl::onSquareClick(Position pos) {
  if (getCollective()->getTerritory().contains(pos))
    if (auto furniture = pos.getFurniture(FurnitureLayer::MIDDLE)) {
      if (furniture->isClickable()) {
        furniture->click(pos); // this can remove the furniture
        updateSquareMemory(pos);
      } else {
        if (auto workshopType = CollectiveConfig::getWorkshopType(furniture->getType()))
          setChosenWorkshop(*workshopType);
        if (furniture->getType() == FurnitureType::BOOK_SHELF)
          handleLibrary(getView());
      }
    }
}

double PlayerControl::getLocalTime() const {
  return getModel()->getLocalTime();
}

bool PlayerControl::isPlayerView() const {
  return false;
}

const Creature* PlayerControl::getKeeper() const {
  return getCollective()->getLeader();
}

Creature* PlayerControl::getKeeper() {
  return getCollective()->getLeader();
}

void PlayerControl::addToMemory(Position pos) {
  if (!pos.needsMemoryUpdate())
    return;
  pos.setNeedsMemoryUpdate(false);
  ViewIndex index;
  getSquareViewIndex(pos, true, index);
  memory->update(pos, index);
}

void PlayerControl::checkKeeperDanger() {
  Creature* controlled = getControlled();
  Creature* keeper = getKeeper();
  auto prompt = [&] {
      return getView()->yesOrNoPrompt("The keeper is in trouble. Do you want to control " +
          keeper->getAttributes().getGender().him() + "?");
  };
  if (!getKeeper()->isDead() && controlled != getKeeper()) { 
    if ((getKeeper()->wasInCombat(5) || getKeeper()->getBody().isWounded())
        && lastControlKeeperQuestion < getCollective()->getGlobalTime() - 50) {
      lastControlKeeperQuestion = getCollective()->getGlobalTime();
      if (prompt()) {
        controlSingle(getKeeper());
        return;
      }
    }
    if (getKeeper()->isAffected(LastingEffect::POISON)
        && lastControlKeeperQuestion < getCollective()->getGlobalTime() - 5) {
      lastControlKeeperQuestion = getCollective()->getGlobalTime();
      if (prompt()) {
        controlSingle(getKeeper());
        return;
      }
    }
  }
}

void PlayerControl::onNoEnemies() {
  getGame()->setCurrentMusic(MusicType::PEACEFUL, false);
}

void PlayerControl::considerNightfallMessage() {
  /*if (getGame()->getSunlightInfo().getState() == SunlightState::NIGHT) {
    if (!isNight) {
      addMessage(PlayerMessage("Night is falling. Killing enemies in their sleep yields double mana.",
            MessagePriority::HIGH));
      isNight = true;
    }
  } else
    isNight = false;*/
}

void PlayerControl::update(bool currentlyActive) {
  updateVisibleCreatures();
  vector<Creature*> addedCreatures;
  vector<Level*> currentLevels {getLevel()};
  if (Creature* c = getControlled())
    if (!contains(currentLevels, c->getLevel()))
      currentLevels.push_back(c->getLevel());
  for (Level* l : currentLevels)
    for (Creature* c : l->getAllCreatures()) 
      if (c->getTribeId() == getTribeId() && canSee(c) && !isEnemy(c)) {
        if (c->getAttributes().getSpawnType() && !contains(getCreatures(), c) && !getCollective()->wasBanished(c)) {
          addedCreatures.push_back(c);
          getCollective()->addCreature(c, {MinionTrait::FIGHTER});
          if (Creature* controlled = getControlled())
            if ((getCollective()->hasTrait(controlled, MinionTrait::FIGHTER)
                  || controlled == getCollective()->getLeader())
                && c->getPosition().isSameLevel(controlled->getPosition()))
              for (auto team : getTeams().getActive(controlled)) {
                getTeams().add(team, c);
                controlled->playerMessage(PlayerMessage(c->getName().a() + " joins your team.",
                      MessagePriority::HIGH));
                break;
              }
        } else  
          if (c->getBody().isMinionFood() && !contains(getCreatures(), c))
            getCollective()->addCreature(c, {MinionTrait::FARM_ANIMAL, MinionTrait::NO_LIMIT});
      }
  if (!addedCreatures.empty()) {
    getCollective()->addNewCreatureMessage(addedCreatures);
  }
}

bool PlayerControl::isConsideredAttacking(const Creature* c, const Collective* enemy) {
  if (enemy && enemy->getModel() == getModel())
    return canSee(c) && contains(getCollective()->getTerritory().getStandardExtended(), c->getPosition());
  else
    return canSee(c) && c->getLevel() == getLevel();
}

const double messageTimeout = 80;

void PlayerControl::tick() {
  for (auto& elem : messages)
    elem.setFreshness(max(0.0, elem.getFreshness() - 1.0 / messageTimeout));
  messages = filter(messages, [&] (const PlayerMessage& msg) {
      return msg.getFreshness() > 0; });
  considerNightfallMessage();
  if (auto msg = getCollective()->getWarnings().getNextWarning(getLocalTime()))
    addMessage(PlayerMessage(*msg, MessagePriority::HIGH));
  checkKeeperDanger();
  for (auto attack : copyOf(ransomAttacks))
    for (const Creature* c : attack.getCreatures())
      if (getCollective()->getTerritory().contains(c->getPosition())) {
        removeElement(ransomAttacks, attack);
        break;
      }
  for (auto attack : copyOf(newAttacks))
    for (const Creature* c : attack.getCreatures())
      if (isConsideredAttacking(c, attack.getAttacker())) {
        addMessage(PlayerMessage("You are under attack by " + attack.getAttackerName() + "!",
            MessagePriority::CRITICAL).setPosition(c->getPosition()));
        getGame()->setCurrentMusic(MusicType::BATTLE, true);
        removeElement(newAttacks, attack);
        if (auto attacker = attack.getAttacker())
          getCollective()->addKnownVillain(attacker);
        if (attack.getRansom())
          ransomAttacks.push_back(attack);
        break;
      }
  double time = getCollective()->getLocalTime();
  if (getGame()->getOptions()->getBoolValue(OptionId::HINTS) && time > hintFrequency) {
    int numHint = int(time) / hintFrequency - 1;
    if (numHint < hints.size() && !hints[numHint].empty()) {
      addMessage(PlayerMessage(hints[numHint], MessagePriority::HIGH));
      hints[numHint] = "";
    }
  }
}

bool PlayerControl::canSee(const Creature* c) const {
  return canSee(c->getPosition());
}

bool PlayerControl::canSee(Position pos) const {
  if (getGame()->getOptions()->getBoolValue(OptionId::SHOW_MAP))
    return true;
  if (visibilityMap->isVisible(pos))
    return true;
  for (Position v : getCollective()->getConstructions().getBuiltPositions(FurnitureType::EYEBALL))
    if (pos.isSameLevel(v) && getLevel()->canSee(v.getCoord(), pos.getCoord(), VisionId::NORMAL))
      return true;
  return false;
}

TribeId PlayerControl::getTribeId() const {
  return getCollective()->getTribeId();
}

bool PlayerControl::isEnemy(const Creature* c) const {
  return getKeeper() && getKeeper()->isEnemy(c);
}

void PlayerControl::onMemberKilled(const Creature* victim, const Creature* killer) {
  if (victim->isPlayer())
    onControlledKilled();
  visibilityMap->remove(victim);
  if (victim == getKeeper() && !getGame()->isGameOver()) {
    getGame()->gameOver(victim, getCollective()->getKills().getSize(), "enemies",
        getCollective()->getDangerLevel() + getCollective()->getPoints());
  }
}

void PlayerControl::onMemberAdded(const Creature* c) {
  updateMinionVisibility(c);
}

Level* PlayerControl::getLevel() const {
  return getCollective()->getLevel();
}

Model* PlayerControl::getModel() const {
  return getLevel()->getModel();
}

Game* PlayerControl::getGame() const {
  return getLevel()->getModel()->getGame();
}

View* PlayerControl::getView() const {
  return getGame()->getView();
}

void PlayerControl::addAttack(const CollectiveAttack& attack) {
  newAttacks.push_back(attack);
}

void PlayerControl::updateSquareMemory(Position pos) {
  ViewIndex index;
  pos.getViewIndex(index, getCollective()->getLeader()); // use the leader as a generic viewer
  memory->update(pos, index);
}

void PlayerControl::onConstructed(Position pos, FurnitureType type) {
  //updateSquareMemory(pos);
}

void PlayerControl::onClaimedSquare(Position position) {
  position.modViewObject().setId(ViewId::KEEPER_FLOOR);
  position.setNeedsRenderUpdate(true);
  updateSquareMemory(position);
}

void PlayerControl::onDestructed(Position pos, const DestroyAction& action) {
  if (action.getType() == DestroyAction::Type::DIG) {
    Vec2 visRadius(3, 3);
    for (Position v : pos.getRectangle(Rectangle(-visRadius, visRadius + Vec2(1, 1)))) {
      getCollective()->addKnownTile(v);
      updateSquareMemory(v);
    }
    pos.modViewObject().setId(ViewId::KEEPER_FLOOR);
    pos.setNeedsRenderUpdate(true);
  }
}

void PlayerControl::updateVisibleCreatures() {
  visibleEnemies.clear();
  for (const Creature* c : getLevel()->getAllCreatures()) 
    if (canSee(c) && isEnemy(c))
        visibleEnemies.push_back(c->getPosition().getCoord());
}

vector<Vec2> PlayerControl::getVisibleEnemies() const {
  return visibleEnemies;
}

template <class Archive>
void PlayerControl::registerTypes(Archive& ar, int version) {
  REGISTER_TYPE(ar, MinionController);
}

REGISTER_TYPES(PlayerControl::registerTypes);

