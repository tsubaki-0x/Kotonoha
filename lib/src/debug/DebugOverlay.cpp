#include <Kotonoha/debug/DebugOverlay.hpp>

#include <Kotonoha/componentes/VNManager.hpp>
#include <Kotonoha/core/Engine.hpp>
#include <Kotonoha/script/TimelinePlayer.hpp>

#include "ImGuiTheme.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace Kotonoha {
namespace {
std::string formatSeconds(double seconds) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.2fs", seconds);
  return buffer;
}

bool isInputEvent(Uint32 type) {
  switch (type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_TEXT_INPUT:
  case SDL_EVENT_MOUSE_MOTION:
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
  case SDL_EVENT_MOUSE_WHEEL:
    return true;
  default:
    return false;
  }
}
} // namespace

DebugOverlay::DebugOverlay(Engine &value) : engine(value) {
  IMGUI_CHECKVERSION();
  context = ImGui::CreateContext();
  if (!context)
    throw std::runtime_error("Cannot create ImGui debug context");
  ImGui::SetCurrentContext(context);
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGuiTheme::loadFonts(engine.GetSettings().fontPath);
  ImGuiTheme::apply(engine.GetSettings().debugUIScale);
  if (!ImGui_ImplSDL3_InitForSDLRenderer(&engine.GetWindow(), &engine.GetRenderer())) {
    ImGui::DestroyContext(context);
    context = nullptr;
    throw std::runtime_error("Cannot initialize ImGui SDL3 debug backend");
  }
  if (!ImGui_ImplSDLRenderer3_Init(&engine.GetRenderer())) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(context);
    context = nullptr;
    throw std::runtime_error("Cannot initialize ImGui renderer debug backend");
  }
  lastCounter = SDL_GetPerformanceCounter();
}

DebugOverlay::~DebugOverlay() {
  if (!context)
    return;
  ImGui::SetCurrentContext(context);
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext(context);
  context = nullptr;
}

void DebugOverlay::update() {
  const Uint64 now = SDL_GetPerformanceCounter();
  const Uint64 frequency = SDL_GetPerformanceFrequency();
  if (lastCounter && frequency && now > lastCounter) {
    const double elapsed = double(now - lastCounter) / double(frequency);
    frameMs = elapsed * 1000.0;
    const double instant = elapsed > 0 ? 1.0 / elapsed : 0.0;
    fps = fps > 0 ? fps * 0.90 + instant * 0.10 : instant;
  }
  lastCounter = now;
}

bool DebugOverlay::handleEvent(const SDL_Event &event) {
  if (!context)
    return false;
  ImGui::SetCurrentContext(context);
  if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
    if (event.key.key == SDLK_F10) {
      visible = !visible;
      return true;
    }
    if (event.key.key == SDLK_F9) {
      settings.showFPS = !settings.showFPS;
      return true;
    }
    if (event.key.key == SDLK_F8) {
      settings.showNodeClock = !settings.showNodeClock;
      return true;
    }
  }
  if (!visible)
    return false;
  ImGui_ImplSDL3_ProcessEvent(&event);
  return isInputEvent(event.type);
}

std::string DebugOverlay::nodeClockText() const {
  std::string node = engine.GetRuntime().debugNode();
  std::optional<double> seconds = engine.GetRuntime().debugNodeSeconds();
  if (node.empty()) {
    auto &manager = GetVNManagerRuntime(const_cast<Engine &>(engine));
    if (manager.getCurrentNodeId() >= 0)
      node = std::to_string(manager.getCurrentNodeId());
    if (!seconds && engine.GetTimeline().isPlaying())
      seconds = engine.GetTimeline().seconds();
  }
  if (node.empty())
    node = "-";
  std::string result = "Node: " + node;
  if (seconds)
    result += " | Clock: " + formatSeconds(*seconds);
  return result;
}

std::string DebugOverlay::performanceText() const {
  char buffer[96];
  const double shownFps = fps > 0 ? fps : 0.0;
  const double shownMs = frameMs > 0 ? frameMs : (shownFps > 0 ? 1000.0 / shownFps : 0.0);
  std::snprintf(buffer, sizeof buffer, "FPS: %.1f | %.2f ms", shownFps, shownMs);
  return buffer;
}

