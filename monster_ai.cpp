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

#include "monster_ai.h"
#include "level.h"
#include "collective.h"
#include "effect.h"
#include "item.h"
#include "creature.h"
#include "player_message.h"
#include "equipment.h"
#include "spell.h"
#include "creature_name.h"
#include "skill.h"
#include "task.h"
#include "game.h"
#include "creature_attributes.h"
#include "creature_factory.h"
#include "spell_map.h"
#include "effect.h"
#include "body.h"
#include "item_class.h"
#include "furniture.h"
#include "furniture_factory.h"
#include "file_path.h"
#include "ranged_weapon.h"
#include "task_map.h"
#include "collective_teams.h"
#include "collective_config.h"
#include "territory.h"
#include "minion_equipment.h"
#include "zones.h"
#include "team_order.h"
#include "navigation_flags.h"
#include "furniture_tick.h"
#include "time_queue.h"
#include "draw_line.h"
#include "furniture_entry.h"
#include "spell_map.h"
#include "vision.h"
#include "effect_type.h"

class Behaviour {
  public:
  Behaviour(Creature*);
  virtual MoveInfo getMove() { return NoMove; }
  virtual void onAttacked(const Creature* attacker) {}
  virtual double itemValue(const Item*) { return 0; }
  Item* getBestWeapon();
  Creature* getClosestCreature();
  MoveInfo tryEffect(const Effect&, TimeInterval maxTurns = 1_visible);
  MoveInfo tryEffect(const Effect&, Position target);

  virtual ~Behaviour() {}

  SERIALIZE_ALL(creature)
  SERIALIZATION_CONSTRUCTOR(Behaviour)

  protected:
  Creature* SERIAL(creature) = nullptr;
};

MonsterAI::~MonsterAI() {}

SERIALIZE_DEF(MonsterAI, behaviours, weights, creature, pickItems)
SERIALIZATION_CONSTRUCTOR_IMPL(MonsterAI);

Behaviour::Behaviour(Creature* c) : creature(c) {
}

Creature* Behaviour::getClosestCreature() {
  int minDist = 1000000000;
  Creature* result = nullptr;
  for (Creature* other : creature->getVisibleCreatures()) {
    auto otherPos = other->getPosition();
    auto myPos = creature->getPosition();
    if (auto dist = otherPos.dist8(myPos))
      if (other != creature && *dist < minDist) {
        result = other;
        minDist = *dist;
      }
  }
  return result;
}

Item* Behaviour::getBestWeapon() {
  Item* best = nullptr;
  int damage = -1;
  for (Item* item : creature->getEquipment().getItems().filter(Item::classPredicate(ItemClass::WEAPON)))
    if (item->getModifier(AttrType::DAMAGE) > damage) {
      damage = item->getModifier(AttrType::DAMAGE);
      best = item;
    }
  return best;
}

MoveInfo Behaviour::tryEffect(const Effect& type, TimeInterval maxTurns) {
  if (auto effect = type.effect->getValueMaybe<Effects::Lasting>())
    if (creature->isAffected(effect->lastingEffect))
      return NoMove;
  for (auto spell : creature->getSpellMap().getAvailable(creature)) {
    if (spell->getEffect() == type)
      if (auto action = creature->castSpell(spell))
        return { 1, action };
  }
  auto items = creature->getEquipment().getItems().filter(Item::effectPredicate(type));
  for (Item* item : items)
    if (item->getApplyTime() <= maxTurns)
      if (auto action = creature->applyItem(item))
        return MoveInfo(1, action);
  return NoMove;
}

MoveInfo Behaviour::tryEffect(const Effect& type, Position target) {
  for (auto spell : creature->getSpellMap().getAvailable(creature)) {
    if (spell->getEffect() == type)
      if (auto action = creature->castSpell(spell, target))
        return { 1, action };
  }
  return NoMove;
}

static bool isObstructed(const Creature* creature, const vector<Position>& trajectory) {
  vector<Creature*> ret;
  for (int i : Range(1, trajectory.size())) {
    auto& pos = trajectory[i];
    if (pos.stopsProjectiles(creature->getVision().getId()) || (pos.getCreature() && pos != trajectory.back()))
      return true;
  }
  return false;
}

class EffectsAI : public Behaviour {
  public:
  EffectsAI(Creature* c) : Behaviour(c) {}

  virtual double itemValue(const Item* item) {
    if (auto& effect = item->getEffect())
      if (effect->effect->contains<Effects::Heal>())
        return 0.5;
    return 0;
  }

  MoveInfo getThrowMove(Creature* other) {
    auto target = other->getPosition();
    auto trajectory = drawLine(creature->getPosition().getCoord(), target.getCoord())
        .transform([&](Vec2 v) { return Position(v, target.getLevel()); });
    if (isObstructed(creature, trajectory))
      return NoMove;
    for (auto item : creature->getEquipment().getItems())
      if (item->effectAppliedWhenThrown())
        if (auto effect = item->getEffect())
          if (effect->shouldAIApply(creature, target) == EffectAIIntent::WANTED)
            if (!creature->getEquipment().isEquipped(item) &&
               creature->getThrowDistance(item).value_or(-1) >=
                   trajectory.back().dist8(creature->getPosition()).value_or(10000))
              if (auto action = creature->throwItem(item, target, creature->isFriend(other)))
                return action;
    return NoMove;
  }

  virtual MoveInfo getMove() {
    PROFILE_BLOCK("EffectsAI::getMove");
    for (auto spell : creature->getSpellMap().getAvailable(creature))
      if (auto move = spell->getAIMove(creature))
        return move;
    // prevent workers from using up items that they're hauling
    if (!creature->getStatus().contains(CreatureStatus::CIVILIAN))
      for (auto item : creature->getEquipment().getItems())
        if (auto effect = item->getEffect()) {
          if (effect->shouldAIApply(creature, creature->getPosition()) == EffectAIIntent::WANTED)
            if (auto move = creature->applyItem(item))
              return move;
          for (Position pos : creature->getPosition().neighbors8())
            if (Creature* c = pos.getCreature())
              if (creature->isFriend(c) && effect->shouldAIApply(c, c->getPosition()) == EffectAIIntent::WANTED &&
                  c->getEquipment().getItems().filter(Item::effectPredicate(*effect)).empty())
                if (auto action = creature->give(c, {item}))
                  return MoveInfo(0.5, action);
        }
    for (auto c : creature->getVisibleCreatures())
      if (auto move = getThrowMove(c))
        return move;
    return NoMove;
  }


  SERIALIZE_ALL(SUBCLASS(Behaviour))
  SERIALIZATION_CONSTRUCTOR(EffectsAI)
};

