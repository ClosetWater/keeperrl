#pragma once

#include "util.h"
#include "effect.h"
#include "spell_id.h"
#include "pretty_archive.h"
#include "lasting_effect.h"
#include "creature_predicate.h"

struct ItemAttrBonus {
  AttrType attr;
  int value;
  COMPARE_ALL(attr, value)
};

class ItemPrefix;

struct JoinPrefixes {
  vector<ItemPrefix> prefixes;
  COMPARE_ALL(prefixes)
};

struct VictimEffect {
  double chance;
  Effect effect;
  COMPARE_ALL(chance, effect)
};

struct AttackerEffect {
  Effect effect;
  COMPARE_ALL(effect)
};

struct SpecialAttr {
  AttrType SERIAL(attr);
  int SERIAL(value);
  CreaturePredicate SERIAL(predicate);
  SERIALIZE_ALL(attr, value, predicate)
};

#define VARIANT_TYPES_LIST\
  X(LastingEffect, 0)\
  X(VictimEffect, 1)\
  X(AttackerEffect, 2)\
  X(ItemAttrBonus, 3)\
  X(JoinPrefixes, 4)\
  X(SpellId, 5)\
  X(SpecialAttr, 6)

#define VARIANT_NAME ItemPrefix

#include "gen_variant.h"
#include "gen_variant_serialize.h"
inline
#include "gen_variant_serialize_pretty.h"

#undef VARIANT_TYPES_LIST
#undef VARIANT_NAME

class ItemAttributes;
extern void applyPrefix(const ContentFactory*, const ItemPrefix&, ItemAttributes&);
extern string getItemName(const ContentFactory*, const ItemPrefix&);
extern string getGlyphName(const ContentFactory*, const ItemPrefix&);
extern vector<string> getEffectDescription(const ContentFactory*, const ItemPrefix&);
