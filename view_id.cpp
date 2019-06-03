#include "stdafx.h"
#include "util.h"
#include "view_id.h"

ViewId::ViewId(const char* s, Color color) : ContentId(s), color(color) {}

bool ViewId::operator == (const ViewId& o) const {
  return ContentId<ViewId>::operator == (o) && color == o.color;
}

bool ViewId::operator !=(const ViewId& o) const {
  return !(*this == o);
}

bool ViewId::operator < (const ViewId& o) const {
  return std::forward_as_tuple(getInternalId(), color) < std::forward_as_tuple(o.getInternalId(), o.color);
}

int ViewId::getHash() const {
  return combineHash(ContentId<ViewId>::getHash(), color);
}

const Color& ViewId::getColor() const {
  return color;
}

std::ostream& operator <<(std::ostream& d, ViewId id) {
  return d << id.data();
}

template <class Archive>
void ViewId::serialize(Archive& ar1, const unsigned int) {
  ar1(SUBCLASS(ContentId), color);
}

SERIALIZABLE(ViewId)

SERIALIZATION_CONSTRUCTOR_IMPL(ViewId)

#include "pretty_archive.h"
template<>
void ViewId::serialize(PrettyInputArchive& ar, unsigned) {
  string SERIAL(id);
  ar(NAMED(id));
  Color colorInfo = Color::WHITE;
  ar >> OPTION(colorInfo);
  ar >> endInput();
  if (colorInfo != Color::WHITE)
    color = colorInfo;
  *this = ViewId(id.data(), colorInfo);
}

#include "text_serialization.h"
template void ViewId::serialize(TextInputArchive&, unsigned);
template void ViewId::serialize(TextOutputArchive&, unsigned);
