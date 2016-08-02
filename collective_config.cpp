#include "stdafx.h"
#include "util.h"
#include "creature.h"
#include "collective_config.h"
#include "tribe.h"
#include "game.h"
#include "technology.h"
#include "collective_warning.h"
#include "item_type.h"
#include "resource_id.h"
#include "inventory.h"

AttractionInfo::AttractionInfo(MinionAttraction a, double cl, double min, bool mand)
  : attraction(a), amountClaimed(cl), minAmount(min), mandatory(mand) {}

template <class Archive>
void CollectiveConfig::serialize(Archive& ar, const unsigned int version) {
  ar& SVAR(immigrantFrequency)
    & SVAR(payoutTime)
    & SVAR(payoutMultiplier)
    & SVAR(maxPopulation)
    & SVAR(populationIncreases)
    & SVAR(immigrantInfo)
    & SVAR(type)
    & SVAR(recruitingMinPopulation)
    & SVAR(leaderAsFighter)
    & SVAR(spawnGhosts)
    & SVAR(ghostProb)
    & SVAR(guardianInfo);
}

SERIALIZABLE(CollectiveConfig);
SERIALIZATION_CONSTRUCTOR_IMPL(CollectiveConfig);

template <class Archive>
void ImmigrantInfo::serialize(Archive& ar, const unsigned int version) {
  ar& SVAR(id)
    & SVAR(frequency)
    & SVAR(attractions)
    & SVAR(traits)
    & SVAR(spawnAtDorm)
    & SVAR(salary)
    & SVAR(techId)
    & SVAR(limit)
    & SVAR(groupSize)
    & SVAR(autoTeam)
    & SVAR(ignoreSpawnType);
}

SERIALIZABLE(ImmigrantInfo);

template <class Archive>
void AttractionInfo::serialize(Archive& ar, const unsigned int version) {
  ar& SVAR(attraction)
    & SVAR(amountClaimed)
    & SVAR(minAmount)
    & SVAR(mandatory);
}

SERIALIZABLE(AttractionInfo);
SERIALIZATION_CONSTRUCTOR_IMPL(AttractionInfo);

template <class Archive>
void PopulationIncrease::serialize(Archive& ar, const unsigned int version) {
  ar& SVAR(type)
    & SVAR(increasePerSquare)
    & SVAR(maxIncrease);
}

SERIALIZABLE(PopulationIncrease);

template <class Archive>
void GuardianInfo::serialize(Archive& ar, const unsigned int version) {
  ar& SVAR(creature)
    & SVAR(probability)
    & SVAR(minEnemies)
    & SVAR(minVictims);
}

SERIALIZABLE(GuardianInfo);

CollectiveConfig::CollectiveConfig(double freq, int payoutT, double payoutM, vector<ImmigrantInfo> im,
    CollectiveType t, int maxPop, vector<PopulationIncrease> popInc)
    : immigrantFrequency(freq), payoutTime(payoutT), payoutMultiplier(payoutM),
    maxPopulation(maxPop), populationIncreases(popInc), immigrantInfo(im), type(t) {}

CollectiveConfig CollectiveConfig::keeper(double freq, int payout, double payoutMult, int maxPopulation,
    vector<PopulationIncrease> increases, vector<ImmigrantInfo> im) {
  return CollectiveConfig(freq, payout, payoutMult, im, KEEPER, maxPopulation, increases);
}

CollectiveConfig CollectiveConfig::withImmigrants(double frequency, int maxPopulation, vector<ImmigrantInfo> im) {
  return CollectiveConfig(frequency, 0, 0, im, VILLAGE, maxPopulation, {});
}

CollectiveConfig CollectiveConfig::noImmigrants() {
  return CollectiveConfig(0, 0, 0, {}, VILLAGE, 10000, {});
}

CollectiveConfig& CollectiveConfig::setLeaderAsFighter() {
  leaderAsFighter = true;
  return *this;
}

CollectiveConfig& CollectiveConfig::setGhostSpawns(double prob, int num) {
  ghostProb = prob;
  spawnGhosts = num;
  return *this;
}

int CollectiveConfig::getNumGhostSpawns() const {
  return spawnGhosts;
}

double CollectiveConfig::getGhostProb() const {
  return ghostProb;
}

bool CollectiveConfig::isLeaderFighter() const {
  return leaderAsFighter;
}

bool CollectiveConfig::getManageEquipment() const {
  return type == KEEPER;
}

bool CollectiveConfig::getWorkerFollowLeader() const {
  return type == KEEPER;
}

bool CollectiveConfig::sleepOnlyAtNight() const {
  return type != KEEPER;
}

bool CollectiveConfig::activeImmigrantion(const Game* game) const {
  return type == KEEPER || game->isSingleModel();
}