class Rest : public Behaviour {
  public:
  Rest(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() {
    return {0.1, creature->wait() };
  }

  SERIALIZATION_CONSTRUCTOR(Rest);
  SERIALIZE_ALL(SUBCLASS(Behaviour));
};

class MoveRandomly : public Behaviour {
  public:
  MoveRandomly(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() {
    if (!visited(creature->getPosition()))
      updateMem(creature->getPosition());
    optional<Position> target;
    double val = 0.0001;
    if (Random.roll(2))
      return {val, creature->wait()};
    for (Position pos : creature->getPosition().neighbors8(Random)) {
      if (Random.roll(10))
        if (auto other = pos.getCreature())
          if (auto petAction = creature->pet(other))
            return petAction;
      if (!visited(pos) && creature->move(pos)) {
        target = pos;
        break;
      }
    }
    if (!target)
      for (Position pos: creature->getPosition().neighbors8(Random))
        if (creature->move(pos)) {
          target = pos;
          break;
        }
    if (!target)
      return {val, creature->wait() };
    else
      return {val, creature->move(*target).append([=] (Creature* creature) {
          updateMem(creature->getPosition());
      })};
  }

  void updateMem(Position pos) {
    memory.push_back(pos);
    if (memory.size() > memSize)
      memory.pop_front();
  }

  bool visited(Position pos) {
    return contains(memory, pos);
  }

  SERIALIZATION_CONSTRUCTOR(MoveRandomly);
  SERIALIZE_ALL(SUBCLASS(Behaviour));

  private:
  deque<Position> memory;
  const int memSize = 3;
};

class StayOnFurniture : public Behaviour {
  public:
  StayOnFurniture(Creature* c, FurnitureType t) : Behaviour(c), type(t) {}

  MoveInfo getMove() override {
    if (!creature->getPosition().getFurniture(type)) {
      if (!nextPigsty)
        for (auto pos : creature->getPosition().getRectangle(Rectangle::centered(Vec2(0, 0), 20)))
          if (pos.getFurniture(type)) {
            nextPigsty = pos;
            break;
          }
      if (nextPigsty)
        if (auto move = creature->moveTowards(*nextPigsty, NavigationFlags().requireStepOnTile()))
          return move;
    }
    if (Random.roll(10))
      for (Position next: creature->getPosition().neighbors8(Random))
        if (next.canEnter(creature) && next.getFurniture(type))
          return creature->move(next);
    return creature->wait();
  }

  SERIALIZATION_CONSTRUCTOR(StayOnFurniture)
  SERIALIZE_ALL(SUBCLASS(Behaviour), type)

  private:
  FurnitureType SERIAL(type);
  optional<Position> nextPigsty;
};

class BirdFlyAway : public Behaviour {
  public:
  BirdFlyAway(Creature* c, int maxDist) : Behaviour(c), maxDist(maxDist) {}

  virtual MoveInfo getMove() override {
    const Creature* enemy = creature->getClosestEnemy();
    if (Random.roll(15) || ( enemy && enemy->getPosition().dist8(creature->getPosition()).value_or(100000) < maxDist))
      if (auto action = creature->flyAway())
        return {1.0, action};
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(BirdFlyAway)
  SERIALIZE_ALL(SUBCLASS(Behaviour), maxDist)

  private:
  int SERIAL(maxDist);
};

class GoldLust : public Behaviour {
  public:
  GoldLust(Creature* c) : Behaviour(c) {}

  virtual double itemValue(const Item* item) {
    if (item->getClass() == ItemClass::GOLD)
      return 1;
    else
      return 0;
  }

  SERIALIZATION_CONSTRUCTOR(GoldLust)
  SERIALIZE_ALL(SUBCLASS(Behaviour))
};

class Wildlife : public Behaviour {
  public:
  Wildlife(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    if (Creature* other = getClosestCreature()) {
      int dist = creature->getPosition().dist8(other->getPosition()).value_or(100000);
      if (dist == 1)
        return creature->attack(other);
      if (dist < 7)
        // pathfinding is expensive so only do it when running away from the player
        return creature->moveAway(other->getPosition(), other->isPlayer());
    }
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(Wildlife)
  SERIALIZE_ALL(SUBCLASS(Behaviour))
};

class AdoxieSacrifice : public Behaviour {
  public:
  AdoxieSacrifice(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    if (creature->getBody().isHumanoid()) {
      auto pos = creature->getPosition();
      for (auto player : pos.getGame()->getPlayerCreatures())
        if (creature->canSee(player)) {
          for (auto v : Vec2::directions8())
            if (auto c = pos.plus(v).getCreature())
              if (c->isAffected(LastingEffect::BLIND) && c->isAffected(LastingEffect::IMMOBILE))
                if (pos.plus(v * 2).canEnter(c) && !pos.plus(v * 2).canEnter(creature))
                  return creature->push(c);
          break;
        }
    }
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(AdoxieSacrifice)
  SERIALIZE_ALL(SUBCLASS(Behaviour))
};

class Fighter : public Behaviour {
  public:
  using Behaviour::Behaviour;

  virtual MoveInfo getMove() override {
    return getMove(true);
  }

  MoveInfo getMove(bool chase) {
    if (Creature* other = creature->getClosestEnemy()) {
      bool chaseThisEnemy = chase && creature->shouldAIChase(other);
      if (!creature->shouldAIAttack(other)) {
        double dist = creature->getPosition().dist8(other->getPosition()).value_or(100000);
        if (dist < 7) {
          if (dist > 3)
            if (auto move = getFireMove(other))
              return move;
          if (chase)
            if (MoveInfo move = getPanicMove(other))
              return move;
          return getAttackMove(other, chaseThisEnemy);
        }
        return NoMove;
      } else
        return getAttackMove(other, chaseThisEnemy);
    } else if (chase)
      return getLastSeenMove();
    else
      return NoMove;
  }

  void addCombatIntent(Creature* attacked, bool immediateAttack) {
    if (attacked->canSee(creature))
      attacked->addCombatIntent(creature, immediateAttack);
    creature->addCombatIntent(attacked, immediateAttack);
  }

  MoveInfo getPanicMove(Creature* other) {
    if (auto teleMove = tryEffect(EffectType(Effects::Escape{})))
      return teleMove;
    if (auto action = creature->moveAway(other->getPosition(), true))
      return {1.0, action.prepend([=](Creature* creature) {
        lastSeen = LastSeen{creature->getPosition(), *creature->getGlobalTime(), LastSeen::PANIC, other->getUniqueId()};
      })};
    else
      return NoMove;
  }

  virtual double itemValue(const Item* item) override {
    if (auto& effect = item->getEffect())
      if (contains<EffectType>({
            Effects::Lasting{LastingEffect::INVISIBLE},
            Effects::Lasting{LastingEffect::SLOWED},
            Effects::Lasting{LastingEffect::BLIND},
            Effects::Lasting{LastingEffect::SLEEP},
            Effects::Lasting{LastingEffect::POISON},
            Effects::Lasting{LastingEffect::POISON_RESISTANT},
            Effects::RemoveLasting{LastingEffect::POISON},
            Effects::Escape{},
            Effects::Lasting{LastingEffect::DAM_BONUS},
            Effects::Lasting{LastingEffect::DEF_BONUS}},
            *effect->effect))
      return 1;
    int damage = item->getModifier(AttrType::DAMAGE);
    Item* best = getBestWeapon();
    if (best && best != item && best->getModifier(AttrType::DAMAGE) >= damage)
        return 0;
    return (double)damage / 50;
  }

