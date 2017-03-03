#pragma once

#include "util.h"

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
    DARKNESS_SOURCE,
    PREGNANT
);

RICH_ENUM(CreatureCondition,
    SLEEPING,
    RESTRICTED_MOVEMENT
);

class LastingEffects {
  public:
  static void onAffected(Creature*, LastingEffect, bool msg);
  static bool affects(const Creature*, LastingEffect);
  static void onRemoved(Creature*, LastingEffect, bool msg);
  static void onTimedOut(Creature*, LastingEffect, bool msg);
  static void modifyAttr(const Creature*, AttrType, int&);
  static void modifyMod(const Creature*, ModifierType, int&);
  static void onCreatureDamage(Creature*, LastingEffect);
  static const char* getGoodAdjective(LastingEffect);
  static const char* getBadAdjective(LastingEffect);
  static const vector<LastingEffect>& getCausingCondition(CreatureCondition);
};


