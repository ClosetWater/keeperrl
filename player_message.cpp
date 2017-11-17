#include "stdafx.h"
#include "player_message.h"
#include "view.h"

PlayerMessage::PlayerMessage(const string& t, MessagePriority p) : text(makeSentence(t)), priority(p), freshness(1) {}
PlayerMessage::PlayerMessage(const char* t, MessagePriority p) : text(makeSentence(t)), priority(p), freshness(1) {}

PlayerMessage::PlayerMessage(const string& title, const string& t) : text(t), priority(MessagePriority::NORMAL), 
    announcementTitle(title) {}

PlayerMessage PlayerMessage::announcement(const string& title, const string& text) {
  return PlayerMessage(title, text);
}

void PlayerMessage::presentMessages(View* view, const vector<PlayerMessage>& messages) {
  view->presentList("Message history", messages.transform(
        [](const PlayerMessage& msg) { return ListElem(msg.text).setMessagePriority(msg.priority);}), true);
}

optional<string> PlayerMessage::getAnnouncementTitle() const {
  return announcementTitle;
}

string PlayerMessage::getText() const {
  if (isClickable())
    return text.substr(0, text.size() - 1);
  return text;
}

double PlayerMessage::getFreshness() const {
  return freshness;
}

void PlayerMessage::setFreshness(double val) {
  freshness = val;
}

MessagePriority PlayerMessage::getPriority() const {
  return priority;
}

PlayerMessage& PlayerMessage::setPosition(Position pos) {
  position = pos;
  return *this;
}

optional<Position> PlayerMessage::getPosition() const {
  return position;
}

PlayerMessage& PlayerMessage::setCreature(UniqueEntity<Creature>::Id id) {
  creature = id;
  return *this;
}

optional<UniqueEntity<Creature>::Id> PlayerMessage::getCreature() const {
  return creature;
}

bool PlayerMessage::isClickable() const {
  return position || creature;
}

int PlayerMessage::getHash() const {
  return combineHash(text, priority, freshness);
}

SERIALIZE_DEF(PlayerMessage, SUBCLASS(UniqueEntity), text, priority, freshness, announcementTitle, position, creature)
SERIALIZATION_CONSTRUCTOR_IMPL(PlayerMessage);