  MoveInfo getFireMove(Creature* enemy) {
    auto target = enemy->getPosition();
    auto trajectory = drawLine(creature->getPosition().getCoord(), target.getCoord())
        .transform([&](Vec2 v) { return Position(v, target.getLevel()); });
    if (isObstructed(creature, trajectory))
      return NoMove;
    int dist = *trajectory.back().dist8(creature->getPosition());
    if (dist <= getFiringRange(creature))
      if (auto action = creature->fire(target))
        return {1.0, action.append([=](Creature*) {
            addCombatIntent(enemy, true);
        })};
    return NoMove;
  }

  MoveInfo getLastSeenMove() {
    if (auto lastSeen = getLastSeen()) {
      auto lastSeenTimeout = 20_visible;
      if (!lastSeen->pos.isSameLevel(creature->getPosition()) ||
          lastSeen->time < *creature->getGlobalTime() - lastSeenTimeout ||
          lastSeen->pos == creature->getPosition()) {
        lastSeen = none;
        return NoMove;
      }
      if (lastSeen->type == LastSeen::ATTACK)
        if (auto action = creature->moveTowards(lastSeen->pos)) {
          return {0.5, action};
        }
      if (lastSeen->type == LastSeen::PANIC && lastSeen->pos.dist8(creature->getPosition()).value_or(4) < 4)
        if (auto action = creature->moveAway(lastSeen->pos, true))
          return {0.5, action};
    }
    return NoMove;
  }

  MoveInfo considerCircularBlast() {
    int numEnemies = 0;
    auto pos = creature->getPosition();
    for (Vec2 v : Vec2::directions8())
      if (auto c = pos.plus(v).getCreature())
        if (c->isEnemy(creature))
          ++numEnemies;
    if (numEnemies >= 3)
      if (MoveInfo move = tryEffect(EffectType(Effects::CircularBlast{})))
        return move;
    return NoMove;
  }

  MoveInfo considerEquippingWeapon(Creature* other, int distance) {
    if (creature->getBody().isHumanoid() && !creature->getFirstWeapon()) {
      if (Item* weapon = getBestWeapon())
        if (auto action = creature->equip(weapon))
          return {3.0 / (2.0 + distance), action.prepend([=](Creature*) {
            addCombatIntent(other, false);
        })};
    }
    return NoMove;
  }

  MoveInfo considerBreakingChokePoint(Creature* other) {
  PROFILE;
    unordered_set<Position, CustomHash<Position>> myNeighbors;
    for (auto pos : creature->getPosition().neighbors8(Random))
      myNeighbors.insert(pos);
    MoveInfo destroyMove = NoMove;
    bool isFriendBetween = false;
    for (auto pos : other->getPosition().neighbors8())
      if (myNeighbors.count(pos)) {
        if (pos.canEnter(creature))
          return NoMove;
        if (auto c = pos.getCreature())
          if (c->isFriend(creature)) {
            isFriendBetween = true;
            continue;
          }
        /*if (auto move = creature->move(pos))
          return move;*/
        if (!destroyMove)
          if (auto destroyAction = pos.getBestDestroyAction(creature->getMovementType()))
            if (auto move = creature->destroy(creature->getPosition().getDir(pos), *destroyAction))
              destroyMove = move;
      }
    if (isFriendBetween) {
      if (auto move = tryEffect(EffectType(Effects::DestroyWalls{})))
        return move;
      return destroyMove;
    } else
      return NoMove;
  }

  int getFiringRange(const Creature* c) {
    auto weapon = c->getEquipment().getSlotItems(EquipmentSlot::RANGED_WEAPON);
    if (weapon.empty())
      return 0;
    return weapon.getOnlyElement()->getRangedWeapon()->getMaxDistance();
  }

  bool isChokePoint1(Position pos) {
    return (!pos.minus(Vec2(1, 0)).canEnterEmpty(creature) && !pos.plus(Vec2(1, 0)).canEnterEmpty(creature)) ||
        (!pos.minus(Vec2(0, 1)).canEnterEmpty(creature) && !pos.plus(Vec2(0, 1)).canEnterEmpty(creature));
  }

  bool isChokePoint2(Position pos) {
    for (auto v : pos.neighbors4())
      if (isChokePoint1(v))
        return true;
    return false;
  }

  enum class FighterPosition {
    MELEE,
    HEALER,
    RANGED
  };

  optional<Position> getHealingPosition(Position target) {
    auto isSafe = [this](Position pos) {
      for (auto v : pos.neighbors8())
        if (auto c = v.getCreature())
          if (c->isEnemy(creature))
            return false;
      return true;
    };
    for (auto pos : target.neighbors8(Random))
      if (isSafe(pos))
        return pos;
    return none;
  }

  MoveInfo getHealerFormationMove() {
    MoveInfo unsafeMove = NoMove;
    for (auto ally : creature->getVisibleCreatures())
      if (ally->isFriend(creature) && ally->getBody().getHealth() < 0.7) {
        auto allyPos = ally->getPosition();
        /*if (allyPos.dist8(creature->getPosition()) == 1)
          return creature->castSpell(Spell::get(SpellId::HEAL_OTHER), allyPos);
        else */if (auto healingPos = getHealingPosition(allyPos))
          return creature->moveTowards(*healingPos);
        else
          unsafeMove = creature->moveTowards(ally->getPosition());
      }
    return unsafeMove;
  }

  MoveInfo considerFormationMove(Creature* other, FighterPosition position) {
    if (!LastingEffects::obeysFormation(creature, other) || !creature->getBody().hasBrain())
      return NoMove;
    auto myPosition = creature->getPosition();
    auto otherPosition = other->getPosition();
    Vec2 enemyDir = myPosition.getDir(otherPosition);
    auto distance = enemyDir.length8();
    if (position == FighterPosition::HEALER)
      if (auto move = getHealerFormationMove())
        return move;
    if (position != FighterPosition::MELEE) {
      if (distance == 1)
        return creature->moveAway(otherPosition);
      if (distance == 2)
        return creature->wait();
    }
    if (distance < 2)
      return NoMove;
    if (other->shouldAIAttack(creature) && !isChokePoint2(myPosition)) {
      auto allies = creature->getVisibleCreatures();
      bool allyBehind = false;
      bool allyInFront = false;
      for (auto ally : allies)
        if (ally->isFriend(creature))
          if (auto allysEnemy = ally->getClosestEnemy())
            if (/*allysEnemy == other && */ally->shouldAIAttack(allysEnemy)) {
              auto allyDist = *ally->getPosition().dist8(allysEnemy->getPosition());
              if (allyDist < distance)
                allyInFront = true;
              if (!ally->getPosition().getModel()->getTimeQueue().willMoveThisTurn(ally))
                ++allyDist;
              if (allyDist >= distance + 1)
                allyBehind = true;
            }
      if (allyBehind && !allyInFront)
        return creature->wait();
    }
    return NoMove;
  }

