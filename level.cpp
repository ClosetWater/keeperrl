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

#include "stdafx.h"

#include "level.h"
#include "location.h"
#include "model.h"
#include "item.h"
#include "creature.h"
#include "square.h"
#include "collective_builder.h"
#include "trigger.h"
#include "progress_meter.h"
#include "level_maker.h"
#include "movement_type.h"
#include "attack.h"
#include "player_message.h"
#include "vision.h"
#include "bucket_map.h"
#include "creature_name.h"
#include "sunlight_info.h"
#include "game.h"
#include "creature_attributes.h"

template <class Archive> 
void Level::serialize(Archive& ar, const unsigned int version) {
  serializeAll(ar, squares, oldSquares, landingSquares, locations, tickingSquares, creatures, model, fieldOfView);
  serializeAll(ar, name, backgroundLevel, backgroundOffset, sunlight, bucketMap, sectors, lightAmount, unavailable);
  serializeAll(ar, levelId, noDiagonalPassing, lightCapAmount, creatureIds, background, memoryUpdates);
}  

SERIALIZABLE(Level);

SERIALIZATION_CONSTRUCTOR_IMPL(Level);

Level::~Level() {}

Level::Level(SquareArray s, Model* m, vector<Location*> l, const string& n,
    Table<double> sun, LevelId id) 
    : squares(std::move(s)), oldSquares(squares.getBounds()), memoryUpdates(squares.getBounds(), true),
      locations(l), model(m), 
      name(n), sunlight(sun), bucketMap(squares.getBounds().width(), squares.getBounds().height(),
      FieldOfView::sightRange), lightAmount(squares.getBounds(), 0), lightCapAmount(squares.getBounds(), 1),
      levelId(id) {
  for (Vec2 pos : squares.getBounds()) {
    const Square* square = squares.getReadonly(pos);
    square->onAddedToLevel(Position(pos, this));
    if (optional<StairKey> link = square->getLandingLink())
      landingSquares[*link].push_back(Position(pos, this));
  }
  for (Location *l : locations)
    l->setLevel(this);
  for (VisionId vision : ENUM_ALL(VisionId))
    fieldOfView[vision] = FieldOfView(squares, vision);
  for (Vec2 pos : squares.getBounds())
    addLightSource(pos, squares.getReadonly(pos)->getLightEmission(), 1);
}

LevelId Level::getUniqueId() const {
  return levelId;
}

Rectangle Level::getMaxBounds() {
  return Rectangle(360, 360);
}

Rectangle Level::getSplashBounds() {
  return Rectangle(80, 40);
}

Rectangle Level::getSplashVisibleBounds() {
  Vec2 sz(40, 20);
  return Rectangle(getSplashBounds().middle() - sz / 2, getSplashBounds().middle() + sz / 2);
}

void Level::addCreature(Vec2 position, PCreature c, double delay) {
  Creature* ref = c.get();
  model->addCreature(std::move(c), delay);
  putCreature(position, ref);
}

const static double darknessRadius = 3.5;

void Level::putCreature(Vec2 position, Creature* c) {
  CHECK(inBounds(position));
  creatures.push_back(c);
  creatureIds.insert(c);
  CHECK(getSafeSquare(position)->getCreature() == nullptr);
  placeCreature(c, position);
}

void Level::addLightSource(Vec2 pos, double radius) {
  addLightSource(pos, radius, 1);
}

void Level::removeLightSource(Vec2 pos, double radius) {
  addLightSource(pos, radius, -1);
}

void Level::addLightSource(Vec2 pos, double radius, int numLight) {
  if (radius > 0) {
    for (Vec2 v : getVisibleTilesNoDarkness(pos, VisionId::NORMAL)) {
      double dist = (v - pos).lengthD();
      if (dist <= radius) {
        lightAmount[v] += min(1.0, 1 - (dist) / radius) * numLight;
        setNeedsRenderUpdate(v, true);
      }
    }
  }
}

void Level::addDarknessSource(Vec2 pos, double radius, int numDarkness) {
  if (radius > 0) {
    for (Vec2 v : getVisibleTilesNoDarkness(pos, VisionId::NORMAL)) {
      double dist = (v - pos).lengthD();
      if (dist <= radius) {
        lightCapAmount[v] -= min(1.0, 1 - (dist) / radius) * numDarkness;
        setNeedsRenderUpdate(v, true);
      }
      updateConnectivity(v);
    }
  }
}

