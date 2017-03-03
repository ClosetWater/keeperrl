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

#include "view.h"
#include "creature.h"
#include "spell.h"
#include "item.h"
#include "game_info.h"
#include "creature_name.h"
#include "skill.h"
#include "modifier_type.h"
#include "view_id.h"
#include "level.h"
#include "position.h"
#include "creature_attributes.h"
#include "view_object.h"
#include "spell_map.h"

ListElem::ListElem(const string& t, ElemMod m, optional<UserInputId> a) : text(t), mod(m), action(a) {
}

ListElem::ListElem(const string& t, const string& sec, ElemMod m) : text(t), secondColumn(sec), mod(m) {
}

ListElem::ListElem(const char* s, ElemMod m, optional<UserInputId> a) : text(s), mod(m), action(a) {
}

ListElem& ListElem::setTip(const string& s) {
  tooltip = s;
  return *this;
}

ListElem& ListElem::setMessagePriority(MessagePriority p) {
  messagePriority = p;
  return *this;
}

optional<MessagePriority> ListElem::getMessagePriority() const {
  return messagePriority;
}

const string& ListElem::getText() const {
  return text;
}

const string& ListElem::getSecondColumn() const {
  return secondColumn;
}

const string& ListElem::getTip() const {
  return tooltip;
}

ListElem::ElemMod ListElem::getMod() const {
  return mod;
}

void ListElem::setMod(ElemMod m) {
  mod = m;
}

optional<UserInputId> ListElem::getAction() const {
  return action;
}

vector<ListElem> ListElem::convert(const vector<string>& v) {
  return transform2(v, [](const string& s) { return ListElem(s); });
}

View::View() {

}

View::~View() {
}

CreatureInfo::CreatureInfo(const Creature* c) 
    : viewId(c->getViewObject().id()),
      uniqueId(c->getUniqueId()),
      name(c->getName().bare()),
      stackName(c->getName().stack()),
      expLevel((int)c->getAttributes().getVisibleExpLevel()),
      morale(c->getMorale()) {}

string PlayerInfo::getFirstName() const {
  if (!firstName.empty())
    return firstName;
  else
    return capitalFirst(name);
}

string PlayerInfo::getTitle() const {
  return title;
}

vector<PlayerInfo::SkillInfo> getSkillNames(const Creature* c) {
  vector<PlayerInfo::SkillInfo> ret;
  for (auto skill : c->getAttributes().getSkills().getAllDiscrete())
    ret.push_back(PlayerInfo::SkillInfo{Skill::get(skill)->getName(), Skill::get(skill)->getHelpText()});
  for (SkillId id : ENUM_ALL(SkillId))
    if (!Skill::get(id)->isDiscrete() && c->getAttributes().getSkills().getValue(id) > 0)
      ret.push_back(PlayerInfo::SkillInfo{Skill::get(id)->getNameForCreature(c), Skill::get(id)->getHelpText()});
  return ret;
}

void PlayerInfo::readFrom(const Creature* c) {
  firstName = c->getName().first().get_value_or("");
  name = c->getName().bare();
  title = c->getName().title();
  description = capitalFirst(c->getAttributes().getDescription());
  Item* weapon = c->getWeapon();
  weaponName = weapon ? weapon->getName() : "";
  viewId = c->getViewObject().id();
  morale = c->getMorale();
  levelName = c->getLevel()->getName();
  positionHash = c->getPosition().getHash();
  creatureId = c->getUniqueId();
  typedef PlayerInfo::AttributeInfo::Id AttrId;
  attributes = {
    { "Attack",
      AttrId::ATT,
      c->getModifier(ModifierType::DAMAGE),
      c->isAffected(LastingEffect::RAGE) ? 1 : c->isAffected(LastingEffect::PANIC) ? -1 : 0,
      "Affects if and how much damage is dealt in combat."},
    { "Defense",
      AttrId::DEF,
      c->getModifier(ModifierType::DEFENSE),
      c->isAffected(LastingEffect::RAGE) ? -1 : (c->isAffected(LastingEffect::PANIC) 
          || c->isAffected(LastingEffect::MAGIC_SHIELD)) ? 1 : 0,
      "Affects if and how much damage is taken in combat."},
    { "Strength",
      AttrId::STR,
      c->getAttr(AttrType::STRENGTH),
      c->isAffected(LastingEffect::STR_BONUS),
      "Affects the values of attack, defense and carrying capacity."},
    { "Dexterity",
      AttrId::DEX,
      c->getAttr(AttrType::DEXTERITY),
      c->isAffected(LastingEffect::DEX_BONUS),
      "Affects the values of melee and ranged accuracy, and ranged damage."},
    { "Accuracy",
      AttrId::ACC,
      c->getModifier(ModifierType::ACCURACY),
      c->accuracyBonus(),
      "Defines the chance of a successful melee attack and dodging."},
    { "Speed",
      AttrId::SPD,
      c->getAttr(AttrType::SPEED),
      c->isAffected(LastingEffect::SPEED) ? 1 : c->isAffected(LastingEffect::SLOWED) ? -1 : 0,
      "Affects how much game time every action uses."},
/*    { "Level",
      ViewId::STAT_LVL,
      c->getExpLevel(), 0,
      "Describes general combat value of the creature."}*/
  };
  levelInfo.level = c->getAttributes().getVisibleExpLevel();
  for (auto expType : ENUM_ALL(ExperienceType)) {
    levelInfo.increases[expType] = c->getAttributes().getExpIncrease(expType);
    levelInfo.limits[expType] = c->getAttributes().getMaxExpIncrease(expType);
  }
  skills = getSkillNames(c);
  effects.clear();
  for (auto& adj : c->getBadAdjectives())
    effects.push_back({adj.name, adj.help, true});
  for (auto& adj : c->getGoodAdjectives())
    effects.push_back({adj.name, adj.help, false});
  spells.clear();
  for (::Spell* spell : c->getAttributes().getSpellMap().getAll()) {
    bool ready = c->isReady(spell);
    spells.push_back({
        spell->getId(),
        spell->getName() + (ready ? "" : " [" + toString<int>(c->getSpellDelay(spell)) + "]"),
        spell->getDescription(),
        c->isReady(spell) ? none : optional<int>(c->getSpellDelay(spell))});
  }
}

const CreatureInfo* CollectiveInfo::getMinion(UniqueEntity<Creature>::Id id) const {
  for (auto& elem : minions)
    if (elem.uniqueId == id)
      return &elem;
  return nullptr;
}

