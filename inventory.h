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

#ifndef _INVENTORY_H
#define _INVENTORY_H

#include <functional>

#include "util.h"
#include "item.h"

class Inventory {
  public:
  void addItem(PItem);
  void addItems(vector<PItem>);
  PItem removeItem(Item* item);
  vector<PItem> removeItems(vector<Item*> items);
  vector<PItem> removeAllItems();

  vector<Item*> getItems() const;
  vector<Item*> getItems(function<bool (Item*)> predicate) const;

  bool hasItem(const Item*) const;
  int size() const;

  bool isEmpty() const;

  SERIALIZATION_DECL(Inventory);

  private:
  vector<PItem> SERIAL(items);
  vector<Item*> SERIAL(itemsCache);
};

#endif