void Level::removeSquare(Position pos, PSquare defaultSquare) {
  if (!oldSquares[pos.getCoord()])
    replaceSquare(pos, std::move(defaultSquare), false);
  else
    replaceSquare(pos, std::move(oldSquares[pos.getCoord()]), false);
}

void Level::replaceSquare(Position position, PSquare newSquare, bool storePrevious) {
  Vec2 pos = position.getCoord();
  Square* oldSquare = squares.getSquare(pos);
  oldSquare->onConstructNewSquare(position, newSquare.get());
  Creature* c = oldSquare->getCreature();
  if (c)
    oldSquare->removeCreature(position);
  for (Item* it : copyOf(oldSquare->getItems()))
    newSquare->dropItem(position, oldSquare->removeItem(position, it));
  newSquare->setCovered(oldSquare->isCovered());
  addLightSource(pos, oldSquare->getLightEmission(), -1);
  for (PTrigger& t : oldSquare->removeTriggers(position))
    newSquare->addTrigger(position, std::move(t));
  if (auto backgroundObj = oldSquare->extractBackground())
    background[pos] = backgroundObj;
  if (auto tribe = oldSquare->getForbiddenTribe())
    newSquare->forbidMovementForTribe(position, *tribe);
  if (storePrevious)
    oldSquares[pos] = squares.extractSquare(pos);
  squares.putSquare(pos, std::move(newSquare));
  squares.getSquare(pos)->onAddedToLevel(position);
  if (c) {
    squares.getSquare(pos)->setCreature(c);
  }
  addLightSource(pos, squares.getSquare(pos)->getLightEmission(), 1);
  updateVisibility(pos);
  updateConnectivity(pos);
}

void Level::updateVisibility(Vec2 changedSquare) {
  for (Vec2 pos : getVisibleTilesNoDarkness(changedSquare, VisionId::NORMAL)) {
    addLightSource(pos, squares.getReadonly(pos)->getLightEmission(), -1);
    if (Creature* c = squares.getReadonly(pos)->getCreature())
      if (c->isDarknessSource())
        addDarknessSource(pos, darknessRadius, -1);
  }
  for (VisionId vision : ENUM_ALL(VisionId))
    fieldOfView[vision].squareChanged(changedSquare);
  for (Vec2 pos : getVisibleTilesNoDarkness(changedSquare, VisionId::NORMAL)) {
    addLightSource(pos, squares.getReadonly(pos)->getLightEmission(), 1);
    if (Creature* c = squares.getReadonly(pos)->getCreature())
      if (c->isDarknessSource())
        addDarknessSource(pos, darknessRadius, 1);
  }
}

Creature* Level::getPlayer() const {
  if (Creature* player = model->getGame()->getPlayer())
    if (player->getLevel() == this)
      return player;
  return nullptr;
}

const vector<Location*> Level::getAllLocations() const {
  return locations;
}

void Level::clearLocations() {
  locations.clear();
}

void Level::addMarkedLocation(Rectangle bounds) {
  locations.push_back(new Location());
  locations.back()->setBounds(bounds);
  locations.back()->setLevel(this);
  locations.back()->setSurprise();
}

const Model* Level::getModel() const {
  return model;
}

Model* Level::getModel() {
  return model;
}

Game* Level::getGame() const {
  return model->getGame();
}

bool Level::isInSunlight(Vec2 pos) const {
  return !getSafeSquare(pos)->isCovered() && lightCapAmount[pos] == 1 &&
      getGame()->getSunlightInfo().getState() == SunlightState::DAY;
}

double Level::getLight(Vec2 pos) const {
  return max(0.0, min(getSafeSquare(pos)->isCovered() ? 1 : lightCapAmount[pos], lightAmount[pos] +
      sunlight[pos] * getGame()->getSunlightInfo().getLightAmount()));
}

vector<Position> Level::getLandingSquares(StairKey key) const {
  if (landingSquares.count(key))
    return landingSquares.at(key);
  else
    return vector<Position>();
}

vector<StairKey> Level::getAllStairKeys() const {
  return getKeys(landingSquares);
}

bool Level::hasStairKey(StairKey key) const {
  return landingSquares.count(key);
}

optional<Position> Level::getStairsTo(const Level* level) const {
  return model->getStairs(this, level);
}

bool Level::landCreature(StairKey key, Creature* creature) {
  vector<Position> landing = landingSquares.at(key);
  return landCreature(landing, creature);
}

bool Level::landCreature(StairKey key, PCreature creature) {
  if (landCreature(key, creature.get())) {
    model->addCreature(std::move(creature));
    return true;
  } else
    return false;
}

