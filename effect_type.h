#ifndef _EFFECT_TYPE_H
#define _EFFECT_TYPE_H

#include "enum_variant.h"

enum class EffectId { 
    TELEPORT,
    HEAL,
    ROLLING_BOULDER,
    FIRE,
    PORTAL,
    DESTROY_EQUIPMENT,
    ENHANCE_ARMOR,
    ENHANCE_WEAPON,
    GUARDING_BOULDER,
    EMIT_POISON_GAS,
    WORD_OF_POWER,
    AIR_BLAST,
    DECEPTION,
    SUMMON,
    ACID,
    ALARM,
    TELE_ENEMIES,
    LEAVE_BODY,
    SILVER_DAMAGE,
    CURE_POISON,
    METEOR_SHOWER,
    LASTING,
};

RICH_ENUM(LastingEffect,
    SLEEP,
    PANIC,
    RAGE,
    SLOWED,
    SPEED,
    STR_BONUS,
    DEX_BONUS,
    HALLU,
    BLIND,
    INVISIBLE,
    POISON,
    ENTANGLED,
    TIED_UP,
    STUNNED,
    POISON_RESISTANT,
    FIRE_RESISTANT,
    FLYING,
    INSANITY,
    MAGIC_SHIELD,
    DARKNESS_SOURCE
);

class EffectType : public EnumVariant<EffectId, TYPES(LastingEffect, CreatureId),
        ASSIGN(LastingEffect, EffectId::LASTING),
        ASSIGN(CreatureId, EffectId::SUMMON)> {
  using EnumVariant::EnumVariant;
};

enum class DirEffectId {
  BLAST,
  CREATURE_EFFECT,
};

class DirEffectType : public EnumVariant<DirEffectId, TYPES(EffectType),
        ASSIGN(EffectType, DirEffectId::CREATURE_EFFECT)> {
  using EnumVariant::EnumVariant;
};

#endif