  FighterPosition getFighterPosition() {
    auto ret = FighterPosition::MELEE;
    for (auto spell : creature->getSpellMap().getAvailable(creature))
      if (spell->getRange() > 0 && spell->getEffect().effect->contains<Effects::Heal>())
        ret = FighterPosition::HEALER;
    else if (!creature->getEquipment().getSlotItems(EquipmentSlot::RANGED_WEAPON).empty()
        && creature->getAttr(AttrType::RANGED_DAMAGE) >= creature->getAttr(AttrType::DAMAGE))
      ret = FighterPosition::RANGED;
    if (ret != FighterPosition::MELEE)
      for (auto ally : creature->getVisibleCreatures())
        if (ally->isFriend(creature) && ally->getAttr(AttrType::DAMAGE) > creature->getAttr(AttrType::DAMAGE))
          return ret;
    return FighterPosition::MELEE;
  }

  MoveInfo getAttackMove(Creature* other, bool chase) {
    CHECK(other);
    auto fighterPosition = getFighterPosition();
    if (fighterPosition == FighterPosition::HEALER)
      chase = false;
    if (auto move = considerFormationMove(other, fighterPosition))
      return move;
    if (other->getAttributes().isBoulder())
      return NoMove;
    INFO << creature->getName().bare() << " enemy " << other->getName().bare();
    auto myPosition = creature->getPosition();
    Vec2 enemyDir = myPosition.getDir(other->getPosition());
    auto distance = enemyDir.length8();
    if (auto move = considerEquippingWeapon(other, distance))
      return move;
    if (distance == 1)
      if (auto move = considerCircularBlast())
        return move;
    if (distance > 1) {
      if (MoveInfo move = getFireMove(other))
        return move;
    }
    if (distance > 1) {
      if (chase && !other->getAttributes().dontChase() && !isChaseFrozen(other)) {
        lastSeen = none;
        if (auto action = creature->moveTowards(other->getPosition())) {
          // TODO: instead consider treating a 0-weighted move as NoMove.
          if (distance < 20) {
            return {max(0., 1.0 - double(distance) / 20), action.prepend([=](Creature* creature) {
              addCombatIntent(other, false);
              lastSeen = LastSeen{other->getPosition(), *creature->getGlobalTime(), LastSeen::ATTACK, other->getUniqueId()};
              auto chaseInfo = chaseFreeze.getMaybe(other);
              auto startChaseFreeze = 20_visible;
              auto endChaseFreeze = 20_visible;
              auto time = *other->getGlobalTime();
              if (!chaseInfo || time > chaseInfo->second)
                chaseFreeze.set(other, make_pair(time + startChaseFreeze, time + endChaseFreeze));
            })};
          } else
            return NoMove;
        }
      }
      if (distance == 2)
        if (auto move = considerBreakingChokePoint(other))
          return move;
    }
    if (distance == 1)
      if (auto action = creature->attack(other, getAttackParams(other)))
        return {1.0, action.prepend([=](Creature*) {
            addCombatIntent(other, true);
        })};
    return NoMove;
  }

  Creature::AttackParams getAttackParams(const Creature*) {
    return Creature::AttackParams {};
  }

  SERIALIZATION_CONSTRUCTOR(Fighter)
  SERIALIZE_ALL(SUBCLASS(Behaviour))

  private:
  struct LastSeen {
    Position SERIAL(pos);
    GlobalTime SERIAL(time);
    enum { ATTACK, PANIC} SERIAL(type);
    Creature::Id SERIAL(creature);
    SERIALIZE_ALL(pos, time, type, creature)
  };
  optional<LastSeen> lastSeen;
  optional<LastSeen>& getLastSeen() {
    if (lastSeen && !creature->getLevel()->containsCreature(lastSeen->creature))
      lastSeen.reset();
    return lastSeen;
  }
  EntityMap<Creature, pair<GlobalTime, GlobalTime>> chaseFreeze;

  bool isChaseFrozen(const Creature* c) {
    auto chaseInfo = chaseFreeze.getMaybe(c);
    auto time = *c->getGlobalTime();
    return chaseInfo && chaseInfo->first <= time && chaseInfo->second >= time;
  }
};

class FighterStandGround : public Behaviour {
  public:
  FighterStandGround(Creature* c) : Behaviour(c), fighter(unique<Fighter>(c)) {
  }

  virtual MoveInfo getMove() override {
    return fighter->getMove(false);
  }

  SERIALIZATION_CONSTRUCTOR(FighterStandGround)
  SERIALIZE_ALL(SUBCLASS(Behaviour), fighter)

  private:
  unique_ptr<Fighter> SERIAL(fighter);
};

class GuardTarget : public Behaviour {
  public:
  GuardTarget(Creature* c, double minD, double maxD) : Behaviour(c), minDist(minD), maxDist(maxD) {}

  SERIALIZATION_CONSTRUCTOR(GuardTarget)
  SERIALIZE_ALL(SUBCLASS(Behaviour), minDist, maxDist)

  protected:
  MoveInfo getMoveTowards(Position target) {
    double dist = creature->getPosition().dist8(target).value_or(maxDist);
    if (dist <= minDist)
      return NoMove;
    double exp = 1.5;
    double weight = pow((dist - minDist) / (maxDist - minDist), exp);
    if (auto action = creature->moveTowards(target))
      return {weight, action};
    else
      return NoMove;
  }

  private:
  double SERIAL(minDist);
  double SERIAL(maxDist);
};

class GuardArea : public Behaviour {
  public:
  GuardArea(Creature* c, Rectangle a) : Behaviour(c), area(a) {}

  virtual MoveInfo getMove() override {
    if (!myLevel)
      myLevel = creature->getLevel();
    if (auto action = creature->stayIn(myLevel, area))
      return {1.0, action};
    else
      return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(GuardArea);
  SERIALIZE_ALL(SUBCLASS(Behaviour), myLevel, area);

  private:
  WLevel SERIAL(myLevel) = nullptr;
  Rectangle SERIAL(area);
};

class Wait : public Behaviour {
  public:
  Wait(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    return {1.0, creature->wait()};
  }

  SERIALIZATION_CONSTRUCTOR(Wait);
  SERIALIZE_ALL(SUBCLASS(Behaviour));
};

class DieTime : public Behaviour {
  public:
  DieTime(Creature* c, GlobalTime t) : Behaviour(c), dieTime(t) {}

  virtual MoveInfo getMove() override {
    if (*creature->getGlobalTime() > dieTime) {
      return {1.0, CreatureAction(creature, [=](Creature* creature) {
        creature->dieNoReason(Creature::DropType::NOTHING);
      })};
    }
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(DieTime);
  SERIALIZE_ALL(SUBCLASS(Behaviour), dieTime);

  private:
  GlobalTime SERIAL(dieTime);
};

class Summoned : public GuardTarget {
  public:
  Summoned(Creature* c, Creature* _target, double minDist, double maxDist)
      : GuardTarget(c, minDist, maxDist), target(_target) {
  }