double CollectiveConfig::getImmigrantFrequency() const {
  return immigrantFrequency;
}

int CollectiveConfig::getPayoutTime() const {
  return payoutTime;
}
 
double CollectiveConfig::getPayoutMultiplier() const {
  return payoutMultiplier;
}

bool CollectiveConfig::getStripSpawns() const {
  return type == KEEPER;
}

bool CollectiveConfig::getFetchItems() const {
  return type == KEEPER;
}

bool CollectiveConfig::getEnemyPositions() const {
  return type == KEEPER;
}

bool CollectiveConfig::getWarnings() const {
  return type == KEEPER;
}

bool CollectiveConfig::getConstructions() const {
  return type == KEEPER;
}

int CollectiveConfig::getMaxPopulation() const {
  return maxPopulation;
}

optional<int> CollectiveConfig::getRecruitingMinPopulation() const {
  return recruitingMinPopulation;
}

CollectiveConfig& CollectiveConfig::allowRecruiting(int minPop) {
  recruitingMinPopulation = minPop;
  return *this;
}

const vector<ImmigrantInfo>& CollectiveConfig::getImmigrantInfo() const {
  return immigrantInfo;
}

const vector<PopulationIncrease>& CollectiveConfig::getPopulationIncreases() const {
  return populationIncreases;
}

CollectiveConfig& CollectiveConfig::setGuardian(GuardianInfo info) {
  guardianInfo = info;
  return *this;
}

const optional<GuardianInfo>& CollectiveConfig::getGuardianInfo() const {
  return guardianInfo;
}

vector<BirthSpawn> CollectiveConfig::getBirthSpawns() const {
  if (type == KEEPER)
    return {{ CreatureId::GOBLIN, 1 },
      { CreatureId::ORC, 1 },
      { CreatureId::ORC_SHAMAN, 0.5 },
      { CreatureId::HARPY, 0.5 },
      { CreatureId::OGRE, 0.5 },
      { CreatureId::WEREWOLF, 0.5 },
      { CreatureId::SPECIAL_HM, 1.0, TechId::HUMANOID_MUT},
      { CreatureId::SPECIAL_BM, 1.0, TechId::BEAST_MUT },
    };
  else 
    return {};
}

optional<SquareType> CollectiveConfig::getSecondarySquare(SquareType type) {
  switch (type.getId()) {
    case SquareId::DORM: return SquareType(SquareId::BED);
    case SquareId::BEAST_LAIR: return SquareType(SquareId::BEAST_CAGE);
    case SquareId::CEMETERY: return SquareType(SquareId::GRAVE);
    default: return none;
  }
}

optional<SquareType> DormInfo::getBedType() const {
  return CollectiveConfig::getSecondarySquare(dormType);
}

const EnumMap<SpawnType, DormInfo>& CollectiveConfig::getDormInfo() const {
  static EnumMap<SpawnType, DormInfo> dormInfo {
    {SpawnType::HUMANOID, {SquareId::DORM, CollectiveWarning::BEDS}},
    {SpawnType::UNDEAD, {SquareId::CEMETERY}},
    {SpawnType::BEAST, {SquareId::BEAST_LAIR}},
    {SpawnType::DEMON, {SquareId::RITUAL_ROOM}},
  };
  return dormInfo;
}

const static unordered_set<SquareType> efficiencySquares {
  SquareId::TRAINING_ROOM,
  SquareId::TORTURE_TABLE,
  SquareId::WORKSHOP,
  SquareId::FORGE,
  SquareId::LABORATORY,
  SquareId::JEWELER,
  SquareId::LIBRARY,
};

unordered_set<SquareType> CollectiveConfig::getEfficiencySquares() const {
  return efficiencySquares;
}

vector<SquareType> CollectiveConfig::getRoomsNeedingLight() const {
  static vector<SquareType> ret {
    SquareId::WORKSHOP,
      SquareId::FORGE,
      SquareId::LABORATORY,
      SquareId::JEWELER,
      SquareId::TRAINING_ROOM,
      SquareId::LIBRARY};
  return ret;
};

int CollectiveConfig::getTaskDuration(const Creature* c, MinionTask task) const {
  switch (task) {
    case MinionTask::CONSUME:
    case MinionTask::COPULATE:
    case MinionTask::GRAVE:
    case MinionTask::LAIR:
    case MinionTask::EAT:
    case MinionTask::SLEEP: return 1;
    default: return 500 + 250 * c->getMorale();
  }
}

const static vector<SquareType> resourceStorage {SquareId::STOCKPILE, SquareId::STOCKPILE_RES};
const static vector<SquareType> equipmentStorage {SquareId::STOCKPILE, SquareId::STOCKPILE_EQUIP};

