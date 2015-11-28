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

#include "map_gui.h"
#include "view_object.h"
#include "map_layout.h"
#include "view_index.h"
#include "tile.h"
#include "window_view.h"
#include "renderer.h"
#include "clock.h"
#include "view_id.h"
#include "level.h"
#include "creature_view.h"
#include "options.h"

using sf::Keyboard;

MapGui::MapGui(Callbacks call, Clock* c, Options* o) : objects(Level::getMaxBounds()), callbacks(call),
    clock(c), options(o), fogOfWar(Level::getMaxBounds(), false), extraBorderPos(Level::getMaxBounds(), {}),
    connectionMap(Level::getMaxBounds()), enemyPositions(Level::getMaxBounds(), false) {
  clearCenter();
}

static int fireVar = 50;

static Color getFireColor() {
  return Color(200 + Random.get(-fireVar, fireVar), Random.get(fireVar), Random.get(fireVar), 150);
}

MapGui::ViewIdMap::ViewIdMap(Rectangle bounds) : ids(bounds, {}) {
}

void MapGui::ViewIdMap::add(Vec2 pos, ViewId id) {
  if (ids.isDirty(pos))
    ids.getDirtyValue(pos).insert(id);
  else
    ids.setValue(pos, {id});
}

bool MapGui::ViewIdMap::has(Vec2 pos, ViewId id) {
  return ids.getValue(pos)[id];
}

void MapGui::ViewIdMap::clear() {
  ids.clear();
}

void MapGui::setButtonViewId(ViewId id) {
  buttonViewId = id;
}

void MapGui::clearButtonViewId() {
  buttonViewId = none;
}

void MapGui::highlightTeam(const vector<UniqueEntity<Creature>::Id>& ids) {
  for (auto& id : ids)
    ++teamHighlight[id];
}

void MapGui::unhighlightTeam(const vector<UniqueEntity<Creature>::Id>& ids) {
  for (auto& id : ids)
    CHECK(--teamHighlight[id] >= 0);
}

Vec2 MapGui::getScreenPos() const {
  return Vec2((center.x - mouseOffset.x) * layout->getSquareSize().x,
      (center.y - mouseOffset.y) * layout->getSquareSize().y);
}

void MapGui::setSpriteMode(bool s) {
  spriteMode = s;
}

void MapGui::addAnimation(PAnimation animation, Vec2 pos) {
  animation->setBegin(clock->getRealMillis());
  animations.push_back({std::move(animation), pos});
}

optional<Vec2> MapGui::getMousePos() {
  if (lastMouseMove && lastMouseMove->inRectangle(getBounds()))
    return lastMouseMove;
  else
    return none;
}

optional<Vec2> MapGui::projectOnMap(Vec2 screenCoord) {
  if (screenCoord.inRectangle(getBounds()))
    return layout->projectOnMap(getBounds(), getScreenPos(), screenCoord);
  else
    return none;
}

optional<Vec2> MapGui::getHighlightedTile(Renderer& renderer) {
  if (auto pos = getMousePos())
    return layout->projectOnMap(getBounds(), getScreenPos(), *pos);
  else
    return none;
}

Color getHighlightColor(HighlightType type, double amount) {
  switch (type) {
    case HighlightType::RECT_DESELECTION: return transparency(colors[ColorId::RED], 90);
    case HighlightType::DIG: return transparency(colors[ColorId::YELLOW], 120);
    case HighlightType::CUT_TREE: return transparency(colors[ColorId::YELLOW], 170);
    case HighlightType::FETCH_ITEMS: return transparency(colors[ColorId::YELLOW], 170);
    case HighlightType::RECT_SELECTION: return transparency(colors[ColorId::YELLOW], 90);
    case HighlightType::FOG: return transparency(colors[ColorId::WHITE], 120 * amount);
    case HighlightType::POISON_GAS: return Color(0, min(255., amount * 500), 0, amount * 140);
    case HighlightType::MEMORY: return transparency(colors[ColorId::BLACK], 80);
    case HighlightType::NIGHT: return transparency(colors[ColorId::NIGHT_BLUE], amount * 160);
    case HighlightType::EFFICIENCY: return transparency(Color(255, 0, 0) , 120 * (1 - amount));
    case HighlightType::PRIORITY_TASK: return transparency(Color(0, 255, 0), 120);
    case HighlightType::FORBIDDEN_ZONE: return transparency(Color(255, 0, 0), 120);
    case HighlightType::UNAVAILABLE: return transparency(Color(0, 0, 0), 120);
  }
}

set<Vec2> shadowed;

optional<ViewId> getConnectionId(ViewId id) {
  switch (id) {
    case ViewId::BLACK_WALL:
    case ViewId::WOOD_WALL:
    case ViewId::CASTLE_WALL:
    case ViewId::MUD_WALL:
    case ViewId::MOUNTAIN:
    case ViewId::GOLD_ORE:
    case ViewId::IRON_ORE:
    case ViewId::STONE:
    case ViewId::WALL: return ViewId::WALL;
    default: return id;
  }
}

