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
#ifndef _COLLECTIVE_H
#define _COLLECTIVE_H

#include "move_info.h"
#include "task_callback.h"
#include "minion_task.h"
#include "square_apply_type.h"
#include "resource_id.h"
#include "square_type.h"
#include "attack_trigger.h"
#include "collective_warning.h"
#include "event_listener.h"
#include "entity_map.h"

class CollectiveAttack;
class Creature;
class CollectiveControl;
class Tribe;
class Level;
class Trigger;
struct ImmigrantInfo;
struct AttractionInfo;
class MinionEquipment;
class TaskMap;
class KnownTiles;
class CollectiveTeams;
class ConstructionMap;
class Technology;
class CollectiveConfig;
class MinionAttraction;
struct CostInfo;
struct TriggerInfo;
class Territory;
class CollectiveName;
template <typename T>
class EventProxy;

class Collective : public TaskCallback {
  public:
  void addCreature(Creature*, EnumSet<MinionTrait>);
  void addCreature(PCreature, Position, EnumSet<MinionTrait>);
  MoveInfo getMove(Creature*);
  void setControl(PCollectiveControl);
  void tick();
  void update(bool currentlyActive);
  TribeId getTribeId() const;
  Tribe* getTribe() const;
  Level* getLevel();
  Game* getGame() const;
  const Level* getLevel() const;
  void addNewCreatureMessage(const vector<Creature*>&);
  void setTask(const Creature*, PTask, bool priority = false);
  bool hasTask(const Creature*) const;
  void cancelTask(const Creature*);
  void banishCreature(Creature*);
  bool wasBanished(const Creature*) const;
  void ownItem(const Creature*, const Item*);
  void setVillainType(VillainType);
  optional<VillainType> getVillainType() const;
  CollectiveControl* getControl() const;
  double getLocalTime() const;
  double getGlobalTime() const;

  typedef CollectiveWarning Warning;
  typedef CollectiveResourceId ResourceId;

  SERIALIZATION_DECL(Collective);

  const vector<Creature*>& getCreatures() const;
  bool isConquered() const;

  const vector<Creature*>& getCreatures(SpawnType) const;
  const vector<Creature*>& getCreatures(MinionTrait) const;
  vector<Creature*> getCreaturesAnyOf(EnumSet<MinionTrait>) const;
  vector<Creature*> getCreatures(EnumSet<MinionTrait> with, EnumSet<MinionTrait> without = {}) const;
  bool hasTrait(const Creature*, MinionTrait) const;
  bool hasAnyTrait(const Creature*, EnumSet<MinionTrait>) const;
  void setTrait(Creature* c, MinionTrait);
  void removeTrait(Creature* c, MinionTrait);

  vector<Creature*> getRecruits() const;
  void recruit(Creature*, Collective* to);
  vector<Item*> getTradeItems() const;
  PItem buyItem(Item*);
  vector<TriggerInfo> getTriggers(const Collective* against) const;

  double getEfficiency(const Creature*) const;
  const Creature* getLeader() const;
  Creature* getLeader();
  bool hasLeader() const;
  void clearLeader();

  const set<Position>& getSquares(SquareType) const;
  const set<Position>& getSquares(SquareApplyType) const;
  vector<SquareType> getSquareTypes() const;
  vector<Position> getAllSquares(const vector<SquareType>&, bool centerOnly = false) const;
  const Territory& getTerritory() const;
  void claimSquare(Position);
  void changeSquareType(Position pos, SquareType from, SquareType to);
  bool isKnownSquare(Position pos) const;
  void limitKnownTilesToModel();

  double getEfficiency(Position) const;
  bool hasEfficiency(Position) const;

  bool usesEquipment(const Creature*) const;

  virtual ~Collective();

  void setWarning(Warning, bool state = true);
  bool isWarning(Warning) const;

  int numResource(ResourceId) const;
  int numResourcePlusDebt(ResourceId) const;
  bool hasResource(const CostInfo&) const;
  void takeResource(const CostInfo&);
  void returnResource(const CostInfo&);

  struct ItemFetchInfo;

  const ConstructionMap& getConstructions() const;

  void setMinionTask(const Creature* c, MinionTask task);
  optional<MinionTask> getMinionTask(const Creature*) const;
  bool isMinionTaskPossible(Creature* c, MinionTask task);

  set<TrapType> getNeededTraps() const;

  vector<Item*> getAllItems(bool includeMinions = true) const;
  vector<Item*> getAllItems(ItemPredicate predicate, bool includeMinions = true) const;
  vector<Item*> getAllItems(ItemIndex, bool includeMinions = true) const;
  static void sortByEquipmentValue(vector<Item*>&);
  static SquareType getHatcheryType(TribeId);

