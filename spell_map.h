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

#pragma once

#include "game_time.h"
#include "spell.h"

class Spell;

class SpellMap {
  public:
  void add(Spell, ExperienceType, int level);
  GlobalTime getReadyTime(const Spell*) const;
  void setReadyTime(const Spell*, GlobalTime);
  vector<const Spell*> getAvailable(const Creature*) const;
  int getLevel(const Spell*) const;
  bool contains(const Spell*) const;
  void onExpLevelReached(Creature*, ExperienceType, int level);
  void setAllReady();

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);

  private:
  struct SpellInfo {
    Spell SERIAL(spell);
    optional<GlobalTime> SERIAL(timeout);
    int SERIAL(level);
    ExperienceType SERIAL(expType);
    SERIALIZE_ALL(spell, timeout, level, expType)
  };
  vector<SpellInfo> SERIAL(elems);
  const SpellInfo* getInfo(const string& id) const;
  SpellInfo* getInfo(const string& id);
};