static Vec2 projectOnBorders(Rectangle area, Vec2 d) {
  Vec2 center = Vec2((area.left() + area.right()) / 2, (area.top() + area.bottom()) / 2);
  if (d.x == 0) {
    return Vec2(center.x, d.y > 0 ? area.bottom() - 1 : area.top());
  }
  int cy = d.y * area.width() / 2 / abs(d.x);
  if (center.y + cy >= area.top() && center.y + cy < area.bottom())
    return Vec2(d.x > 0 ? area.right() - 1 : area.left(), center.y + cy);
  int cx = d.x * area.height() / 2 / abs(d.y);
  return Vec2(center.x + cx, d.y > 0 ? area.bottom() - 1: area.top());
}

Position Level::getLandingSquare(StairKey key, Vec2 travelDir) const {
  vector<Position> landing = landingSquares.at(key);
  Vec2 entryPos = projectOnBorders(getBounds(), travelDir);
  Position target = landing.at(0);
  for (Position p : landing)
    if (p.getCoord().distD(entryPos) < target.getCoord().distD(entryPos))
      target = p;
  return target;
}

bool Level::landCreature(StairKey key, PCreature creature, Vec2 travelDir) {
  Position bestLanding = getLandingSquare(key, travelDir);
  if (landCreature({bestLanding}, creature.get()) ||
      landCreature(bestLanding.getRectangle(Rectangle::centered(Vec2(0, 0), 10)), creature.get()) ||
      landCreature(landingSquares.at(key), creature.get()) ||
      landCreature(getAllPositions(), creature.get())) {
    model->addCreature(std::move(creature));
    return true;
  } else
    return false;
}

bool Level::landCreature(vector<Position> landing, PCreature creature) {
  if (landCreature(landing, creature.get())) {
    model->addCreature(std::move(creature));
    return true;
  } else
    return false;
}

bool Level::landCreature(vector<Position> landing, Creature* creature) {
  CHECK(creature);
  queue<Position> q;
  set<Position> marked;
  for (Position pos : Random.permutation(landing)) {
    q.push(pos);
    marked.insert(pos);
  }
  while (!q.empty()) {
    Position v = q.front();
    q.pop();
    if (v.canEnter(creature)) {
      v.putCreature(creature);
      return true;
    } else
      for (Position next : v.neighbors8(Random))
        if (!marked.count(next) && next.canEnterEmpty(creature)) {
          q.push(next);
          marked.insert(next);
        }
  }
  return false;
}

void Level::throwItem(PItem item, const Attack& attack, int maxDist, Vec2 position, Vec2 direction, VisionId vision) {
  vector<PItem> v;
  v.push_back(std::move(item));
  throwItem(std::move(v), attack, maxDist, position, direction, vision);
}

void Level::throwItem(vector<PItem> item, const Attack& attack, int maxDist, Vec2 position, Vec2 direction,
    VisionId vision) {
  CHECK(!item.empty());
  CHECK(direction.length8() == 1);
  int cnt = 1;
  vector<Vec2> trajectory;
  for (Vec2 v = position + direction;; v += direction) {
    trajectory.push_back(v);
    if (getSafeSquare(v)->itemBounces(item[0].get(), vision)) {
        item[0]->onHitSquareMessage(Position(v, this), item.size());
        trajectory.pop_back();
        getGame()->addEvent({EventId::ITEMS_THROWN, EventInfo::ItemsThrown{this, extractRefs(item), trajectory}});
        if (!item[0]->isDiscarded())
          modSafeSquare(v - direction)->dropItems(Position(v - direction, this), std::move(item));
        return;
    }
    if (++cnt > maxDist || getSafeSquare(v)->itemLands(extractRefs(item), attack)) {
      getGame()->addEvent({EventId::ITEMS_THROWN, EventInfo::ItemsThrown{this, extractRefs(item), trajectory}});
      modSafeSquare(v)->onItemLands(Position(v, this), std::move(item), attack, maxDist - cnt - 1, direction,
          vision);
      return;
    }
  }
}

void Level::killCreature(Creature* creature) {
  eraseCreature(creature, creature->getPosition().getCoord());
  getModel()->killCreature(creature);
}

void Level::removeCreature(Creature* creature) {
  eraseCreature(creature, creature->getPosition().getCoord());
}

const static int hearingRange = 30;

