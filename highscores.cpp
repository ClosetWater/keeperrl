#include "stdafx.h"
#include "highscores.h"
#include "view.h"
#include "file_sharing.h"
#include "options.h"
#include "campaign_type.h"
#include "player_role.h"
#include "parse_game.h"

const static int highscoreVersion = 2;

Highscores::Highscores(const FilePath& local, FileSharing& sharing, Options* o)
    : localPath(local), fileSharing(sharing), options(o) {
  localScores = fromFile(localPath);
  remoteScores = fromString(fileSharing.downloadHighscores(highscoreVersion));
  fileSharing.uploadHighscores(localPath);
}

void Highscores::add(Score s) {
  s.version = highscoreVersion;
  localScores.push_back(s);
  saveToFile(localScores, localPath);
  remoteScores.push_back(s);
  fileSharing.uploadHighscores(localPath);
}

vector<Highscores::Score> Highscores::fromStream(istream& in) {
  vector<Score> ret;
  while (1) {
    char buf[1000];
    in.getline(buf, 1000);
    if (!in)
      break;
    if (auto score = Score::parse(buf))
      if (score->version == highscoreVersion)
        ret.push_back(*score);
  }
  return ret;
}

void Highscores::saveToFile(const vector<Score>& scores, const FilePath& path) {
  CompressedOutput out(path.getPath());
  out.getArchive() << scores;
}

bool Highscores::Score::operator == (const Score& s) const {
  return gameId == s.gameId && playerName == s.playerName && worldName == s.worldName && gameResult == s.gameResult
    && points == s.points && turns == s.turns;
}

vector<Highscores::Score> Highscores::fromFile(const FilePath& path) {
  vector<Highscores::Score> scores;
  try {
    CompressedInput in(path.getPath());
    in.getArchive() >> scores;
  } catch (...) {}
  return scores.filter([](const Score& s) { return s.version == highscoreVersion;});
}

vector<Highscores::Score> Highscores::fromString(const string& s) {
  stringstream in(s);
  return fromStream(in);
}

optional<Highscores::Score> Highscores::Score::parse(const string& buf) {
  vector<string> p = split(buf, {','});
  if (p.size() == 10)
    try {
      return CONSTRUCT(Score,
          c.gameId = p[0];
          c.playerName = p[1];
          c.worldName = p[2];
          c.gameResult = p[3];
          c.gameWon = ::fromString<int>(p[4]);
          c.points = ::fromString<int>(p[5]);
          c.turns = ::fromString<int>(p[6]);
          c.campaignType = EnumInfo<CampaignType>::fromStringWithException(p[7]);
          c.playerRole = EnumInfo<PlayerRole>::fromStringWithException(p[8]);
          c.version = ::fromString<int>(p[9]);
      );
    } catch (ParsingException&) {}
#ifndef RELEASE
    return CONSTRUCT(Score, c.playerName = "ERROR: " + buf;);
#else
    return none;
#endif
}

bool sortByShortestMostPoints(const Highscores::Score& a, const Highscores::Score& b) {
  if (a.gameWon) {
    if (!b.gameWon)
      return true;
    else
      return a.turns < b.turns;
  } else
  if (b.gameWon)
    return false;
  else
    return a.points > b.points || (a.points == b.points && a.gameId < b.gameId);
}

bool sortByLongest(const Highscores::Score& a, const Highscores::Score& b) {
  return a.turns > b.turns;
}

enum class SortingType {
  SHORTEST_MOST_POINTS,
  LONGEST
};

typedef Highscores::Score Score;

typedef bool (*SortingFun)(const Score&, const Score&);

SortingFun getSortingFun(SortingType type) {
  switch (type) {
    case SortingType::SHORTEST_MOST_POINTS:
      return sortByShortestMostPoints;
    case SortingType::LONGEST:
      return sortByLongest;
  }
}

string getPointsColumn(const Score& score, SortingType sorting) {
  switch (sorting) {
    case SortingType::SHORTEST_MOST_POINTS:
      return score.gameWon ? toString(score.turns) + " turns" : toString(score.points) + " points";
    case SortingType::LONGEST:
      return toString(score.turns) + " turns";
  }
}

static HighscoreList fillScores(const string& name, optional<Score> lastElem, vector<Score> scores, SortingType sorting) {
  HighscoreList list { name, {} };
  sort(scores.begin(), scores.end(), getSortingFun(sorting));
  for (auto& score : scores) {
    bool highlight = lastElem == score;
    string points = getPointsColumn(score, sorting);
    if (score.gameResult.empty())
      list.scores.push_back({score.playerName + " of " + score.worldName, points, highlight});
    else
      list.scores.push_back({score.playerName + " of " + score.worldName +
          ", " + score.gameResult, points, highlight});
  }
  return list;
}

struct PublicScorePage {
  CampaignType type;
  PlayerRole role;
  const char* name;
  SortingType sorting;
};

static vector<PublicScorePage> getPublicScores() {
  return {
    {CampaignType::CAMPAIGN, PlayerRole::KEEPER, "Keeper", SortingType::SHORTEST_MOST_POINTS},
    {CampaignType::CAMPAIGN, PlayerRole::ADVENTURER, "Adventurer", SortingType::SHORTEST_MOST_POINTS},
    {CampaignType::SINGLE_KEEPER, PlayerRole::KEEPER, "Single map", SortingType::SHORTEST_MOST_POINTS},
    {CampaignType::ENDLESS, PlayerRole::KEEPER, "Endless", SortingType::LONGEST},
  };
}

bool Score::isPublic() const {
  for (auto& elem : getPublicScores())
    if (elem.type == campaignType && elem.role == playerRole)
      return true;
  return false;
}

void Highscores::present(View* view, optional<Score> lastAdded) const {
  if (lastAdded && !lastAdded->isPublic())
    return;
  vector<HighscoreList> lists;
  for (auto& elem : getPublicScores())
    lists.push_back(fillScores(elem.name, lastAdded, localScores.filter(
        [&] (const Score& s) { return s.campaignType == elem.type && s.playerRole == elem.role;}), elem.sorting));
  for (auto& elem : getPublicScores())
    lists.push_back(fillScores(elem.name, lastAdded, remoteScores.filter(
        [&] (const Score& s) { return s.campaignType == elem.type && s.playerRole == elem.role;}), elem.sorting));
  view->presentHighscores(lists);
}

