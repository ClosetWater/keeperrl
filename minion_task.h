#ifndef _MINION_TASK_H
#define _MINION_TASK_H

#include "util.h"

RICH_ENUM(MinionTask,
  SLEEP,
  GRAVE,
  TRAIN,
  EAT,
  WORKSHOP,
  FORGE,
  LABORATORY,
  JEWELER,
  STUDY,
  PRISON,
  TORTURE,
  //SACRIFICE,
  EXECUTE,
  //WORSHIP,
  LAIR,
  EXPLORE,
  EXPLORE_NOCTURNAL,
  EXPLORE_CAVES,
  COPULATE,
  CONSUME,
  RITUAL,
  CROPS,
  SPIDER,
  THRONE
);

inline vector<MinionTask> getWorkshopTasks() {
  return {MinionTask::WORKSHOP, MinionTask::FORGE, MinionTask::LABORATORY, MinionTask::JEWELER};
}

RICH_ENUM(MinionTrait,
  LEADER,
  FIGHTER,
  WORKER,
  PRISONER,
  SUMMONED,
  NO_EQUIPMENT,
  NO_RETURNING,
  NO_LIMIT,
  FARM_ANIMAL
);


RICH_ENUM(SpawnType,
  HUMANOID,
  UNDEAD,
  BEAST,
  DEMON
);

#endif
