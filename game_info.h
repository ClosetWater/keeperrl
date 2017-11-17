#pragma once

#include "unique_entity.h"
#include "item_action.h"
#include "village_action.h"
#include "view_id.h"
#include "player_message.h"
#include "experience_type.h"
#include "entity_set.h"
#include "immigrant_auto_state.h"
#include "tutorial_highlight.h"
#include "hashing.h"
#include "experience_type.h"
#include "attr_type.h"
#include "best_attack.h"
#include "villain_type.h"

enum class SpellId;

class PlayerMessage;

struct CreatureInfo {
  CreatureInfo(WConstCreature);
  ViewId HASH(viewId);
  UniqueEntity<Creature>::Id HASH(uniqueId);
  string HASH(name);
  string HASH(stackName);
  BestAttack HASH(bestAttack);
  double HASH(morale);
  HASH_ALL(viewId, uniqueId, name, stackName, bestAttack, morale)
};


struct ItemInfo {
  string HASH(name);
  string HASH(fullName);
  string HASH(description);
  int HASH(number);
  ViewId HASH(viewId);
  EntitySet<Item> HASH(ids);
  vector<ItemAction> HASH(actions);
  bool HASH(equiped);
  bool HASH(locked);
  bool HASH(pending);
  bool HASH(unavailable);
  string HASH(unavailableReason);
  optional<EquipmentSlot> HASH(slot);
  optional<CreatureInfo> HASH(owner);
  enum Type {EQUIPMENT, CONSUMABLE, OTHER} HASH(type);
  optional<pair<ViewId, int>> HASH(price);
  double HASH(productionState);
  optional<double> HASH(weight);
  bool HASH(tutorialHighlight);
  HASH_ALL(name, fullName, description, number, viewId, ids, actions, equiped, locked, pending, unavailable, slot, owner, type, price, productionState, unavailableReason, weight, tutorialHighlight)
};

struct AttributeInfo {
  static vector<AttributeInfo> fromCreature(WConstCreature);
  string HASH(name);
  AttrType HASH(attr);
  int HASH(value);
  int HASH(bonus);
  const char* HASH(help);
  HASH_ALL(name, attr, value, bonus, help)
};

class PlayerInfo {
  public:
  PlayerInfo(WConstCreature);
  string getFirstName() const;
  string getTitle() const;
  vector<AttributeInfo> HASH(attributes);
  BestAttack HASH(bestAttack);
  struct SkillInfo {
    string HASH(name);
    string HASH(help);
    HASH_ALL(name, help)
  };
  vector<SkillInfo> HASH(skills);
  string HASH(firstName);
  string HASH(name);
  string HASH(title);
  struct LevelInfo {
    EnumMap<ExperienceType, double> HASH(level);
    EnumMap<ExperienceType, int> HASH(limit);
    EnumMap<ExperienceType, optional<string>> HASH(warning);
    HASH_ALL(level, limit, warning)
  };
  LevelInfo HASH(levelInfo);
  string description;
  string HASH(levelName);
  int HASH(positionHash);
  string HASH(weaponName);
  struct Effect {
    string HASH(name);
    string HASH(help);
    bool HASH(bad);
    HASH_ALL(name, help, bad)
  };
  vector<Effect> HASH(effects);
  struct Spell {
    SpellId HASH(id);
    string HASH(name);
    string HASH(help);
    optional<int> HASH(timeout);
    HASH_ALL(id, name, help, timeout)
  };
  vector<Spell> HASH(spells);
  vector<ItemInfo> HASH(lyingItems);
  vector<ItemInfo> HASH(inventory);
  int HASH(debt);
  struct TeamMemberInfo {
    ViewId HASH(viewId);
    int HASH(bestAttack);
    bool HASH(active);
    bool HASH(leader);
    HASH_ALL(viewId, bestAttack, active, leader)
  };
  vector<TeamMemberInfo> HASH(team);
  struct CommandInfo {
    string HASH(name);
    optional<char> HASH(keybinding);
    string HASH(description);
    bool HASH(active);
    bool HASH(tutorialHighlight) = false;
    HASH_ALL(name, keybinding, description, active, tutorialHighlight)
  };
  vector<CommandInfo> HASH(commands);
  struct MinionTaskInfo {
    MinionTask HASH(task);
    bool HASH(inactive);
    bool HASH(current);
    bool HASH(locked);
    HASH_ALL(task, inactive, current, locked)
  };
  vector<MinionTaskInfo> HASH(minionTasks);
  UniqueEntity<Creature>::Id HASH(creatureId);
  double HASH(morale);
  ViewId HASH(viewId);
  enum Action {
    CONTROL,
    RENAME,
    BANISH,
    CONSUME
  };
  vector<Action> HASH(actions);
  HASH_ALL(attributes, skills, firstName, name, title, levelInfo, levelName, positionHash, weaponName, effects, spells, lyingItems, inventory, team, minionTasks, creatureId, morale, viewId, actions, commands, debt, bestAttack)
};