  virtual MoveInfo getMove() override {
    if (target->isDead() || target->isAffected(LastingEffect::STUNNED)) {
      return {1.0, CreatureAction(creature, [=](Creature* creature) {
        creature->dieNoReason(Creature::DropType::NOTHING);
      })};
    }
    if (MoveInfo move = getMoveTowards(target->getPosition()))
      return move.withValue(0.5);
    else
      return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(Summoned);
  SERIALIZE_ALL(SUBCLASS(GuardTarget), target, dieTime);

  private:
  Creature* SERIAL(target) = nullptr;
  GlobalTime SERIAL(dieTime);
};

class Thief : public Behaviour {
  public:
  Thief(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    PROFILE;
    if (!creature->isAffected(LastingEffect::STEALING_SKILL))
      return NoMove;
    for (const Creature* other : creature->getVisibleEnemies()) {
      if (robbed.contains(other)) {
        if (MoveInfo teleMove = tryEffect(EffectType(Effects::Escape{})))
          return teleMove;
        if (auto action = creature->moveAway(other->getPosition()))
        return {1.0, action};
      }
    }
    for (Position pos : creature->getPosition().neighbors8(Random)) {
      const Creature* other = pos.getCreature();
      if (other && !robbed.contains(other)) {
        vector<Item*> allGold;
        for (Item* it : other->getEquipment().getItems())
          if (it->getClass() == ItemClass::GOLD)
            allGold.push_back(it);
        if (allGold.size() > 0)
          if (auto action = creature->stealFrom(creature->getPosition().getDir(other->getPosition()), allGold))
          return {1.0, action.append([=](Creature* creature) {
            other->secondPerson(creature->getName().the() + " steals all your gold!");
            robbed.insert(other);
          })};
      }
    }
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(Thief);
  SERIALIZE_ALL(SUBCLASS(Behaviour), robbed);

  private:
  EntitySet<Creature> SERIAL(robbed);
};

class ByCollective : public Behaviour {
  public:
  ByCollective(Creature* c, WCollective col, unique_ptr<Fighter> fighter) : Behaviour(c), collective(col),
      fighter(std::move(fighter)) {}

  MoveInfo priorityTask() {
    auto& taskMap = collective->getTaskMap();
    if (auto task = taskMap.getTask(creature))
      if (taskMap.isPriorityTask(task))
        return task->getMove(creature).orWait();
    for (auto activity : ENUM_ALL(MinionActivity))
      if (creature->getAttributes().getMinionActivities().isAvailable(collective, creature, activity))
        if (auto task = taskMap.getClosestTask(creature, activity, true)) {
          taskMap.freeFromTask(creature);
          taskMap.takeTask(creature, task);
          return task->getMove(creature).orWait();
        }
    return NoMove;
  }

  optional<TeamId> getActiveTeam() {
    auto& teams = collective->getTeams();
    for (auto team : teams.getContaining(creature))
      if (teams.isActive(team))
        return team;
    return none;
  }

  MoveInfo getFighterMove() {
    auto& teams = collective->getTeams();
    if (auto currentTeam = getActiveTeam())
      if (teams.hasTeamOrder(*currentTeam, creature, TeamOrder::FLEE) ||
          teams.hasTeamOrder(*currentTeam, creature, TeamOrder::STAND_GROUND)) {
        return fighter->getMove(false);
      }
    return fighter->getMove(true);
  }

  MoveInfo followTeamLeader() {
    auto& teams = collective->getTeams();
    if (auto team = getActiveTeam()) {
      if (!teams.hasTeamOrder(*team, creature, TeamOrder::STAND_GROUND)) {
        const Creature* leader = teams.getLeader(*team);
        if (creature != leader) {
          if (leader->getPosition().dist8(creature->getPosition()).value_or(2) > 1)
            return MoveInfo{creature->moveTowards(leader->getPosition())}.orWait();
          else
            return creature->wait();
        } else
          if (creature == leader && !teams.isPersistent(*team))
            return creature->wait();
      } else
        return creature->wait();
    }
    return NoMove;
  }

  PTask getEquipmentTask() {
    if (!collective->usesEquipment(creature))
      return nullptr;
    auto& minionEquipment = collective->getMinionEquipment();
    if (!collective->hasTrait(creature, MinionTrait::NO_AUTO_EQUIPMENT) && Random.roll(40))
      minionEquipment.autoAssign(creature, collective->getAllItems(ItemIndex::MINION_EQUIPMENT, false));
    vector<PTask> tasks;
    for (Item* it : creature->getEquipment().getItems())
      if (!creature->getEquipment().isEquipped(it) && creature->getEquipment().canEquip(it, creature))
        tasks.push_back(Task::equipItem(it));
    {
      PROFILE_BLOCK("tasks assignment");
      for (Position v : collective->getZones().getPositions(ZoneId::STORAGE_EQUIPMENT)) {
        vector<Item*> consumables;
        for (auto item : v.getItems(ItemIndex::MINION_EQUIPMENT))
          if (minionEquipment.isOwner(item, creature)) {
            if (item->canEquip())
              tasks.push_back(Task::pickAndEquipItem(v, item));
            else
              consumables.push_back(item);
          }
        if (!consumables.empty())
          tasks.push_back(Task::pickUpItem(v, consumables));
      }
      if (!tasks.empty())
        return Task::chain(std::move(tasks));
    }
    return nullptr;
  }

  optional<pair<MinionActivity, PTask>> setRandomTask() {
    PROFILE;
    vector<pair<MinionActivity, PTask>> goodTasks;
    for (MinionActivity t : ENUM_ALL(MinionActivity))
      if (creature->getAttributes().getMinionActivities().canChooseRandomly(creature, t)) {
        if (collective->isActivityGoodAssumingHaveTasks(creature, t)) {
          if (MinionActivities::getExisting(collective, creature, t) || t == MinionActivity::IDLE)
            goodTasks.push_back({t, nullptr});
          if (auto generated = collective->getMinionActivities().generate(collective, creature, t))
            goodTasks.push_back({t, std::move(generated)});
        }
      }
    if (!goodTasks.empty()) {
      auto ret = Random.choose(std::move(goodTasks));
      collective->setMinionActivity(creature, ret.first);
      return std::move(ret);
    }
    return none;
  }

  EnumMap<MinionActivity, optional<LocalTime>> lastTimeGeneratedActivity;
  optional<LocalTime> lastTimeSetRandomTask;

  WTask getStandardTask() {
    PROFILE;
    auto& taskMap = collective->getTaskMap();
    auto current = collective->getCurrentActivity(creature);
    optional<pair<MinionActivity, PTask>> generatedCache;
    auto generate = [&] (MinionActivity activity) -> PTask {
      if (generatedCache && generatedCache->first == activity)
        return std::move(generatedCache->second);
      /*if ((!collective->hasTrait(creature, MinionTrait::WORKER) || activity == MinionActivity::IDLE)
          && !Random.roll(30)
          && lastTimeGeneratedActivity[activity]
          && *lastTimeGeneratedActivity[activity] >= collective->getLocalTime() - 10_visible)
        return nullptr;*/
      lastTimeGeneratedActivity[activity] = collective->getLocalTime();
      return collective->getMinionActivities().generate(collective, creature, activity);
    };
    if (current.activity == MinionActivity::IDLE || !collective->isActivityGood(creature, current.activity)) {
      collective->setMinionActivity(creature, MinionActivity::IDLE);
      /*if (Random.roll(30) || !lastTimeSetRandomTask ||
          *lastTimeSetRandomTask < collective->getLocalTime() - 3_visible) {*/
        generatedCache = setRandomTask();
        lastTimeSetRandomTask = collective->getLocalTime();
      //}
    }
    current = collective->getCurrentActivity(creature);
    MinionActivity activity = current.activity;
    if (current.finishTime < collective->getLocalTime())
      collective->setMinionActivity(creature, MinionActivity::IDLE);
    if (PTask ret = MinionActivities::generateDropTask(collective, creature, activity))
      return taskMap.addTaskFor(std::move(ret), creature);
    if (WTask ret = MinionActivities::getExisting(collective, creature, activity)) {
      taskMap.takeTask(creature, ret);
      return ret;
    }
    if (PTask ret = generate(activity))
      return taskMap.addTaskFor(std::move(ret), creature);
    return nullptr;
  }

