#pragma once

#include <Kotonoha/media/upscaling/Upscaler.hpp>
#include <string>

namespace Kotonoha {

enum class WindowMode { Windowed, Borderless, Exclusive };

struct EngineSettings {
  int width = 1280, height = 720;
  int display = 0;
  WindowMode mode = WindowMode::Windowed;
  int fullscreenWidth = 0, fullscreenHeight = 0;
  float refreshRate = 0;
  bool resizable = true;
  bool vsync = false;
  float masterVolume = 1;
  bool masterMuted = false;
  float debugUIScale = 1;
  // Fraction of the fitted physical Novel viewport used for the intermediate
  // render target. Screen-space UI is always rendered at native output size.
  float renderScale = 1;
  std::string renderer;
  UpscaleSettings upscaling;
  SDL_LogPriority logPriority = SDL_LOG_PRIORITY_INFO;
  std::string translationPath;
  std::string fontPath;
  void validate() const;
  static EngineSettings load(const std::string &path);
  static EngineSettings load(const std::string &path,
                             const EngineSettings &defaults);
  // <pref path>/Kotonoha/<identify>/settings.json, as used by the Engine.
  static std::string defaultPath(const std::string &identify);
  void save(const std::string &path) const;
};

} // namespace Kotonoha