void Level::globalMessage(Vec2 position, const PlayerMessage& ifPlayerCanSee, const PlayerMessage& cannot) const {
  if (Creature* player = getPlayer()) {
    if (playerCanSee(position))
      player->playerMessage(ifPlayerCanSee);
    else if (player->getPosition().getCoord().dist8(position) < hearingRange)
      player->playerMessage(cannot);
  }
}

void Level::globalMessage(Vec2 position, const PlayerMessage& playerCanSee) const {
  globalMessage(position, playerCanSee, "");
}

void Level::globalMessage(const Creature* c, const PlayerMessage& ifPlayerCanSee, const PlayerMessage& cant) const {
  if (Creature* player = getPlayer()) {
    if (player->canSee(c))
      player->playerMessage(ifPlayerCanSee);
    else if (player->getPosition().dist8(c->getPosition()) < hearingRange)
      player->playerMessage(cant);
  }
}

void Level::changeLevel(StairKey key, Creature* c) {
  Vec2 oldPos = c->getPosition().getCoord();
  Level* otherLevel = model->getLinkedLevel(this, key);
  if (otherLevel->landCreature(key, c))
    eraseCreature(c, oldPos);
  else {
    Position otherPos = Random.choose(otherLevel->landingSquares.at(key));
    if (Creature* other = otherPos.getCreature()) {
      if (!other->isPlayer() && c->getPosition().canEnterEmpty(other) && otherPos.canEnterEmpty(c)) {
        otherLevel->eraseCreature(other, otherPos.getCoord());
        eraseCreature(c, oldPos);
        putCreature(oldPos, other);
        otherLevel->putCreature(otherPos.getCoord(), c);
        c->playerMessage("You switch levels with " + other->getName().a());
      }
    }
  }
}

void Level::changeLevel(Position destination, Creature* c) {
  Vec2 oldPos = c->getPosition().getCoord();
  if (destination.isValid() && destination.getLevel()->landCreature({destination}, c))
    eraseCreature(c, oldPos);
}

void Level::eraseCreature(Creature* c, Vec2 coord) {
  removeElement(creatures, c);
  unplaceCreature(c, coord);
  creatureIds.erase(c);
}

const vector<Creature*>& Level::getAllCreatures() const {
  return creatures;
}

vector<Creature*>& Level::getAllCreatures() {
  return creatures;
}

vector<Creature*> Level::getAllCreatures(Rectangle bounds) const {
  return bucketMap->getElements(bounds);
}

bool Level::containsCreature(UniqueEntity<Creature>::Id id) const {
  return creatureIds.contains(id);
}

const int darkViewRadius = 5;

bool Level::isWithinVision(Vec2 from, Vec2 to, VisionId v) const {
  return Vision::get(v)->isNightVision() || from.distD(to) <= darkViewRadius || getLight(to) > 0.3;
}

FieldOfView& Level::getFieldOfView(VisionId vision) const {
  return fieldOfView[vision];
}

bool Level::canSee(Vec2 from, Vec2 to, VisionId vision) const {
  return isWithinVision(from, to, vision) && getFieldOfView(vision).canSee(from, to);
}

bool Level::canSee(const Creature* c, Vec2 pos) const {
  return canSee(c->getPosition().getCoord(), pos, c->getVision());
}

bool Level::playerCanSee(Vec2 pos) const {
  if (Creature* player = getPlayer())
    return player->canSee(pos);
  else
    return false;
}

bool Level::playerCanSee(const Creature* c) const {
  if (Creature* player = getPlayer())
    return player->canSee(c);
  else
    return false;
}

static bool canPass(const Square* square, const Creature* c) {
  return square->canEnterEmpty(c) && (!square->getCreature() ||
      !square->getCreature()->getAttributes().isStationary());
}

bool Level::canMoveCreature(const Creature* creature, Vec2 direction) const {
  Vec2 position = creature->getPosition().getCoord();
  Vec2 destination = position + direction;
  if (!inBounds(destination) || unavailable[destination])
    return false;
  if (noDiagonalPassing && direction.isCardinal8() && !direction.isCardinal4() &&
      !canPass(getSafeSquare(position + Vec2(direction.x, 0)), creature) &&
      !canPass(getSafeSquare(position + Vec2(0, direction.y)), creature))
    return false;
  return getSafeSquare(destination)->canEnter(creature);
}

void Level::moveCreature(Creature* creature, Vec2 direction) {
  CHECK(canMoveCreature(creature, direction));
  Vec2 position = creature->getPosition().getCoord();
  unplaceCreature(creature, position);
  placeCreature(creature, position + direction);
}

