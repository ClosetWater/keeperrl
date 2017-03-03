#include "stdafx.h"
#include "game.h"
#include "view.h"
#include "clock.h"
#include "tribe.h"
#include "music.h"
#include "player_control.h"
#include "village_control.h"
#include "model.h"
#include "creature.h"
#include "spectator.h"
#include "statistics.h"
#include "collective.h"
#include "options.h"
#include "territory.h"
#include "level.h"
#include "highscores.h"
#include "player.h"
#include "item_factory.h"
#include "item.h"
#include "map_memory.h"
#include "creature_attributes.h"
#include "name_generator.h"
#include "campaign.h"
#include "save_file_info.h"
#include "file_sharing.h"
#include "villain_type.h"
#include "square_type.h"
#include "attack_trigger.h"
#include "view_object.h"
#include "campaign.h"
#include "construction_map.h"
#include "campaign_builder.h"
#include "campaign_type.h"
#include "game_save_type.h"
#include "player_role.h"

template <class Archive> 
void Game::serialize(Archive& ar, const unsigned int version) { 
  serializeAll(ar, villainsByType, collectives, lastTick, playerControl, playerCollective, currentTime);
  serializeAll(ar, musicType, portals, statistics, spectator, tribes, gameIdentifier, player);
  serializeAll(ar, gameDisplayName, finishCurrentMusic, models, visited, baseModel, campaign, localTime, turnEvents);
  if (Archive::is_loading::value)
    sunlightInfo.update(currentTime);
}

SERIALIZABLE(Game);
SERIALIZATION_CONSTRUCTOR_IMPL(Game);

static string getGameId(SaveFileInfo info) {
  return info.filename.substr(0, info.filename.size() - 4);
}

Game::Game(Table<PModel>&& m, Vec2 basePos, const CampaignSetup& c)
    : models(std::move(m)), visited(models.getBounds(), false), baseModel(basePos),
      tribes(Tribe::generateTribes()), musicType(MusicType::PEACEFUL), campaign(c.campaign) {
  sunlightInfo.update(currentTime);
  gameIdentifier = c.gameIdentifier;
  gameDisplayName = c.gameDisplayName;
  for (Vec2 v : models.getBounds())
    if (Model* m = models[v].get()) {
      for (Collective* c : m->getCollectives()) {
        collectives.push_back(c);
        if (auto type = c->getVillainType()) {
          villainsByType[*type].push_back(c);
          if (*type == VillainType::PLAYER) {
            playerControl = NOTNULL(dynamic_cast<PlayerControl*>(c->getControl()));
            playerCollective = c;
          }
        }
      }
      m->setGame(this);
      m->updateSunlightMovement();
    }
  turnEvents = {0, 10, 50, 100, 300, 500};
  for (int i : Range(200))
    turnEvents.insert(1000 * (i + 1));
}

Game::~Game() {}

PGame Game::campaignGame(Table<PModel>&& models, CampaignSetup& setup) {
  PGame ret(new Game(std::move(models), *setup.campaign.getPlayerPos(), setup));
  if (setup.campaign.getPlayerRole() == PlayerRole::ADVENTURER)
    ret->getMainModel()->landHeroPlayer(std::move(setup.player));
  return ret;
}

PGame Game::splashScreen(PModel&& model, const CampaignSetup& s) {
  Table<PModel> t(1, 1);
  t[0][0] = std::move(model);
  PGame game(new Game(std::move(t), Vec2(0, 0), s));
  game->spectator.reset(new Spectator(game->models[0][0]->getTopLevel()));
  game->turnEvents.clear();
  return game;
}

bool Game::isTurnBased() {
  return !spectator && (!playerControl || playerControl->isTurnBased());
}

double Game::getGlobalTime() const {
  return currentTime;
}

const vector<Collective*>& Game::getVillains(VillainType type) const {
  static vector<Collective*> empty;
  if (villainsByType.count(type))
    return villainsByType.at(type);
  else
    return empty;
}

Model* Game::getCurrentModel() const {
  if (Creature* c = getPlayer())
    return c->getPosition().getModel();
  else
    return models[baseModel].get();
}

PModel& Game::getMainModel() {
  return models[baseModel];
}

vector<Model*> Game::getAllModels() const {
  vector<Model*> ret;
  for (Vec2 v : models.getBounds())
    if (models[v])
      ret.push_back(models[v].get());
  return ret;
}

