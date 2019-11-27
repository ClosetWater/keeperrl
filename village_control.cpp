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
#include "village_control.h"
#include "collective.h"
#include "creature.h"
#include "level.h"
#include "collective_teams.h"
#include "tribe.h"
#include "effect.h"
#include "task.h"
#include "collective_attack.h"
#include "territory.h"
#include "game.h"
#include "collective_name.h"
#include "lasting_effect.h"
#include "body.h"
#include "attack_trigger.h"
#include "immigration.h"
#include "village_behaviour.h"
#include "furniture.h"
#include "creature_attributes.h"
#include "game_event.h"
#include "content_factory.h"
#include "enemy_aggression_level.h"
#include "immigrant_info.h"

SERIALIZATION_CONSTRUCTOR_IMPL(VillageControl)

SERIALIZE_DEF(VillageControl, SUBCLASS(CollectiveControl), SUBCLASS(EventListener), behaviour, victims, myItems, stolenItemCount, attackSizes, entries, maxEnemyPower)
REGISTER_TYPE(ListenerTemplate<VillageControl>)

VillageControl::VillageControl(Private, WCollective col, optional<VillageBehaviour> v) : CollectiveControl(col),
    behaviour(v) {
  for (Position v : col->getTerritory().getAll())
    for (Item* it : v.getItems())
      myItems.insert(it);
}

PVillageControl VillageControl::create(WCollective col, optional<VillageBehaviour> v) {
  auto ret = makeOwner<VillageControl>(Private{}, col, v);
  ret->subscribeTo(col->getModel());
  return ret;
}

WCollective VillageControl::getEnemyCollective() const {
  return collective->getGame()->getPlayerCollective();
}

bool VillageControl::isEnemy(const Creature* c) {
  if (WCollective col = getEnemyCollective())
    return col->getCreatures().contains(c) && !col->hasTrait(c, MinionTrait::DOESNT_TRIGGER);
  else
    return false;
}

void VillageControl::onOtherKilled(const Creature* victim, const Creature* killer) {
  if (victim->getTribe() == collective->getTribe())
    if (isEnemy(killer))
      victims += 0.15; // small increase for same tribe but different village
}

void VillageControl::onMemberKilled(const Creature* victim, const Creature* killer) {
  if (isEnemy(killer))
    victims += 1;
}

void VillageControl::onEvent(const GameEvent& event) {
  using namespace EventInfo;
  event.visit(
      [&](const ItemsPickedUp& info) {
        if (!collective->isConquered() && collective->getTerritory().contains(info.creature->getPosition()))
          if (isEnemy(info.creature) && behaviour)
            if (behaviour->triggers.contains(AttackTrigger(StolenItems{}))) {
              bool wasTheft = false;
              for (const Item* it : info.items)
                if (myItems.contains(it)) {
                  wasTheft = true;
                  ++stolenItemCount;
                  myItems.erase(it);
                }
              if (wasTheft) {
                info.creature->privateMessage(PlayerMessage("You are going to regret this", MessagePriority::HIGH));
              }
            }
      },
      [&](const FurnitureDestroyed& info) {
        if (collective->getTerritory().contains(info.position) &&
            collective->getGame()->getContentFactory()->furniture.getData(info.type).isWall())
          for (auto neighbor : info.position.neighbors8())
            if (auto c = neighbor.getCreature())
              if (isEnemy(c)) {
                entries = true;
                break;
              }
      },
      [&](const auto&) {}
  );
}

void VillageControl::updateAggression(EnemyAggressionLevel level) {
  switch (level) {
    case EnemyAggressionLevel::NONE:
      if (behaviour)
        behaviour->triggers.clear();
      break;
    case EnemyAggressionLevel::EXTREME:
      if (behaviour && !behaviour->triggers.empty())
        behaviour->triggers.push_back(Timer{3000});
      break;
    default:
      break;
  }
}

void VillageControl::launchAttack(vector<Creature*> attackers) {
  if (WCollective enemy = getEnemyCollective()) {
    for (Creature* c : attackers)
//      if (getCollective()->getGame()->canTransferCreature(c, enemy->getLevel()->getModel()))
        collective->getGame()->transferCreature(c, enemy->getModel());
    optional<int> ransom;
    int hisGold = enemy->numResource(CollectiveResourceId::GOLD);
    if (behaviour->ransom && hisGold >= behaviour->ransom->second)
      ransom = max<int>(behaviour->ransom->second,
          (Random.getDouble(behaviour->ransom->first * 0.6, behaviour->ransom->first * 1.5)) * hisGold);
    TeamId team = collective->getTeams().createPersistent(attackers);
    collective->getTeams().activate(team);
    collective->freeTeamMembers(attackers);
    vector<WConstTask> attackTasks;
    for (Creature* c : attackers) {
      auto task = Task::withTeam(collective, team, behaviour->getAttackTask(this));
      attackTasks.push_back(task.get());
      collective->setTask(c, std::move(task));
    }
    enemy->addAttack(CollectiveAttack(std::move(attackTasks), collective, attackers, ransom));
    attackSizes[team] = attackers.size();
  }
}

