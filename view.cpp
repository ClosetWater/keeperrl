/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "view.h"

ListElem::ListElem(const string& t, ElemMod m, optional<UserInputId> a) : text(t), mod(m), action(a) {
}

ListElem::ListElem(const string& t, const string& sec, ElemMod m) : text(t), secondColumn(sec), mod(m) {
}

ListElem::ListElem(const char* s, ElemMod m, optional<UserInputId> a) : text(s), mod(m), action(a) {
}

ListElem& ListElem::setTip(const string& s) {
  tooltip = s;
  return *this;
}

ListElem& ListElem::setMessagePriority(MessagePriority p) {
  messagePriority = p;
  return *this;
}

optional<MessagePriority> ListElem::getMessagePriority() const {
  return messagePriority;
}

const string& ListElem::getText() const {
  return text;
}

const string& ListElem::getSecondColumn() const {
  return secondColumn;
}

const string& ListElem::getTip() const {
  return tooltip;
}

ListElem::ElemMod ListElem::getMod() const {
  return mod;
}

void ListElem::setMod(ElemMod m) {
  mod = m;
}

optional<UserInputId> ListElem::getAction() const {
  return action;
}

vector<ListElem> ListElem::convert(const vector<string>& v) {
  return v.transform([](const string& s) { return ListElem(s); });
}

View::View() {

}

View::~View() {
}