bool Game::isSingleModel() const {
  return models.getBounds().getSize() == Vec2(1, 1);
}

int Game::getSaveProgressCount() const {
  int saveTime = 0;
  for (auto model : getAllModels())
    saveTime += model->getSaveProgressCount();
  return saveTime;
}

void Game::prepareSiteRetirement() {
  Model* mainModel = models[baseModel].get();
  for (Vec2 v : models.getBounds())
    if (models[v]) {
      if (v != baseModel)
        models[v]->lockSerialization();
      else {
        for (Collective* col : models[v]->getCollectives())
          if (col->getLeader()->isDead())
            col->clearLeader();
        models[v]->setGame(nullptr);
      }
    }
  playerCollective->setVillainType(VillainType::MAIN);
  playerCollective->retire();
  set<Position> locationPosTmp =
      playerCollective->getConstructions().getBuiltPositions(FurnitureType::BOOK_SHELF);
  vector<Position> locationPos(locationPosTmp.begin(), locationPosTmp.end());
  if (locationPos.empty())
    locationPos = playerCollective->getTerritory().getAll();
  if (!locationPos.empty())
    playerCollective->getLevel()->addMarkedLocation(Rectangle::boundingBox(transform2(locationPos,
      [](const Position& p) { return p.getCoord();})));
  for (auto c : playerCollective->getCreatures())
    c->retire();
  playerControl = nullptr;
  playerCollective->setControl(PCollectiveControl(
        new VillageControl(playerCollective, CONSTRUCT(VillageBehaviour,
          c.minPopulation = 24;
          c.minTeamSize = 5;
          c.triggers = LIST(
              {AttackTriggerId::ROOM_BUILT, FurnitureType::THRONE},
              {AttackTriggerId::SELF_VICTIMS},
              AttackTriggerId::STOLEN_ITEMS,
          );
          c.attackBehaviour = AttackBehaviour(AttackBehaviourId::KILL_LEADER);
          c.ransom = make_pair(0.8, Random.get(500, 700));))));
  for (Collective* col : models[baseModel]->getCollectives())
    for (Creature* c : col->getCreatures())
      if (c->getPosition().getModel() != mainModel)
        transferCreature(c, mainModel);
  for (Vec2 v : models.getBounds())
    if (models[v] && v != baseModel)
      for (Collective* col : models[v]->getCollectives())
        for (Creature* c : col->getCreatures())
          if (c->getPosition().getModel() == mainModel)
            transferCreature(c, models[v].get());
  // So we don't have references to creatures in another model.
  for (Creature* c : mainModel->getAllCreatures())
    c->clearLastAttacker();
  TribeId::switchForSerialization(TribeId::getKeeper(), TribeId::getRetiredKeeper());
  UniqueEntity<Item>::offsetForSerialization(Random.getLL());
}

void Game::doneRetirement() {
  TribeId::clearSwitch();
  UniqueEntity<Item>::clearOffset();
}

optional<ExitInfo> Game::update(double timeDiff) {
  ScopeTimer timer("Game::update timer");
  currentTime += timeDiff;
  Model* currentModel = getCurrentModel();
  // Give every model a couple of turns so that things like shopkeepers can initialize.
  for (Vec2 v : models.getBounds())
    if (models[v] && !localTime.count(models[v].get())) {
      localTime[models[v].get()] = models[v]->getLocalTime() + 2;
      updateModel(models[v].get(), localTime[models[v].get()]);
    }
  localTime[currentModel] += timeDiff;
  while (!lastTick || currentTime > *lastTick + 1) {
    if (!lastTick)
      lastTick = currentTime;
    else
      *lastTick += 1;
    tick(*lastTick);
  }
  considerRealTimeRender();
  considerRetiredLoadedEvent(getModelCoords(currentModel));
  return updateModel(currentModel, localTime[currentModel]);
}

void Game::considerRealTimeRender() {
  auto absoluteTime = view->getTimeMilliAbsolute();
  if (!lastUpdate || absoluteTime - *lastUpdate > milliseconds{10}) {
    if (playerControl)
      playerControl->render(view);
    if (spectator)
      view->updateView(spectator.get(), false);
    lastUpdate = absoluteTime;
  }
}

