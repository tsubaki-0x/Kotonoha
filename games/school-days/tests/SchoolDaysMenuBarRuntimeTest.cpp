#include "SchoolDaysAssets.hpp"
#include "SchoolDaysRuntime.hpp"

#include <Kotonoha/core/Engine.hpp>
#include <Kotonoha/routing/SchoolDaysConfigStorage.hpp>
#include <Kotonoha/routing/SchoolDaysMenuBarPresenter.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void Check(bool value, const std::string &message) {
  if (!value) throw std::runtime_error(message);
}

bool Near(double a, double b) { return std::abs(a - b) < 0.000001; }

struct TemporaryDirectory {
  fs::path path = fs::temp_directory_path() /
                  ("school-days-menubar-runtime-" +
                   std::to_string(SDL_GetPerformanceCounter()));
  TemporaryDirectory() {
    Check(fs::create_directory(path), "cannot create temporary directory");
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }
};

Kotonoha::EngineOptions Options(
    const std::shared_ptr<SchoolDays::Assets> &assets) {
  Kotonoha::EngineOptions options;
  options.assets = assets;
  options.persistSettings = false;
  options.rendererOverride = "software";
  return options;
}

struct Application {
  SchoolDays::Runtime runtime;
  Kotonoha::Engine engine;

  Application(const std::shared_ptr<SchoolDays::Assets> &assets,
              const char *route, const fs::path &documents)
      : runtime(assets, route, documents), engine(runtime, Options(assets)) {
    runtime.SetState(Kotonoha::GameState::Gameplay);
  }
  ~Application() { runtime.shutdown(); }
};

} // namespace

namespace SchoolDays {

class RuntimeMenuBarTestAccess {
public:
  using Presenter = Kotonoha::SchoolDaysMenuBarPresenter;

  static void Apply(Runtime &runtime, Presenter::ActionKind kind,
                    int value = -1) {
    Presenter::Action action;
    action.kind = kind;
    action.value = value;
    runtime.activateMenuAction(action);
  }

  static double Speed(const Runtime &runtime) { return runtime.speed; }
  static int SpeedIndex(const Runtime &runtime) { return runtime.speedIndex; }
  static bool Paused(const Runtime &runtime) { return runtime.paused; }
  static bool ConfigDirty(const Runtime &runtime) {
    return runtime.configStorage.IsDirty();
  }
  static bool SpeedControlsEnabled(const Runtime &runtime) {
    return runtime.menuBarProjection().speedControlsEnabled;
  }
  static bool TransportLocked(const Runtime &runtime) {
    return runtime.menuBarProjection().transportLocked;
  }
  static bool ActionInhibit(const Runtime &runtime) {
    return runtime.menuBarProjection().actionInhibit;
  }
  static bool FullBarSuppressed(const Runtime &runtime) {
    return runtime.menuBarProjection().fullBarSuppressed;
  }
  static bool ReplayIndicator(const Runtime &runtime) {
    return runtime.menuBarProjection().replayIndicator;
  }
  static void StartOrdinaryReplay(Runtime &runtime) {
    runtime.startReplayPlayback(1);
  }
};

} // namespace SchoolDays

int main(int argc, char **argv) {
  try {
    Check(argc == 3,
          "usage: SchoolDaysMenuBarRuntimeTest <route> <fixture-root>");
    TemporaryDirectory temporary;
    const auto documents = temporary.path / "documents";
    fs::create_directories(documents);
    auto assets = std::make_shared<SchoolDays::Assets>(argv[2]);

    using Access = SchoolDays::RuntimeMenuBarTestAccess;
    using Kind = Kotonoha::SchoolDaysMenuBarPresenter::ActionKind;
    {
      Application app(assets, argv[1], documents);
      Check(!app.runtime.retailConfig().autoMode,
            "default AutoMode is unexpectedly enabled");

      Access::Apply(app.runtime, Kind::ToggleAutoMode);
      Check(app.runtime.retailConfig().autoMode,
            "MenuBar Auto did not update retail config state");
      Check(Access::ConfigDirty(app.runtime),
            "MenuBar Auto did not cache Config.DAT state");

      Access::Apply(app.runtime, Kind::TogglePause);
      Check(Access::Paused(app.runtime), "MenuBar Pause did not pause runtime");
      Access::Apply(app.runtime, Kind::TogglePause);
      Check(!Access::Paused(app.runtime), "second MenuBar Pause did not resume runtime");

      constexpr double expected[] = {1.0, 2.0, 4.0, 12.0, 24.0};
      for (int index = 0; index < 5; ++index) {
        Access::Apply(app.runtime, Kind::SetPlaybackSpeed, index);
        Check(Access::SpeedIndex(app.runtime) == index &&
                  Near(Access::Speed(app.runtime), expected[index]),
              "MenuBar playback-speed mapping drifted");
      }

      const double before = Access::Speed(app.runtime);
      Access::Apply(app.runtime, Kind::SetReplayIndicatorOpacity, 10);
      Check(Near(Access::Speed(app.runtime), before),
            "ordinary Replay-opacity action contaminated runtime speed");

      bool rejected = false;
      try {
        Access::Apply(app.runtime, Kind::SetPlaybackSpeed, 5);
      } catch (const std::runtime_error &) {
        rejected = true;
      }
      Check(rejected, "invalid MenuBar speed index did not fail closed");

      Check(Access::SpeedControlsEnabled(app.runtime) &&
                !Access::TransportLocked(app.runtime) &&
                !Access::ActionInhibit(app.runtime) &&
                !Access::FullBarSuppressed(app.runtime) &&
                !Access::ReplayIndicator(app.runtime),
            "normal Gameplay MenuBar projection drifted");

      Access::StartOrdinaryReplay(app.runtime);
      Check(Access::SpeedControlsEnabled(app.runtime) &&
                !Access::TransportLocked(app.runtime) &&
                Access::ActionInhibit(app.runtime) &&
                !Access::FullBarSuppressed(app.runtime) &&
                !Access::ReplayIndicator(app.runtime),
            "ordinary HScene Replay MenuBar action-inhibit projection drifted");
      Access::Apply(app.runtime, Kind::OpenSave);
      Check(!app.runtime.panels().IsSaveLoadOpen(),
            "ordinary HScene Replay exposed nested SAVE");
    }

    Kotonoha::SchoolDaysConfigStorage storage;
    std::string error;
    Kotonoha::SchoolDaysConfigStorage::LoadResult loaded;
    Check(storage.Initialize(documents, &error), error);
    Check(storage.Load(&loaded, &error), error);
    Check(loaded.state.autoMode,
          "MenuBar Auto cache was not flushed to Config.DAT on shutdown");

    std::cout << "SchoolDaysMenuBarRuntimeTest PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "SchoolDaysMenuBarRuntimeTest: " << error.what() << '\n';
    return 1;
  }
}