  vector<pair<Item*, Position>> getTrapItems(TrapType, const vector<Position>&) const;

  void orderExecution(Creature*);
  void orderSacrifice(Creature*);
  void orderTorture(Creature*);
  void orderWhipping(Creature*);
  bool canWhip(Creature*) const;

  void addTrap(Position, TrapType);
  void removeTrap(Position);
  void addConstruction(Position, SquareType, const CostInfo&, bool immediately, bool noCredit);
  void removeConstruction(Position);
  void destroySquare(Position);
  bool isPlannedTorch(Position) const;
  bool canPlaceTorch(Position) const;
  void removeTorch(Position);
  void addTorch(Position);
  void fetchAllItems(Position);
  void dig(Position);
  void cancelMarkedTask(Position);
  void cutTree(Position);
  double getDangerLevel() const;
  bool isMarked(Position) const;
  HighlightType getMarkHighlight(Position) const;
  void setPriorityTasks(Position);
  bool hasPriorityTasks(Position) const;

  bool hasTech(TechId id) const;
  void acquireTech(Technology*, bool free = false);
  vector<Technology*> getTechnologies() const;
  double getTechCost(Technology*);
  bool addKnownTile(Position);

  const EntitySet<Creature>& getKills() const;
  int getPoints() const;

  MinionEquipment& getMinionEquipment();
  const MinionEquipment& getMinionEquipment() const;

  struct MinionPaymentInfo {
    int SERIAL(salary);
    double SERIAL(workAmount);
    int SERIAL(debt);
    SERIALIZE_ALL(salary, workAmount, debt);
  };


  int getNextPayoutTime() const;
  int getSalary(const Creature*) const;
  int getNextSalaries() const;
  bool hasMinionDebt() const;

  int getPopulationSize() const;
  int getMaxPopulation() const;

  bool tryLockingDoor(Position);
  void orderConsumption(Creature* consumer, Creature* who);
  vector<Creature*>getConsumptionTargets(Creature* consumer);

  void addAttack(const CollectiveAttack&);
  void onRansomPaid();

  CollectiveTeams& getTeams();
  const CollectiveTeams& getTeams() const;
  void freeTeamMembers(TeamId);
  void ownItems(const Creature* who, const vector<Item*>);

  const CollectiveName& getName() const;
  const TaskMap& getTaskMap() const;

  template <class Archive>
  static void registerTypes(Archive& ar, int version);

  protected:
  // From Task::Callback
  virtual void onAppliedItem(Position, Item* item) override;
  virtual void onAppliedItemCancel(Position) override;
  virtual void onTaskPickedUp(Position, EntitySet<Item>) override;
  virtual void onCantPickItem(EntitySet<Item> items) override;
  virtual void onConstructed(Position, const SquareType&) override;
  virtual void onTorchBuilt(Position, Trigger*) override;
  virtual void onAppliedSquare(Position) override;
  virtual void onKillCancelled(Creature*) override;
  virtual void onBedCreated(Position, const SquareType& fromType, const SquareType& toType) override;
  virtual void onCopulated(Creature* who, Creature* with) override;
  virtual void onConsumed(Creature* consumer, Creature* who) override;
  virtual bool isConstructionReachable(Position) override;
  virtual void onWhippingDone(Creature* whipped, Position postPosition) override;

  private:
  HeapAllocated<EventProxy<Collective>> SERIAL(eventProxy);
  friend EventProxy<Collective>;
  void onEvent(const GameEvent&);

  friend class CollectiveBuilder;
  Collective(Level*, const CollectiveConfig&, TribeId, EnumMap<ResourceId, int> credit, const CollectiveName&);
  void addCreatureInTerritory(PCreature, EnumSet<MinionTrait>);
  void updateEfficiency(Position, SquareType);
  int getPaymentAmount(const Creature*) const;
  void makePayouts();
  void cashPayouts();
  void removeCreature(Creature*);
  void onMinionKilled(Creature* victim, Creature* killer);
  void onKilledSomeone(Creature* victim, Creature* killer);

  const vector<ItemFetchInfo>& getFetchInfo() const;
  void fetchItems(Position, const ItemFetchInfo&);

  void addMoraleForKill(const Creature* killer, const Creature* victim);
  void decreaseMoraleForKill(const Creature* killer, const Creature* victim);
  void decreaseMoraleForBanishing(const Creature*);

  double getAttractionValue(const MinionAttraction&);
  double getImmigrantChance(const ImmigrantInfo&);

  bool isItemNeeded(const Item*) const;
  void addProducesMessage(const Creature*, const vector<PItem>&);
  
