#include "stdafx.h"
#include "tile_efficiency.h"
#include "collective_config.h"
#include "furniture.h"

SERIALIZE_DEF(TileEfficiency, efficiency)
SERIALIZATION_CONSTRUCTOR_IMPL(TileEfficiency)

static const double lightBase = 0.5;
static const double flattenVal = 0.9;

double TileEfficiency::getEfficiency(Position pos) const {
  return (1 + efficiency.get(pos)) * min(1.0, (lightBase + pos.getLight() * (1 - lightBase)) / flattenVal);
}

void TileEfficiency::update(Position pos) {
  for (Position updated : concat({pos}, pos.neighbors8())) {
    double value = 0;
    for (Position p : concat({updated}, updated.neighbors8()))
      for (auto f : p.getFurniture())
        value += CollectiveConfig::getEfficiencyBonus(f->getType());
    efficiency.set(updated, value);
    updated.setNeedsRenderUpdate(true);
  }
}