  MoveInfo goToAlarm() {
    auto& alarmInfo = collective->getAlarmInfo();
    if (collective->hasTrait(creature, MinionTrait::FIGHTER) && alarmInfo && alarmInfo->finishTime > collective->getGlobalTime())
      if (auto action = creature->moveTowards(alarmInfo->position))
        return {1.0, action};
    return NoMove;
  }

  MoveInfo normalTask() {
    if (WTask task = collective->getTaskMap().getTask(creature))
      return task->getMove(creature).orWait();
    return NoMove;
  }

  MoveInfo newEquipmentTask() {
    if (PTask t = getEquipmentTask())
      if (auto move = t->getMove(creature)) {
        collective->getTaskMap().addTaskFor(std::move(t), creature);
        return move;
      }
    return NoMove;
  }

  MoveInfo newStandardTask() {
    if (auto t = getStandardTask())
      if (auto move = t->getMove(creature))
        return move;
    return creature->wait();
  }

  void considerHealingActivity() {
    PROFILE;
    // don't switch the activity every turn as this may cancel and existing sleep task
    // at the moment the creature is about to go to sleep causing it to wonder around the bedroom
    if (Random.roll(5) && (collective->getConfig().allowHealingTaskOutsideTerritory() ||
        collective->getTerritory().contains(creature->getPosition()))) {
      const static EnumSet<MinionActivity> healingActivities {MinionActivity::SLEEP};
      auto currentActivity = collective->getCurrentActivity(creature).activity;
      if (creature->getBody().canHeal(HealthType::FLESH) && !creature->isAffected(LastingEffect::POISON) &&
          !healingActivities.contains(currentActivity))
        for (MinionActivity activity : healingActivities) {
          if (creature->getAttributes().getMinionActivities().isAvailable(collective, creature, activity) &&
              collective->isActivityGood(creature, activity)) {
            collective->freeFromTask(creature);
            collective->setMinionActivity(creature, activity);
            return;
          }
        }
    }
  }

  static MoveInfo getFirstGoodMove() {
    return NoMove;
  }

  template <typename MoveFun1, typename... MoveFuns>
  static MoveInfo getFirstGoodMove(MoveFun1&& f1, MoveFuns&&... funs) {
    if (auto move = std::forward<MoveFun1>(f1)())
      return move;
    else
      return getFirstGoodMove(std::forward<MoveFuns>(funs)...);
  }

  virtual MoveInfo getMove() override {
    PROFILE_BLOCK("ByCollective::getMove");
    considerHealingActivity();
    return getFirstGoodMove(
        bindMethod(&ByCollective::getFighterMove, this),
        bindMethod(&ByCollective::priorityTask, this),
        bindMethod(&ByCollective::followTeamLeader, this),
        bindMethod(&ByCollective::goToAlarm, this),
        bindMethod(&ByCollective::normalTask, this),
        bindMethod(&ByCollective::newEquipmentTask, this),
        bindMethod(&ByCollective::newStandardTask, this)
    );
  }

  SERIALIZATION_CONSTRUCTOR(ByCollective)
  SERIALIZE_ALL(SUBCLASS(Behaviour), collective, fighter)

  private:
  WCollective SERIAL(collective) = nullptr;
  unique_ptr<Fighter> SERIAL(fighter);
};

class ChooseRandom : public Behaviour {
  public:
  ChooseRandom(Creature* c, vector<PBehaviour>&& beh, vector<double> w)
      : Behaviour(c), behaviours(std::move(beh)), weights(w) {}

  virtual MoveInfo getMove() override {
    return behaviours[Random.get(weights)]->getMove();
  }

  SERIALIZATION_CONSTRUCTOR(ChooseRandom);
  SERIALIZE_ALL(SUBCLASS(Behaviour), behaviours, weights);

  private:
  vector<PBehaviour> SERIAL(behaviours);
  vector<double> SERIAL(weights);
};

class SingleTask : public Behaviour {
  public:
  SingleTask(Creature* c, PTask t) : Behaviour(c), task(std::move(t)) {}

  virtual MoveInfo getMove() override {
    return task->getMove(creature);
  };

  SERIALIZATION_CONSTRUCTOR(SingleTask);
  SERIALIZE_ALL(SUBCLASS(Behaviour), task);

  private:
  PTask SERIAL(task);
};

const static Vec2 splashTarget = Level::getSplashBounds().middle() - Vec2(3, 0);
const static Vec2 splashLeaderPos = Vec2(Level::getSplashVisibleBounds().right(),
    Level::getSplashBounds().middle().y) - Vec2(4, 0);

class SplashHeroes : public Behaviour {
  public:
  SplashHeroes(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    creature->getAttributes().setCourage(1);
    if (!started && creature->getPosition().withCoord(splashLeaderPos).getCreature())
      started = true;
    if (!started)
      return creature->wait();
    else {
      if (creature->getPosition().getCoord().x > splashTarget.x)
        return {0.1, creature->move(Vec2(-1, 0))};
      else
        return {0.1, creature->wait()};
    }
  };

  SERIALIZATION_CONSTRUCTOR(SplashHeroes);
  SERIALIZE_ALL(SUBCLASS(Behaviour));

  private:
  bool started = false;
};

class SplashHeroLeader : public Behaviour {
  public:
  SplashHeroLeader(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    creature->getAttributes().setCourage(1);
    Vec2 pos = creature->getPosition().getCoord();
    if (started)
      return creature->moveTowards(creature->getPosition().withCoord(splashTarget));
    if (pos == splashLeaderPos)
      for (Vec2 v : {Vec2(2, 0), Vec2(2, -1), Vec2(2, 1), Vec2(3, 0), Vec2(3, -1), Vec2(3, 1)})
        if (creature->getPosition().plus(v).getCreature())
          started = true;
    if (pos != splashLeaderPos) {
      if (pos.y == splashLeaderPos.y)
        return creature->move(Vec2(-1, 0));
      else
        return creature->moveTowards(creature->getPosition().withCoord(splashLeaderPos));
    } else
      return creature->wait();
  };

  SERIALIZATION_CONSTRUCTOR(SplashHeroLeader);
  SERIALIZE_ALL(SUBCLASS(Behaviour));

  private:

  bool started = false;
};

class SplashMonsters : public Behaviour {
  public:
  SplashMonsters(Creature* c) : Behaviour(c) {}