optional<ExitInfo> Game::updateModel(Model* model, double totalTime) {
  do {
    if (spectator)
      while (1) {
        UserInput input = view->getAction();
        if (input.getId() == UserInputId::EXIT)
          return ExitInfo(ExitAndQuit());
        if (input.getId() == UserInputId::IDLE)
          break;
      }
    if (playerControl && !playerControl->isTurnBased()) {
      while (1) {
        UserInput input = view->getAction();
        if (input.getId() == UserInputId::IDLE)
          break;
        else
          lastUpdate = none;
        playerControl->processInput(view, input);
        if (exitInfo)
          return exitInfo;
      }
    }
    if (model->getLocalTime() >= totalTime)
      return none;
    model->update(totalTime);
    if (exitInfo)
      return exitInfo;
    if (wasTransfered) {
      wasTransfered = false;
      return none;
    }
  } while (1);
}

bool Game::isVillainActive(const Collective* col) {
  const Model* m = col->getModel();
  return m == getMainModel().get() || campaign->isInInfluence(getModelCoords(m));
}

void Game::tick(double time) {
  if (!turnEvents.empty() && time > *turnEvents.begin()) {
    int turn = *turnEvents.begin();
    if (turn == 0)
      uploadEvent("campaignStarted", campaign->getParameters());
    else
      uploadEvent("turn", {{"turn", toString(turn)}});
    turnEvents.erase(turn);
  }
  auto previous = sunlightInfo.getState();
  sunlightInfo.update(currentTime);
  if (previous != sunlightInfo.getState())
    for (Vec2 v : models.getBounds())
      if (Model* m = models[v].get())
        m->updateSunlightMovement();
  INFO << "Global time " << time;
  for (Collective* col : collectives) {
    if (isVillainActive(col))
      col->update(col->getModel() == getCurrentModel());
  }
}

void Game::exitAction() {
  enum Action { SAVE, RETIRE, OPTIONS, ABANDON};
#ifdef RELEASE
  bool canRetire = playerControl && gameWon() && !getPlayer() && campaign->getType() != CampaignType::SINGLE_KEEPER;
#else
  bool canRetire = playerControl && !getPlayer();
#endif
  vector<ListElem> elems { "Save the game",
    {"Retire", canRetire ? ListElem::NORMAL : ListElem::INACTIVE} , "Change options", "Abandon the game" };
  auto ind = view->chooseFromList("Would you like to:", elems);
  if (!ind)
    return;
  switch (Action(*ind)) {
    case RETIRE:
      if (view->yesOrNoPrompt("Retire your dungeon and share it online?")) {
        exitInfo = ExitInfo(GameSaveType::RETIRED_SITE);
        return;
      }
      break;
    case SAVE:
      if (!playerControl) {
        exitInfo = ExitInfo(GameSaveType::ADVENTURER);
        return;
      } else {
        exitInfo = ExitInfo(GameSaveType::KEEPER);
        return;
      }
    case ABANDON:
      if (view->yesOrNoPrompt("Do you want to abandon your game? This is permanent and the save file will be removed!")) {
        exitInfo = ExitInfo(ExitAndQuit());
        return;
      }
      break;
    case OPTIONS: options->handle(view, OptionSet::GENERAL); break;
  }
}

Position Game::getTransferPos(Model* from, Model* to) const {
  return to->getTopLevel()->getLandingSquare(StairKey::transferLanding(),
      getModelCoords(from) - getModelCoords(to));
}

void Game::transferCreature(Creature* c, Model* to) {
  Model* from = c->getLevel()->getModel();
  if (from != to)
    to->transferCreature(from->extractCreature(c), getModelCoords(from) - getModelCoords(to));
}

bool Game::canTransferCreature(Creature* c, Model* to) {
  return to->canTransferCreature(c, getModelCoords(c->getLevel()->getModel()) - getModelCoords(to));
}

int Game::getModelDistance(const Collective* c1, const Collective* c2) const {
  return getModelCoords(c1->getModel()).dist8(getModelCoords(c2->getModel()));
}
 
Vec2 Game::getModelCoords(const Model* m) const {
  for (Vec2 v : models.getBounds())
    if (models[v].get() == m)
      return v;
  FATAL << "Model not found";
  return Vec2();
}

void Game::presentWorldmap() {
  view->presentWorldmap(*campaign);
}