struct ImmigrantDataInfo {
  vector<string> HASH(requirements);
  vector<string> HASH(info);
  optional<pair<ViewId, int>> HASH(cost);
  string HASH(name);
  ViewId HASH(viewId);
  vector<AttributeInfo> HASH(attributes);
  int HASH(count);
  optional<double> HASH(timeLeft);
  int HASH(id);
  enum AutoState { AUTO_REJECT, AUTO_ACCEPT};
  optional<ImmigrantAutoState> HASH(autoState);
  optional<milliseconds> HASH(generatedTime);
  optional<Keybinding> HASH(keybinding);
  optional<TutorialHighlight> HASH(tutorialHighlight);
  HASH_ALL(requirements, info, name, viewId, attributes, count, timeLeft, id, autoState, cost, generatedTime, keybinding, tutorialHighlight)
};

class CollectiveInfo {
  public:
  CollectiveInfo() {}
  CollectiveInfo(const CollectiveInfo&) = delete;
  CollectiveInfo(CollectiveInfo&&) = default;
  CollectiveInfo& operator = (CollectiveInfo&&) = default;
  CollectiveInfo& operator = (const CollectiveInfo&) = delete;
  string HASH(warning);
  struct Button {
    ViewId HASH(viewId);
    string HASH(name);
    optional<pair<ViewId, int>> HASH(cost);
    string HASH(count);
    enum { ACTIVE, GRAY_CLICKABLE, INACTIVE} HASH(state);
    string HASH(help);
    char HASH(hotkey);
    string HASH(groupName);
    bool HASH(hotkeyOpensGroup);
    optional<TutorialHighlight> HASH(tutorialHighlight);
    HASH_ALL(viewId, name, cost, count, state, help, hotkey, groupName, hotkeyOpensGroup, tutorialHighlight)
  };
  vector<Button> HASH(buildings);
  int HASH(minionCount);
  int HASH(minionLimit);
  string HASH(monsterHeader);
  vector<CreatureInfo> HASH(minions);
  struct CreatureGroup {
    UniqueEntity<Creature>::Id HASH(creatureId);
    string HASH(name);
    ViewId HASH(viewId);
    int HASH(count);
    bool HASH(highlight);
    HASH_ALL(creatureId, name, viewId, count, highlight)
  };
  vector<CreatureGroup> HASH(minionGroups);
  vector<CreatureGroup> HASH(enemyGroups);
  struct ChosenCreatureInfo {
    UniqueEntity<Creature>::Id HASH(chosenId);
    vector<PlayerInfo> HASH(creatures);
    optional<TeamId> HASH(teamId);
    HASH_ALL(chosenId, creatures, teamId)
  };
  optional<ChosenCreatureInfo> HASH(chosenCreature);
  vector<ImmigrantDataInfo> HASH(immigration);
  vector<ImmigrantDataInfo> HASH(allImmigration);
  struct WorkshopButton {
    string HASH(name);
    ViewId HASH(viewId);
    bool HASH(active);
    bool HASH(unavailable);
    HASH_ALL(name, viewId, active, unavailable)
  };
  vector<WorkshopButton> HASH(workshopButtons);
  struct ChosenWorkshopInfo {
    vector<ItemInfo> HASH(options);
    vector<ItemInfo> HASH(queued);
    int HASH(index);
    HASH_ALL(index, options, queued)
  };
  optional<ChosenWorkshopInfo> HASH(chosenWorkshop);
  struct LibraryInfo {
    struct TechInfo {
      string HASH(name);
      string HASH(description);
      pair<ViewId, int> HASH(cost);
      bool HASH(active);
      optional<TutorialHighlight> HASH(tutorialHighlight);
      HASH_ALL(name, cost, active, description, tutorialHighlight)
    };
    optional<string> HASH(warning);
    pair<ViewId, int> HASH(resource);
    vector<TechInfo> HASH(available);
    vector<TechInfo> HASH(researched);
    HASH_ALL(warning, resource, available, researched)
  };
  optional<LibraryInfo> HASH(libraryInfo);
  struct Resource {
    ViewId HASH(viewId);
    int HASH(count);
    string HASH(name);
    optional<TutorialHighlight> HASH(tutorialHighlight);
    HASH_ALL(viewId, count, name, tutorialHighlight)
  };
  vector<Resource> HASH(numResource);
  struct Team {
    TeamId HASH(id);
    vector<UniqueEntity<Creature>::Id> HASH(members);
    bool HASH(active);
    bool HASH(highlight);
    HASH_ALL(id, members, active, highlight)
  };
  vector<Team> HASH(teams);
  const CreatureInfo* getMinion(UniqueEntity<Creature>::Id) const;
  bool hasMinion(UniqueEntity<Creature>::Id);
  int HASH(nextPayout);
  int HASH(payoutTimeRemaining);

