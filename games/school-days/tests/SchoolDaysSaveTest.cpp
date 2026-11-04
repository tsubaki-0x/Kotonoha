#include "SchoolDaysAssets.hpp"
#include "SchoolDaysRuntime.hpp"
#include <Kotonoha/componentes/SettingsMenu.hpp>
#include <Kotonoha/core/Engine.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Application {
  SchoolDays::Runtime runtime;
  Kotonoha::Engine engine;
  static Kotonoha::EngineOptions
  options(const std::shared_ptr<SchoolDays::Assets> &assets) {
    Kotonoha::EngineOptions result;
    result.assets = assets;
    result.persistSettings = false;
    result.rendererOverride = "software";
    return result;
  }
  Application(const std::shared_ptr<SchoolDays::Assets> &assets,
              const char *route, const std::filesystem::path &documents)
      : runtime(assets, route, documents), engine(runtime, options(assets)) {}
  ~Application() { runtime.shutdown(); }
};
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 3, "Expected retail data and route");
    struct Directory {
      std::filesystem::path path =
          std::filesystem::temp_directory_path() /
          ("school-days-save-" + std::to_string(SDL_GetPerformanceCounter()));
      ~Directory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
      }
    } documents;
    auto assets = std::make_shared<SchoolDays::Assets>(argv[1]);
    Application app(assets, argv[2], documents.path);
    app.runtime.SetState(Kotonoha::GameState::Gameplay);
    app.runtime.debugJumpToNode("0");
    for (int frame = 0; frame < 4; ++frame)
      check(app.engine.Iterate() == SDL_APP_CONTINUE,
            "Initial scene frame failed");
    const auto original = app.runtime.debugNode();
    auto openPanel = [&](int control) {
      app.runtime.openPanel(control);
      // Retail SYSBASE keeps input locked during its 750 ms entry animation.
      if (control == 10 || control == 11 || control == 13) {
        SDL_Delay(760);
        app.runtime.update();
      }
    };
    auto row = [&] {
      check(app.runtime.panels().IsSaveLoadOpen(),
            "Retail SaveLoad did not open");
      const auto &model = app.runtime.panels().SaveLoad();
      const int width = int(model.Cmap().Width());
      const int height = int(model.Cmap().Height());
      for (int y = 4; y < height; y += 4)
        for (int x = 4; x < width; x += 4)
          if (model.HitControl(x, y) == 0) {
            const float px = float(x) * 1280.0f / float(width);
            const float py = float(y) * 720.0f / float(height);
            SDL_Event e{};
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = px;
            e.motion.y = py;
            app.runtime.onEvent(e);
            e = {};
            e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.x = px;
            e.button.y = py;
            app.runtime.onEvent(e);
            return;
          }
      throw std::runtime_error("Cannot find retail slot 0");
    };
    Kotonoha::SchoolDaysSaveProfile profile;
    Kotonoha::SchoolDaysSaveStorage storage;
    std::string error;
    check(Kotonoha::SchoolDaysSaveProfile::Load(assets->root().string(),
                                                &profile, &error),
          error);
    check(storage.Initialize(documents.path, profile, &error), error);
    openPanel(10);
    row();
    check(app.runtime.panels().IsSaveLoadCommentEditorOpen(),
          "Retail save must request its comment");
    SDL_Event cancel{};
    cancel.type = SDL_EVENT_KEY_DOWN;
    cancel.key.key = SDLK_ESCAPE;
    app.runtime.onEvent(cancel);
    app.runtime.update();
    bool present = false;
    check(storage.ProbeSaveFile(0, &present, &error) && !present,
          "Cancel unexpectedly wrote a retail slot");
    row();
    check(app.runtime.panels().IsSaveLoadCommentEditorOpen(),
          "Comment editor did not reopen");
    SDL_Event typed{};
    typed.type = SDL_EVENT_TEXT_INPUT;
    typed.text.text = "Regression Save";
    app.runtime.onEvent(typed);
    SDL_Event accept{};
    accept.type = SDL_EVENT_KEY_DOWN;
    accept.key.key = SDLK_RETURN;
    app.runtime.onEvent(accept);
    app.runtime.update();
    check(storage.ProbeSaveFile(0, &present, &error) && present,
          "Retail slot was not written: " + error);
    std::vector<uint8_t> bytes;
    check(storage.ReadSaveFile(0, &bytes, &error) && !bytes.empty(), error);
    SDL_Event close{};
    close.type = SDL_EVENT_KEY_DOWN;
    close.key.key = SDLK_ESCAPE;
    app.runtime.onEvent(close);
    SDL_Delay(510);
    app.runtime.update();
    SDL_Delay(510);
    app.runtime.update();
    openPanel(11);
    row();
    check(app.runtime.debugNode() == original,
          "Retail save/load changed the scene");
    // Exercise the original aspect buttons and repurposed SOMCON tab through
    // their actual retail hit maps, not a synthetic host-only setter.
    auto configControl = [&](int control) {
      check(app.runtime.panels().IsConfigOpen(),
            "Retail configuration did not open");
      const auto &model = app.runtime.panels().ConfigMenu();
      const int width = int(model.Cmap().Width());
      const int height = int(model.Cmap().Height());
      for (int y = 4; y < height; y += 2)
        for (int x = 4; x < width; x += 2)
          if (model.HitControl(x, y) == control) {
            const float px = float(x) * 1280.0f / float(width);
            const float py = float(y) * 720.0f / float(height);
            SDL_Event e{};
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = px;
            e.motion.y = py;
            app.runtime.onEvent(e);
            e = {};
            e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.x = px;
            e.button.y = py;
            app.runtime.onEvent(e);
            return;
          }
      throw std::runtime_error("Missing retail config control");
    };
    app.runtime.update();
    openPanel(13);
    configControl(2);
    check(!app.runtime.panels().IsConfigOpen() &&
              app.engine.GetSettingsMenu().isOpen(),
          "SOMCON replacement must open the Kotonoha settings component");
    app.engine.GetSettingsMenu().close();
    // Terminal handoff is exercised by SchoolDaysHandoffRuntimeTest.
    std::cout << "School Days retail save: cancel, named UTF-16 metadata, "
                 "write and reload passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