void VillageControl::considerCancellingAttack() {
  for (auto team : collective->getTeams().getAll()) {
    vector<Creature*> members = collective->getTeams().getMembers(team);
    if (members.size() < (attackSizes[team] + 1) / 2 || (members.size() == 1 &&
          members[0]->getBody().isSeriouslyWounded())) {
      for (Creature* c : members)
        collective->freeFromTask(c);
      collective->getTeams().cancel(team);
    }
  }
}

void VillageControl::onRansomPaid() {
  for (auto team : collective->getTeams().getAll()) {
    vector<Creature*> members = collective->getTeams().getMembers(team);
    for (Creature* c : members)
      collective->freeFromTask(c);
    collective->getTeams().cancel(team);
  }
}

vector<TriggerInfo> VillageControl::getTriggers(WConstCollective against) const {
  vector<TriggerInfo> ret;
  if (collective->getVillainType() != VillainType::ALLY && behaviour && against == getEnemyCollective())
    for (auto& elem : behaviour->triggers) {
      auto value = behaviour->getTriggerValue(elem, this);
      if (value > 0)
        ret.push_back({elem, value});
    }
  return ret;
}

void VillageControl::considerWelcomeMessage() {
  PROFILE;
  auto leader = collective->getLeader();
  if (!leader)
    return;
  if (behaviour)
    if (behaviour->welcomeMessage)
      switch (*behaviour->welcomeMessage) {
        case VillageBehaviour::WelcomeMessage::DRAGON_WELCOME:
          for (Position pos : collective->getTerritory().getAll())
            if (Creature* c = pos.getCreature())
              if (c->isAffected(LastingEffect::INVISIBLE) && isEnemy(c) && c->isPlayer()
                  && leader->canSee(c->getPosition())) {
                c->privateMessage(PlayerMessage("\"Well thief! I smell you and I feel your air. "
                      "I hear your breath. Come along!\"", MessagePriority::CRITICAL));
                behaviour->welcomeMessage.reset();
              }
          break;
      }
}

bool VillageControl::canPerformAttack(bool currentlyActive) {
  // don't attack from remote site when player is currently here
  if (currentlyActive && collective->getModel() != collective->getGame()->getMainModel().get())
    return false;
  // don't attack during the day when having undead minions
  auto& sunlightInfo = collective->getGame()->getSunlightInfo();
  if (sunlightInfo.getState() != SunlightState::NIGHT || sunlightInfo.getTimeRemaining() < 300_visible)
    for (auto c : collective->getCreatures(MinionTrait::FIGHTER))
      if (c->isAffected(LastingEffect::SUNLIGHT_VULNERABLE))
        return false;
  return true;
}

void VillageControl::acceptImmigration() {
  for (int i : All(collective->getImmigration().getImmigrants()))
    collective->getImmigration().setAutoState(i, ImmigrantAutoState::AUTO_ACCEPT);
}

void VillageControl::healAllCreatures() {
  PROFILE;
  const double freq = 0.1;
  if (Random.chance(freq))
    for (auto c : collective->getCreatures())
      c->heal(0.002 / freq);
}

void VillageControl::update(bool currentlyActive) {
  considerWelcomeMessage();
  considerCancellingAttack();
  acceptImmigration();
  healAllCreatures();
  for (auto& c : collective->getCreatures(MinionTrait::FIGHTER))
    if (c->getBody().isHumanoid())
      if (!c->isAffected(LastingEffect::BRIDGE_BUILDING_SKILL))
        c->addPermanentEffect(LastingEffect::BRIDGE_BUILDING_SKILL);
  vector<Creature*> allMembers = collective->getCreatures();
  for (auto team : collective->getTeams().getAll()) {
    for (const Creature* c : collective->getTeams().getMembers(team))
      if (!collective->hasTask(c)) {
        collective->getTeams().cancel(team);
        break;
      }
    return;
  }
  double updateFreq = 0.1;
  if (collective->getVillainType() != VillainType::ALLY && canPerformAttack(currentlyActive) && Random.chance(updateFreq))
    if (behaviour) {
      if (WCollective enemy = getEnemyCollective())
        maxEnemyPower = max(maxEnemyPower, enemy->getDangerLevel());
      double prob = behaviour->getAttackProbability(this) / updateFreq;
      if (Random.chance(prob)) {
        vector<Creature*> fighters;
        fighters = collective->getCreatures(MinionTrait::FIGHTER)
            .filter([](const Creature* c) { return !c->isAffected(LastingEffect::INSANITY); });
        /*if (getCollective()->getGame()->isSingleModel())
          fighters = filter(fighters, [this] (const Creature* c) {
              return contains(getCollective()->getTerritory().getAll(), c->getPosition()); });*/
        /*if (auto& name = collective->getName())
          INFO << name->shortened << " fighters: " << int(fighters.size())
            << (!collective->getTeams().getAll().empty() ? " attacking " : "");*/
        if (fighters.size() >= behaviour->minTeamSize &&
            allMembers.size() >= behaviour->minPopulation + behaviour->minTeamSize)
        launchAttack(getPrefix(Random.permutation(fighters),
          Random.get(behaviour->minTeamSize, min(fighters.size(), allMembers.size() - behaviour->minPopulation) + 1)));
      }
    }
}

