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

#include "creature_attributes.h"
#include "creature.h"
#include "sound.h"
#include "player_message.h"
#include "item.h"
#include "item_factory.h"
#include "body.h"
#include "attack_level.h"
#include "attack_type.h"

CreatureAttributes::CreatureAttributes(function<void(CreatureAttributes&)> fun) {
  fun(*this);
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    lastingEffects[effect] = -500;
}

CreatureAttributes::~CreatureAttributes() {}

template <class Archive> 
void CreatureAttributes::serialize(Archive& ar, const unsigned int version) {
  serializeAll(ar, viewId, illusionViewObject, spawnType, name, attr, chatReactionFriendly);
  serializeAll(ar, chatReactionHostile, barehandedDamage, barehandedAttack, attackEffect, passiveAttack, gender);
  serializeAll(ar, body, innocent);
  serializeAll(ar, animal, stationary, cantEquip, courage);
  serializeAll(ar, carryAnything, invincible, noChase, isSpecial, attributeGain, skills, spells);
  serializeAll(ar, permanentEffects, lastingEffects, minionTasks, attrIncrease, recruitmentCost);
  serializeAll(ar, noAttackSound);
}

SERIALIZABLE(CreatureAttributes);

SERIALIZATION_CONSTRUCTOR_IMPL(CreatureAttributes);

CreatureName& CreatureAttributes::getName() {
  return *name;
}

const CreatureName& CreatureAttributes::getName() const {
  return *name;
}

double CreatureAttributes::getRawAttr(AttrType type) const {
  return attr[type] + attrIncrease[type];
}

void CreatureAttributes::setBaseAttr(AttrType type, int v) {
  attr[type] = v;
}

double CreatureAttributes::getCourage() const {
  if (!body->hasBrain())
    return 1000;
  return courage;
}

void CreatureAttributes::setCourage(double c) {
  courage = c;
}

const Gender& CreatureAttributes::getGender() const {
  return gender;
}

double CreatureAttributes::getExpLevel() const {
  vector<pair<AttrType, int>> countAttr {
    {AttrType::STRENGTH, 12},
    {AttrType::DEXTERITY, 12}};
  double sum = 0;
  for (auto elem : countAttr)
    sum += 10.0 * (getRawAttr(elem.first) / elem.second - 1);
  return max(1.0, sum);
}

void CreatureAttributes::increaseExpLevel(double increase) {
  double l = getExpLevel();
  for (int i : Range(100000)) {
    exerciseAttr(Random.choose<AttrType>(), 0.05);
    if (getExpLevel() >= l + increase)
      break;
  }
}

double exerciseMax = 2.0;
double increaseMult = 0.001; // This translates to about 690 stat exercises to reach 50% of the max increase,
                             // and 2300 to reach 90%

void CreatureAttributes::exerciseAttr(AttrType t, double value) {
  attrIncrease[t] += ((exerciseMax - 1) * attr[t] - attrIncrease[t]) * increaseMult * value;
}

SpellMap& CreatureAttributes::getSpellMap() {
  return spells;
}

Body& CreatureAttributes::getBody() {
  return *body;
}

const Body& CreatureAttributes::getBody() const {
  return *body;
}

const SpellMap& CreatureAttributes::getSpellMap() const {
  return spells;
}

optional<SoundId> CreatureAttributes::getAttackSound(AttackType type, bool damage) const {
  if (!noAttackSound)
    switch (type) {
      case AttackType::HIT:
      case AttackType::PUNCH:
      case AttackType::CRUSH: return damage ? SoundId::BLUNT_DAMAGE : SoundId::BLUNT_NO_DAMAGE;
      case AttackType::CUT:
      case AttackType::STAB: return damage ? SoundId::BLADE_DAMAGE : SoundId::BLADE_NO_DAMAGE;
      default: return none;
    }
  else
    return none;
}

string CreatureAttributes::getDescription() const {
  if (!isSpecial)
    return "";
  string attack;
  if (attackEffect)
    attack = " It has a " + Effect::getName(*attackEffect) + " attack.";
  return body->getDescription() + ". " + attack;
}

void CreatureAttributes::chatReaction(Creature* me, Creature* other) {
  if (me->isEnemy(other) && chatReactionHostile) {
    if (chatReactionHostile->front() == '\"')
      other->playerMessage(*chatReactionHostile);
    else
      other->playerMessage(me->getName().the() + " " + *chatReactionHostile);
  }
  if (!me->isEnemy(other) && chatReactionFriendly) {
    if (chatReactionFriendly->front() == '\"')
      other->playerMessage(*chatReactionFriendly);
    else
      other->playerMessage(me->getName().the() + " " + *chatReactionFriendly);
  }
}

bool CreatureAttributes::isAffected(LastingEffect effect, double time) const {
  return lastingEffects[effect] >= time || isAffectedPermanently(effect);
}

bool CreatureAttributes::considerTimeout(LastingEffect effect, double globalTime) {
  if (lastingEffects[effect] > 0 && lastingEffects[effect] < globalTime) {
    clearLastingEffect(effect);
    if (!isAffected(effect, globalTime))
      return true;
  }
  return false;
}
  
bool CreatureAttributes::considerAffecting(LastingEffect effect, double globalTime, double timeout) {
  bool ret = false;
  if (lastingEffects[effect] < globalTime + timeout) {
    ret = !isAffected(effect, globalTime);
    lastingEffects[effect] = globalTime + timeout;
  }
  return ret;
}

static bool consumeProb() {
  return true;
}

