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

template <class Archive> 
void PlayerControl::serialize(Archive& ar, const unsigned int version) {
  ar& SUBCLASS(CollectiveControl);
  serializeAll(ar, memory, showWelcomeMsg, lastControlKeeperQuestion, startImpNum, payoutWarning);
  serializeAll(ar, surprises, newAttacks, ransomAttacks, messages, hints, visibleEnemies, knownLocations);
  serializeAll(ar, knownVillains, knownVillainLocations, visibilityMap, warningTimes);
  serializeAll(ar, lastWarningTime, messageHistory, eventProxy);
}

SERIALIZABLE(PlayerControl);

SERIALIZATION_CONSTRUCTOR_IMPL(PlayerControl);

typedef Collective::ResourceId ResourceId;

struct PlayerControl::BuildInfo {
  struct SquareInfo {
    SquareType type;
    CostInfo cost;
    string name;
    bool buildImmediatly;
    bool noCredit;
    double costExponent;
    optional<int> maxNumber;
  } squareInfo;

  struct TrapInfo {
    TrapType type;
    string name;
    ViewId viewId;
  } trapInfo;

  enum BuildType {
    DIG,
    SQUARE,
    IMP,
    TRAP,
    DESTROY,
    FETCH,
    DISPATCH,
    CLAIM_TILE,
    FORBID_ZONE,
    TORCH,
  } buildType;

  vector<Requirement> requirements;
  string help;
  char hotkey;
  string groupName;
  bool hotkeyOpensGroup = false;

  BuildInfo(SquareInfo info, vector<Requirement> req = {}, const string& h = "", char key = 0, string group = "",
      bool hotkeyOpens = false) : squareInfo(info), buildType(SQUARE), requirements(req), help(h), hotkey(key),
      groupName(group), hotkeyOpensGroup(hotkeyOpens) {}

  BuildInfo(TrapInfo info, vector<Requirement> = {}, const string& h = "", char hotkey = 0,
      string group = "");
  BuildInfo(const Creature*, CostInfo, const string& groupName, const string& h = "", char hotkey = 0);
  BuildInfo(BuildType type, const string& h = "", char hotkey = 0, string group = "");
};

PlayerControl::BuildInfo::BuildInfo(TrapInfo info, vector<Requirement> req, const string& h, char key, string group)
    : trapInfo(info), buildType(TRAP), requirements(req), help(h), hotkey(key), groupName(group) {}

PlayerControl::BuildInfo::BuildInfo(const Creature* c, CostInfo cost, const string& group, const string& h, char key)
    : squareInfo({SquareType(SquareId::CREATURE_ALTAR, c), cost, "To " + c->getName().bare(), false}),
    buildType(SQUARE), help(h), hotkey(key), groupName(group) {}

PlayerControl::BuildInfo::BuildInfo(BuildType type, const string& h, char key, string group)
    : buildType(type), help(h), hotkey(key), groupName(group) {
  CHECK(contains({DIG, IMP, DESTROY, FORBID_ZONE, FETCH, DISPATCH, CLAIM_TILE, TORCH}, type));
}

vector<PlayerControl::BuildInfo> PlayerControl::getBuildInfo() const {
  return getBuildInfo(getTribeId());
}

