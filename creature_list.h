#pragma once

#include "util.h"
#include "experience_type.h"

class TribeId;
class MonsterAIFactory;
class ItemType;

struct CreatureList {
  CreatureList();
  ~CreatureList();
  CreatureList(const CreatureList&);
  CreatureList(CreatureList&&);
  CreatureList& operator = (const CreatureList&);
  CreatureList& operator = (CreatureList&&);
  CreatureList(int count, CreatureId);
  explicit CreatureList(CreatureId);
  CreatureList(int count, vector<CreatureId>);
  CreatureList(int count, vector<pair<int, CreatureId>>);
  CreatureList& addUnique(CreatureId);
  CreatureList& increaseBaseLevel(EnumMap<ExperienceType, int>);
  CreatureList& increaseExpLevel(EnumMap<ExperienceType, int>);
  CreatureList& addInventory(vector<ItemType>);

  string getSummary(CreatureFactory* factory) const;

  ViewId getViewId(CreatureFactory*) const;
  vector<PCreature> generate(RandomGen&, CreatureFactory*, TribeId, MonsterAIFactory) const;

  template<typename Archive>
  void serialize(Archive&, const unsigned int);

  private:
  Range SERIAL(count) = Range(0, 1);
  vector<CreatureId> SERIAL(uniques);
  using Freq = pair<int, CreatureId>;
  vector<Freq> SERIAL(all);
  EnumMap<ExperienceType, int> SERIAL(baseLevelIncrease);
  EnumMap<ExperienceType, int> SERIAL(expLevelIncrease);
  vector<ItemType> SERIAL(inventory);
};
