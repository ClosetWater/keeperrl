#include "stdafx.h"
#include "dirent.h"
#include "sound_library.h"
#include "sound.h"
#include "options.h"

using sf::Music;

SoundLibrary::SoundLibrary(Options* options, const string& path) {
  on = options->getBoolValue(OptionId::SOUND);
  options->addTrigger(OptionId::SOUND, [this](bool turnOn) { on = turnOn; });
  for (SoundId id : ENUM_ALL(SoundId))
    addSounds(id, path + "/" + toLower(EnumInfo<SoundId>::getString(id)));
}

void SoundLibrary::addSounds(SoundId id, const string& path) {
  DIR* dir = opendir(path.c_str());
  CHECK(dir) << path;
  vector<string> files;
  while (dirent* ent = readdir(dir)) {
    string name(ent->d_name);
    if (endsWith(name, ".ogg")) {
      sounds[id].emplace_back(new Music());
      CHECK(sounds[id].back()->openFromFile(path + "/" + name));
    }
  }
}

void SoundLibrary::playSound(const Sound& s) {
  if (!on)
    return;
  if (int numSounds = sounds[s.getId()].size()) {
    int ind = Random.get(numSounds);
    sounds[s.getId()][ind]->setPitch(s.getPitch());
    sounds[s.getId()][ind]->play();
  }
}