optional<ViewId> getConnectionId(const ViewObject& object) {
  return getConnectionId(object.id());
}

vector<Vec2>& getConnectionDirs(ViewId id) {
  static vector<Vec2> v4 = Vec2::directions4();
  static vector<Vec2> v8 = Vec2::directions8();
  switch (id) {
    case ViewId::DORM:
    case ViewId::CEMETERY:
    case ViewId::BEAST_LAIR:
    case ViewId::STOCKPILE1:
    case ViewId::STOCKPILE2:
    case ViewId::STOCKPILE3:
    case ViewId::LIBRARY:
    case ViewId::WORKSHOP:
    case ViewId::FORGE:
    case ViewId::LABORATORY:
    case ViewId::JEWELER:
    case ViewId::TORTURE_TABLE:
    case ViewId::RITUAL_ROOM:
    case ViewId::MOUNTAIN:
    case ViewId::GOLD_ORE:
    case ViewId::IRON_ORE:
    case ViewId::STONE:
    case ViewId::TRAINING_ROOM: return v8;
    default: return v4;
  }
}

void MapGui::softScroll(double x, double y) {
  if (softCenter)
    center = *softCenter;
  softCenter = {center.x + x, center.y + y};
  lastRenderTime = clock->getRealMillis();
}

bool MapGui::onKeyPressed2(Event::KeyEvent key) {
  const double scrollDist = key.shift ? 20 : 5;
  if (!keyScrolling)
    return false;
  switch (key.code) {
    case Keyboard::W:
      if (!options->getBoolValue(OptionId::WASD_SCROLLING) || key.alt)
        break;
    case Keyboard::Up:
    case Keyboard::Numpad8:
      softScroll(0, -scrollDist);
      break;
    case Keyboard::Numpad9:
      softScroll(scrollDist, -scrollDist);
      break;
    case Keyboard::D:
      if (!options->getBoolValue(OptionId::WASD_SCROLLING) || key.alt)
        break;
    case Keyboard::Right: 
    case Keyboard::Numpad6:
      softScroll(scrollDist, 0);
      break;
    case Keyboard::Numpad3:
      softScroll(scrollDist, scrollDist);
      break;
    case Keyboard::S:
      if (!options->getBoolValue(OptionId::WASD_SCROLLING) || key.alt)
        break;
    case Keyboard::Down:
    case Keyboard::Numpad2:
      softScroll(0, scrollDist);
      break;
    case Keyboard::Numpad1:
      softScroll(-scrollDist, scrollDist);
      break;
    case Keyboard::A:
      if (!options->getBoolValue(OptionId::WASD_SCROLLING) || key.alt)
        break;
    case Keyboard::Left:
    case Keyboard::Numpad4:
      softScroll(-scrollDist, 0);
      break;
    case Keyboard::Numpad7:
      softScroll(-scrollDist, -scrollDist);
      break;
    default: break;
  }
  if (softCenter) {
    softCenter->x = max(0.0, min<double>(softCenter->x, levelBounds.getKX()));
    softCenter->y = max(0.0, min<double>(softCenter->y, levelBounds.getKY()));
  }
  return false;
}

bool MapGui::onLeftClick(Vec2 v) {
  if (v.inRectangle(getBounds())) {
    if (auto c = getCreature(v)) {
      callbacks.creatureDragFun(c->id, c->viewId, v);
      creatureClick = true;
    } else
      creatureClick = false;
    mouseHeldPos = v;
    mouseOffset.x = mouseOffset.y = 0;
    return true;
  }
  return false;
}

bool MapGui::onRightClick(Vec2 pos) {
  if (clock->getRealMillis() - lastRightClick < 200) {
    lockedView = true;
    lastRightClick = -100000;
    return true;
  }
  lastRightClick = clock->getRealMillis();
  if (pos.inRectangle(getBounds())) {
    lastMousePos = pos;
    isScrollingNow = true;
    mouseOffset.x = mouseOffset.y = 0;
    lockedView = false;
    return true;
  }
  return false;
}

void MapGui::onMouseGone() {
  lastMouseMove = none;
}

void MapGui::considerMapLeftClick(Vec2 mousePos) {
  Vec2 pos = layout->projectOnMap(getBounds(), getScreenPos(), mousePos);
  if (!lastMapLeftClick || lastMapLeftClick != pos) {
    callbacks.leftClickFun(pos);
    lastMapLeftClick = pos;
  }
}

bool MapGui::onMouseMove(Vec2 v) {
  lastMouseMove = v;
  if (v.inRectangle(getBounds()) && mouseHeldPos && !creatureClick)
    considerMapLeftClick(v);
  if (isScrollingNow) {
    mouseOffset.x = double(v.x - lastMousePos.x) / layout->getSquareSize().x;
    mouseOffset.y = double(v.y - lastMousePos.y) / layout->getSquareSize().y;
    mouseOffset.x = min(mouseOffset.x, center.x);
    mouseOffset.y = min(mouseOffset.y, center.y);
    mouseOffset.x = max(mouseOffset.x, center.x - levelBounds.getKX());
    mouseOffset.y = max(mouseOffset.y, center.y - levelBounds.getKY());
    callbacks.refreshFun();
  }
  return false;
}