void Level::unplaceCreature(Creature* creature, Vec2 pos) {
  bucketMap->removeElement(pos, creature);
  modSafeSquare(pos)->removeCreature(Position(pos, this));
  if (creature->isDarknessSource())   
    addDarknessSource(pos, darknessRadius, -1);
}

void Level::placeCreature(Creature* creature, Vec2 pos) {
  creature->setPosition(Position(pos, this));
  bucketMap->addElement(pos, creature);
  modSafeSquare(pos)->putCreature(creature);
  if (creature->isDarknessSource())
    addDarknessSource(pos, darknessRadius, 1);
}

void Level::swapCreatures(Creature* c1, Creature* c2) {
  Vec2 pos1 = c1->getPosition().getCoord();
  Vec2 pos2 = c2->getPosition().getCoord();
  unplaceCreature(c1, pos1);
  unplaceCreature(c2, pos2);
  placeCreature(c1, pos2);
  placeCreature(c2, pos1);
}

vector<Vec2> Level::getVisibleTilesNoDarkness(Vec2 pos, VisionId vision) const {
  return getFieldOfView(vision).getVisibleTiles(pos);
}

vector<Vec2> Level::getVisibleTiles(Vec2 pos, VisionId vision) const {
  return filter(getFieldOfView(vision).getVisibleTiles(pos),
      [&](Vec2 v) { return isWithinVision(pos, v, vision); });
}

void Level::setBackgroundLevel(const Level* l, Vec2 offs) {
  backgroundLevel = l;
  backgroundOffset = offs;
}

const Square* Level::getSafeSquare(Vec2 pos) const {
  CHECK(inBounds(pos));
  return squares.getReadonly(pos);
}

Square* Level::modSafeSquare(Vec2 pos) {
  CHECK(inBounds(pos));
  return squares.getSquare(pos);
}

Position Level::getPosition(Vec2 pos) const {
  return Position(pos, const_cast<Level*>(this)); 
}

vector<Position> Level::getAllPositions() const {
  vector<Position> ret;
  for (Vec2 v : getBounds())
    ret.emplace_back(v, const_cast<Level*>(this)); 
  return ret;
}

void Level::addTickingSquare(Vec2 pos) {
  tickingSquares.insert(pos);
}

void Level::tick() {
  for (Vec2 pos : tickingSquares)
    squares.getSquare(pos)->tick(Position(pos, this));
}

bool Level::inBounds(Vec2 pos) const {
  return pos.inRectangle(getBounds());
}

Rectangle Level::getBounds() const {
  return Rectangle(0, 0, getWidth(), getHeight());
}

int Level::getWidth() const {
  return squares.getBounds().width();
}

int Level::getHeight() const {
  return squares.getBounds().height();
}

const string& Level::getName() const {
  return name;
}

void Level::updateConnectivity(Vec2 pos) {
  for (auto& elem : sectors)
    if (getSafeSquare(pos)->canNavigate(elem.first))
      elem.second.add(pos);
    else
      elem.second.remove(pos);
}

bool Level::areConnected(Vec2 p1, Vec2 p2, const MovementType& movement) const {
  return inBounds(p1) && inBounds(p2) && getSectors(movement).same(p1, p2);
}

Sectors& Level::getSectors(const MovementType& movement) const {
  if (!sectors.count(movement)) {
    sectors[movement] = Sectors(getBounds());
    Sectors& newSectors = sectors.at(movement);
    for (Vec2 v : getBounds())
      if (getSafeSquare(v)->canNavigate(movement))
        newSectors.add(v);
  }
  return sectors.at(movement);
}

bool Level::isChokePoint(Vec2 pos, const MovementType& movement) const {
  return getSectors(movement).isChokePoint(pos);
}

void Level::updateSunlightMovement() {
  sectors.clear();
}

const optional<ViewObject>& Level::getBackgroundObject(Vec2 pos) const {
  return background[pos];
}

int Level::getNumModifiedSquares() const {
  return squares.getNumModified();
}

void Level::setNeedsMemoryUpdate(Vec2 pos, bool s) {
  memoryUpdates[pos] = s;
}

bool Level::needsRenderUpdate(Vec2 pos) const {
  return renderUpdates[pos];
}

void Level::setNeedsRenderUpdate(Vec2 pos, bool s) {
  renderUpdates[pos] = s;
}

bool Level::needsMemoryUpdate(Vec2 pos) const {
  return memoryUpdates[pos];
}

bool Level::isUnavailable(Vec2 pos) const {
  return unavailable[pos];
}