  virtual MoveInfo getMove() override {
    creature->getAttributes().setCourage(1);
    if (!initialPos)
      initialPos = creature->getPosition().getCoord();
    vector<Creature*> heroes;
    for (auto c : creature->getLevel()->getAllCreatures())
      if (c->isEnemy(creature))
        heroes.push_back(c);
    if (heroes.empty()) {
      if (creature->getPosition().getCoord() == *initialPos)
        return creature->wait();
      else
        return creature->moveTowards(Position(*initialPos, creature->getLevel()));
    }
    if (const Creature* other = Position(splashTarget, creature->getLevel()).getCreature())
      if (creature->isEnemy(other))
        attack = true;
    if (!attack)
      return creature->wait();
    else
      return {0.1, creature->moveTowards(Random.choose(heroes)->getPosition())};
  };

  SERIALIZATION_CONSTRUCTOR(SplashMonsters);
  SERIALIZE_ALL(SUBCLASS(Behaviour));

  private:
  optional<Vec2> initialPos;
  bool attack = false;
};

class SplashItems {
  public:
  void addItems(Vec2 pos, vector<Item*> v) {
    items[pos] = v;
  }

  Vec2 chooseClosest(Vec2 pos) {
    Vec2 ret(1000, 1000);
    for (Vec2 v : getKeys(items))
      if (v.dist8(pos) < ret.dist8(pos))
        ret = v;
    return ret;
  }

  PTask getNextTask(Vec2 position, WLevel level) {
    if (items.empty())
      return nullptr;
    Vec2 pos = chooseClosest(position);
    vector<Item*> it = {Random.choose(items[pos])};
    if (it[0]->getClass() == ItemClass::GOLD)
      for (Item* it2 : copyOf(items[pos]))
        if (it[0] != it2 && it2->getClass() == it[0]->getClass() && Random.roll(10))
          it.push_back(it2);
    for (Item* it2 : it)
      items[pos].removeElement(it2);
    if (items[pos].empty())
      items.erase(pos);
    vector<Vec2>& targets = it[0]->getClass() == ItemClass::GOLD ? targetsGold : targetsCorpse;
    if (targets.empty())
      return nullptr;
    Vec2 target = Random.choose(targets);
    targets.removeElement(target);
    return Task::bringItem(Position(pos, level), it, {Position(target, level)});
  }

  void setInitialized(const FilePath& splashPath) {
    initialized = true;
    ifstream iff(splashPath.getPath());
    CHECK(!!iff);
    Vec2 sz;
    iff >> sz.x >> sz.y;
    for (int i : Range(sz.y)) {
      string s;
      iff >> s;
      for (int j : Range(sz.x)) {
        if (s[j] == '1')
          targetsGold.push_back(Level::getSplashVisibleBounds().topLeft() + Vec2(j, i));
        else if (s[j] == '2')
          targetsCorpse.push_back(Level::getSplashVisibleBounds().topLeft() + Vec2(j, i));
      }
    }
  }

  bool isInitialized() {
    return initialized;
  }

  private:
  map<Vec2, vector<Item*>> items;
  bool initialized = false;
  vector<Vec2> targetsGold;
  vector<Vec2> targetsCorpse;
};

static SplashItems splashItems;

class SplashImps : public Behaviour {
  public:
  SplashImps(Creature* c, const FilePath& splash) : Behaviour(c), splashPath(splash) {}

  void initializeSplashItems() {
    for (Vec2 v : Level::getSplashVisibleBounds()) {
      vector<Item*> inv = Position(v, creature->getLevel()).getItems().filter(
          [](const Item* it) { return it->getClass() == ItemClass::GOLD || it->getClass() == ItemClass::CORPSE;});
      if (!inv.empty())
        splashItems.addItems(v, inv);
    }
    splashItems.setInitialized(*splashPath);
  }

  virtual MoveInfo getMove() override {
    creature->addEffect(LastingEffect::SPEED, 1000_visible);
    if (!initialPos)
      initialPos = creature->getPosition().getCoord();
    bool heroesDead = true;
    for (const Creature* c : creature->getLevel()->getAllCreatures())
      if (c->isEnemy(creature)) {
        heroesDead = false;
      }
    if (heroesDead) {
      if (!splashItems.isInitialized())
        initializeSplashItems();
      if (task) {
        if (!task->isDone())
          return task->getMove(creature);
        else
          task.clear();
      }
      task = splashItems.getNextTask(creature->getPosition().getCoord(), creature->getLevel());
      if (!task)
        return creature->moveTowards(Position(*initialPos, creature->getLevel()));
      else
        return task->getMove(creature);
    }
    return creature->wait();
  }

  SERIALIZATION_CONSTRUCTOR(SplashImps);
  SERIALIZE_ALL(SUBCLASS(Behaviour));

  private:
  optional<Vec2> initialPos;
  PTask task;
  optional<FilePath> splashPath;
};

class AvoidFire : public Behaviour {
  public:
  using Behaviour::Behaviour;

  virtual MoveInfo getMove() override {
    auto myPosition = creature->getPosition();
    if (myPosition.isBurning() && !creature->isAffected(LastingEffect::FIRE_RESISTANT)) {
      for (Position pos : myPosition.neighbors8(Random))
        if (!pos.isBurning())
          if (auto action = creature->move(pos))
            return action;
      for (Position pos : myPosition.neighbors8(Random))
        if (auto action = creature->forceMove(pos))
          return action;
    }
    if (creature->isAffected(LastingEffect::ON_FIRE)) {
      optional<Position> closestWater;
      for (auto pos : myPosition.getRectangle(Rectangle::centered(7)))
        if (auto f = pos.getFurniture(FurnitureLayer::GROUND))
          if (f->getEntryType() && f->getEntryType()->entryData.contains<FurnitureEntry::Water>() &&
              (!closestWater || *closestWater->dist8(myPosition) > *pos.dist8(myPosition)))
            closestWater = pos;
      if (closestWater)
        return creature->moveTowards(*closestWater, NavigationFlags().requireStepOnTile());
    }
    return NoMove;
  }