optional<MapGui::CreatureInfo&> MapGui::getCreature(Vec2 mousePos) {
  for (auto& elem : creatureMap)
    if (mousePos.inRectangle(elem.bounds))
      return elem;
  return none;
}

void MapGui::onMouseRelease(Vec2 v) {
  if (isScrollingNow) {
    if (fabs(mouseOffset.x) + fabs(mouseOffset.y) < 1)
      callbacks.rightClickFun(layout->projectOnMap(getBounds(), getScreenPos(), lastMousePos));
    else {
      center.x -= mouseOffset.x;
      center.y -= mouseOffset.y;
    }
    isScrollingNow = false;
    callbacks.refreshFun();
    mouseOffset.x = mouseOffset.y = 0;
  }
  if (mouseHeldPos) {
    if (mouseHeldPos->distD(v) > 10) {
      if (!creatureClick)
        considerMapLeftClick(v);
    }
    else {
      if (auto c = getCreature(*mouseHeldPos))
        callbacks.creatureClickFun(c->id);
      else    
        considerMapLeftClick(v);
    }
  }
  mouseHeldPos = none;
  lastMapLeftClick = none;
}

/*void MapGui::drawFloorBorders(Renderer& renderer, DirSet borders, int x, int y) {
  for (const Dir& dir : borders) {
    int coord;
    switch (dir) {
      case Dir::N: coord = 0; break;
      case Dir::E: coord = 1; break;
      case Dir::S: coord = 2; break;
      case Dir::W: coord = 3; break;
      default: continue;
    }
    renderer.drawTile(x, y, {Vec2(coord, 18), 1});
  }
}*/

static Color getMoraleColor(double morale) {
  if (morale < 0)
    return Color(255, 0, 0, -morale * 150);
  else
    return Color(0, 255, 0, morale * 150);
}

static Vec2 getAttachmentOffset(Dir dir, Vec2 size) {
  switch (dir) {
    case Dir::N: return Vec2(0, -size.y * 2 / 3);
    case Dir::S: return Vec2(0, size.y / 4);
    case Dir::E:
    case Dir::W: return Vec2(dir) * size.x / 2;
    default: FAIL << "Bad attachment dir " << int(dir);
  }
  return Vec2();
}

static double getJumpOffset(const ViewObject& object, double state) {
  if (object.hasModifier(ViewObjectModifier::NO_UP_MOVEMENT))
    return 0;
  if (state > 0.5)
    state -= 0.5;
  state *= 2;
  const double maxH = 0.09;
  return maxH * (1.0 - (2.0 * state - 1) * (2.0 * state - 1));
}

Vec2 MapGui::getMovementOffset(const ViewObject& object, Vec2 size, double time, int curTimeReal) {
  if (!object.hasAnyMovementInfo())
    return Vec2(0, 0);
  double state;
  Vec2 dir;
  if (screenMovement && 
      curTimeReal >= screenMovement->startTimeReal &&
      curTimeReal <= screenMovement->endTimeReal) {
    state = (double(curTimeReal) - screenMovement->startTimeReal) /
          (screenMovement->endTimeReal - screenMovement->startTimeReal);
    dir = object.getMovementInfo(screenMovement->startTimeGame, screenMovement->endTimeGame,
        screenMovement->creatureId);
  } else if (object.hasAnyMovementInfo() && !screenMovement) {
    ViewObject::MovementInfo info = object.getLastMovementInfo();
    dir = info.direction;
    if (info.direction.length8() == 0 || time >= info.tEnd || time <= info.tBegin)
      return Vec2(0, 0);
    state = (time - info.tBegin) / (info.tEnd - info.tBegin);
    double minStopTime = 0.2;
    state = min(1.0, max(0.0, (state - minStopTime) / (1.0 - 2 * minStopTime)));
  } else
    return Vec2(0, 0);
  if (object.getLastMovementInfo().type == ViewObject::MovementInfo::ATTACK)
    if (dir.length8() == 1)
      return Vec2(0.8 * (state < 0.5 ? state : 1 - state) * dir.x * size.x,
          (0.8 * (state < 0.5 ? state : 1 - state)* dir.y - getJumpOffset(object, state)) * size.y);
  return Vec2((state - 1) * dir.x * size.x, ((state - 1)* dir.y - getJumpOffset(object, state)) * size.y);
}

