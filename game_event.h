#pragma once

#include "util.h"
#include "position.h"
#include "fx_info.h"

class Model;
class Technology;
class Collective;

namespace EventInfo {

  struct CreatureMoved {
    WCreature creature;
  };

  struct CreatureKilled {
    WCreature victim;
    WCreature attacker;
  };

  struct ItemsPickedUp {
    WCreature creature;
    vector<WItem> items;
  };

  struct ItemsDropped {
    WCreature creature;
    vector<WItem> items;
  };

  struct ItemsAppeared {
    Position position;
    vector<WItem> items;
  };

  struct Projectile {
    optional<FXInfo> fx;
    optional<ViewId> viewId;
    Position begin;
    Position end;
  };

  struct ConqueredEnemy {
    WCollective collective;
  };

  struct WonGame {};

  struct RetiredGame {};

  struct TechbookRead {
    TechId technology;
  };

  struct Alarm {
    Position pos;
    bool silent = false;
  };

  struct CreatureTortured {
    WCreature victim;
    WCreature torturer;
  };

  struct CreatureStunned {
    WCreature victim;
    WCreature attacker;
  };

  struct CreatureAttacked {
    WCreature victim;
    WCreature attacker;
  };

  struct TrapTriggered {
    Position pos;
  };

  struct TrapDisarmed {
    Position pos;
    WCreature creature;
  };

  struct FurnitureDestroyed {
    Position position;
    FurnitureType type;
    FurnitureLayer layer;
  };

  struct FX {
    Position position;
    FXInfo fx;
    optional<Vec2> direction = none;
  };

  struct ItemsEquipped {
    WCreature creature;
    vector<WItem> items;
  };

  struct CreatureEvent {
    WCreature creature;
    string message;
  };

  struct VisibilityChanged {
    Position pos;
  };

  struct MovementChanged {
    Position pos;
  };

  class GameEvent : public variant<CreatureMoved, CreatureKilled, ItemsPickedUp, ItemsDropped, ItemsAppeared, Projectile,
      ConqueredEnemy, WonGame, TechbookRead, Alarm, CreatureTortured, CreatureStunned, MovementChanged,
      TrapTriggered, TrapDisarmed, FurnitureDestroyed, ItemsEquipped, CreatureEvent, VisibilityChanged, RetiredGame,
      CreatureAttacked, FX> {
    using variant::variant;
  };

}

class GameEvent : public EventInfo::GameEvent {
  using EventInfo::GameEvent::GameEvent;
};