void DebugOverlay::drawHUD() {
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  if (!viewport)
    return;
  const float pad = 10.0f * engine.GetSettings().debugUIScale;
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoInputs;
  if (settings.showFPS) {
    ImGui::SetNextWindowPos({viewport->WorkPos.x + pad, viewport->WorkPos.y + pad},
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##kotonoha-fps", nullptr, flags))
      ImGui::TextUnformatted(performanceText().c_str());
    ImGui::End();
  }
  if (settings.showNodeClock) {
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - pad,
                             viewport->WorkPos.y + pad},
                            ImGuiCond_Always, {1.0f, 0.0f});
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##kotonoha-node", nullptr, flags))
      ImGui::TextUnformatted(nodeClockText().c_str());
    ImGui::End();
  }
}

void DebugOverlay::drawInspector() {
  if (!visible)
    return;
  bool open = visible;
  if (ImGui::Begin("Kotonoha Debug [F10]", &open,
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::Checkbox("Show FPS [F9]", &settings.showFPS);
    ImGui::Checkbox("Show node / clock [F8]", &settings.showNodeClock);
    ImGui::Separator();
    ImGui::TextUnformatted(performanceText().c_str());
    ImGui::TextUnformatted(nodeClockText().c_str());

    if (ImGui::CollapsingHeader("Audio channels")) {
      for (const auto &channel : engine.GetAudio().getChannels()) {
        ImGui::PushID(channel->getName().c_str());
        ImGui::TextUnformatted(channel->getName().c_str());
        bool muted = channel->isMuted();
        if (ImGui::Checkbox("Mute", &muted))
          channel->setMuted(muted);
        ImGui::SameLine();
        float volume = channel->getVolume();
        ImGui::SetNextItemWidth(180.0f * engine.GetSettings().debugUIScale);
        if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f"))
          channel->setVolume(volume);
        ImGui::TextDisabled("%zu active stream(s)", channel->activeStreams());
        ImGui::PopID();
      }
    }

    if (ImGui::CollapsingHeader("Nodes")) {
      for (const auto &node : engine.debugNodes()) {
        ImGui::PushID(node.id.c_str());
        const std::string label = node.label.empty() ? node.id : node.label;
        if (ImGui::Button(label.c_str()))
          engine.debugJumpToNode(node.id);
        if (!node.assets.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("%zu asset(s)", node.assets.size());
        }
        ImGui::PopID();
      }
    }
  }
  ImGui::End();
  visible = open;
}

void DebugOverlay::beginFrame(RendererRef renderer) {
  ImGui::SetCurrentContext(context);
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  auto &io = ImGui::GetIO();
  const auto screen = engine.screenSize();
  io.DisplaySize = {screen.x, screen.y};
  int pixelWidth = 0, pixelHeight = 0;
  if (SDL_Texture *target = SDL_GetRenderTarget(&renderer)) {
    float width = 0, height = 0;
    if (!SDL_GetTextureSize(target, &width, &height))
      throw std::runtime_error(SDL_GetError());
    pixelWidth = static_cast<int>(std::lround(width));
    pixelHeight = static_cast<int>(std::lround(height));
  } else if (!SDL_GetRenderOutputSize(&renderer, &pixelWidth, &pixelHeight)) {
    throw std::runtime_error(SDL_GetError());
  }
  if (screen.x > 0 && screen.y > 0 && pixelWidth > 0 && pixelHeight > 0)
    io.DisplayFramebufferScale = {pixelWidth / screen.x, pixelHeight / screen.y};
  else
    io.DisplayFramebufferScale = {1, 1};
  ImGuiTheme::apply(engine.GetSettings().debugUIScale);
  ImGui::NewFrame();
  frameOpen = true;
}

void DebugOverlay::renderHUD(RendererRef renderer) {
  if (!context || frameOpen)
    return;
  beginFrame(renderer);
  drawHUD();
}

void DebugOverlay::renderMenu(RendererRef renderer) {
  if (!context)
    return;
  if (!frameOpen)
    beginFrame(renderer);
  drawInspector();
  ImGui::Render();

  auto &io = ImGui::GetIO();
  if (!SDL_SetRenderLogicalPresentation(&renderer, 0, 0,
                                        SDL_LOGICAL_PRESENTATION_DISABLED) ||
      !SDL_SetRenderScale(&renderer, io.DisplayFramebufferScale.x,
                          io.DisplayFramebufferScale.y))
    throw std::runtime_error(SDL_GetError());
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), &renderer);
  frameOpen = false;
}

} // namespace Kotonoha
