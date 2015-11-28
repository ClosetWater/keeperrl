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

#ifndef _CREATURE_FACTORY
#define _CREATURE_FACTORY

#include <map>
#include <string>
#include <functional>

#include "util.h"

class Creature;
class MonsterAIFactory;
class Location;
class Tribe;
class ItemType;

RICH_ENUM(CreatureId,
    KEEPER,

    GOBLIN, 
    ORC,
    ORC_SHAMAN,
    HARPY,
    SUCCUBUS,
    DOPPLEGANGER,
    BANDIT,

    SPECIAL_BL,
    SPECIAL_BM,
    SPECIAL_HL,
    SPECIAL_HM,

    GHOST,
    SPIRIT,
    LOST_SOUL,
    GREEN_DRAGON,
    RED_DRAGON,
    CYCLOPS,
    MINOTAUR,
    HYDRA,
    SHELOB,
    WITCH,
    WITCHMAN,

    CLAY_GOLEM,
    STONE_GOLEM,
    IRON_GOLEM,
    LAVA_GOLEM,
    AUTOMATON,

    FIRE_ELEMENTAL,
    WATER_ELEMENTAL,
    EARTH_ELEMENTAL,
    AIR_ELEMENTAL,
    ENT,
    ANGEL,

    ZOMBIE,
    VAMPIRE,
    VAMPIRE_LORD,
    MUMMY,
    MUMMY_LORD,
    SKELETON,
    
    GNOME,
    GNOME_CHIEF,
    DWARF,
    DWARF_FEMALE,
    DWARF_BARON,

    KOBOLD,

    IMP,
    PRISONER,
    OGRE,
    CHICKEN,

    KNIGHT,
    AVATAR,
    CASTLE_GUARD,
    ARCHER,
    PESEANT,
    CHILD,
    SHAMAN,
    WARRIOR,

    LIZARDMAN,
    LIZARDLORD,

    ELEMENTALIST,
    
    ELF,
    ELF_ARCHER,
    ELF_CHILD,
    ELF_LORD,
    DARK_ELF,
    DARK_ELF_WARRIOR,
    DARK_ELF_CHILD,
    DARK_ELF_LORD,
    DRIAD,
    HORSE,
    COW,
    PIG,
    GOAT,
    DONKEY,
    
    LEPRECHAUN,
    
    JACKAL,
    DEER,
    BOAR,
    FOX,
    RAVEN,
    VULTURE,
    WOLF,
    WEREWOLF,
    DOG,

    DEATH,
    FIRE_SPHERE,
    KRAKEN,
    BAT,
    SNAKE,
    CAVE_BEAR,
    SPIDER,
    FLY,
    RAT,

    ANT_WORKER,
    ANT_SOLDIER,
    ANT_QUEEN,

    SOKOBAN_BOULDER
);

class CreatureFactory {
  public:
  struct SingleCreature {
    SingleCreature(Tribe*, CreatureId);
    CreatureId SERIAL(id);
    Tribe* SERIAL(tribe);
    bool operator == (const SingleCreature&) const;
    SERIALIZATION_DECL(SingleCreature);
  };
  CreatureFactory(const SingleCreature&);

  static PCreature fromId(CreatureId, Tribe*, const MonsterAIFactory&);
  static PCreature fromId(CreatureId, Tribe*);
  static vector<PCreature> getFlock(int size, CreatureId, Creature* leader);
  static CreatureFactory humanVillage(Tribe*);
  static CreatureFactory humanPeaceful(Tribe*);
  static CreatureFactory splashHeroes(Tribe*);
  static CreatureFactory splashLeader(Tribe*);
  static CreatureFactory splashMonsters(Tribe*);
  static CreatureFactory koboldVillage(Tribe*);
  static CreatureFactory gnomeVillage(Tribe*);
  static CreatureFactory gnomeEntrance(Tribe*);
  static CreatureFactory humanCastle(Tribe*);
  static CreatureFactory elvenVillage(Tribe*);
  static CreatureFactory darkElfVillage(Tribe*);
  static CreatureFactory darkElfEntrance(Tribe*);
  static CreatureFactory forrest(Tribe*);
  static CreatureFactory crypt(Tribe*);
  static SingleCreature coffins(Tribe*);
  static CreatureFactory dwarfTown(Tribe*);
  static CreatureFactory antNest(Tribe*);
  static CreatureFactory vikingTown(Tribe*);
  static CreatureFactory lizardTown(Tribe*);
  static CreatureFactory orcTown(Tribe*);
  static CreatureFactory splash(Tribe*);
  static CreatureFactory singleType(Tribe*, CreatureId);
  static CreatureFactory pyramid(Tribe*, int level);
  static CreatureFactory insects(Tribe* tribe);
  static CreatureFactory lavaCreatures(Tribe* tribe);
  static CreatureFactory waterCreatures(Tribe* tribe);
  static CreatureFactory elementals(Tribe* tribe);
  static CreatureFactory gnomishMines(Tribe* peaceful, Tribe* enemy, int level);
  
  PCreature random(const MonsterAIFactory&);
  PCreature random();

  CreatureFactory& increaseLevel(double);

  static PCreature getShopkeeper(Location* shopArea, Tribe*);
  static PCreature getGuardingBoulder(Tribe* tribe);
  static PCreature getGhost(Creature*);

  static PCreature addInventory(PCreature c, const vector<ItemType>& items);

  static void init();

  template <class Archive>
  static void registerTypes(Archive& ar, int version);

  SERIALIZATION_DECL(CreatureFactory);

  private:
  CreatureFactory(Tribe* tribe, const vector<CreatureId>& creatures, const vector<double>& weights,
      const vector<CreatureId>& unique = {}, EnumMap<CreatureId, Tribe*> overrides = {}, double levelIncrease = 0);
  CreatureFactory(const vector<tuple<CreatureId, double, Tribe*>>& creatures,
      const vector<CreatureId>& unique = {}, double levelIncrease = 0);
  static void initSplash(Tribe*);
  Tribe* getTribeFor(CreatureId);
  Tribe* SERIAL(tribe);
  vector<CreatureId> SERIAL(creatures);
  vector<double> SERIAL(weights);
  vector<CreatureId> SERIAL(unique);
  EnumMap<CreatureId, Tribe*> SERIAL(tribeOverrides);
  double SERIAL(levelIncrease) = 0;
};


#endif