const vector<SquareType>& CollectiveConfig::getEquipmentStorage() {
  return equipmentStorage;
}

const vector<SquareType>& CollectiveConfig::getResourceStorage() {
  return resourceStorage;
}

const map<CollectiveResourceId, ResourceInfo>& CollectiveConfig::getResourceInfo() {
  static map<CollectiveResourceId, ResourceInfo> ret = {
    {CollectiveResourceId::MANA, { {}, none, ItemId::GOLD_PIECE, "mana"}},
    {CollectiveResourceId::PRISONER_HEAD, { {}, none, ItemId::GOLD_PIECE, "", true}},
    {CollectiveResourceId::GOLD, {{SquareId::TREASURE_CHEST}, ItemIndex::GOLD, ItemId::GOLD_PIECE,"gold",}},
    {CollectiveResourceId::WOOD, { resourceStorage, ItemIndex::WOOD, ItemId::WOOD_PLANK, "wood"}},
    {CollectiveResourceId::IRON, { resourceStorage, ItemIndex::IRON, ItemId::IRON_ORE, "iron"}},
    {CollectiveResourceId::STONE, { resourceStorage, ItemIndex::STONE, ItemId::ROCK, "granite"}},
    {CollectiveResourceId::CORPSE, { {SquareId::CEMETERY}, ItemIndex::REVIVABLE_CORPSE, ItemId::GOLD_PIECE,
                                     "corpses", true}}
  };
  return ret;
}

MinionTaskInfo::MinionTaskInfo(vector<SquareType> s, const string& desc, optional<CollectiveWarning> w,
    double _cost, bool center) 
    : type(APPLY_SQUARE), squares(s), description(desc), warning(w), cost(_cost), centerOnly(center) {
}

MinionTaskInfo::MinionTaskInfo(Type t, const string& desc, optional<CollectiveWarning> w)
    : type(t), description(desc), warning(w) {
  CHECK(type != APPLY_SQUARE);
}


map<MinionTask, MinionTaskInfo> CollectiveConfig::getTaskInfo() const {
  map<MinionTask, MinionTaskInfo> ret {
    {MinionTask::TRAIN, {{SquareId::TRAINING_ROOM}, "training", CollectiveWarning::TRAINING, 1}},
    {MinionTask::WORKSHOP, {{SquareId::WORKSHOP}, "workshop", CollectiveWarning::WORKSHOP, 1}},
    {MinionTask::FORGE, {{SquareId::FORGE}, "forge", none, 1}},
    {MinionTask::LABORATORY, {{SquareId::LABORATORY}, "lab", none, 1}},
    {MinionTask::JEWELER, {{SquareId::JEWELER}, "jewellery", none, 1}},
    {MinionTask::SLEEP, {{SquareId::BED}, "sleeping", CollectiveWarning::BEDS}},
    {MinionTask::EAT, {MinionTaskInfo::EAT, "eating"}},
    {MinionTask::GRAVE, {{SquareId::GRAVE}, "sleeping", CollectiveWarning::GRAVES}},
    {MinionTask::LAIR, {{SquareId::BEAST_CAGE}, "sleeping"}},
    {MinionTask::LAIR, {{SquareId::BEAST_CAGE}, "sleeping"}},
    {MinionTask::THRONE, {{SquareId::THRONE}, "throne"}},
    {MinionTask::STUDY, {{SquareId::LIBRARY}, "studying", CollectiveWarning::LIBRARY, 1}},
    {MinionTask::PRISON, {{SquareId::PRISON}, "prison", CollectiveWarning::NO_PRISON}},
    {MinionTask::TORTURE, {{SquareId::TORTURE_TABLE}, "torture ordered",
                            CollectiveWarning::TORTURE_ROOM, 0, true}},
    {MinionTask::CROPS, {{SquareId::CROPS}, "crops"}},
    {MinionTask::RITUAL, {{SquareId::RITUAL_ROOM}, "rituals"}},
    {MinionTask::COPULATE, {MinionTaskInfo::COPULATE, "copulation"}},
    {MinionTask::CONSUME, {MinionTaskInfo::CONSUME, "consumption"}},
    {MinionTask::EXPLORE, {MinionTaskInfo::EXPLORE, "spying"}},
    {MinionTask::SPIDER, {MinionTaskInfo::SPIDER, "spider"}},
    {MinionTask::EXPLORE_NOCTURNAL, {MinionTaskInfo::EXPLORE, "spying"}},
    {MinionTask::EXPLORE_CAVES, {MinionTaskInfo::EXPLORE, "spying"}},
    {MinionTask::EXECUTE, {{SquareId::PRISON}, "execution ordered", CollectiveWarning::NO_PRISON}}};
  return ret;
};

