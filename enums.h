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

#ifndef _ENUMS_H
#define _ENUMS_H

#include "serialization.h"

typedef int TeamId;

enum class MusicType;
enum class SunlightState;

enum class CollectiveResourceId;
enum class CollectiveConfigId;
enum class MsgType;
enum class BodyPart;
enum class AttackType;
enum class AttackLevel;
enum class ModifierType;
enum class AttrType;
enum class ItemClass;
enum class EquipmentSlot;
enum class SplashType;
enum class GameTypeChoice;
enum class MenuType;

enum class MovementTrait;
enum class SpawnType;
enum class CreatureSize;

enum class SquareApplyType;
enum class SquareId;
enum class ItemAction;
enum class WorshipType;

enum class MinionTask;
enum class MinionTrait;
enum class TrapType;

enum class SquareAttrib;

enum class Dir;

enum class CreatureId;
enum class ItemId;

enum class ViewLayer;
enum class HighlightType;
enum class StairLook;

enum class SettlementType;
enum class VillainType;

enum class ViewId;

enum class EffectStrength;
enum class EffectId;

enum class AnimationId;

enum class SpellId;
enum class TribeId;
enum class TechId;
enum class SkillId;
enum class VisionId;

enum class LastingEffect;

enum class DeityHabitat;

enum class NameGeneratorId;
enum class EpithetId;
enum class ItemIndex;

enum class CollectiveWarning;
#endif