vector<PlayerControl::BuildInfo> PlayerControl::getBuildInfo(TribeId tribe) {
  const CostInfo altarCost {ResourceId::STONE, 30};
  const string workshop = "Manufactories";
  vector<BuildInfo> buildInfo {
    BuildInfo(BuildInfo::DIG, "", 'd'),
    BuildInfo({SquareId::MOUNTAIN, {ResourceId::STONE, 50}, "Fill up tunnel"}, {},
        "Fill up one tile at a time. Cutting off an area is not allowed."),
    BuildInfo({SquareId::STOCKPILE, {ResourceId::GOLD, 0}, "Everything", true}, {},
        "All possible items in your dungeon can be stored here.", 's', "Storage", true),
    BuildInfo({SquareId::STOCKPILE_EQUIP, {ResourceId::GOLD, 0}, "Equipment", true}, {},
        "All equipment for your minions can be stored here.", 0, "Storage"),
    BuildInfo({SquareId::STOCKPILE_RES, {ResourceId::GOLD, 0}, "Resources", true}, {},
        "Only wood, iron and granite can be stored here.", 0, "Storage"),
    BuildInfo({SquareId::LIBRARY, {ResourceId::WOOD, 20}, "Library"}, {},
        "Mana is regenerated here.", 'y'),
    BuildInfo({SquareId::THRONE, {ResourceId::GOLD, 800}, "Throne", false, false, 0, 1},
        {{RequirementId::VILLAGE_CONQUERED}},
        "Increases population limit by " + toString(ModelBuilder::getThronePopulationIncrease())),
    BuildInfo({SquareId::TREASURE_CHEST, {ResourceId::WOOD, 5}, "Treasure room"}, {},
        "Stores gold."),
    BuildInfo({Collective::getHatcheryType(tribe),
        {ResourceId::WOOD, 20}, "Pigsty"}, {{RequirementId::TECHNOLOGY, TechId::PIGSTY}},
        "Increases minion population limit by up to " +
            toString(ModelBuilder::getPigstyPopulationIncrease()) + ".", 'p'),
    BuildInfo({SquareId::DORM, {ResourceId::WOOD, 10}, "Dormitory"}, {},
        "Humanoid minions place their beds here.", 'm'),
    BuildInfo({SquareId::TRAINING_ROOM, {ResourceId::IRON, 20}, "Training room"}, {},
        "Used to level up your minions.", 't'),
    BuildInfo({SquareId::WORKSHOP, {ResourceId::WOOD, 20}, "Workshop"},
        {{RequirementId::TECHNOLOGY, TechId::CRAFTING}},
        "Produces leather equipment, traps, first-aid kits and other.", 'w', workshop),
    BuildInfo({SquareId::FORGE, {ResourceId::IRON, 15}, "Forge"},
        {{RequirementId::TECHNOLOGY, TechId::IRON_WORKING}}, "Produces iron weapons and armor.", 'f', workshop),
    BuildInfo({SquareId::LABORATORY, {ResourceId::STONE, 15}, "Laboratory"},
        {{RequirementId::TECHNOLOGY, TechId::ALCHEMY}}, "Produces magical potions.", 'r', workshop),
    BuildInfo({SquareId::JEWELER, {ResourceId::WOOD, 20}, "Jeweler"},
        {{RequirementId::TECHNOLOGY, TechId::JEWELLERY}}, "Produces magical rings and amulets.", 'j', workshop),
    BuildInfo({SquareId::RITUAL_ROOM, {ResourceId::MANA, 15}, "Ritual room"}, {},
        "Summons various demons to your dungeon."),
    BuildInfo({SquareId::BEAST_LAIR, {ResourceId::WOOD, 12}, "Beast lair"}, {},
        "Beasts have their cages here."),
    BuildInfo({SquareId::CEMETERY, {ResourceId::STONE, 20}, "Graveyard"}, {},
        "Corpses are dragged here. Sometimes an undead creature makes their home here.", 'v'),
    BuildInfo({SquareId::PRISON, {ResourceId::IRON, 20}, "Prison"}, {},
        "Captured enemies are kept here.", 0),
    BuildInfo({SquareId::TORTURE_TABLE, {ResourceId::IRON, 20}, "Torture room"}, {},
        "Can be used to torture prisoners.", 'u'),
    BuildInfo(BuildInfo::CLAIM_TILE, "Claim a tile. Building anything has the same effect.", 0, "Orders"),
    BuildInfo(BuildInfo::FETCH, "Order imps to fetch items from outside the dungeon.", 0, "Orders"),
    BuildInfo(BuildInfo::DISPATCH, "Click on an existing task to give it a high priority.", 'a', "Orders"),
    BuildInfo(BuildInfo::DESTROY, "", 'e', "Orders"),
    BuildInfo(BuildInfo::FORBID_ZONE, "Mark tiles to keep minions from entering.", 'b', "Orders"),
    BuildInfo({{SquareId::TRIBE_DOOR, tribe}, {ResourceId::WOOD, 5}, "Door"},
        {{RequirementId::TECHNOLOGY, TechId::CRAFTING}}, "Click on a built door to lock it.", 'o', "Installations"),
    BuildInfo({SquareId::BRIDGE, {ResourceId::WOOD, 20}, "Bridge"}, {},
      "Build it to pass over water or lava.", 0, "Installations"),
    BuildInfo({{SquareId::BARRICADE, tribe}, {ResourceId::WOOD, 20}, "Barricade"},
      {{RequirementId::TECHNOLOGY, TechId::CRAFTING}}, "", 0, "Installations"),
    BuildInfo(BuildInfo::TORCH, "Place it on tiles next to a wall.", 'c', "Installations"),
    BuildInfo({SquareId::KEEPER_BOARD, {ResourceId::WOOD, 80}, "Message board"}, {},
        "A board where you can leave a message for other players.", 0, "Installations"),
    BuildInfo({SquareId::EYEBALL, {ResourceId::MANA, 10}, "Eyeball"}, {},
      "Makes the area around it visible.", 0, "Installations"),
    BuildInfo({SquareId::MINION_STATUE, {ResourceId::GOLD, 300}, "Statue", false, false}, {},
      "Increases minion population limit by " +
            toString(ModelBuilder::getStatuePopulationIncrease()) + ".", 0, "Installations"),
    BuildInfo({SquareId::WHIPPING_POST, {ResourceId::WOOD, 30}, "Whipping post"}, {},
        "A place to whip your minions if they need a morale boost.", 0, "Installations"),
    BuildInfo({SquareId::IMPALED_HEAD, {ResourceId::PRISONER_HEAD, 1}, "Prisoner head", false, true}, {},
        "Impaled head of an executed prisoner. Aggravates enemies.", 0, "Installations"),
    BuildInfo({TrapType::TERROR, "Terror trap", ViewId::TERROR_TRAP}, {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Causes the trespasser to panic.", 0, "Traps"),
    BuildInfo({TrapType::POISON_GAS, "Gas trap", ViewId::GAS_TRAP}, {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Releases a cloud of poisonous gas.", 0, "Traps"),
    BuildInfo({TrapType::ALARM, "Alarm trap", ViewId::ALARM_TRAP}, {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Summons all minions", 0, "Traps"),
    BuildInfo({TrapType::WEB, "Web trap", ViewId::WEB_TRAP}, {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Immobilises the trespasser for some time.", 0, "Traps"),
    BuildInfo({TrapType::BOULDER, "Boulder trap", ViewId::BOULDER}, {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Causes a huge boulder to roll towards the enemy.", 0, "Traps"),
    BuildInfo({TrapType::SURPRISE, "Surprise trap", ViewId::SURPRISE_TRAP},
        {{RequirementId::TECHNOLOGY, TechId::TRAPS}},
        "Teleports nearby minions to deal with the trespasser.", 0, "Traps"),
  };
  return buildInfo;
}

vector<PlayerControl::BuildInfo> PlayerControl::libraryInfo {
  BuildInfo(BuildInfo::IMP, "Click on a visible square on the map to summon an imp.", 'i'),
};

vector<PlayerControl::BuildInfo> PlayerControl::minionsInfo {
};

vector<PlayerControl::RoomInfo> PlayerControl::getRoomInfo() {
  vector<RoomInfo> ret;
  for (BuildInfo bInfo : getBuildInfo(TribeId::getKeeper()))
    if (bInfo.buildType == BuildInfo::SQUARE)
      ret.push_back({bInfo.squareInfo.name, bInfo.help, bInfo.requirements});
  return ret;
}

typedef Collective::Warning Warning;

string PlayerControl::getWarningText(Collective::Warning w) {
  switch (w) {
    case Warning::DIGGING: return "Dig into the mountain and start building a dungeon.";
    case Warning::RESOURCE_STORAGE: return "Storage room for resources is needed.";
    case Warning::EQUIPMENT_STORAGE: return "Storage room for equipment is needed.";
    case Warning::LIBRARY: return "Build a library to start research.";
    case Warning::BEDS: return "You need to build a dormitory for your minions.";
    case Warning::TRAINING: return "Build a training room for your minions.";
    case Warning::NO_HATCHERY: return "You need to build a pigsty.";
    case Warning::WORKSHOP: return "Build a workshop to produce equipment and traps.";
    case Warning::NO_WEAPONS: return "You need weapons for your minions.";
    case Warning::LOW_MORALE: return "Kill some enemies or summon a succubus to increase morale of your minions.";
    case Warning::GRAVES: return "You need a graveyard to collect corpses";
    case Warning::CHESTS: return "You need to build a treasure room.";
    case Warning::NO_PRISON: return "You need to build a prison.";
    case Warning::LARGER_PRISON: return "You need a larger prison.";
    case Warning::TORTURE_ROOM: return "You need to build a torture room.";
//    case Warning::ALTAR: return "You need to build a shrine to sacrifice.";
    case Warning::MORE_CHESTS: return "You need a larger treasure room.";
    case Warning::MANA: return "Kill or torture some innocent beings for more mana.";
    case Warning::MORE_LIGHTS: return "Place some torches to light up your dungeon.";
  }
  return "";
}

static bool seeEverything = false;

const int hintFrequency = 700;
static vector<string> getHints() {
  return {
    "Research geology to uncover ores in the mountain.",
    "Morale affects minion productivity and chances of fleeing from battle.",
 //   "You can turn these hints off in the settings (F2).",
    "Killing a leader greatly lowers the morale of his tribe and stops immigration.",
    "Your minions' morale is boosted when they are commanded by the Keeper.",
  };
}

PlayerControl::PlayerControl(Collective* col, Level* level) : CollectiveControl(col),
    eventProxy(this, level->getModel()), hints(getHints()) {
  bool hotkeys[128] = {0};
  for (BuildInfo info : getBuildInfo(TribeId::getKeeper())) {
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
  for (Position v : level->getAllPositions())
    if (contains({"gold ore", "iron ore", "granite"}, v.getName()))
      memory->addObject(v, v.getViewObject());
  for(const Location* loc : level->getAllLocations())
    if (loc->isMarkedAsSurprise())
      surprises.insert(loc->getMiddle());
  //col->getGame()->getOptions()->addTrigger(OptionId::SHOW_MAP, [&] (bool val) { seeEverything = val; });
}

PlayerControl::~PlayerControl() {
}

const int basicImpCost = 20;

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
        getGame()->transferCreature(c, getCollective()->getLevel()->getModel());
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

ViewId PlayerControl::getResourceViewId(ResourceId id) const {
  switch (id) {
    case ResourceId::CORPSE:
    case ResourceId::PRISONER_HEAD:
    case ResourceId::MANA: return ViewId::MANA;
    case ResourceId::GOLD: return ViewId::GOLD;
    case ResourceId::WOOD: return ViewId::WOOD_PLANK;
    case ResourceId::IRON: return ViewId::IRON_ROCK;
    case ResourceId::STONE: return ViewId::ROCK;
  }
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

Creature* PlayerControl::getConsumptionTarget(View* view, Creature* consumer) {
  vector<Creature*> res;
  vector<ListElem> opt;
  for (Creature* c : getCollective()->getConsumptionTargets(consumer)) {
    res.push_back(c);
    opt.emplace_back(c->getName().bare() + ", level " + toString(c->getAttributes().getExpLevel()));
  }
  if (auto index = view->chooseFromList("Choose minion to absorb:", opt))
    return res[*index];
  return nullptr;
}

void PlayerControl::addConsumableItem(Creature* creature) {
  double scrollPos = 0;
  while (1) {
    const Item* chosenItem = chooseEquipmentItem(creature, {}, [&](const Item* it) {
        return !getCollective()->getMinionEquipment().isOwner(it, creature) && !it->canEquip()
        && getCollective()->getMinionEquipment().needs(creature, it, true); }, &scrollPos);
    if (chosenItem)
      getCollective()->ownItem(creature, chosenItem);
    else
      break;
  }
}

void PlayerControl::addEquipment(Creature* creature, EquipmentSlot slot) {
  vector<Item*> currentItems = creature->getEquipment().getItem(slot);
  const Item* chosenItem = chooseEquipmentItem(creature, currentItems, [&](const Item* it) {
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
      getCollective()->ownItem(creature, chosenItem);
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
    c.ids = transform2<UniqueEntity<Item>::Id>(stack, [](const Item* it) { return it->getUniqueId();});
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
    c.ids = transform2<UniqueEntity<Item>::Id>(stack, [](const Item* it) { return it->getUniqueId();});
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
  vector<Item*> ownedItems = getCollective()->getAllItems([this, creature](const Item* it) {
      return getCollective()->getMinionEquipment().isOwner(it, creature); });
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
      bool equiped = creature->getEquipment().isEquiped(item);
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
    double* scrollPos) {
  vector<Item*> availableItems;
  vector<Item*> usedItems;
  vector<Item*> allItems = getCollective()->getAllItems(predicate);
  getCollective()->sortByEquipmentValue(allItems);
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
        return " owned by " + c->getName().a() + " (level " + toString(c->getAttributes().getExpLevel()) + ")";});
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
  return 2 * getCollective()->getTechnologies().size();
}

void PlayerControl::handleLibrary(View* view) {
  if (getCollective()->getSquares(SquareId::LIBRARY).empty()) {
    view->presentText("", "You need to build a library to start research.");
    return;
  }
  vector<ListElem> options;
  bool allInactive = false;
  if (getCollective()->getSquares(SquareId::LIBRARY).size() <= getMinLibrarySize()) {
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

optional<pair<ViewId, int>> PlayerControl::getCostObj(CostInfo cost) const {
  if (cost.value > 0 && !CollectiveConfig::getResourceInfo().at(cost.id).dontDisplay)
    return make_pair(getResourceViewId(cost.id), cost.value);
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
      return "that at least one enemy village is conquered";
  }
}

static ViewId getSquareViewId(SquareType type) {
  static unordered_map<SquareType, ViewId> ids;
  if (!ids.count(type))
    ids.insert(make_pair(type, SquareFactory::get(type)->getViewObject().id()));
  return ids.at(type);
}

vector<Button> PlayerControl::fillButtons(const vector<BuildInfo>& buildInfo) const {
  vector<Button> buttons;
  for (BuildInfo button : buildInfo) {
    switch (button.buildType) {
      case BuildInfo::SQUARE: {
           BuildInfo::SquareInfo& elem = button.squareInfo;
           ViewId viewId = getSquareViewId(elem.type);
           string description;
           if (elem.cost.value > 0)
             if (int num = getCollective()->getSquares(elem.type).size())
               description = "[" + toString(num) + "]";
           int availableNow = !elem.cost.value ? 1 : getCollective()->numResource(elem.cost.id) / elem.cost.value;
           if (CollectiveConfig::getResourceInfo().at(elem.cost.id).dontDisplay && availableNow)
             description += " (" + toString(availableNow) + " available)";
           if (auto square = CollectiveConfig::getSecondarySquare(elem.type))
             viewId = getSquareViewId(*square);
           buttons.push_back({viewId, elem.name,
               getCostObj(getRoomCost(elem.type, elem.cost, elem.costExponent)),
               description,
               (elem.noCredit && !availableNow) ?
                  CollectiveInfo::Button::GRAY_CLICKABLE : CollectiveInfo::Button::ACTIVE });
           }
           break;
      case BuildInfo::DIG:
           buttons.push_back({ViewId::DIG_ICON, "Dig or cut tree", none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::FETCH:
           buttons.push_back({ViewId::FETCH_ICON, "Fetch items", none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::CLAIM_TILE:
           buttons.push_back({ViewId::KEEPER_FLOOR, "Claim tile", none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::DISPATCH:
           buttons.push_back({ViewId::IMP, "Prioritize task", none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::TRAP: {
             BuildInfo::TrapInfo& elem = button.trapInfo;
             int numTraps = getCollective()->getTrapItems(elem.type,
                 asVector<Position>(getCollective()->getSquares(SquareId::WORKSHOP))).size();
             buttons.push_back({elem.viewId, elem.name, none, "(" + toString(numTraps) + " ready)" });
           }
           break;
      case BuildInfo::IMP: {
           buttons.push_back({ViewId::IMP, "Summon imp", make_pair(ViewId::MANA, getImpCost()),
               "[" + toString(
                   getCollective()->getCreatures({MinionTrait::WORKER}, {MinionTrait::PRISONER}).size()) + "]",
               getImpCost() <= getCollective()->numResource(ResourceId::MANA) ?
                  CollectiveInfo::Button::ACTIVE : CollectiveInfo::Button::GRAY_CLICKABLE});
           break; }
      case BuildInfo::DESTROY:
           buttons.push_back({ViewId::DESTROY_BUTTON, "Remove construction", none, "",
                   CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::FORBID_ZONE:
           buttons.push_back({ViewId::FORBID_ZONE, "Forbid zone", none, "", CollectiveInfo::Button::ACTIVE});
           break;
      case BuildInfo::TORCH:
           buttons.push_back({ViewId::TORCH, "Torch", none, "", CollectiveInfo::Button::ACTIVE});
           break;
    }
    vector<string> unmetReqText;
    for (auto& req : button.requirements)
      if (!meetsRequirement(req)) {
        unmetReqText.push_back("Requires " + getRequirementText(req) + ".");
        buttons.back().state = CollectiveInfo::Button::INACTIVE;
      }
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
    case AttackTriggerId::GOLD: return "Gold";
    case AttackTriggerId::STOLEN_ITEMS: return "Item theft";
    case AttackTriggerId::ROOM_BUILT:
      switch (trigger.get<SquareType>().getId()) {
        case SquareId::THRONE: return "Throne";
        case SquareId::IMPALED_HEAD: return "Impaled heads";
        default: FAIL << "Unsupported ROOM_BUILT type"; return "";
      }
    case AttackTriggerId::POWER: return "Keeper's power";
    case AttackTriggerId::FINISH_OFF: return "Finishing off";
    case AttackTriggerId::ENEMY_POPULATION: return "Dungeon population";
    case AttackTriggerId::TIMER: return "Time";
    case AttackTriggerId::ENTRY: return "Entry";
    case AttackTriggerId::PROXIMITY: return "Proximity";
  }
}

VillageInfo::Village PlayerControl::getVillageInfo(const Collective* col) const {
  VillageInfo::Village info;
  info.name = col->getName().getShort();
  info.tribeName = col->getName().getRace();
  info.triggers.clear();
  if (getGame()->isSingleModel()) {
    if (!knownVillainLocations.count(col))
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
  } else if (hostile)
    info.state = info.HOSTILE;
  else {
    info.state = info.FRIENDLY;
    if (knownVillains.count(col)) {
      if (!col->getRecruits().empty())
        info.actions.push_back({VillageAction::RECRUIT});
      if (!col->getTradeItems().empty())
        info.actions.push_back({VillageAction::TRADE});
    } else if (getGame()->isVillainActive(col)){
      if (!col->getRecruits().empty())
        info.actions.push_back({VillageAction::RECRUIT,
            string("You must discover the location of the ally first.")});
      if (!col->getTradeItems().empty())
        info.actions.push_back({VillageAction::TRADE, string("You must discover the location of the ally first.")});
    }
  }
  return info;
}

void PlayerControl::handleRecruiting(Collective* ally) {
  double scrollPos = 0;
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
}

void PlayerControl::handleTrading(Collective* ally) {
  double scrollPos = 0;
  vector<Position> storage = getCollective()->getAllSquares(CollectiveConfig::getEquipmentStorage());
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
    vector<ItemInfo> itemInfo = transform2<ItemInfo>(items,
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
  if (!getGame()->isSingleModel())
    return getGame()->getVillains(type);
  else
    return filter(getGame()->getVillains(type), [this](Collective* c) {
        return seeEverything || knownVillains.count(c);});
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
      int l1 = c1->getAttributes().getExpLevel();
      int l2 = c2->getAttributes().getExpLevel();
      return l1 > l2 || (l1 == l2 && c1->getUniqueId() > c2->getUniqueId());
      });
}

vector<PlayerInfo> PlayerControl::getPlayerInfos(vector<Creature*> creatures) const {
  sortMinionsForUI(creatures);
  vector<PlayerInfo> minions;
  for (Creature* c : creatures) {
    minions.emplace_back();
    minions.back().readFrom(c);
    for (MinionTask t : ENUM_ALL(MinionTask))
      if (c->getAttributes().getMinionTasks().getValue(t, true) > 0) {
        minions.back().minionTasks.push_back({t,
            !getCollective()->isMinionTaskPossible(c, t),
            getCollective()->getMinionTask(c) == t,
            c->getAttributes().getMinionTasks().isLocked(t)});
      }
    minions.back().creatureId = c->getUniqueId();
    if (getCollective()->usesEquipment(c))
      fillEquipment(c, minions.back());
    if (getCollective()->hasTrait(c, MinionTrait::PRISONER))
      minions.back().actions = { PlayerInfo::EXECUTE, PlayerInfo::TORTURE };
    else {
      minions.back().actions = { PlayerInfo::CONTROL, PlayerInfo::RENAME };
      if (c != getCollective()->getLeader()) {
        minions.back().actions.push_back(PlayerInfo::BANISH);
        if (getCollective()->canWhip(c))
          minions.back().actions.push_back(PlayerInfo::WHIP);
      }
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
  info.minions = transform2<CreatureInfo>(minions, [](const Creature* c) { return CreatureInfo(c) ;});
  info.minionCount = getCollective()->getPopulationSize();
  info.minionLimit = getCollective()->getMaxPopulation();
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
  for (const Collective* col : getKnownVillains(VillainType::LESSER))
    gameInfo.villageInfo.villages.push_back(getVillageInfo(col));
  SunlightInfo sunlightInfo = getGame()->getSunlightInfo();
  gameInfo.sunlightInfo = { sunlightInfo.getText(), (int)sunlightInfo.getTimeRemaining() };
  gameInfo.infoType = GameInfo::InfoType::BAND;
  CollectiveInfo& info = gameInfo.collectiveInfo;
  info.buildings = fillButtons(getBuildInfo());
  info.libraryButtons = fillButtons(libraryInfo);
  fillMinions(info);
  info.chosen.reset();
  if (chosenCreature)
    if (Creature* c = getCreature(*chosenCreature)) {
      if (!getChosenTeam())
        info.chosen = {*chosenCreature, getPlayerInfos(getMinionsLike(c))};
      else
        info.chosen = {*chosenCreature, getPlayerInfos(getTeams().getMembers(*getChosenTeam())), *getChosenTeam()};
    }
  info.monsterHeader = "Minions: " + toString(info.minionCount) + " / " + toString(info.minionLimit);
  info.enemyGroups = getEnemyGroups();
  info.numResource.clear();
  for (auto elem : CollectiveConfig::getResourceInfo())
    if (!elem.second.dontDisplay)
      info.numResource.push_back(
          {getResourceViewId(elem.first), getCollective()->numResourcePlusDebt(elem.first), elem.second.name});
  info.warning = "";
  gameInfo.time = getCollective()->getGame()->getGlobalTime();
  gameInfo.modifiedSquares = gameInfo.totalSquares = 0;
  for (Collective* col : getCollective()->getGame()->getCollectives()) {
    gameInfo.modifiedSquares += col->getLevel()->getNumModifiedSquares();
    gameInfo.totalSquares += col->getLevel()->getBounds().area();
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
    info.ransom = {make_pair(ViewId::GOLD, *elem.getRansom()), elem.getAttacker()->getName().getFull(),
        getCollective()->hasResource({ResourceId::GOLD, *elem.getRansom()})};
    break;
  }
}

void PlayerControl::addMessage(const PlayerMessage& msg) {
  messages.push_back(msg);
  messageHistory.push_back(msg);
}

void PlayerControl::addImportantLongMessage(const string& msg, optional<Position> pos) {
  if (Creature* c = getControlled())
    c->playerMessage(PlayerMessage(msg, MessagePriority::CRITICAL));
  for (string s : removeEmpty(split(msg, {'.'}))) {
    trim(s);
    auto msg = PlayerMessage(s, MessagePriority::CRITICAL);
    if (pos)
      msg.setPosition(*pos);
    addMessage(msg);
    getView()->stopClock();
  }
}

void PlayerControl::initialize() {
  for (Creature* c : getCreatures())
    onEvent({EventId::MOVED, c});
}

void PlayerControl::onEvent(const GameEvent& event) {
  switch (event.getId()) {
    case EventId::MOVED: {
        Creature* c = event.get<Creature*>();
        if (contains(getCreatures(), c)) {
          vector<Position> visibleTiles = c->getVisibleTiles();
          visibilityMap->update(c, visibleTiles);
          for (Position pos : visibleTiles) {
            if (getCollective()->addKnownTile(pos))
              updateKnownLocations(pos);
            addToMemory(pos);
          }
        }
      }
      break;
    case EventId::PICKED_UP: {
        auto info = event.get<EventInfo::ItemsHandled>();
        if (info.creature == getControlled() && !getCollective()->hasTrait(info.creature, MinionTrait::WORKER))
          getCollective()->ownItems(info.creature, info.items);
      }
      break;
    case EventId::CONQUERED_ENEMY: {
        Collective* col = event.get<Collective*>();
        if (col->getVillainType() == VillainType::MAIN || col->getVillainType() == VillainType::LESSER)
          addImportantLongMessage("The tribe of " + col->getName().getFull() + " is destroyed.");
      }
      break;
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
            getCollective()->acquireTech(tech, true);
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
      knownVillains.insert(col);
      knownVillainLocations.insert(col);
    }
}


const MapMemory& PlayerControl::getMemory() const {
  return *memory;
}

ViewObject PlayerControl::getTrapObject(TrapType type, bool armed) {
  for (const PlayerControl::BuildInfo& info : getBuildInfo(TribeId::getKeeper()))
    if (info.buildType == BuildInfo::TRAP && info.trapInfo.type == type) {
      if (!armed)
        return ViewObject(info.trapInfo.viewId, ViewLayer::LARGE_ITEM, "Unarmed " + Item::getTrapName(type) + " trap")
          .setModifier(ViewObject::Modifier::PLANNED);
      else
        return ViewObject(info.trapInfo.viewId, ViewLayer::LARGE_ITEM, Item::getTrapName(type) + " trap");
    }
  FAIL << "trap not found" << int(type);
  return ViewObject(ViewId::EMPTY, ViewLayer::LARGE_ITEM);
}

static const ViewObject& getConstructionObject(SquareType type) {
  static unordered_map<SquareType, ViewObject> objects;
  if (!objects.count(type)) {
    objects.insert(make_pair(type, SquareFactory::get(type)->getViewObject()));
    objects.at(type).setModifier(ViewObject::Modifier::PLANNED);
  }
  return objects.at(type);
}

void PlayerControl::getSquareViewIndex(Position pos, bool canSee, ViewIndex& index) const {
  if (canSee)
    pos.getViewIndex(index, getCollective()->getLeader()); // use the leader as a generic viewer
  else
    index.setHiddenId(pos.getViewObject().id());
  if (const Creature* c = pos.getCreature())
    if (canSee)
      index.insert(c->getViewObject());
}

void PlayerControl::getViewIndex(Vec2 pos, ViewIndex& index) const {
  Position position(pos, getCollective()->getLevel());
  bool canSeePos = canSee(position);
  getSquareViewIndex(position, canSeePos, index);
  if (!canSeePos)
    if (auto memIndex = getMemory().getViewIndex(position))
    index.mergeFromMemory(*memIndex);
  if (getCollective()->isMarked(position))
    index.setHighlight(getCollective()->getMarkHighlight(position));
  if (getCollective()->hasPriorityTasks(position))
    index.setHighlight(HighlightType::PRIORITY_TASK);
  if (position.isTribeForbidden(getTribeId()))
    index.setHighlight(HighlightType::FORBIDDEN_ZONE);
  if (rectSelection
      && pos.inRectangle(Rectangle::boundingBox({rectSelection->corner1, rectSelection->corner2})))
    index.setHighlight(rectSelection->deselect ? HighlightType::RECT_DESELECTION : HighlightType::RECT_SELECTION);
  const ConstructionMap& constructions = getCollective()->getConstructions();
  if (!index.hasObject(ViewLayer::LARGE_ITEM)) {
    if (constructions.containsTrap(position))
      index.insert(getTrapObject(constructions.getTrap(position).getType(),
            constructions.getTrap(position).isArmed()));
    if (constructions.containsSquare(position) && !constructions.getSquare(position).isBuilt())
      index.insert(getConstructionObject(constructions.getSquare(position).getSquareType()));
  }
  if (constructions.containsTorch(position) && !constructions.getTorch(position).isBuilt())
    index.insert(copyOf(Trigger::getTorchViewObject(constructions.getTorch(position).getAttachmentDir()))
        .setModifier(ViewObject::Modifier::PLANNED));
  if (surprises.count(position) && !getCollective()->isKnownSquare(position))
    index.insert(ViewObject(ViewId::UNKNOWN_MONSTER, ViewLayer::CREATURE, "Surprise"));
  if (getCollective()->hasEfficiency(position) && index.hasObject(ViewLayer::FLOOR))
    index.getObject(ViewLayer::FLOOR).setAttribute(
        ViewObject::Attribute::EFFICIENCY, getCollective()->getEfficiency(position));
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

enum Selection { SELECT, DESELECT, NONE } selection = NONE;

CostInfo PlayerControl::getRoomCost(SquareType type, CostInfo baseCost, double exponent) const {
  return {baseCost.id, int(baseCost.value * pow(2, exponent * 
        getCollective()->getConstructions().getSquareCount(type)))};
}

int PlayerControl::getImpCost() const {
  int numImps = 0;
  for (Creature* c : getCollective()->getCreatures({MinionTrait::WORKER}, {MinionTrait::PRISONER}))
    ++numImps;
  if (numImps < startImpNum)
    return 0;
  return basicImpCost * pow(2, double(numImps - startImpNum) / 5);
}

class MinionController : public Player {
  public:
  MinionController(Creature* c, Model* m, MapMemory* memory, PlayerControl* ctrl)
      : Player(c, m, false, memory), control(ctrl) {}

  virtual bool swapTeam() override {
    return control->swapTeam();
  }

  virtual bool unpossess() override {
    control->leaveControl();
    return true;
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

bool PlayerControl::canBuildDoor(Position pos) const {
  if (!pos.canConstruct({SquareId::TRIBE_DOOR, getTribeId()}))
    return false;
  Rectangle innerRect = getLevel()->getBounds().minusMargin(1);
  auto wallFun = [=](Position pos) {
      return pos.canConstruct(SquareId::FLOOR) ||
          !pos.getCoord().inRectangle(innerRect); };
  return !getCollective()->getConstructions().containsTrap(pos) && pos.getCoord().inRectangle(innerRect) && 
      ((wallFun(pos.minus(Vec2(0, 1))) && wallFun(pos.minus(Vec2(0, -1)))) ||
       (wallFun(pos.minus(Vec2(1, 0))) && wallFun(pos.minus(Vec2(-1, 0)))));
}

bool PlayerControl::canPlacePost(Position pos) const {
  return !getCollective()->getConstructions().containsTrap(pos) &&
      pos.canEnterEmpty({MovementTrait::WALK}) && getCollective()->isKnownSquare(pos);
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
  c->pushController(PController(new MinionController(c, getModel(), memory.get(), this)));
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
    if (getCollective()->isKnownSquare(v))
      visible.push_back(v.getCoord());
  CHECK(!visible.empty());
  getView()->setScrollPos(Rectangle::boundingBox(visible).middle());
}

Collective* PlayerControl::getVillain(int num) {
  return concat(getKnownVillains(VillainType::MAIN), getKnownVillains(VillainType::LESSER))[num];
}

optional<TeamId> PlayerControl::getChosenTeam() const {
  if (chosenTeam && getTeams().exists(*chosenTeam))
    return chosenTeam;
  else
    return none;
}

void PlayerControl::setChosenTeam(optional<TeamId> team) {
  chosenTeam = team;
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
    case UserInputId::CANCEL_TEAM:
        if (getChosenTeam() == input.get<TeamId>()) {
          setChosenTeam(none);
          chosenCreature = none;
        }
        getTeams().cancel(input.get<TeamId>());
        break;
    case UserInputId::SELECT_TEAM:
        if (getChosenTeam() == input.get<TeamId>()) {
          setChosenTeam(none);
          chosenCreature = none;
        } else {
          setChosenTeam(input.get<TeamId>());
          chosenCreature = getTeams().getLeader(input.get<TeamId>())->getUniqueId();
        }
        break;
    case UserInputId::ACTIVATE_TEAM:
        if (!getTeams().isActive(input.get<TeamId>()))
          getTeams().activate(input.get<TeamId>());
        else
          getTeams().deactivate(input.get<TeamId>());
        break;
    case UserInputId::TILE_CLICK: {
        Vec2 pos = input.get<Vec2>();
        if (pos.inRectangle(getLevel()->getBounds()))
          tryLockingDoor(Position(pos, getLevel()));
        break;
        }

/*    case UserInputId::MOVE_TO:
        if (getCurrentTeam() && getTeams().isActive(*getCurrentTeam()) &&
            getCollective()->isKnownSquare(Position(input.get<Vec2>(), getLevel()))) {
          getCollective()->freeTeamMembers(*getCurrentTeam());
          getCollective()->setTask(getTeams().getLeader(*getCurrentTeam()),
              Task::goTo(Position(input.get<Vec2>(), getLevel())), true);
 //         view->continueClock();
        }
        break;*/
    case UserInputId::DRAW_LEVEL_MAP: view->drawLevelMap(this); break;
    case UserInputId::DRAW_WORLD_MAP: getGame()->presentWorldmap(); break;
    case UserInputId::TECHNOLOGY: getTechInfo()[input.get<int>()].butFun(this, view); break;
    case UserInputId::CREATURE_GROUP_BUTTON: 
        if (Creature* c = getCreature(input.get<Creature::Id>()))
          if (!chosenCreature || getChosenTeam() || !getCreature(*chosenCreature) ||
              getCreature(*chosenCreature)->getName().stack() != c->getName().stack()) {
            setChosenTeam(none);
            chosenCreature = input.get<Creature::Id>();
            break;
          }
        setChosenTeam(none);
        chosenCreature = none;
        break;
    case UserInputId::CREATURE_BUTTON:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          chosenCreature = input.get<Creature::Id>();
          if (getChosenTeam() && !getTeams().contains(*getChosenTeam(), c))
            setChosenTeam(none);
        } else {
          chosenCreature = none;
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
                    chosenCreature = like[i + 1]->getUniqueId();
                  else
                    chosenCreature = like[like.size() - 2]->getUniqueId();
                  break;
                }
            getCollective()->banishCreature(c);
          }
        break;
    case UserInputId::CREATURE_WHIP:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          getCollective()->orderWhipping(c);
          chosenCreature = none;
        }
        break;
    case UserInputId::CREATURE_EXECUTE:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          getCollective()->orderExecution(c);
          chosenCreature = none;
        }
        break;
    case UserInputId::CREATURE_TORTURE:
        if (Creature* c = getCreature(input.get<Creature::Id>())) {
          getCollective()->orderTorture(c);
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
                chosenCreature = getTeams().getLeader(info.team)->getUniqueId();
            } else
              chosenCreature = none;
          }
        break; }
    case UserInputId::RECT_SELECTION:
        updateSelectionSquares();
        if (rectSelection) {
          rectSelection->corner2 = input.get<Vec2>();
        } else
          rectSelection = CONSTRUCT(SelectionInfo, c.corner1 = c.corner2 = input.get<Vec2>(););
        updateSelectionSquares();
        break;
    case UserInputId::RECT_DESELECTION:
        updateSelectionSquares();
        if (rectSelection) {
          rectSelection->corner2 = input.get<Vec2>();
        } else
          rectSelection = CONSTRUCT(SelectionInfo, c.corner1 = c.corner2 = input.get<Vec2>(); c.deselect = true;);
        updateSelectionSquares();
        break;
    case UserInputId::BUILD:
        handleSelection(input.get<BuildingInfo>().pos,
            getBuildInfo()[input.get<BuildingInfo>().building], false);
        break;
    case UserInputId::LIBRARY:
        handleSelection(input.get<BuildingInfo>().pos, libraryInfo[input.get<BuildingInfo>().building], false);
        break;
    case UserInputId::VILLAGE_ACTION: 
        if (Collective* village = getVillain(input.get<VillageActionInfo>().villageIndex))
          switch (input.get<VillageActionInfo>().action) {
            case VillageAction::RECRUIT: 
              handleRecruiting(village);
              break;
            case VillageAction::TRADE: 
              handleTrading(village);
              break;
          }
    case UserInputId::PAY_RANSOM:
        handleRansom(true);
        break;
    case UserInputId::IGNORE_RANSOM:
        handleRansom(false);
        break;
    case UserInputId::SHOW_HISTORY:
        PlayerMessage::presentMessages(getView(), messageHistory);
        break;
    case UserInputId::BUTTON_RELEASE:
        if (rectSelection) {
          selection = rectSelection->deselect ? DESELECT : SELECT;
          for (Vec2 v : Rectangle::boundingBox({rectSelection->corner1, rectSelection->corner2}))
            handleSelection(v, getBuildInfo()[input.get<BuildingInfo>().building], true, rectSelection->deselect);
        }
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
  if (!getLevel()->inBounds(pos))
    return;
  if (deselectOnly && !contains({BuildInfo::FORBID_ZONE, BuildInfo::DIG, BuildInfo::SQUARE}, building.buildType))
    return;
  switch (building.buildType) {
    case BuildInfo::IMP:
        if (getCollective()->numResource(ResourceId::MANA) >= getImpCost() && selection == NONE && !rectangle) {
          selection = SELECT;
          PCreature imp = CreatureFactory::fromId(CreatureId::IMP, getTribeId(),
              MonsterAIFactory::collective(getCollective()));
          for (Position v : concat(position.neighbors8(Random), {position}))
            if (v.canEnter(imp.get()) && (canSee(v) || getCollective()->getTerritory().contains(v))) {
              getCollective()->takeResource({ResourceId::MANA, getImpCost()});
              getCollective()->addCreature(std::move(imp), v,
                  {MinionTrait::WORKER, MinionTrait::NO_LIMIT, MinionTrait::NO_EQUIPMENT});
              getView()->addSound(Sound(SoundId::CREATE_IMP).setPitch(2));
              break;
            }
        }
        break;
    case BuildInfo::TRAP:
        if (getCollective()->getConstructions().containsTrap(position)) {
          getCollective()->removeTrap(position);
          getView()->addSound(SoundId::DIG_UNMARK);
          // Does this mean I can remove the order if the trap physically exists?
        } else
        if (canPlacePost(position) && (getCollective()->getSquares(SquareId::FLOOR).count(position) || 
              getCollective()->getSquares(SquareId::BRIDGE).count(position))) {
          getCollective()->addTrap(position, building.trapInfo.type);
          getView()->addSound(SoundId::ADD_CONSTRUCTION);
        }
      break;
    case BuildInfo::DESTROY:
        if (getCollective()->getConstructions().containsSquare(position) &&
            !getCollective()->getConstructions().getSquare(position).isBuilt()) {
          getCollective()->removeConstruction(position);
          getView()->addSound(SoundId::DIG_UNMARK);
          selection = SELECT;
        } else
        if (getCollective()->isKnownSquare(position) && !position.isBurning()) {
          selection = SELECT;
          getCollective()->destroySquare(position);
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
          if (position.canConstruct(SquareId::TREE_TRUNK)) {
            getCollective()->cutTree(position);
            getView()->addSound(SoundId::DIG_MARK);
            selection = SELECT;
          } else
            if (position.canConstruct(SquareId::FLOOR) ||
                (!getCollective()->isKnownSquare(position) && !position.isUnavailable())) {
              getCollective()->dig(position);
              getView()->addSound(SoundId::DIG_MARK);
              selection = SELECT;
            }
        }
        break;
        }
    case BuildInfo::FETCH:
        if (getCollective()->isMarked(position) &&
            getCollective()->getMarkHighlight(position) == HighlightType::FETCH_ITEMS && selection != SELECT) {
          getCollective()->cancelMarkedTask(position);
          selection = DESELECT;
        } else if (selection != DESELECT && !getCollective()->isMarked(position)) {
          getCollective()->fetchAllItems(position);
          selection = SELECT;
        }
        break;
    case BuildInfo::CLAIM_TILE:
        if (getCollective()->isKnownSquare(position) && position.canConstruct(SquareId::STOCKPILE))
          getCollective()->claimSquare(position);
        break;
    case BuildInfo::DISPATCH:
        getCollective()->setPriorityTasks(position);
        break;
    case BuildInfo::SQUARE:
        if (getCollective()->getConstructions().containsSquare(position) &&
            !getCollective()->getConstructions().getSquare(position).isBuilt()) {
          if (selection != SELECT) {
            getCollective()->removeConstruction(position);
            selection = DESELECT;
            getView()->addSound(SoundId::DIG_UNMARK);
          }
        } else {
          BuildInfo::SquareInfo info = building.squareInfo;
          if (getCollective()->isKnownSquare(position) && position.canConstruct(info.type) 
              && !getCollective()->getConstructions().containsTrap(position)
              && (info.type.getId() != SquareId::TRIBE_DOOR || canBuildDoor(position)) && selection != DESELECT
              && (!info.maxNumber || *info.maxNumber > getCollective()->getSquares(info.type).size()
                  + getCollective()->getConstructions().getSquareCount(info.type))) {
            CostInfo cost = getRoomCost(info.type, info.cost, info.costExponent);
            getCollective()->addConstruction(position, info.type, cost, info.buildImmediatly, info.noCredit);
            selection = SELECT;
            getView()->addSound(SoundId::ADD_CONSTRUCTION);
          }
        }
        break;
  }
}

void PlayerControl::tryLockingDoor(Position pos) {
  if (getCollective()->tryLockingDoor(pos))
    updateSquareMemory(pos);
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
  if (!getKeeper()->isDead() && controlled != getKeeper()) { 
    if ((getKeeper()->wasInCombat(5) || getKeeper()->getBody().isWounded())
        && lastControlKeeperQuestion < getCollective()->getGlobalTime() - 50) {
      lastControlKeeperQuestion = getCollective()->getGlobalTime();
      if (getView()->yesOrNoPrompt("The keeper is in trouble. Do you want to control him?")) {
        controlSingle(getKeeper());
        return;
      }
    }
    if (getKeeper()->isAffected(LastingEffect::POISON)
        && lastControlKeeperQuestion < getCollective()->getGlobalTime() - 5) {
      lastControlKeeperQuestion = getCollective()->getGlobalTime();
      if (getView()->yesOrNoPrompt("The keeper is in trouble. Do you want to control him?")) {
        controlSingle(getKeeper());
        return;
      }
    }
  }
}

const double messageTimeout = 80;
const double anyWarningFrequency = 100;
const double warningFrequency = 500;

void PlayerControl::onNoEnemies() {
  getGame()->setCurrentMusic(MusicType::PEACEFUL, false);
}

void PlayerControl::considerNightfallMessage() {
  if (getGame()->getSunlightInfo().getState() == SunlightState::NIGHT) {
    if (!isNight) {
      addMessage(PlayerMessage("Night is falling. Killing enemies in their sleep yields double mana.",
            MessagePriority::HIGH));
      isNight = true;
    }
  } else
    isNight = false;
}

void PlayerControl::considerWarning() {
  double time = getLocalTime();
  if (time > lastWarningTime + anyWarningFrequency)
    for (Warning w : ENUM_ALL(Warning))
      if (getCollective()->isWarning(w) && (warningTimes[w] == 0 || time > warningTimes[w] + warningFrequency)) {
        addMessage(PlayerMessage(getWarningText(w), MessagePriority::HIGH));
        lastWarningTime = warningTimes[w] = time;
        break;
      }
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

bool PlayerControl::isConsideredAttacking(const Creature* c) {
  if (getGame()->isSingleModel())
    return canSee(c) && contains(getCollective()->getTerritory().getStandardExtended(), c->getPosition());
  else
    return canSee(c) && c->getLevel() == getLevel();
}

void PlayerControl::tick() {
  for (auto& elem : messages)
    elem.setFreshness(max(0.0, elem.getFreshness() - 1.0 / messageTimeout));
  messages = filter(messages, [&] (const PlayerMessage& msg) {
      return msg.getFreshness() > 0; });
  considerNightfallMessage();
  considerWarning();
  if (startImpNum == -1)
    startImpNum = getCollective()->getCreatures(MinionTrait::WORKER).size();
  checkKeeperDanger();
  if (getCollective()->hasMinionDebt() && !payoutWarning) {
    getView()->presentText("Warning", "You don't have enough gold for salaries. "
        "Your minions will refuse to work if they are not paid.\n \n"
        "You can get more gold by mining or retrieve it from killed heroes and conquered villages.");
    payoutWarning = true;
  }
  for (auto attack : copyOf(ransomAttacks))
    for (const Creature* c : attack.getCreatures())
      if (getCollective()->getTerritory().contains(c->getPosition())) {
        removeElement(ransomAttacks, attack);
        break;
      }
  for (auto attack : copyOf(newAttacks))
    for (const Creature* c : attack.getCreatures())
      if (isConsideredAttacking(c)) {
        addImportantLongMessage("You are under attack by " + attack.getAttacker()->getName().getFull() + "!",
            c->getPosition());
        getGame()->setCurrentMusic(MusicType::BATTLE, true);
        removeElement(newAttacks, attack);
        knownVillains.insert(attack.getAttacker());
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
  for (Position v : getCollective()->getSquares(SquareId::EYEBALL))
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

Level* PlayerControl::getLevel() const {
  return getCollective()->getLevel();
}

const Model* PlayerControl::getModel() const {
  return getLevel()->getModel();
}

Model* PlayerControl::getModel() {
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

void PlayerControl::onConstructed(Position pos, const SquareType& type) {
  updateSquareMemory(pos);
  if (type == SquareId::FLOOR) {
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