static string getAttrNameMore(AttrType attr) {
  switch (attr) {
    case AttrType::STRENGTH: return "stronger";
    case AttrType::DEXTERITY: return "more agile";
    case AttrType::SPEED: return "faster";
  }
}

template <typename T>
void consumeAttr(T& mine, const T& his, vector<string>& adjectives, const string& adj) {
  if (consumeProb() && mine < his) {
    mine = his;
    if (!adj.empty())
      adjectives.push_back(adj);
  }
}

void consumeAttr(Gender& mine, const Gender& his, vector<string>& adjectives) {
  if (consumeProb() && mine != his) {
    mine = his;
    adjectives.emplace_back(mine == Gender::male ? "more masculine" : "more feminine");
  }
}


template <typename T>
void consumeAttr(optional<T>& mine, const optional<T>& his, vector<string>& adjectives, const string& adj) {
  if (consumeProb() && !mine && his) {
    mine = *his;
    if (!adj.empty())
      adjectives.push_back(adj);
  }
}

void consumeAttr(Skillset& mine, const Skillset& his, vector<string>& adjectives) {
  bool was = false;
  for (SkillId id : his.getAllDiscrete())
    if (!mine.hasDiscrete(id) && Skill::get(id)->transferOnConsumption() && consumeProb()) {
      mine.insert(id);
      was = true;
    }
  for (SkillId id : ENUM_ALL(SkillId)) {
    if (!Skill::get(id)->isDiscrete() && mine.getValue(id) < his.getValue(id)) {
      mine.setValue(id, his.getValue(id));
      was = true;
    }
  }
  if (was)
    adjectives.push_back("more skillfull");
}

void CreatureAttributes::consumeEffects(const EnumMap<LastingEffect, int>& effects) {
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    if (effects[effect] > 0 && !isAffectedPermanently(effect) && consumeProb()) {
      addPermanentEffect(effect);
    }
}

void CreatureAttributes::consume(Creature* self, const CreatureAttributes& other) {
  Debug() << name->bare() << " consume " << other.name->bare();
  self->you(MsgType::CONSUME, other.name->the());
  vector<string> adjectives;
  body->consumeBodyParts(self, other.getBody(), adjectives);
  for (auto t : ENUM_ALL(AttrType))
    consumeAttr(attr[t], other.attr[t], adjectives, getAttrNameMore(t));
  consumeAttr(barehandedDamage, other.barehandedDamage, adjectives, "more dangerous");
  consumeAttr(barehandedAttack, other.barehandedAttack, adjectives, "");
  consumeAttr(attackEffect, other.attackEffect, adjectives, "");
  consumeAttr(passiveAttack, other.passiveAttack, adjectives, "");
  consumeAttr(gender, other.gender, adjectives);
  consumeAttr(skills, other.skills, adjectives);
  if (!adjectives.empty()) {
    self->you(MsgType::BECOME, combine(adjectives));
    self->addPersonalEvent(getName().the() + " becomes " + combine(adjectives));
  }
  consumeEffects(other.permanentEffects);
}

AttackType CreatureAttributes::getAttackType(const Item* weapon) const {
  if (weapon)
    return weapon->getAttackType();
  else if (barehandedAttack)
    return *barehandedAttack;
  else
    return body->isHumanoid() ? AttackType::PUNCH : AttackType::BITE;
}

string CreatureAttributes::getRemainingString(LastingEffect effect, double time) const {
  return "[" + toString<int>(lastingEffects[effect] - time) + "]";
}

bool CreatureAttributes::isStationary() const {
  return stationary;
}

void CreatureAttributes::setStationary(bool s) {
  stationary = s;
}

bool CreatureAttributes::isInvincible() const {
  return invincible;
}

int CreatureAttributes::getRecruitmentCost() const {
  return recruitmentCost;
}

Skillset& CreatureAttributes::getSkills() {
  return skills;
}

const Skillset& CreatureAttributes::getSkills() const {
  return skills;
}

ViewObject CreatureAttributes::createViewObject() const {
  return ViewObject(*viewId, ViewLayer::CREATURE, name->bare());
}

const optional<ViewObject>& CreatureAttributes::getIllusionViewObject() const {
  return illusionViewObject;
}

bool CreatureAttributes::canEquip() const {
  return !cantEquip;
}

bool CreatureAttributes::isAffectedPermanently(LastingEffect effect) const {
  return permanentEffects[effect] > 0 || body->isIntrinsicallyAffected(effect);
}

void CreatureAttributes::shortenEffect(LastingEffect effect, double time) {
  CHECK(lastingEffects[effect] >= time);
  lastingEffects[effect] -= time;
}

void CreatureAttributes::clearLastingEffect(LastingEffect effect) {
  lastingEffects[effect] = 0;
}

void CreatureAttributes::addPermanentEffect(LastingEffect effect) {
  ++permanentEffects[effect];
}

void CreatureAttributes::removePermanentEffect(LastingEffect effect) {
  CHECK(--permanentEffects[effect] >= 0);
}

bool CreatureAttributes::canCarryAnything() const {
  return carryAnything;
}

int CreatureAttributes::getBarehandedDamage() const {
  return barehandedDamage;
}

optional<EffectType> CreatureAttributes::getAttackEffect() const {
  return attackEffect;
}

bool CreatureAttributes::isInnocent() const {
  return innocent;
}

optional<SpawnType> CreatureAttributes::getSpawnType() const {
  return spawnType;
}
 
const MinionTaskMap& CreatureAttributes::getMinionTasks() const {
  return minionTasks;
}

MinionTaskMap& CreatureAttributes::getMinionTasks() {
  return minionTasks;
}

bool CreatureAttributes::dontChase() const {
  return noChase;
}

