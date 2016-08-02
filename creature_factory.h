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

#include "util.h"
#include "tribe.h"

class Creature;
class MonsterAIFactory;
class Location;
class Tribe;
class ItemType;
class CreatureAttributes;
class ControllerFactory;

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
    SingleCreature(TribeId, CreatureId);
    CreatureId SERIAL(id);
    TribeId SERIAL(tribe);
    bool operator == (const SingleCreature&) const;
    SERIALIZATION_DECL(SingleCreature);
  };
  CreatureFactory(const SingleCreature&);

  static PCreature fromId(CreatureId, TribeId, const MonsterAIFactory&);
  static PCreature fromId(CreatureId, TribeId);
  static vector<PCreature> getFlock(int size, CreatureId, Creature* leader);
  static CreatureFactory humanVillage(TribeId);
  static CreatureFactory humanPeaceful(TribeId);
  static CreatureFactory splashHeroes(TribeId);
  static CreatureFactory splashLeader(TribeId);
  static CreatureFactory splashMonsters(TribeId);
  static CreatureFactory koboldVillage(TribeId);
  static CreatureFactory gnomeVillage(TribeId);
  static CreatureFactory gnomeEntrance(TribeId);
  static CreatureFactory humanCastle(TribeId);
  static CreatureFactory elvenVillage(TribeId);
  static CreatureFactory elvenCottage(TribeId);
  static CreatureFactory darkElfVillage(TribeId);
  static CreatureFactory darkElfEntrance(TribeId);
  static CreatureFactory forrest(TribeId);
  static CreatureFactory crypt(TribeId);
  static SingleCreature coffins(TribeId);
  static CreatureFactory dwarfTown(TribeId);
  static CreatureFactory dwarfCave(TribeId);
  static CreatureFactory antNest(TribeId);
  static CreatureFactory vikingTown(TribeId);
  static CreatureFactory lizardTown(TribeId);
  static CreatureFactory orcTown(TribeId);
  static CreatureFactory splash(TribeId);
  static CreatureFactory singleType(TribeId, CreatureId);
  static CreatureFactory pyramid(TribeId, int level);
  static CreatureFactory insects(TribeId tribe);
  static CreatureFactory lavaCreatures(TribeId tribe);
  static CreatureFactory waterCreatures(TribeId tribe);
  static CreatureFactory elementals(TribeId tribe);
  static CreatureFactory gnomishMines(TribeId peaceful, TribeId enemy, int level);
  
  PCreature random(const MonsterAIFactory&);
  PCreature random();

  CreatureFactory& increaseLevel(double);

  static PCreature getShopkeeper(Location* shopArea, TribeId);
  static PCreature getGuardingBoulder(TribeId);
  static PCreature getGhost(Creature*);
  static PCreature getIllusion(Creature*);
  static PCreature getAdventurer(Model*, int handicap);

  static PCreature addInventory(PCreature c, const vector<ItemType>& items);
  static CreatureAttributes getKrakenAttributes(ViewId);

  static void init();

  template <class Archive>
  static void registerTypes(Archive& ar, int version);

  SERIALIZATION_DECL(CreatureFactory);

  private:
  CreatureFactory(TribeId tribe, const vector<CreatureId>& creatures, const vector<double>& weights,
      const vector<CreatureId>& unique = {}, EnumMap<CreatureId, optional<TribeId>> overrides = {},
      double levelIncrease = 0);
  CreatureFactory(const vector<tuple<CreatureId, double, TribeId>>& creatures,
      const vector<CreatureId>& unique = {}, double levelIncrease = 0);
  static void initSplash(TribeId);
  static PCreature getSokobanBoulder(TribeId);
  static PCreature getSpecial(TribeId, bool humanoid, bool large, const ControllerFactory&);
  static PCreature get(CreatureId, TribeId, MonsterAIFactory);
  static PCreature get(const CreatureAttributes&, TribeId, const ControllerFactory&);
  static CreatureAttributes getAttributes(CreatureId id);
  TribeId getTribeFor(CreatureId);
  optional<TribeId> SERIAL(tribe);
  vector<CreatureId> SERIAL(creatures);
  vector<double> SERIAL(weights);
  vector<CreatureId> SERIAL(unique);
  EnumMap<CreatureId, optional<TribeId>> SERIAL(tribeOverrides);
  double SERIAL(levelIncrease) = 0;
};


#endif
