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

#ifndef _FIRE_H
#define _FIRE_H

#include "util.h"

class Level;
class Fire {
  public:
  Fire(double objectWeight, double objectFlamability);
  void tick();
  void set(double amount);
  bool isBurning() const;
  double getSize() const;
  bool isBurntOut() const;
  double getFlamability() const;

  SERIALIZATION_DECL(Fire);

  private:
  double SERIAL(burnt) = 0;
  double SERIAL(size) = 0;
  double SERIAL(weight);
  double SERIAL(flamability);
};

#endif