  SERIALIZATION_CONSTRUCTOR(AvoidFire)
  SERIALIZE_ALL(SUBCLASS(Behaviour))
};

REGISTER_TYPE(EffectsAI);
REGISTER_TYPE(Rest);
REGISTER_TYPE(MoveRandomly);
REGISTER_TYPE(BirdFlyAway);
REGISTER_TYPE(GoldLust);
REGISTER_TYPE(Wildlife);
REGISTER_TYPE(Fighter);
REGISTER_TYPE(FighterStandGround);
REGISTER_TYPE(GuardTarget);
REGISTER_TYPE(GuardArea);
REGISTER_TYPE(Summoned);
REGISTER_TYPE(DieTime);
REGISTER_TYPE(Wait);
REGISTER_TYPE(Thief);
REGISTER_TYPE(ByCollective);
REGISTER_TYPE(ChooseRandom);
REGISTER_TYPE(SingleTask);
REGISTER_TYPE(AvoidFire);
REGISTER_TYPE(StayOnFurniture);
REGISTER_TYPE(AdoxieSacrifice);

MonsterAI::MonsterAI(Creature* c, const vector<Behaviour*>& beh, const vector<int>& w, bool pick) :
    weights(w), creature(c), pickItems(pick) {
  CHECK(beh.size() == w.size());
  for (auto b : beh)
    behaviours.push_back(PBehaviour(b));
}

void MonsterAI::makeMove() {
  vector<MoveInfo> moves;
  for (int i : All(behaviours)) {
    MoveInfo move = behaviours[i]->getMove();
    move.setValue(max(0.0, min(1.0, move.getValue())) * weights[i]);
    moves.push_back(move);
    bool skipNextMoves = false;
    if (i < behaviours.size() - 1) {
      CHECK(weights[i] >= weights[i + 1]);
      if (moves.back().getValue() > weights[i + 1])
        skipNextMoves = true;
    }
    if (pickItems)
      for (auto& stack : Item::stackItems(creature->getPickUpOptions())) {
        Item* item = stack[0];
        if (!item->isOrWasForSale() && creature->pickUp(stack))
          moves.push_back(MoveInfo({ behaviours[i]->itemValue(item) * weights[i], creature->pickUp(stack)}));
      }
    if (skipNextMoves)
      break;
  }
  /*vector<Item*> inventory = creature->getEquipment().getItems([this](Item* item) { return !creature->getEquipment().isEquiped(item);});
  for (Item* item : inventory) {
    bool useless = true;
    for (PBehaviour& behaviour : behaviours)
      if (behaviour->itemValue(item) > 0)
        useless = false;
    if (useless)
      moves.push_back({ 0.01, [=]() {
        creature->drop({item});
      }});
  }*/
  MoveInfo winner = NoMove;
  for (int i : All(moves))
    if (moves[i].getValue() > winner.getValue())
      winner = moves[i];
  CHECK(winner.getValue() > 0);
  winner.getMove().perform(creature);
}

PMonsterAI MonsterAIFactory::getMonsterAI(Creature* c) const {
  return PMonsterAI(maker(c));
}

MonsterAIFactory::MonsterAIFactory(MakerFun _maker) : maker(_maker) {
}

MonsterAIFactory MonsterAIFactory::guard() {
  return MonsterAIFactory([=](Creature* c) {
      vector<Behaviour*> actors {
          new AvoidFire(c),
          new EffectsAI(c),
          new FighterStandGround(c),
          new Wait(c)
      };
      vector<int> weights { 10, 5, 4, 1 };
      return new MonsterAI(c, actors, weights);
  });
}

MonsterAIFactory MonsterAIFactory::monster() {
  return stayInLocation(Level::getMaxBounds());
}

MonsterAIFactory MonsterAIFactory::collective(WCollective col) {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
        new AvoidFire(c),
        new AdoxieSacrifice(c),
        new EffectsAI(c),
        new ByCollective(c, col, unique<Fighter>(c)),
        new ChooseRandom(c, makeVec(PBehaviour(new Rest(c)), PBehaviour(new MoveRandomly(c))), {3, 1})},
        { 10, 9, 6, 2, 1}, false);
      });
}

MonsterAIFactory MonsterAIFactory::stayInLocation(Rectangle rect, bool moveRandomly) {
  return MonsterAIFactory([=](Creature* c) {
      vector<Behaviour*> actors {
          new AvoidFire(c),
          new EffectsAI(c),
          new Thief(c),
          new Fighter(c),
          new GoldLust(c),
          new GuardArea(c, rect)
      };
      vector<int> weights { 10, 5, 4, 3, 1, 1 };
      if (moveRandomly) {
        actors.push_back(new MoveRandomly(c));
        weights.push_back(1);
      } else {
        actors.push_back(new Wait(c));
        weights.push_back(1);
      }
      return new MonsterAI(c, actors, weights);
  });
}

MonsterAIFactory MonsterAIFactory::singleTask(PTask&& t, bool chaseEnemies) {
  // Since the lambda can't capture OwnedPointer, let's release it to shared_ptr and recreate PTask inside the lambda.
  auto released = t.giveMeSharedPointer();
  return MonsterAIFactory([=](Creature* c) mutable {
      CHECK(released);
      auto task = PTask(released);
      released = nullptr;
      return new MonsterAI(c, {
        new EffectsAI(c),
        chaseEnemies ? (Behaviour*)(new Fighter(c)) : (Behaviour*)(new FighterStandGround(c)),
        new SingleTask(c, std::move(task)),
        new ChooseRandom(c, makeVec(PBehaviour(new Rest(c)), PBehaviour(new MoveRandomly(c))), {3, 1})},
        { 6, 5, 2, 1}, true);
      });
}

MonsterAIFactory MonsterAIFactory::wildlifeNonPredator() {
  return MonsterAIFactory([](Creature* c) {
      return new MonsterAI(c, {
          new Wildlife(c),
          new FighterStandGround(c),
          new MoveRandomly(c)},
          {6, 5, 1});
      });
}

MonsterAIFactory MonsterAIFactory::moveRandomly() {
  return MonsterAIFactory([](Creature* c) {
      return new MonsterAI(c, {
          new MoveRandomly(c)},
          {1});
      });
}

MonsterAIFactory MonsterAIFactory::stayOnFurniture(FurnitureType type) {
  return MonsterAIFactory([type](Creature* c) {
      return new MonsterAI(c, {
          new AvoidFire(c),
          new Fighter(c),
          new StayOnFurniture(c, type)},
          {5, 2, 1});
      });
}

MonsterAIFactory MonsterAIFactory::idle() {
  return MonsterAIFactory([](Creature* c) {
      return new MonsterAI(c, {
          new Rest(c)},
          {1});
      });
}

MonsterAIFactory MonsterAIFactory::scavengerBird() {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
          new BirdFlyAway(c, 3),
          new MoveRandomly(c),
      },
      {2, 1});
      });
}

MonsterAIFactory MonsterAIFactory::summoned(Creature* leader) {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
          new Summoned(c, leader, 1, 3),
          new AvoidFire(c),
          new EffectsAI(c),
          new Fighter(c),
          new MoveRandomly(c),
          new GoldLust(c)},
          { 6, 5, 4, 3, 1, 1 });
      });
}

MonsterAIFactory MonsterAIFactory::splashHeroes(bool leader) {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
        leader ? (Behaviour*)new SplashHeroLeader(c) : (Behaviour*)new SplashHeroes(c),
        new EffectsAI(c),
        new Fighter(c),
        new ChooseRandom(c, makeVec(PBehaviour(new Rest(c)), PBehaviour(new MoveRandomly(c))), {3, 1})},
        { 6, 5, 2, 1}, false);
      });
}

MonsterAIFactory MonsterAIFactory::splashMonsters() {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
        new SplashMonsters(c),
        new EffectsAI(c),
        new Fighter(c),
        new ChooseRandom(c, makeVec(PBehaviour(new Rest(c)), PBehaviour(new MoveRandomly(c))), {3, 1})},
        { 6, 5, 2, 1}, false);
      });
}

MonsterAIFactory MonsterAIFactory::splashImps(const FilePath& splashPath) {
  return MonsterAIFactory([=](Creature* c) {
      return new MonsterAI(c, {
        new SplashImps(c, splashPath),
        new EffectsAI(c),
        new Fighter(c),
        new ChooseRandom(c, makeVec(PBehaviour(new Rest(c)), PBehaviour(new MoveRandomly(c))), {3, 1})},
        { 6, 5, 2, 1}, false);
      });
}