void MapGui::drawCreatureHighlights(Renderer& renderer, const ViewObject& object, Vec2 pos, Vec2 sz, int curTime) {
  if (object.hasModifier(ViewObject::Modifier::PLAYER))
    drawCreatureHighlight(renderer, pos, sz, colors[ColorId::ALMOST_WHITE]);
  if (object.hasModifier(ViewObject::Modifier::DRAW_MORALE) && showMorale)
    drawCreatureHighlight(renderer, pos, sz, getMoraleColor(object.getAttribute(ViewObject::Attribute::MORALE)));
  if (object.hasModifier(ViewObject::Modifier::TEAM_LEADER_HIGHLIGHT) && (curTime / 1000) % 2) {
    drawCreatureHighlight(renderer, pos, sz, colors[ColorId::YELLOW]);
  } else
  if (object.hasModifier(ViewObject::Modifier::TEAM_HIGHLIGHT))
    drawCreatureHighlight(renderer, pos, sz, colors[ColorId::YELLOW]);
  if (object.getCreatureId()) {
    Color c = getCreatureHighlight(*object.getCreatureId(), curTime);
    if (c.a > 0)
      drawCreatureHighlight(renderer, pos, sz, c);
  }
}

Color MapGui::getCreatureHighlight(UniqueEntity<Creature>::Id creature, int curTime) {
  if (teamHighlight[creature] > 0)
    return colors[ColorId::YELLOW];
  else
    return Color(0, 0, 0, 0);
}

static bool mirrorSprite(ViewId id) {
  switch (id) {
    case ViewId::GRASS:
    case ViewId::HILL:
      return true;
    default:
      return false;
  }
}

void MapGui::drawObjectAbs(Renderer& renderer, Vec2 pos, const ViewObject& object, Vec2 size,
    Vec2 tilePos, int curTimeReal, const EnumMap<HighlightType, double>& highlightMap) {
  const Tile& tile = Tile::getTile(object.id(), spriteMode);
  Color color = Renderer::getBleedingColor(object);
  if (object.hasModifier(ViewObject::Modifier::INVISIBLE) || object.hasModifier(ViewObject::Modifier::HIDDEN))
    color = transparency(color, 70);
  else
    if (tile.translucent > 0)
      color = transparency(color, 255 * (1 - tile.translucent));
    else if (object.hasModifier(ViewObject::Modifier::ILLUSION))
      color = transparency(color, 150);
  if (object.hasModifier(ViewObject::Modifier::PLANNED))
    color = transparency(color, 100);
  double waterDepth = object.getAttribute(ViewObject::Attribute::WATER_DEPTH);
  if (waterDepth > 0) {
    int val = max(0.0, 255.0 - min(2.0, waterDepth) * 60);
    color = Color(val, val, val);
  }
  if (spriteMode && tile.hasSpriteCoord()) {
    DirSet dirs;
    DirSet borderDirs;
    if (auto connectionId = getConnectionId(object))
      for (Vec2 dir : getConnectionDirs(object.id())) {
        if ((tilePos + dir).inRectangle(levelBounds) && connectionMap.has(tilePos + dir, *connectionId))
          dirs.insert(dir.getCardinalDir());
        else
          borderDirs.insert(dir.getCardinalDir());
      }
    Vec2 move;
    Vec2 movement = getMovementOffset(object, size, currentTimeGame, curTimeReal);
    drawCreatureHighlights(renderer, object, pos + movement, size, curTimeReal);
    if ((object.layer() == ViewLayer::CREATURE && object.id() != ViewId::BOULDER)
        || object.hasModifier(ViewObject::Modifier::ROUND_SHADOW)) {
      static auto coord = renderer.getTileCoord("round_shadow");
      renderer.drawTile(pos + movement, coord, size, sf::Color(255, 255, 255, 160));
      move.y = -4* size.y / renderer.getNominalSize().y;
    }
    static auto shortShadow = renderer.getTileCoord("short_shadow");
    if (auto background = tile.getBackgroundCoord()) {
      renderer.drawTile(pos, *background, size, color);
      if (shadowed.count(tilePos))
        renderer.drawTile(pos, shortShadow, size, sf::Color(255, 255, 255, 170));
    }
    if (auto dir = object.getAttachmentDir())
      move = getAttachmentOffset(*dir, size);
    move += movement;
    if (mirrorSprite(object.id()))
      renderer.drawTile(pos + move, tile.getSpriteCoord(dirs), size, color,
          object.getPositionHash() % 2, object.getPositionHash() % 4 > 1);
    else
      renderer.drawTile(pos + move, tile.getSpriteCoord(dirs), size, color);
    if (object.layer() == ViewLayer::FLOOR && highlightMap[HighlightType::CUT_TREE] > 0)
      if (auto coord = tile.getHighlightCoord())
        renderer.drawTile(pos + move, *coord, size, color);
    if (!buttonViewId)
      if (auto id = object.getCreatureId())
        creatureMap.push_back(CreatureInfo{Rectangle(pos + move, pos + move + size), *id, object.id()});
    if (tile.hasCorners()) {
      for (auto coord : tile.getCornerCoords(dirs))
        renderer.drawTile(pos + move, coord, size, color);
    }
/*    if (tile.floorBorders) {
      drawFloorBorders(renderer, borderDirs, x, y);
    }*/
    if ((object.layer() == ViewLayer::FLOOR || object.layer() == ViewLayer::FLOOR_BACKGROUND) && 
        shadowed.count(tilePos) && !tile.noShadow)
      renderer.drawTile(pos, shortShadow, size, sf::Color(255, 255, 255, 170));
    if (object.getAttribute(ViewObject::Attribute::BURNING) > 0) {
      static auto fire1 = renderer.getTileCoord("fire1");
      static auto fire2 = renderer.getTileCoord("fire2");
      renderer.drawTile(pos, Random.choose({fire1, fire2}), size);
    }
    static auto key = renderer.getTileCoord("key");
    if (object.hasModifier(ViewObject::Modifier::LOCKED))
      renderer.drawTile(pos, key, size);
  } else {
    Vec2 movement = getMovementOffset(object, size, currentTimeGame, curTimeReal);
    Vec2 tilePos = pos + movement + Vec2(size.x / 2, -3);
    drawCreatureHighlights(renderer, object, pos, size, curTimeReal);
    renderer.drawText(tile.symFont ? Renderer::SYMBOL_FONT : Renderer::TILE_FONT, size.y, Tile::getColor(object),
        tilePos.x, tilePos.y, tile.text, Renderer::HOR);
    if (!buttonViewId)
      if (auto id = object.getCreatureId())
        creatureMap.push_back(CreatureInfo{Rectangle(tilePos, tilePos + size), *id, object.id()});
    double burningVal = object.getAttribute(ViewObject::Attribute::BURNING);
    if (burningVal > 0) {
      renderer.drawText(Renderer::SYMBOL_FONT, size.y, getFireColor(), pos.x + size.x / 2, pos.y - 3, L'ѡ',
          Renderer::HOR);
      if (burningVal > 0.5)
        renderer.drawText(Renderer::SYMBOL_FONT, size.y, getFireColor(), pos.x + size.x / 2, pos.y - 3, L'Ѡ',
          Renderer::HOR);
    }
  }
}

