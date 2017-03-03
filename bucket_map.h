#pragma once

#include "util.h"

template <typename T>
class BucketMap {
  public:
  BucketMap(int width, int height, int bucketSize);

  void addElement(Vec2, T);
  void removeElement(Vec2, T);
  void moveElement(Vec2 from, Vec2 to, T);

  vector<T> getElements(Rectangle area) const;

  SERIALIZATION_DECL(BucketMap);

  private:
  int SERIAL(bucketSize);
  Table<unordered_set<T>> SERIAL(buckets);
};

class Creature;
class CreatureBucketMap : public BucketMap<Creature*> {
  public:
  using BucketMap::BucketMap;
};
