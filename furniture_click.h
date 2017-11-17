#pragma once

class Furniture;
class Position;

#include "util.h"

enum class FurnitureClickType { LOCK, UNLOCK };

class FurnitureClick {
  public:
  static void handle(FurnitureClickType, Position, WConstFurniture);
};