void MapGui::resetScrolling() {
  lockedView = true;
}

void MapGui::clearCenter() {
  center = mouseOffset = {0.0, 0.0};
}

bool MapGui::isCentered() const {
  return center.x != 0 || center.y != 0;
}

void MapGui::setCenter(double x, double y) {
  center = {x, y};
  center.x = max(0.0, min<double>(center.x, levelBounds.getKX()));
  center.y = max(0.0, min<double>(center.y, levelBounds.getKY()));
}

void MapGui::setCenter(Vec2 v) {
  setCenter(v.x, v.y);
}

enum class MapGui::HintPosition {
  LEFT, RIGHT
};

void MapGui::drawHint(Renderer& renderer, Color color, const vector<string>& text) {
  int lineHeight = 30;
  int height = lineHeight * text.size();
  int width = 0;
  for (auto& s : text)
    width = max(width, renderer.getTextLength(s) + 110);
  Vec2 pos(getBounds().getKX() - width, getBounds().getKY() - height);
  renderer.drawFilledRectangle(pos.x, pos.y, pos.x + width, pos.y + height, Color(0, 0, 0, 150));
  for (int i : All(text))
    renderer.drawText(color, pos.x + 10, pos.y + 1 + i * lineHeight, text[i]);
}

void MapGui::drawFoWSprite(Renderer& renderer, Vec2 pos, Vec2 size, DirSet dirs) {
  const Tile& tile = Tile::getTile(ViewId::FOG_OF_WAR, true); 
  const Tile& tile2 = Tile::getTile(ViewId::FOG_OF_WAR_CORNER, true); 
  static DirSet fourDirs = DirSet({Dir::N, Dir::S, Dir::E, Dir::W});
  auto coord = tile.getSpriteCoord(dirs & fourDirs);
  renderer.drawTile(pos, coord, size);
  for (Dir dir : dirs.intersection(fourDirs.complement())) {
    static DirSet ne({Dir::N, Dir::E});
    static DirSet se({Dir::S, Dir::E});
    static DirSet nw({Dir::N, Dir::W});
    static DirSet sw({Dir::S, Dir::W});
    switch (dir) {
      case Dir::NE: if (!dirs.contains(ne)) continue;
      case Dir::SE: if (!dirs.contains(se)) continue;
      case Dir::NW: if (!dirs.contains(nw)) continue;
      case Dir::SW: if (!dirs.contains(sw)) continue;
      default: break;
    }
    renderer.drawTile(pos, tile2.getSpriteCoord(DirSet::oneElement(dir)), size);
  }
}

bool MapGui::isFoW(Vec2 pos) const {
  return !pos.inRectangle(Level::getMaxBounds()) || fogOfWar.getValue(pos);
}

