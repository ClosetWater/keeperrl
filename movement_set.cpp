#include "stdafx.h"
#include "movement_set.h"
#include "movement_type.h"
#include "tribe.h"

template <class Archive> 
void MovementSet::serialize(Archive& ar, const unsigned int version) {
  ar(traits, forcibleTraits, blockingEnemies, tribe);
}

SERIALIZABLE(MovementSet);
SERIALIZATION_CONSTRUCTOR_IMPL(MovementSet)

MovementSet::MovementSet(TribeId id) : tribe(id) {
}

bool MovementSet::canEnter(const MovementType& movementType, bool covered, bool onFire,
    const optional<TribeId>& forbidden) const {
  if (blockingEnemies && !movementType.isCompatible(tribe))
    return false;
  if (!movementType.isForced()) {
    if ((!covered && movementType.isSunlightVulnerable()) ||
        (onFire && !movementType.isFireResistant()) ||
        (forbidden && movementType.isCompatible(*forbidden)))
      return false;
  }
  EnumSet<MovementTrait> rightTraits(traits);
  if (movementType.isForced())
    rightTraits = rightTraits.sum(forcibleTraits);
  for (auto trait : rightTraits)
    if (movementType.hasTrait(trait))
      return true;
  return false;
}

bool MovementSet::canEnter(const MovementType& t) const {
  return canEnter(t, true, false, none);
}

bool MovementSet::hasTrait(MovementTrait trait) const {
  return traits.contains(trait);
}

MovementSet& MovementSet::addTrait(MovementTrait trait) {
  traits.insert(trait);
  return *this;
}

MovementSet& MovementSet::removeTrait(MovementTrait trait) {
  traits.erase(trait);
  return *this;
}

MovementSet& MovementSet::addForcibleTrait(MovementTrait trait) {
  forcibleTraits.insert(trait);
  return *this;
}

MovementSet& MovementSet::setBlockingEnemies() {
  blockingEnemies = true;
  return *this;
}

TribeId MovementSet::getTribe() const {
  return tribe;
}

void MovementSet::setTribe(TribeId id) {
  tribe = id;
}

MovementSet& MovementSet::clearTraits() {
  traits.clear();
  forcibleTraits.clear();
  return *this;
}
