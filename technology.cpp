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
#include "technology.h"
#include "util.h"
#include "skill.h"
#include "collective.h"
#include "level.h"
#include "square_factory.h"
#include "square.h"
#include "cost_info.h"
#include "spell.h"
#include "creature.h"
#include "creature_attributes.h"

void Technology::init() {
  Technology::set(TechId::ALCHEMY, new Technology(
        "alchemy", "Build a laboratory and produce basic potions.", 80));
  Technology::set(TechId::ALCHEMY_ADV, new Technology(
        "advanced alchemy", "Produce more powerful potions.", 200, {TechId::ALCHEMY}));
  Technology::set(TechId::HUMANOID_MUT, new Technology(
        "humanoid mutation", "Breed new, very powerful humanoid species.", 400,{}));
  Technology::set(TechId::BEAST_MUT, new Technology(
        "beast mutation", "Breed new, very powerful beast species.", 400, {}));
  Technology::set(TechId::CRAFTING, new Technology(
        "crafting", "Build a workshop and produce basic equipment.", 40, {}));
  Technology::set(TechId::PIGSTY, new Technology(
        "pig breeding", "Build a pigsty to feed your minions.", 120, {}));
  Technology::set(TechId::IRON_WORKING, new Technology(
        "iron working", "Build a forge and produce metal weapons and armor.", 60, {TechId::CRAFTING}));
  Technology::set(TechId::JEWELLERY, new Technology(
        "jewellery", "Build a jeweler room and produce magical rings and amulets.", 200, {TechId::IRON_WORKING}));
  Technology::set(TechId::TWO_H_WEAP, new Technology(
        "two-handed weapons", "Produce war hammers and battle axes.", 100, {TechId::IRON_WORKING}));
  Technology::set(TechId::TRAPS, new Technology(
        "traps", "Produce traps in the workshop.", 100, {TechId::CRAFTING}));
  Technology::set(TechId::ARCHERY, new Technology(
        "archery", "Produce bows and arrows.", 100, {TechId::CRAFTING}));
  Technology::set(TechId::SPELLS, new Technology(
        "sorcery", "Learn basic spells.", 60, {}));
  Technology::set(TechId::SPELLS_ADV, new Technology(
        "advanced sorcery", "Learn more advanced spells.", 120, {TechId::SPELLS}));
  Technology::set(TechId::SPELLS_MAS, new Technology(
        "master sorcery", "Learn the most powerful spells.", 350, {TechId::SPELLS_ADV}));
  Technology::set(TechId::GEOLOGY1, new Technology(
        "geology", "Discover precious ores in the earth.", 60, {}));
  Technology::set(TechId::GEOLOGY2, new Technology(
        "advanced geology", "Discover more precious ores in the earth.", 180, {TechId::GEOLOGY1}));
  Technology::set(TechId::GEOLOGY3, new Technology(
        "expert geology", "Discover even more precious ores in the earth.", 400, {TechId::GEOLOGY2}));
}

bool Technology::canResearch() const {
  return research;
}

int Technology::getCost() const {
  return cost;
}

vector<Technology*> Technology::getNextTechs(const vector<Technology*>& current) {
  vector<Technology*> ret;
  for (Technology* t : Technology::getAll())
    if (t->canLearnFrom(current) && !contains(current, t))
      ret.push_back(t);
  return ret;
}

Technology::Technology(const string& n, const string& d, int c, const vector<TechId>& pre, bool canR)
    : name(n), description(d), cost(c), research(canR) {
  for (TechId id : pre)
    prerequisites.push_back(Technology::get(id));
}

bool Technology::canLearnFrom(const vector<Technology*>& techs) const {
  vector<Technology*> myPre = prerequisites;
  for (Technology* t : techs)
    removeElementMaybe(myPre, t);
  return myPre.empty();
}

const string& Technology::getName() const {
  return name;
}

const string& Technology::getDescription() const {
  return description;
}

vector<Technology*> Technology::getSorted() {
  vector<Technology*> ret;
  while (ret.size() < getAll().size()) {
    append(ret, getNextTechs(ret));
  }
  return ret;
}

const vector<Technology*> Technology::getPrerequisites() const {
  return prerequisites;
}
  
const vector<Technology*> Technology::getAllowed() const {
  vector<Technology*> ret;
  for (Technology* t : getAll())
    if (contains(t->prerequisites, this))
      ret.push_back(t);
  return ret;
}

static bool areaOk(const vector<Position>& v, SquareId id) {
  for (Position pos : v)
    if (!pos.canConstruct(id))
      return false;
  return true;
}

