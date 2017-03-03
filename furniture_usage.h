#pragma once

#include "util.h"

class Position;
class Furniture;
class Creature;

enum class FurnitureUsageType {
    CHEST,
    COFFIN,
    VAMPIRE_COFFIN,
    SLEEP,
    FOUNTAIN,
    KEEPER_BOARD,
    CROPS,
    STAIRS,
    TIE_UP,
    TRAIN
};
class FurnitureUsage {
  public:
  static void handle(FurnitureUsageType, Position, const Furniture*, Creature*);
  static bool canHandle(FurnitureUsageType, const Creature*);
  static string getUsageQuestion(FurnitureUsageType, string furnitureName);
};
