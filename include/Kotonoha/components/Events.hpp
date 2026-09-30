#pragma once
#include <Kotonoha/components/Audio.hpp>
#include <Kotonoha/components/Image.hpp>
#include <Kotonoha/components/Prompt.hpp>
#include <Kotonoha/components/Video.hpp>
extern "C" {
#include <Kotonoha/parsers/Ors.h>
#include <Kotonoha/renders/TextRender.h>
}

namespace Kotonoha {
class Event {
private:
  Kotonoha_orsData eventsFromScript;
  static int EventManager(void *data);
  SDL_Mutex *eventMutex = nullptr;

  // Non-owning pointer. Lifetime is managed by Kotonoha::processPoolTasks.
  // It lets Gameplay synchronously prime a scene before its first render.
  void **eventManagerParams = nullptr;

  std::vector<Video*> videoToDelete;
  std::vector<Image*> imageToDelete;
  std::vector<Audio*> audioToDeleta;

public:
  Uint64 lastTime = 0;
  Event(const char *orsPath, void *gameplay, struct Kotonoha_Game *gameCtx);
  void Reset(void* gameplay);
  bool ProcessNow(struct Kotonoha_Game *gameCtx);
  bool CheckEnd(void *gameplay);
  ~Event();
};
} // namespace Kotonoha