static vector<Vec2> cutShape(Rectangle rect) {
  vector<Vec2> ret;
  for (Vec2 v : rect)
    if ((v.inRectangle(rect.minusMargin(1)) || !Random.roll(4)) &&
        v != rect.topRight() - Vec2(1, 0) &&
        v != rect.topLeft() &&
        v != rect.bottomLeft() - Vec2(0, 1) &&
        v != rect.bottomRight() - Vec2(1, 1))
      ret.push_back(v);
  return ret;
}


static void addResource(Collective* col, SquareId square, int maxDist) {
  Position init = Random.choose(col->getSquares(SquareId::LIBRARY));
  Rectangle resourceArea(Random.get(4, 7), Random.get(4, 7));
  resourceArea.translate(-resourceArea.middle());
  for (int t = 0; t < 200; ++t) {
    Position center = init.plus(Vec2(Random.get(-maxDist, maxDist + 1), Random.get(-maxDist, maxDist + 1)));
    vector<Position> all = center.getRectangle(resourceArea);
    if (areaOk(all, square)) {
      for (Vec2 pos : cutShape(resourceArea))
        col->addConstruction(center.plus(pos), square, CostInfo::noCost(), true, true);
      return;
    }
  }
}

static void addResources(Collective* col, int numGold, int numIron, int numStone, int maxDist) {
  for (int i : Range(numGold))
    addResource(col, SquareId::GOLD_ORE, maxDist);
  for (int i : Range(numIron))
    addResource(col, SquareId::IRON_ORE, maxDist);
  for (int i : Range(numStone))
    addResource(col, SquareId::STONE, maxDist);
}

struct SpellLearningInfo {
  SpellId id;
  TechId techId;
};

static vector<SpellLearningInfo> spellLearning {
    { SpellId::HEALING, TechId::SPELLS },
    { SpellId::SUMMON_INSECTS, TechId::SPELLS},
    { SpellId::DECEPTION, TechId::SPELLS},
    { SpellId::SPEED_SELF, TechId::SPELLS},
    { SpellId::STUN_RAY, TechId::SPELLS},
    { SpellId::MAGIC_SHIELD, TechId::SPELLS_ADV},
    { SpellId::STR_BONUS, TechId::SPELLS_ADV},
    { SpellId::DEX_BONUS, TechId::SPELLS_ADV},
    { SpellId::FIRE_SPHERE_PET, TechId::SPELLS_ADV},
    { SpellId::TELEPORT, TechId::SPELLS_ADV},
    { SpellId::CURE_POISON, TechId::SPELLS_ADV},
    { SpellId::INVISIBILITY, TechId::SPELLS_MAS},
    { SpellId::BLAST, TechId::SPELLS_MAS},
    { SpellId::WORD_OF_POWER, TechId::SPELLS_MAS},
    { SpellId::PORTAL, TechId::SPELLS_MAS},
    { SpellId::METEOR_SHOWER, TechId::SPELLS_MAS},
};

void Technology::onAcquired(TechId id, Collective* col) {
  switch (id) {
    case TechId::GEOLOGY1: addResources(col, 0, 2, 1, 25); break;
    case TechId::GEOLOGY2: addResources(col, 1, 3, 1, 45); break;
    case TechId::GEOLOGY3: addResources(col, 4, 6, 3, 70); break;
    default: break;
  } 
  if (col->hasLeader())
    for (auto elem : spellLearning)
      if (elem.techId == id)
        col->getLeader()->getAttributes().getSpellMap().add(Spell::get(elem.id));
}

vector<Spell*> Technology::getSpellLearning(TechId tech) {
  vector<Spell*> ret;
  for (auto elem : spellLearning)
    if (elem.techId == tech)
      ret.push_back(Spell::get(elem.id));
  return ret;
}

vector<Spell*> Technology::getAllKeeperSpells() {
  vector<Spell*> ret;
  for (auto elem : spellLearning)
    ret.push_back(Spell::get(elem.id));
  return ret;
}

vector<Spell*> Technology::getAvailableSpells(const Collective* col) {
  vector<Spell*> ret;
  for (auto elem : spellLearning)
    if (col->hasTech(elem.techId))
      ret.push_back(Spell::get(elem.id));
  return ret;
}

TechId Technology::getNeededTech(Spell* spell) {
  for (auto elem : spellLearning)
    if (elem.id == spell->getId())
      return elem.techId;
  FAIL << "Spell not found";
  return TechId(0);
}

