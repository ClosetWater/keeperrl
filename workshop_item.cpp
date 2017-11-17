#include "stdafx.h"
#include "workshop_item.h"
#include "item_factory.h"
#include "item.h"
#include "view_object.h"

WorkshopItem WorkshopItem::fromType(ItemType type, double workNeeded, CostInfo cost) {
  PItem item = type.get();
  return {
    type,
    item->getName(),
    item->getName(true),
    item->getViewObject().id(),
    cost,
    item->getDescription(),
    1,
    1,
    workNeeded,
    none
  };
}

WorkshopItem& WorkshopItem::setBatchSize(int size) {
  batchSize = size;
  return *this;
}

WorkshopItem& WorkshopItem::setTechId(TechId id) {
  techId = id;
  return *this;
}

WorkshopItem& WorkshopItem::setTutorialHighlight(TutorialHighlight h) {
  tutorialHighlight = h;
  return *this;
}