void MapGui::renderExtraBorders(Renderer& renderer, int currentTimeReal) {
  extraBorderPos.clear();
  for (Vec2 wpos : layout->getAllTiles(getBounds(), levelBounds, getScreenPos()))
    if (objects[wpos] && objects[wpos]->hasObject(ViewLayer::FLOOR_BACKGROUND)) {
      ViewId viewId = objects[wpos]->getObject(ViewLayer::FLOOR_BACKGROUND).id();
      if (Tile::getTile(viewId, true).hasExtraBorders())
        for (Vec2 v : wpos.neighbors4())
          if (v.inRectangle(extraBorderPos.getBounds())) {
            if (extraBorderPos.isDirty(v))
              extraBorderPos.getDirtyValue(v).push_back(viewId);
            else
              extraBorderPos.setValue(v, {viewId});
          }
    }
  for (Vec2 wpos : layout->getAllTiles(getBounds(), levelBounds, getScreenPos()))
    for (ViewId id : extraBorderPos.getValue(wpos)) {
      const Tile& tile = Tile::getTile(id, true);
      for (ViewId underId : tile.getExtraBorderIds())
        if (connectionMap.has(wpos, underId)) {
          DirSet dirs = 0;
          for (Vec2 v : Vec2::directions4())
            if ((wpos + v).inRectangle(levelBounds) && connectionMap.has(wpos + v, id))
              dirs.insert(v.getCardinalDir());
          if (auto coord = tile.getExtraBorderCoord(dirs)) {
            Vec2 pos = projectOnScreen(wpos, currentTimeReal);
            renderer.drawTile(pos, *coord, layout->getSquareSize());
          }
        }
    }
}

Vec2 MapGui::projectOnScreen(Vec2 wpos, int curTime) {
  double x = wpos.x;
  double y = wpos.y;
  if (screenMovement) {
    if (curTime >= screenMovement->startTimeReal && curTime <= screenMovement->endTimeReal) {
      double state = (double(curTime) - screenMovement->startTimeReal) /
          (screenMovement->endTimeReal - screenMovement->startTimeReal);
      x += (1 - state) * (screenMovement->to.x - screenMovement->from.x);
      y += (1 - state) * (screenMovement->to.y - screenMovement->from.y);
    }
  }
  return layout->projectOnScreen(getBounds(), getScreenPos(), x, y);
}

void MapGui::renderHighlights(Renderer& renderer, Vec2 size, int currentTimeReal) {
  Rectangle allTiles = layout->getAllTiles(getBounds(), levelBounds, getScreenPos());
  Vec2 topLeftCorner = projectOnScreen(allTiles.getTopLeft(), currentTimeReal);
  for (Vec2 wpos : allTiles)
    if (auto& index = objects[wpos])
      if (index->hasAnyHighlight()) {
        Vec2 pos = topLeftCorner + (wpos - allTiles.getTopLeft()).mult(size);
        for (HighlightType highlight : ENUM_ALL(HighlightType))
          if (index->getHighlight(highlight) > 0)
            switch (highlight) {
              case HighlightType::CUT_TREE:
                if (spriteMode && index->hasObject(ViewLayer::FLOOR))
                  break;
              case HighlightType::FORBIDDEN_ZONE:
              case HighlightType::FETCH_ITEMS:
              case HighlightType::RECT_SELECTION:
              case HighlightType::RECT_DESELECTION:
              case HighlightType::PRIORITY_TASK:
              case HighlightType::DIG:
              case HighlightType::UNAVAILABLE:
                if (spriteMode) {
                  renderer.drawTile(pos, Tile::getTile(ViewId::DIG_MARK, true).getSpriteCoord(), size,
                      getHighlightColor(highlight, index->getHighlight(highlight)));
                  break;
                }
              default:
                renderer.addQuad(Rectangle(pos, pos + size),
                    getHighlightColor(highlight, index->getHighlight(highlight)));
                break;
            }
      }
  renderer.drawQuads();
}

void MapGui::renderAnimations(Renderer& renderer, int currentTimeReal) {
  animations = filter(std::move(animations), [=](const AnimationInfo& elem) 
      { return !elem.animation->isDone(currentTimeReal);});
  for (auto& elem : animations)
    elem.animation->render(
        renderer,
        getBounds(),
        projectOnScreen(elem.position, currentTimeReal),
        currentTimeReal);
}

MapGui::HighlightedInfo MapGui::getHighlightedInfo(Renderer& renderer, Vec2 size, int currentTimeReal) {
  HighlightedInfo ret {};
  Rectangle allTiles = layout->getAllTiles(getBounds(), levelBounds, getScreenPos());
  Vec2 topLeftCorner = projectOnScreen(allTiles.getTopLeft(), currentTimeReal);
  if (auto mousePos = getMousePos())
    if (mouseUI) {
      ret.tilePos = layout->projectOnMap(getBounds(), getScreenPos(), *mousePos);
      if (!buttonViewId && ret.tilePos->inRectangle(objects.getBounds()))
        for (Vec2 wpos : Rectangle(*ret.tilePos - Vec2(2, 2), *ret.tilePos + Vec2(2, 2))
            .intersection(objects.getBounds())) {
          Vec2 pos = topLeftCorner + (wpos - allTiles.getTopLeft()).mult(size);
          if (objects[wpos] && objects[wpos]->hasObject(ViewLayer::CREATURE)) {
            const ViewObject& object = objects[wpos]->getObject(ViewLayer::CREATURE);
            Vec2 movement = getMovementOffset(object, size, currentTimeGame, currentTimeReal);
            if (mousePos->inRectangle(Rectangle(pos + movement, pos + movement + size))) {
              ret.tilePos = none;
              ret.object = object;
              ret.creaturePos = pos + movement;
              ret.isEnemy = enemyPositions.getValue(wpos);
              break;
            }
          }
        }
    }
  return ret;
}