void Game::transferAction(vector<Creature*> creatures) {
  if (auto dest = view->chooseSite("Choose destination site:", *campaign,
        getModelCoords(creatures[0]->getLevel()->getModel()))) {
    Model* to = NOTNULL(models[*dest].get());
    vector<CreatureInfo> cant;
    for (Creature* c : copyOf(creatures))
      if (!canTransferCreature(c, to)) {
        cant.push_back(c);
        removeElement(creatures, c);
      }
    if (!cant.empty() && !view->creaturePrompt("These minions will be left behind due to sunlight.", cant))
      return;
    if (!creatures.empty()) {
      for (Creature* c : creatures)
        transferCreature(c, models[*dest].get());
      wasTransfered = true;
    }
  }
}

void Game::considerRetiredLoadedEvent(Vec2 coord) {
  if (!visited[coord]) {
    visited[coord] = true;
    if (auto retired = campaign->getSites()[coord].getRetired())
        uploadEvent("retiredLoaded", {
            {"retiredId", getGameId(retired->fileInfo)},
            {"playerName", getPlayerName()}});
  }
}

Statistics& Game::getStatistics() {
  return *statistics;
}

const Statistics& Game::getStatistics() const {
  return *statistics;
}

Tribe* Game::getTribe(TribeId id) const {
  return tribes.at(id).get();
}

Collective* Game::getPlayerCollective() const {
  return playerCollective;
}

MusicType Game::getCurrentMusic() const {
  return musicType;
}

void Game::setCurrentMusic(MusicType type, bool now) {
  if (type == MusicType::PEACEFUL && sunlightInfo.getState() == SunlightState::NIGHT)
    musicType = MusicType::NIGHT;
  else
    musicType = type;
  finishCurrentMusic = now;
}

bool Game::changeMusicNow() const {
  return true;
}

const SunlightInfo& Game::getSunlightInfo() const {
  return sunlightInfo;
}

string Game::getGameDisplayName() const {
  return gameDisplayName;
}

string Game::getGameIdentifier() const {
  return gameIdentifier;
}

View* Game::getView() const {
  return view;
}

void Game::conquered(const string& title, int numKills, int points) {
  string text= "You have conquered this land. You killed " + toString(numKills) +
      " enemies and scored " + toString(points) +
      " points. Thank you for playing KeeperRL alpha.\n \n";
  for (string stat : statistics->getText())
    text += stat + "\n";
  view->presentText("Victory", text);
  Highscores::Score score = CONSTRUCT(Highscores::Score,
        c.worldName = getWorldName();
        c.points = points;
        c.gameId = getGameIdentifier();
        c.playerName = title;
        c.gameResult = "achieved world domination";
        c.gameWon = true;
        c.turns = (int) getGlobalTime();
        c.campaignType = campaign->getType();
        c.playerRole = campaign->getPlayerRole();
  );
  highscores->add(score);
  highscores->present(view, score);
}

bool Game::isGameOver() const {
  return !!exitInfo;
}

void Game::gameOver(const Creature* creature, int numKills, const string& enemiesString, int points) {
  string text = "And so dies " + creature->getName().title();
  if (auto reason = creature->getDeathReason()) {
    text += ", " + *reason;
  }
  text += ". " + creature->getAttributes().getGender().he() + " killed " + toString(numKills)
      + " " + enemiesString + " and scored " + toString(points) + " points.\n \n";
  for (string stat : statistics->getText())
    text += stat + "\n";
  view->presentText("Game over", text);
  Highscores::Score score = CONSTRUCT(Highscores::Score,
        c.worldName = getWorldName();
        c.points = points;
        c.gameId = getGameIdentifier();
        c.playerName = *creature->getName().first();
        c.gameResult = creature->getDeathReason().get_value_or("");
        c.gameWon = false;
        c.turns = (int) getGlobalTime();
        c.campaignType = campaign->getType();
        c.playerRole = campaign->getPlayerRole();
  );
  highscores->add(score);
  highscores->present(view, score);
  exitInfo = ExitInfo(ExitAndQuit());
}

Options* Game::getOptions() {
  return options;
}

void Game::initialize(Options* o, Highscores* h, View* v, FileSharing* f) {
  options = o;
  highscores = h;
  view = v;
  fileSharing = f;
}

const string& Game::getWorldName() const {
  return campaign->getWorldName();
}

optional<Position> Game::getOtherPortal(Position position) const {
  if (auto index = findElement(portals, position)) {
    if (*index % 2 == 1)
      return portals[*index - 1];
    if (*index < portals.size() - 1)
      return portals[*index + 1];
  }
  return none;
}

void Game::registerPortal(Position pos) {
  if (!contains(portals, pos))
    portals.push_back(pos);
}

