#pragma once

#include <Kotonoha/core/SDL.hpp>

#include <string>

struct ImGuiContext;

namespace Kotonoha {
class Engine;

struct DebugOverlaySettings {
  bool showFPS = true;
  bool showNodeClock = true;
};

class DebugOverlay {
  Engine &engine;
  DebugOverlaySettings settings;
  ImGuiContext *context = nullptr;
  bool visible = false;
  bool frameOpen = false;
  Uint64 lastCounter = 0;
  double fps = 0.0;
  double frameMs = 0.0;

  void beginFrame(RendererRef renderer);
  void drawHUD();
  void drawInspector();

public:
  explicit DebugOverlay(Engine &engine);
  ~DebugOverlay();
  DebugOverlay(const DebugOverlay &) = delete;
  DebugOverlay &operator=(const DebugOverlay &) = delete;

  void update();
  bool handleEvent(const SDL_Event &event);
  void renderHUD(RendererRef renderer);
  void renderMenu(RendererRef renderer);

  bool isVisible() const { return visible; }
  void setVisible(bool value) { visible = value; }
  DebugOverlaySettings &getSettings() { return settings; }
  const DebugOverlaySettings &getSettings() const { return settings; }

  std::string nodeClockText() const;
  std::string performanceText() const;
};

} // namespace Kotonoha

\n