void MapGui::renderMapObjects(Renderer& renderer, Vec2 size, HighlightedInfo& highlightedInfo,int currentTimeReal) {
  Rectangle allTiles = layout->getAllTiles(getBounds(), levelBounds, getScreenPos());
  Vec2 topLeftCorner = projectOnScreen(allTiles.getTopLeft(), currentTimeReal);
  renderer.drawFilledRectangle(getBounds(), colors[ColorId::BLACK]);
  renderer.drawFilledRectangle(Rectangle(
        projectOnScreen(levelBounds.getTopLeft(), currentTimeReal),
        projectOnScreen(levelBounds.getBottomRight(), currentTimeReal)), colors[ColorId::BLACK]);
  fogOfWar.clear();
  creatureMap.clear();
  for (ViewLayer layer : layout->getLayers()) {
    for (Vec2 wpos : allTiles) {
      Vec2 pos = topLeftCorner + (wpos - allTiles.getTopLeft()).mult(size);
      if (!objects[wpos] || objects[wpos]->noObjects()) {
        if (layer == layout->getLayers().back()) {
          if (wpos.inRectangle(levelBounds))
            renderer.addQuad(Rectangle(pos, pos + size), colors[ColorId::BLACK]);
        }
        fogOfWar.setValue(wpos, true);
        continue;
      }
      const ViewIndex& index = *objects[wpos];
      const ViewObject* object = nullptr;
      if (spriteMode) {
        if (index.hasObject(layer))
          object = &index.getObject(layer);
      } else
        object = index.getTopObject(layout->getLayers());
      if (object) {
        drawObjectAbs(renderer, pos, *object, size, wpos, currentTimeReal, index.getHighlightMap());
        if (highlightedInfo.tilePos == wpos && object->layer() != ViewLayer::CREATURE)
          highlightedInfo.object = *object;
      }
      if (spriteMode && layer == layout->getLayers().back())
        if (!isFoW(wpos))
          drawFoWSprite(renderer, pos, size, DirSet(
              !isFoW(wpos + Vec2(Dir::N)),
              !isFoW(wpos + Vec2(Dir::S)),
              !isFoW(wpos + Vec2(Dir::E)),
              !isFoW(wpos + Vec2(Dir::W)),
              isFoW(wpos + Vec2(Dir::NE)),
              isFoW(wpos + Vec2(Dir::NW)),
              isFoW(wpos + Vec2(Dir::SE)),
              isFoW(wpos + Vec2(Dir::SW))));
    }
    if (layer == ViewLayer::FLOOR || !spriteMode) {
      if (!buttonViewId && highlightedInfo.creaturePos)
        drawCreatureHighlight(renderer, *highlightedInfo.creaturePos, size, colors[ColorId::ALMOST_WHITE]);
      if (highlightedInfo.tilePos)
        drawSquareHighlight(renderer, topLeftCorner + (*highlightedInfo.tilePos - allTiles.getTopLeft()).mult(size),
            size);
    }
    if (!spriteMode)
      break;
    if (layer == ViewLayer::FLOOR_BACKGROUND)
      renderExtraBorders(renderer, currentTimeReal);
  }
  renderer.drawQuads();
}

void MapGui::drawCreatureHighlight(Renderer& renderer, Vec2 pos, Vec2 size, Color color) {
  if (spriteMode)
    renderer.drawViewObject(pos + Vec2(0, size.y / 5), ViewId::CREATURE_HIGHLIGHT, true, size, color);
  else
    renderer.drawFilledRectangle(Rectangle(pos, pos + size), Color::Transparent, color);
}

void MapGui::drawSquareHighlight(Renderer& renderer, Vec2 pos, Vec2 size) {
  if (spriteMode)
    renderer.drawViewObject(pos, ViewId::SQUARE_HIGHLIGHT, true, size, colors[ColorId::ALMOST_WHITE]);
  else
    renderer.drawFilledRectangle(Rectangle(pos, pos + size), Color::Transparent, colors[ColorId::LIGHT_GRAY]);
}

void MapGui::considerRedrawingSquareHighlight(Renderer& renderer, int currentTimeReal, Vec2 pos, Vec2 size) {
  Rectangle allTiles = layout->getAllTiles(getBounds(), levelBounds, getScreenPos());
  Vec2 topLeftCorner = projectOnScreen(allTiles.getTopLeft(), currentTimeReal);
  for (Vec2 v : concat({pos}, pos.neighbors8()))
    if (v.inRectangle(objects.getBounds()) && (!objects[v] || objects[v]->noObjects())) {
      drawSquareHighlight(renderer, topLeftCorner + (pos - allTiles.getTopLeft()).mult(size), size);
      break;
    }
}

