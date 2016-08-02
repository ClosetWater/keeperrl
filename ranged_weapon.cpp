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

#include "ranged_weapon.h"
#include "creature.h"
#include "level.h"
#include "attack.h"
#include "modifier_type.h"
#include "view.h"
#include "game.h"
#include "sound.h"
#include "attack_level.h"
#include "attack_type.h"

template <class Archive> 
void RangedWeapon::serialize(Archive& ar, const unsigned int version) {
  ar & SUBCLASS(Item);
}

SERIALIZABLE(RangedWeapon);

SERIALIZATION_CONSTRUCTOR_IMPL(RangedWeapon);

RangedWeapon::RangedWeapon(const ItemAttributes& attr) : Item(attr) {}

void RangedWeapon::fire(Creature* c, PItem ammo, Vec2 dir) {
  c->getGame()->getView()->addSound(SoundId::SHOOT_BOW);
  int toHitVariance = 10;
  int attackVariance = 15;
  int toHit = Random.get(-toHitVariance, toHitVariance) + 
    c->getModifier(ModifierType::FIRED_ACCURACY) +
    ammo->getModifier(ModifierType::FIRED_ACCURACY) +
    getModifier(ModifierType::FIRED_ACCURACY);
  int damage = Random.get(-attackVariance, attackVariance) + 
    c->getModifier(ModifierType::FIRED_DAMAGE) +
    ammo->getModifier(ModifierType::FIRED_DAMAGE) +
    getModifier(ModifierType::FIRED_DAMAGE);
  Attack attack(c, Random.choose(AttackLevel::LOW, AttackLevel::MIDDLE, AttackLevel::HIGH),
      AttackType::SHOOT, toHit, damage, false, none);
  c->getPosition().throwItem(std::move(ammo), attack, 20, dir, c->getVision());
}

