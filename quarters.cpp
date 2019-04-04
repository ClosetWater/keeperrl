#include "stdafx.h"
#include "quarters.h"
#include "view_id.h"


void Quarters::assign(optional<int> index, UniqueEntity<Creature>::Id id) {
  if (index)
    assignment.set(id, *index);
  else
    assignment.erase(id);
}

optional<int> Quarters::getAssigned(UniqueEntity<Creature>::Id id) const {
  return assignment.getMaybe(id);
}

const static vector<Quarters::QuartersInfo> allQuarters {
  { ZoneId::QUARTERS1, ViewId("quarters1") },
  { ZoneId::QUARTERS2, ViewId("quarters2") },
  { ZoneId::QUARTERS3, ViewId("quarters3") },
};

const vector<Quarters::QuartersInfo>& Quarters::getAllQuarters() {
  return allQuarters;
}

SERIALIZE_DEF(Quarters, assignment)