void MapGui::render(Renderer& renderer) {
  Vec2 size = layout->getSquareSize();
  int currentTimeReal = clock->getRealMillis();
  if (softCenter) {
    double moveDist = (currentTimeReal - lastRenderTime) / 20;
    double offsetx = softCenter->x - center.x;
    double offsety = softCenter->y - center.y;
    double offset = sqrt(offsetx * offsetx + offsety * offsety);
    if (offset <= moveDist)
      softCenter = none;
    else {
      offsetx /= offset;
      offsety /= offset;
      center.x += offsetx * moveDist;
      center.y += offsety * moveDist;
    }
    lastRenderTime = currentTimeReal;
  }
  HighlightedInfo highlightedInfo = getHighlightedInfo(renderer, size, currentTimeReal);
  renderMapObjects(renderer, size, highlightedInfo, currentTimeReal);
  renderHighlights(renderer, size, currentTimeReal);
  renderAnimations(renderer, currentTimeReal);
  if (highlightedInfo.tilePos)
    considerRedrawingSquareHighlight(renderer, currentTimeReal, *highlightedInfo.tilePos, size);
  if (displayScrollHint && isScrollingNow) {
    drawHint(renderer, colors[ColorId::LIGHT_BLUE], {"Double right-click to scroll back to creature."});
  } else
  if (!hint.empty())
    drawHint(renderer, colors[ColorId::WHITE], hint);
  else
  if (highlightedInfo.object) {
    Color col = colors[ColorId::WHITE];
    if (highlightedInfo.isEnemy)
      col = colors[ColorId::RED];
    drawHint(renderer, col, highlightedInfo.object->getLegend());
  }
  if (spriteMode && buttonViewId && renderer.getMousePos().inRectangle(getBounds()))
    renderer.drawViewObject(renderer.getMousePos() + Vec2(15, 15), *buttonViewId, spriteMode, size);
}

void MapGui::setHint(const vector<string>& h) {
  hint = h;
}

void MapGui::updateEnemyPositions(const vector<Vec2>& positions) {
  enemyPositions.clear();
  for (Vec2 v : positions)
    enemyPositions.setValue(v, true);
}

void MapGui::updateObjects(const CreatureView* view, MapLayout* mapLayout, bool smoothMovement, bool ui,
    bool moral) {
  const Level* level = view->getLevel();
  levelBounds = view->getLevel()->getBounds();
  updateEnemyPositions(view->getVisibleEnemies());
  mouseUI = ui;
  showMorale = moral;
  layout = mapLayout;
  for (Vec2 pos : mapLayout->getAllTiles(getBounds(), Level::getMaxBounds(), getScreenPos()))
    objects[pos] = none;
  displayScrollHint = view->isPlayerView() && !lockedView;
  if (!isCentered() || (view->isPlayerView() && lockedView))
    setCenter(view->getPosition());
  keyScrolling = !view->isPlayerView();
  for (Vec2 pos : mapLayout->getAllTiles(getBounds(), Level::getMaxBounds(), getScreenPos())) 
    if (level->inBounds(pos)) {
      objects[pos].emplace();
      view->getViewIndex(pos, *objects[pos]);
      if (objects[pos]->hasObject(ViewLayer::FLOOR) || objects[pos]->hasObject(ViewLayer::FLOOR_BACKGROUND))
        objects[pos]->setHighlight(HighlightType::NIGHT, 1.0 - view->getLevel()->getLight(pos));
    }
  currentTimeGame = smoothMovement ? view->getTime() : 1000000000;
  if (smoothMovement) {
    if (auto movement = view->getMovementInfo()) {
      if (!screenMovement || screenMovement->startTimeGame != movement->prevTime) {
        screenMovement = {
          movement->from,
          movement->to,
          clock->getRealMillis(),
          clock->getRealMillis() + 100,
          movement->prevTime,
          currentTimeGame,
          movement->creatureId
        };
      }
    } else
      screenMovement = none;
  }
  connectionMap.clear();
  shadowed.clear();
  for (Vec2 wpos : layout->getAllTiles(getBounds(), objects.getBounds(), getScreenPos()))
    if (auto& index = objects[wpos]) {
      if (index->hasObject(ViewLayer::FLOOR)) {
        const ViewObject& object = index->getObject(ViewLayer::FLOOR);
        if (object.hasModifier(ViewObject::Modifier::CASTS_SHADOW)) {
          shadowed.erase(wpos);
          shadowed.insert(wpos + Vec2(0, 1));
        }
        if (auto id = getConnectionId(object))
          connectionMap.add(wpos, *id);
      }
      if (index->hasObject(ViewLayer::FLOOR_BACKGROUND)) {
        if (auto id = getConnectionId(index->getObject(ViewLayer::FLOOR_BACKGROUND)))
          connectionMap.add(wpos, *id);
      }
      if (auto viewId = index->getHiddenId())
        if (auto id = getConnectionId(*viewId))
          connectionMap.add(wpos, *id);
    }
}

