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

#ifndef _TASK_H
#define _TASK_H

#include "move_info.h"
#include "unique_entity.h"
#include "entity_set.h"
#include "position.h"

class SquareType;
class Location;
class TaskCallback;
class CreatureFactory;

class Task : public UniqueEntity<Task> {
  public:

  Task(bool transferable = false);
  virtual ~Task();

  virtual MoveInfo getMove(Creature*) = 0;
  virtual bool isImpossible(const Level*);
  virtual bool canTransfer();
  virtual void cancel() {}
  virtual string getDescription() const = 0;
  virtual bool canPerform(const Creature* c);
  bool isDone();

  static PTask construction(TaskCallback*, Position, const SquareType&);
  static PTask buildTorch(TaskCallback*, Position, Dir attachmentDir);
  static PTask bringItem(TaskCallback*, Position position, vector<Item*>, vector<Position> target,
      int numRetries = 10);
  static PTask applyItem(TaskCallback*, Position, Item*, Position target);
  static PTask applySquare(TaskCallback*, vector<Position>);
  static PTask pickAndEquipItem(TaskCallback*, Position, Item*);
  static PTask equipItem(Item*);
  static PTask pickItem(TaskCallback*, Position, vector<Item*>);
  static PTask kill(TaskCallback*, Creature*);
  static PTask torture(TaskCallback*, Creature*);
  static PTask sacrifice(TaskCallback*, Creature*);
  static PTask destroySquare(Position);
  static PTask disappear();
  static PTask chain(PTask, PTask);
  static PTask chain(PTask, PTask, PTask);
  static PTask chain(vector<PTask>);
  static PTask explore(Position);
  static PTask attackLeader(Collective*);
  static PTask campAndSpawn(Collective* target, Collective* self, const CreatureFactory&, int defenseSize,
      Range attackSize, int numAttacks);
  static PTask killFighters(Collective*, int numFighters);
  static PTask stealFrom(Collective*, TaskCallback*);
  static PTask createBed(TaskCallback*, Position, const SquareType& fromType, const SquareType& toType);
  static PTask consumeItem(TaskCallback*, vector<Item*> items);
  static PTask copulate(TaskCallback*, Creature* target, int numTurns);
  static PTask consume(TaskCallback*, Creature* target);
  static PTask stayInLocationUntil(const Location*, double time);
  static PTask eat(set<Position> hatcherySquares);
  static PTask goTo(Position);
  static PTask goToAndWait(Position, double maxTime);
  static PTask whipping(TaskCallback*, Position, Creature* whipped, double interval, double timeout);
  static PTask dropItems(vector<Item*>);
  static PTask spider(Position origin, const vector<Position>& posClose, const vector<Position>& posFurther);

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);
  
  template <class Archive>
  static void registerTypes(Archive& ar, int version);

  protected:
  void setDone();

  private:
  bool SERIAL(done) = false;
  bool SERIAL(transfer);
};

#endif