const vector<Collective*>& Game::getCollectives() const {
  return collectives;
}

void Game::setPlayer(Creature* c) {
  player = c;
}

Creature* Game::getPlayer() const {
  if (player && !player->isDead())
    return player;
  else
    return nullptr;
}

void Game::clearPlayer() {
  player = nullptr;
}

static SavedGameInfo::MinionInfo getMinionInfo(const Creature* c) {
  SavedGameInfo::MinionInfo ret;
  ret.level = (int)c->getAttributes().getVisibleExpLevel();
  ret.viewId = c->getViewObject().id();
  return ret;
}

string Game::getPlayerName() const {
  if (playerCollective)
    return *playerCollective->getLeader()->getName().first();
  else
    return *getPlayer()->getName().first();
}

SavedGameInfo Game::getSavedGameInfo() const {
  if (Collective* col = getPlayerCollective()) {
    vector<Creature*> creatures = col->getCreatures();
    CHECK(!creatures.empty());
    Creature* leader = col->getLeader();
    //  CHECK(!leader->isDead());
    sort(creatures.begin(), creatures.end(), [leader] (const Creature* c1, const Creature* c2) {
        return c1 == leader
        || (c2 != leader && c1->getAttributes().getExpLevel() > c2->getAttributes().getExpLevel());});
    CHECK(creatures[0] == leader);
    creatures.resize(min<int>(creatures.size(), 4));
    vector<SavedGameInfo::MinionInfo> minions;
    for (Creature* c : creatures)
      minions.push_back(getMinionInfo(c));
    return SavedGameInfo(minions, col->getDangerLevel(), getPlayerName(), getSaveProgressCount());
  } else
    return SavedGameInfo({getMinionInfo(getPlayer())}, 0, getPlayer()->getName().bare(), getSaveProgressCount());
}

void Game::uploadEvent(const string& name, const map<string, string>& m) {
  auto values = m;
  values["eventType"] = name;
  values["gameId"] = getGameIdentifier();
  fileSharing->uploadGameEvent(values);
}

void Game::handleMessageBoard(Position pos, Creature* c) {
  int boardId = pos.getHash();
  vector<ListElem> options;
  atomic<bool> cancelled(false);
  view->displaySplash(nullptr, "Fetching board contents...", SplashType::SMALL, [&] {
      cancelled = true;
      fileSharing->cancel();
      });
  optional<vector<FileSharing::BoardMessage>> messages;
  thread t([&] { messages = fileSharing->getBoardMessages(boardId); view->clearSplash(); });
  view->refreshView();
  t.join();
  if (!messages || cancelled) {
    view->presentText("", "Couldn't download board contents. Please check your internet connection.");
    return;
  }
  for (auto message : *messages) {
    options.emplace_back(message.author + " wrote:", ListElem::TITLE);
    options.emplace_back("\"" + message.text + "\"", ListElem::TEXT);
  }
  if (messages->empty())
    options.emplace_back("The board is empty.", ListElem::TITLE);
  options.emplace_back("", ListElem::TEXT);
  options.emplace_back("[Write something]");
  if (auto index = view->chooseFromList("", options))
    if (auto text = view->getText("Enter message", "", 80)) {
      if (text->size() >= 2)
        fileSharing->uploadBoardMessage(getGameIdentifier(), pos.getHash(), c->getName().title(), *text);
      else
        view->presentText("", "The message was too short.");
    }
}

bool Game::gameWon() const {
  for (Collective* col : getCollectives())
    if (!col->isConquered() && col->getVillainType() == VillainType::MAIN)
      return false;
  return true;
}

void Game::addEvent(const GameEvent& event) {
  for (Vec2 v : models.getBounds())
    if (models[v])
      models[v]->addEvent(event);
  switch (event.getId()) {
    case EventId::CONQUERED_ENEMY: {
        Collective* col = event.get<Collective*>();
        if (col->getVillainType()) {
          Vec2 coords = getModelCoords(col->getModel());
          if (!campaign->isDefeated(coords)) {
            if (auto retired = campaign->getSites()[coords].getRetired())
              uploadEvent("retiredConquered", {
                  {"retiredId", getGameId(retired->fileInfo)},
                  {"playerName", getPlayerName()}});
            campaign->setDefeated(coords);
          }
        }
        if (col->getVillainType() == VillainType::MAIN && gameWon()) {
          addEvent(EventId::WON_GAME);
        }
      }
      break;
    default:break;
  }
}

