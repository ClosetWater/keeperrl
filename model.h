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

#ifndef _MODEL_H
#define _MODEL_H

#include "util.h"
#include "map_memory.h"
#include "stair_key.h"
#include "position.h"

class PlayerControl;
class Level;
class ProgressMeter;
class Options;
class CreatureView;
class Trigger;
class Highscores;
class Technology;
class View;
class LevelMaker;
struct SettlementInfo;
class LevelBuilder;
class TimeQueue;
class Statistics;
struct TribeSet;
class StairKey;

enum class SunlightState { DAY, NIGHT};
enum class VillainType { MAIN, LESSER };

/**
  * Main class that holds all game logic.
  */
class Model {
  public:
  ~Model();

  enum class GameType { ADVENTURER, KEEPER, RETIRED_KEEPER, AUTOSAVE };


  class ExitInfo {
    public:
    static ExitInfo saveGame(GameType);
    static ExitInfo abandonGame();

    bool isAbandoned() const;
    GameType getGameType() const;
    private:
    GameType type;
    bool abandon;
  };

  /** Makes an update to the game. This method is repeatedly called to make the game run.
    Returns the total logical time elapsed.*/
  optional<ExitInfo> update(double totalTime);

  /** Returns the level that the stairs lead to. */
  Level* getLinkedLevel(Level* from, StairKey) const;

  Position getStairs(const Level* from, const Level* to);

  /** Adds new creature to the queue. Assumes this creature has already been added to a level. */
  void addCreature(PCreature, double delay = 0);

  /** Removes creature from the queue. Assumes it has already been removed from its level. */
  void killCreature(Creature*, Creature* attacker);

  const vector<Collective*>& getVillains(VillainType) const;
  const vector<Collective*>& getAllVillains() const;

  bool isTurnBased();

  string getGameIdentifier() const;
  string getGameDisplayName() const;
  void exitAction();
  double getTime() const;
  MusicType getCurrentMusic() const;
  void setCurrentMusic(MusicType, bool now);
  bool changeMusicNow() const;

  View* getView();
  void setView(View*);
  Options* getOptions();
  void setOptions(Options*);
  void setHighscores(Highscores*);

  Statistics& getStatistics();
  const Statistics& getStatistics() const;

  void tick(double time);
  void gameOver(const Creature* player, int numKills, const string& enemiesString, int points);
  void conquered(const string& title, vector<const Creature*> kills, int points);
  void killedKeeper(const string& title, const string& keeper, const string& land,
    vector<const Creature*> kills, int points);
  bool isGameOver() const;
  static void showCredits(View*);
  void retireCollective();

  struct SunlightInfo {
    double lightAmount;
    double timeRemaining;
    SunlightState state;
    const char* getText();
  };
  const SunlightInfo& getSunlightInfo() const;

  const string& getWorldName() const;

  SERIALIZATION_DECL(Model);

  optional<Position> getDanglingPortal();
  void setDanglingPortal(Position);
  void resetDanglingPortal();

  void addWoodCount(int);
  int getWoodCount() const;

  Tribe* getPestTribe();
  Tribe* getKillEveryoneTribe();
  Tribe* getPeacefulTribe();

  void onTechBookRead(Technology*);
  void onAlarm(Position);
  void onKilledLeader(const Collective*, const Creature*);
  void onTorture(const Creature* who, const Creature* torturer);
  void onSurrender(Creature* who, const Creature* to);
  void onAttack(Creature* victim, Creature* attacker);
  void onTrapTrigger(Position);
  void onTrapDisarm(Position, const Creature*);
  void onSquareDestroyed(Position);
  void onEquip(const Creature*, const Item*);

  vector<Level*> getLevels() const;

  private:
  Model(View* view, const string& worldName, TribeSet&&);

  friend class ModelBuilder;

  void updateSunlightInfo();
  PCreature makePlayer(int handicap);
  const Creature* getPlayer() const;
  void landHeroPlayer();
  Level* buildLevel(LevelBuilder&&, PLevelMaker);
  Level* prepareTopLevel(ProgressMeter&, vector<SettlementInfo> settlements);

  HeapAllocated<TribeSet> SERIAL(tribeSet);
  vector<PLevel> SERIAL(levels);
  PLevel SERIAL(cemetery);
  vector<PCollective> SERIAL(collectives);
  Collective* SERIAL(playerCollective);
  map<VillainType, vector<Collective*>> SERIAL(villainsByType);
  vector<Collective*> SERIAL(allVillains);
  View* view;
  HeapAllocated<TimeQueue> SERIAL(timeQueue);
  vector<PCreature> SERIAL(deadCreatures);
  double SERIAL(lastTick) = -1000;
  PlayerControl* SERIAL(playerControl) = nullptr;
  bool SERIAL(won) = false;
  bool SERIAL(addHero) = false;
  double SERIAL(currentTime) = 0;
  SunlightInfo sunlightInfo;
  double lastUpdate = -10;
  Options* options;
  Highscores* highscores;
  string SERIAL(worldName);
  MusicType SERIAL(musicType);
  bool SERIAL(finishCurrentMusic) = true;
  optional<ExitInfo> exitInfo;
  unique_ptr<CreatureView> SERIAL(spectator);
  optional<Position> SERIAL(danglingPortal);
  int SERIAL(woodCount) = 0;
  HeapAllocated<Statistics> SERIAL(statistics);
  string SERIAL(gameIdentifier);
  string SERIAL(gameDisplayName);
  void calculateStairNavigation();
  optional<StairKey> getStairsBetween(const Level* from, const Level* to);
  map<pair<const Level*, const Level*>, StairKey> SERIAL(stairNavigation);
};

#endif
