#pragma once

#include "creature_factory.h"
#include "attack_behaviour.h"
#include "task_callback.h"
#include "inhabitants_info.h"

struct ExternalEnemy {
  CreatureList SERIAL(creatures);
  AttackBehaviour SERIAL(behaviour);
  string SERIAL(name);
  Range SERIAL(attackTime);
  int SERIAL(maxOccurences);
  SERIALIZE_ALL(creatures, behaviour, name, attackTime, maxOccurences)
};

struct EnemyEvent {
  ExternalEnemy SERIAL(enemy);
  int SERIAL(attackTime);
  ViewId SERIAL(viewId);
  SERIALIZE_ALL(enemy, attackTime, viewId)
};

class ExternalEnemies {
  public:
  ExternalEnemies(RandomGen&, vector<ExternalEnemy>);
  void update(WLevel, double localTime);
  struct CurrentWave {
    string SERIAL(name);
    vector<WCreature> SERIAL(attackers);
    SERIALIZE_ALL(name, attackers)
  };
  optional<const EnemyEvent&> getNextWave() const;
  int getNextWaveIndex() const;
  optional<EnemyEvent> popNextWave(double localTime);

  SERIALIZATION_DECL(ExternalEnemies)

  private:
  void updateCurrentWaves(WCollective target);
  OwnerPointer<TaskCallback> callbackDummy = makeOwner<TaskCallback>();
  vector<CurrentWave> SERIAL(currentWaves);
  int SERIAL(nextWave) = 0;
  vector<EnemyEvent> SERIAL(waves);
  PTask getAttackTask(WCollective target, AttackBehaviour);
};
