#pragma once
#include <vector>

extern "C" {
#include <Kotonoha/Kotonoha.h>
#include <Kotonoha/utils/UserEvents.h>
#include <SDL3/SDL.h>

}

struct CanvasItem {
  Kotonoha_Scene_Status (*drawingPoint)(KOTONOHA_SCENE_CALL);
  SDL_Texture *target;

  Sint16 zIndex;
  SDL_FRect place;
  bool swapTexture;
  bool holdTexture;

  // KTN-0001-DIAG2: last status returned by this render layer.
  int debugLastStatus;

  void *userData;
};

namespace Kotonoha {
class Canvas {
private:
  std::vector<CanvasItem> drawingList = std::vector<CanvasItem>();
  SDL_Texture *dirtyTexture = nullptr;
  SDL_FRect dirtyPlace = {0, 0, 0, 0};

public:
  void RegisterCanva(Kotonoha_Scene_Status (*drawingPoint)(KOTONOHA_SCENE_CALL),
                     Sint16 zIndex, SDL_FRect place, void *userData);

  void UnregisterCanva(
      Kotonoha_Scene_Status (*drawingPoint)(KOTONOHA_SCENE_CALL));

  void UpdateCanva(Kotonoha_Scene_Status (*drawingPoint)(KOTONOHA_SCENE_CALL),
                   Sint16 zIndex, SDL_FRect place);

  SDL_AppResult RenderCanvas(SDL_Window *window, SDL_Renderer *render,
                             struct Kotonoha_eventStack *eventQueu);

  // Transfers ownership of the retained last-frame texture to another Canvas.
  // Used during Gameplay transitions so the window does not flash black while
  // the next scene is still preparing its first drawable frame.
  bool TransferLastFrameTo(Canvas *target);

  int CanvasCount();
  void Reset();

  ~Canvas();
};
} // namespace Kotonoha