#ifndef _LEVEL_BUILDER_H
#define _LEVEL_BUILDER_H


#include "util.h"
#include "square_type.h"
#include "level.h"
#include "square_array.h"

class ProgressMeter;
class Model;
class LevelMaker;
class Square;

RICH_ENUM(SquareAttrib,
  NO_DIG,
  GLACIER,
  MOUNTAIN,
  HILL,
  LOWLAND,
  CONNECT_ROAD, 
  CONNECT_CORRIDOR,
  CONNECTOR,
  LAKE,
  RIVER,
  ROAD_CUT_THRU,
  NO_ROAD,
  ROOM,
  COLLECTIVE_START,
  COLLECTIVE_STAIRS,
  EMPTY_ROOM,
  BUILDINGS_CENTER,
  CASTLE_CORNER,
  FOG,
  FORREST,
  LOCATION,
  SOKOBAN_ENTRY,
  SOKOBAN_PRIZE
);

class LevelBuilder {
  public:
  /** Constructs a builder with given size and name. */
  LevelBuilder(ProgressMeter*, RandomGen&, int width, int height, const string& name, bool covered = true,
      optional<double> defaultLight = none);
  LevelBuilder(RandomGen&, int width, int height, const string& name, bool covered = true);
  
  LevelBuilder(LevelBuilder&&) = default;

  /** Returns a given square.*/
  const Square* getSquare(Vec2);
  Square* modSquare(Vec2);

  /** Checks if it's possible to put a creature on given square.*/
  bool canPutCreature(Vec2, Creature*);

  /** Puts a creatue on a given square. If the square is later changed to something else, the creature remains.*/
  void putCreature(Vec2, PCreature);

  /** Puts items on a given square. If the square is later changed to something else, the items remain.*/
  void putItems(Vec2, vector<PItem> items);

  /** Sets the message displayed when the player first enters the level.*/
  void setMessage(const string& message);

  /** Builds the level. The level will keep reference to the model.
      \paramname{surface} tells if this level is on the Earth surface.*/
  PLevel build(Model*, LevelMaker*, LevelId);

  //@{
  /** Puts a square on given position. Sets optional attributes of the square. The attributes remain if the square is changed.*/
  void putSquare(Vec2, SquareType, optional<SquareAttrib> = none);
  void putSquare(Vec2, SquareType, vector<SquareAttrib> attribs);
  void putSquare(Vec2, PSquare, SquareType, optional<SquareAttrib> = none);
  void putSquare(Vec2, PSquare, SquareType, vector<SquareAttrib> attribs);
  //@}

  /** Returns the square type.*/
  const SquareType& getType(Vec2);

  /** Checks if the given square has an attribute.*/
  bool hasAttrib(Vec2 pos, SquareAttrib attr);

  /** Adds attribute to given square. The attribute will remain if the square is changed.*/
  void addAttrib(Vec2 pos, SquareAttrib attr);

  /** Removes attribute from given square.*/
  void removeAttrib(Vec2 pos, SquareAttrib attr);

  /** Sets the height of the given square.*/
  void setHeightMap(Vec2 pos, double h);

  /** Returns the height of the given square.*/
  double getHeightMap(Vec2 pos);

  /** Adds a location to the level and sets its coordinates.*/
  void addLocation(Location*, Rectangle area);

  Rectangle toGlobalCoordinates(Rectangle);

  /** Adds a collective to the level and initializes it.*/
  void addCollective(CollectiveBuilder*);

  /** Sets the cover of the square. The value will remain if square is changed.*/
  void setCoverOverride(Vec2, bool covered);
  void setSunlight(Vec2, double);

  void setNoDiagonalPassing();

  void setUnavailable(Vec2);
 
  enum Rot { CW0, CW1, CW2, CW3};

  void pushMap(Rectangle bounds, Rot);
  void popMap();

  RandomGen& getRandom();
  
  private:
  Vec2 transform(Vec2);
  SquareArray squares;
  Table<optional<ViewObject>> background;
  Table<bool> unavailable;
  Table<double> heightMap;
  Table<double> dark;
  vector<Location*> locations;
  vector<CollectiveBuilder*> collectives;
  Table<optional<bool>> coverOverride;
  Table<double> sunlight;
  bool allCovered;
  Table<EnumSet<SquareAttrib>> attrib;
  Table<SquareType> type;
  vector<pair<PCreature, Vec2>> creatures;
  Table<vector<PItem>> items;
  string name;
  vector<Vec2::LinearMap> mapStack;
  ProgressMeter* progressMeter = nullptr;
  RandomGen& random;
  bool noDiagonalPassing = false;
};

#endif