  struct TechButton {
    ViewId HASH(viewId);
    string HASH(name);
    char HASH(hotkey);
    bool HASH(active);
    HASH_ALL(viewId, name, hotkey, active)
  };
  vector<TechButton> HASH(techButtons);

  struct Task {
    string HASH(name);
    optional<UniqueEntity<Creature>::Id> HASH(creature);
    bool HASH(priority);
    HASH_ALL(name, creature, priority)
  };
  vector<Task> HASH(taskMap);

  struct Ransom {
    pair<ViewId, int> HASH(amount);
    string HASH(attacker);
    bool HASH(canAfford);
    HASH_ALL(amount, attacker, canAfford)
  };
  optional<Ransom> HASH(ransom);

  struct NextWave {
    ViewId HASH(viewId);
    string HASH(attacker);
    int HASH(numTurns);
    HASH_ALL(viewId, attacker, numTurns)
  };
  optional<NextWave> HASH(nextWave);

  HASH_ALL(warning, buildings, minionCount, minionLimit, monsterHeader, minions, minionGroups, enemyGroups, chosenCreature, numResource, teams, nextPayout, payoutTimeRemaining, techButtons, taskMap, ransom, nextWave, chosenWorkshop, workshopButtons, immigration, allImmigration, libraryInfo)
};

class VillageInfo {
  public:
  struct Village {
    string HASH(name);
    string HASH(tribeName);
    enum Access { ACTIVE, INACTIVE, LOCATION, NO_LOCATION };
    Access HASH(access);
    enum State { FRIENDLY, HOSTILE, CONQUERED } HASH(state);
    struct ActionInfo {
      VillageAction HASH(action);
      optional<string> HASH(disabledReason);
      HASH_ALL(action, disabledReason)
    };
    struct TriggerInfo {
      string HASH(name);
      double HASH(value);
      HASH_ALL(name, value)
    };
    vector<ActionInfo> HASH(actions);
    vector<TriggerInfo> HASH(triggers);
    UniqueEntity<Collective>::Id HASH(id);
    HASH_ALL(name, tribeName, access, state, actions, triggers, id)
  };
  EnumMap<VillainType, vector<Village>> HASH(villages);
  int HASH(numTotalVillains);
  HASH_ALL(villages, numTotalVillains)
};

class GameSunlightInfo {
  public:
  string HASH(description);
  int HASH(timeRemaining);
  HASH_ALL(description, timeRemaining)
};

class TutorialInfo {
  public:
  string HASH(message);
  optional<string> HASH(warning);
  bool HASH(canContinue);
  bool HASH(canGoBack);
  EnumSet<TutorialHighlight> HASH(highlights);
  vector<Vec2> HASH(highlightedSquaresHigh);
  vector<Vec2> HASH(highlightedSquaresLow);
  HASH_ALL(message, warning, canContinue, canGoBack, highlights, highlightedSquaresHigh, highlightedSquaresLow)
};

/** Represents all the game information displayed around the map window.*/
class GameInfo {
  public:
  enum class InfoType { PLAYER, BAND, SPECTATOR} HASH(infoType) = InfoType::PLAYER;
  int HASH(time);
  int HASH(modifiedSquares);
  int HASH(totalSquares);

  bool HASH(singleModel);
  GameInfo() {}
  GameInfo(const GameInfo&) = delete;
  GameInfo(GameInfo&&) = default;
  GameInfo& operator = (const GameInfo&) = delete;
  GameInfo& operator = (GameInfo&&) = default;

  variant<CollectiveInfo, PlayerInfo> HASH(playerInfo);
  VillageInfo HASH(villageInfo);
  GameSunlightInfo HASH(sunlightInfo);
  optional<TutorialInfo> HASH(tutorial);

  vector<PlayerMessage> HASH(messageBuffer);
  HASH_ALL(infoType, time, playerInfo, villageInfo, sunlightInfo, messageBuffer, singleModel, modifiedSquares, totalSquares, tutorial)
};
