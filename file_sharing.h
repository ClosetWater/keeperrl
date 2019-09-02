#pragma once

#include "util.h"
#include "highscores.h"
#include "save_file_info.h"
#include "saved_game_info.h"

class ProgressMeter;
class ModInfo;

class FileSharing {
  public:
  FileSharing(const string& uploadUrl, const string& modVersion, int saveVersion, Options&, string installId);

  optional<string> uploadSite(const FilePath& path, const string& title, const SavedGameInfo&, ProgressMeter&,
      optional<string>& url);
  optional<string> downloadSite(const string& filename, const DirectoryPath& targetDir, ProgressMeter&);
  struct SiteInfo {
    SavedGameInfo gameInfo;
    SaveFileInfo fileInfo;
    int totalGames;
    int wonGames;
    int version;
  };
  optional<vector<SiteInfo>> listSites();

  typedef map<string, string> GameEvent;
  bool uploadGameEvent(const GameEvent&, bool requireGameEventsPermission = true);
  void uploadHighscores(const FilePath&);
  optional<string> uploadBugReport(const string& text, optional<FilePath> savefile, optional<FilePath> screenshot,
      ProgressMeter&);

  struct BoardMessage {
    string text;
    string author;
  };
  optional<vector<BoardMessage>> getBoardMessages(int boardId);
  bool uploadBoardMessage(const string& gameId, int hash, const string& author, const string& text);

  optional<vector<ModInfo>> getOnlineMods();
  optional<string> downloadMod(const string& name, SteamId, const DirectoryPath& modsDir, ProgressMeter&);
  optional<string> uploadMod(ModInfo& modInfo, const DirectoryPath& modsDir, ProgressMeter&);

  string downloadHighscores(int version);

  void cancel();
  bool consumeCancelled();
  ~FileSharing();

  private:
  string uploadUrl;
  string modVersion;
  int saveVersion;
  Options& options;
  SyncQueue<function<void()>> uploadQueue;
  AsyncLoop uploadLoop;
  void uploadingLoop();
  void uploadGameEventImpl(const GameEvent&, int tries);
  optional<string> downloadContent(const string& url);
  optional<string> uploadSiteToSteam(const FilePath&, const string& title, const SavedGameInfo&,
      ProgressMeter&, optional<string>& url);
  optional<vector<ModInfo>> getSteamMods();
  optional<vector<SiteInfo>> getSteamSites();
  optional<string> downloadSteamMod(SteamId, const string& name, const DirectoryPath& modsDir, ProgressMeter&);
  optional<string> downloadSteamSite(SteamId, const DirectoryPath& targetDir, ProgressMeter&);
  optional<string> download(const string& filename, const string& remoteDir, const DirectoryPath& dir, ProgressMeter&);
  string installId;
  atomic<bool> wasCancelled;
};