  HeapAllocated<MinionEquipment> SERIAL(minionEquipment);
  EnumMap<ResourceId, int> SERIAL(credit);
  HeapAllocated<TaskMap> SERIAL(taskMap);
  vector<TechId> SERIAL(technologies);
  int SERIAL(numFreeTech) = 0;
  bool isItemMarked(const Item*) const;
  void markItem(const Item*);
  void unmarkItem(UniqueEntity<Item>::Id);

  HeapAllocated<KnownTiles> SERIAL(knownTiles);

  struct CurrentTaskInfo {
    MinionTask SERIAL(task);
    double SERIAL(finishTime);
    SERIALIZE_ALL(task, finishTime);
  };

  EntityMap<Creature, CurrentTaskInfo> SERIAL(currentTasks);
  optional<Position> getTileToExplore(const Creature*, MinionTask) const;
  PTask getStandardTask(Creature* c);
  PTask getEquipmentTask(Creature* c);
  PTask getHealingTask(Creature* c);
  bool isTaskGood(const Creature*, MinionTask, bool ignoreTaskLock = false) const;
  PTask generateMinionTask(Creature*, MinionTask);
  void setRandomTask(const Creature*);

  void handleSurprise(Position);
  EnumSet<Warning> SERIAL(warnings);
  MoveInfo getDropItems(Creature*);
  MoveInfo getWorkerMove(Creature*);
  MoveInfo getTeamMemberMove(Creature*);
  void autoEquipment(Creature* creature, bool replace);
  Item* getWorstItem(const Creature*, vector<Item*> items) const;
  int getTaskDuration(const Creature*, MinionTask) const;
  bool considerImmigrant(const ImmigrantInfo&);
  void considerBuildingBeds();
  bool considerNonSpawnImmigrant(const ImmigrantInfo&, vector<PCreature>);
  vector<Position> getSpawnPos(const vector<Creature*>&);
  void considerImmigration();
  int tryBuildingBeds(SpawnType spawnType, int numBeds);
  void considerBirths();
  void considerWeaponWarning();
  void considerMoraleWarning();
  void decayMorale();
  vector<Creature*> SERIAL(creatures);
  Creature* SERIAL(leader) = nullptr;
  EnumMap<MinionTrait, vector<Creature*>> SERIAL(byTrait);
  EnumMap<SpawnType, vector<Creature*>> SERIAL(bySpawnType);
  PCollectiveControl SERIAL(control);
  TribeId SERIAL(tribe);
  Level* SERIAL(level) = nullptr;
  unordered_map<SquareType, set<Position>> SERIAL(mySquares);
  unordered_map<SquareApplyType, set<Position>, CustomHash<SquareApplyType>> SERIAL(mySquares2);
  map<Position, int> SERIAL(squareEfficiency);
  HeapAllocated<Territory> SERIAL(territory);
  struct AlarmInfo {
    double SERIAL(finishTime);
    Position SERIAL(position);
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar & SVAR(finishTime) & SVAR(position);
    }
  };
  optional<AlarmInfo> SERIAL(alarmInfo);
  MoveInfo getAlarmMove(Creature* c);
  HeapAllocated<ConstructionMap> SERIAL(constructions);
  EntitySet<Item> SERIAL(markedItems);
  set<Position> SERIAL(squaresInUse);
  ItemPredicate unMarkedItems() const;
  EntitySet<Creature> SERIAL(surrendering);
  void updateConstructions();
  void delayDangerousTasks(const vector<Position>& enemyPos, double delayTime);
  bool isDelayed(Position);
  unordered_map<Position, double, CustomHash<Position>> SERIAL(delayedPos);
  vector<Position> getEnemyPositions() const;
  double manaRemainder = 0;
  double getKillManaScore(const Creature*) const;
  void addMana(double);
  EntitySet<Creature> SERIAL(kills);
  int SERIAL(points) = 0;
  EntityMap<Creature, MinionPaymentInfo> SERIAL(minionPayment);
  int SERIAL(nextPayoutTime);
  EntityMap<Creature, vector<AttractionInfo>> SERIAL(minionAttraction);
  double getAttractionOccupation(const MinionAttraction&);
  Creature* getCopulationTarget(Creature* succubus);
  Creature* getConsumptionTarget(Creature* consumer);
  EntitySet<Creature> SERIAL(pregnancies);
  mutable vector<ItemFetchInfo> itemFetchInfo;
  HeapAllocated<CollectiveTeams> SERIAL(teams);
  HeapAllocated<CollectiveName> SERIAL(name);
  HeapAllocated<CollectiveConfig> SERIAL(config);
  EntitySet<Creature> SERIAL(banished);
  EntitySet<Creature> SERIAL(equipmentUpdates);
  optional<VillainType> SERIAL(villainType);
};

#endif
