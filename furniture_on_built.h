#pragma once

#include "util.h"

class Position;

RICH_ENUM(
  FurnitureOnBuilt,
  DOWN_STAIRS,
  SET_ON_FIRE
);

void handleOnBuilt(Position, Furniture*, FurnitureOnBuilt);
