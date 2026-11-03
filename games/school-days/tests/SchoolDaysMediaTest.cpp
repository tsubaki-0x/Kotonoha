#include "SchoolDaysAssets.hpp"
#include "SchoolDaysRuntime.hpp"
#include <Kotonoha/core/Engine.hpp>
#include <Kotonoha/debug/DebugOverlay.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
Kotonoha::EngineOptions Options(const std::shared_ptr<SchoolDays::Assets> &assets) {
  Kotonoha::EngineOptions options;
  options.assets = assets;
  options.persistSettings = false;
  options.rendererOverride = "software";
  return options;
}
struct Application {
  SchoolDays::Runtime runtime;
  Kotonoha::Engine engine;
  Application(const std::shared_ptr<SchoolDays::Assets> &assets, const char *route, const char *documents)
      : runtime(assets, route, documents), engine(runtime, Options(assets)) {}
  ~Application() { runtime.shutdown(); }
};
}

namespace SchoolDays {

class RuntimeMediaParityTestAccess {
public:
  static void Run(Runtime &runtime, Kotonoha::RendererRef renderer) {
    runtime.playing.clear();
    runtime.movieStates.clear();
    runtime.moveSom.Reset();

    // Commercial EventRuntime prepares media before its semantic START. The
    // paused decoder must be adopted, not reopened, when dispatch reaches it.
    char preparedPath[] = "Voice/test";
    Kotonoha_orsTypePlaySe preparedData{4, preparedPath};
    Kotonoha_orsEvent prepared{};
    prepared.command = PLAY_SE;
    prepared.startTick = 120;
    prepared.endTick = 168;
    prepared.data.play_se = &preparedData;
    Require(runtime.prepareMediaEvent(prepared) && prepared.eventPrepared &&
                runtime.preparedMedia.count(&prepared) == 1 && runtime.playing.empty(),
            "future SE was not prepared without semantic playback");
    auto *preparedPlayer = runtime.preparedMedia.at(&prepared).player.get();
    runtime.dispatch(prepared);
    Require(runtime.preparedMedia.count(&prepared) == 0 && runtime.playing.size() == 1 &&
                runtime.playing.front().player.get() == preparedPlayer,
            "SE START reopened media instead of adopting the prepared decoder");
    runtime.playing.clear();

    char preparedBgPath[] = "Bg/test";
    Kotonoha_orsTypeCreateBg preparedBgData{nullptr, preparedBgPath};
    Kotonoha_orsEvent preparedBg{};
    preparedBg.command = CREATE_BG;
    preparedBg.startTick = 96;
    preparedBg.endTick = 192;
    preparedBg.data.create_bg = &preparedBgData;
    Require(runtime.prepareMediaEvent(preparedBg) && preparedBg.eventPrepared &&
                runtime.preparedBackgrounds.count(&preparedBg) == 1 &&
                runtime.preparedBackgroundAbc.count(&preparedBg) == 1,
            "future CREATE_BG did not predecode/register its base+ABC resources");
    const std::string preparedBackground = runtime.preparedBackgrounds.at(&preparedBg);
    runtime.dispatch(preparedBg);
    Require(runtime.background == preparedBackground &&
                runtime.preparedBackgrounds.count(&preparedBg) == 0 &&
                runtime.abcGroups.size() == 1 && runtime.abcGroups.front().key == "mak",
            "CREATE_BG START did not adopt its prepared base+ABC resources");

    char audioPath[] = "Voice/test";
    Kotonoha_orsTypePlaySe seAData{2, audioPath};
    Kotonoha_orsTypePlaySe seBData{2, audioPath};
    Kotonoha_orsTypePlaySe seCData{3, audioPath};
    Kotonoha_orsTypePlaySe seInvalidData{9, audioPath};
    Kotonoha_orsEvent seA{}, seB{}, seC{}, seInvalid{};
    for (auto *event : {&seA, &seB, &seC, &seInvalid}) {
      event->command = PLAY_SE;
      event->startTick = 0;
      event->endTick = 48;
    }
    seA.data.play_se = &seAData;
    seB.data.play_se = &seBData;
    seC.data.play_se = &seCData;
    seInvalid.data.play_se = &seInvalidData;
    runtime.dispatch(seA);
    runtime.dispatch(seC);
    Require(runtime.playing.size() == 2, "different SE slots did not coexist in Runtime");
    runtime.dispatch(seB);
    Require(runtime.playing.size() == 2 &&
                runtime.playing[0].event == &seC && runtime.playing[1].event == &seB,
            "same SE slot did not transfer ownership to latest event");
    runtime.dispatch(seInvalid);
    Require(runtime.playing.size() == 2,
            "out-of-range SE slot created Runtime media");

    runtime.playing.clear();
    runtime.config.menVoice = false;
    runtime.tick = 0;
    runtime.background = runtime.assets->resolve("Bg/test", true);
    runtime.discoverAbcForBackground();
    Require(runtime.abcGroups.size() == 1 && runtime.abcGroups.front().key == "mak" &&
                runtime.abcGroups.front().Complete(),
            "Runtime did not discover the complete ABC sibling group");
    char animationKey[] = "mak";
    Kotonoha_orsTypePlayVoice blockedVoiceData{audioPath, 1, animationKey};
    Kotonoha_orsTypePlayVoice allowedVoiceData{audioPath, 0, animationKey};
    Kotonoha_orsEvent blockedVoice{}, allowedVoice{};
    blockedVoice.command = allowedVoice.command = PLAY_VOICE;
    blockedVoice.startTick = allowedVoice.startTick = 0;
    blockedVoice.endTick = allowedVoice.endTick = 48;
    blockedVoice.data.play_voice = &blockedVoiceData;
    allowedVoice.data.play_voice = &allowedVoiceData;
    runtime.dispatch(blockedVoice);
    Require(runtime.playing.empty(), "MenVoice=off did not block numbered voice");
    runtime.dispatch(allowedVoice);
    Require(runtime.playing.size() == 1 && runtime.playing.front().event == &allowedVoice,
            "numeric-zero voice incorrectly depended on MenVoice");
    Require(runtime.abcVoices.size() == 1 && runtime.abcVoices.front().groupIndex == 0 &&
                runtime.abcActivityCache.count(&allowedVoice) == 1,
            "PLAY_VOICE did not analyze/bind its ABC mouth group");
    runtime.tick = 3;
    runtime.updateAbcVoices();
    Require(runtime.abcVoices.front().cursor.initialized,
            "ABC scheduler did not advance with the scene clock");
    runtime.renderAbcOverlays(renderer, {0, 0, 1280, 720});
    Require(!runtime.abcTextures.empty(),
            "ABC compositor did not decode the selected mouth frame");

    runtime.playing.clear();
    Kotonoha_orsTypeMoveSom somData{3};
    Kotonoha_orsEvent som{};
    som.command = MOVE_SOM;
    som.startTick = 10;
    som.endTick = 20;
    som.data.move_som = &somData;
    runtime.dispatch(som);
    Require(runtime.moveSom.Active(&som), "Runtime ignored MOVE_SOM window start");
    runtime.moveSom.AdvanceTo(20);
    Require(!runtime.moveSom.Active(&som), "Runtime MOVE_SOM window survived END");

    runtime.playing.clear();
    runtime.movieStates.clear();
    char moviePath[] = "Movie/test";
    Kotonoha_orsTypePlayMovie movieAData{moviePath, 0};
    Kotonoha_orsTypePlayMovie movieBData{moviePath, 0};
    Kotonoha_orsEvent movieA{}, movieB{};
    movieA.command = movieB.command = PLAY_MOVIE;
    movieA.startTick = 0;
    movieA.endTick = 12;
    movieB.startTick = 12;
    movieB.endTick = 24;
    movieA.data.play_movie = &movieAData;
    movieB.data.play_movie = &movieBData;
    runtime.tick = 0;
    runtime.elapsed = 0.0;
    runtime.dispatch(movieA);
    Require(runtime.movieStates[&movieA].prepared &&
                runtime.movieStates[&movieA].mediaReady &&
                runtime.movieStates[&movieA].active,
            "Runtime movie did not prepare/activate at START");
    runtime.renderGameplay(renderer, 1.0f);
    runtime.tick = 12;
    runtime.elapsed = 0.5;
    runtime.movieStates[&movieA].AdvanceTo(runtime.tick);
    runtime.dispatch(movieB);
    Require(runtime.playing.size() == 2,
            "adjacent WMV activation discarded outgoing terminal texture early");
    runtime.renderGameplay(renderer, 1.0f);
    Require(runtime.playing.size() == 2 && runtime.playing.front().terminalFramePresented,
            "movie END did not publish terminal frame during adjacent handoff");
    for (int attempt = 0; attempt < 30 && runtime.playing.size() == 2; ++attempt) {
      SDL_Delay(2);
      runtime.elapsed += 1.0 / 120.0;
      runtime.renderGameplay(renderer, 1.0f);
    }
    Require(runtime.playing.size() == 1 && runtime.playing.front().event == &movieB,
            "outgoing WMV survived after adjacent decoder produced frame");

    runtime.playing.clear();
    runtime.movieStates.clear();
    Kotonoha_orsTypePlayMovie unsupportedData{moviePath, 7};
    Kotonoha_orsEvent unsupported{};
    unsupported.command = PLAY_MOVIE;
    unsupported.startTick = 0;
    unsupported.endTick = 12;
    unsupported.data.play_movie = &unsupportedData;
    runtime.tick = 0;
    runtime.dispatch(unsupported);
    Require(runtime.playing.empty() && runtime.movieStates.count(&unsupported) == 1 &&
                !runtime.movieStates[&unsupported].Supported() &&
                !runtime.movieStates[&unsupported].mediaReady,
            "unsupported PlayMovie numeric acquired invented playback semantics");
  }
};

} // namespace SchoolDays

int main(int argc, char **argv) {
  try {
    Require(argc == 4, "Usage: SchoolDaysMediaTest route fixtures documents");
    auto assets = std::make_shared<SchoolDays::Assets>(argv[2]);
    Application app(assets, argv[1], argv[3]);
    app.runtime.SetState(Kotonoha::GameState::Gameplay);
    app.engine.debugJumpToNode("0");
    for (int i = 0; i < 3; ++i)
      Require(app.engine.Iterate() == SDL_APP_CONTINUE, "Engine media frame failed");
    bool subtitleScene = false, gameScene = false;
    for (const auto *scene : app.runtime.GetActiveScenes()) {
      subtitleScene = subtitleScene || dynamic_cast<const SchoolDays::Subtitles *>(scene) != nullptr;
      gameScene = gameScene || dynamic_cast<const SchoolDays::GameScene *>(scene) != nullptr;
    }
    Require(subtitleScene && gameScene, "School Days must register its custom presentation scenes");
    const double before = *app.runtime.debugNodeSeconds();
    SDL_Delay(40);
    Require(app.engine.Iterate() == SDL_APP_CONTINUE, "Clock frame failed");
    Require(*app.runtime.debugNodeSeconds() > before, "Runtime node clock did not advance");
    Require(app.engine.GetDebug().nodeClockText().find("Clock:") != std::string::npos,
            "School Days node has no debug clock");
    Require(assets->cachedTextures() > 0, "ORS background was not decoded");
    for (const char *channel : {"BGM", "Voice", "Movie"})
      Require(app.engine.GetAudio().channel(channel)->activeStreams() > 0,
              "ORS audio/movie did not attach its channel stream");
    Kotonoha::SurfacePtr frame(SDL_RenderReadPixels(&app.engine.GetRenderer(), nullptr));
    Require(bool(frame), "Cannot inspect rendered movie");
    Uint8 r = 0, g = 0, b = 0, a = 0;
    Require(SDL_ReadSurfacePixel(frame.get(), frame->w * 3 / 4, frame->h * 4 / 5,
                                &r, &g, &b, &a), "Cannot inspect movie pixel");
    Require(r > 180 && g < 80 && b < 80, "WMV frame was not presented over the blue background");
    // This fixture intentionally does not ship the retail MenuBar artwork, so
    // exercise the Save control semantically instead of relying on coordinates
    // from a presenter that is unavailable in this synthetic media corpus.
    app.runtime.openPanel(10);
    Require(app.runtime.anyPanelOpen(), "Save panel did not open");
    SDL_Event event{};
    const double saveClock = *app.runtime.debugNodeSeconds();
    SDL_Delay(35);
    Require(app.engine.Iterate() == SDL_APP_CONTINUE, "Save panel frame failed");
    Require(*app.runtime.debugNodeSeconds() == saveClock, "Save panel must pause the scene");
    event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_RIGHT;
    app.runtime.onEvent(event);
    Require(*app.runtime.debugNodeSeconds() == saveClock, "Save panel leaked gameplay input");
    event.key.key = SDLK_ESCAPE;
    app.runtime.onEvent(event);
    Require(!app.runtime.anyPanelOpen(), "Escape did not close the save panel");
    SDL_Delay(25);
    Require(app.engine.Iterate() == SDL_APP_CONTINUE, "Resume frame failed");
    Require(*app.runtime.debugNodeSeconds() > saveClock, "Scene did not resume after closing save panel");
    app.engine.debugJumpToNode("00/00-00-A03");
    Require(app.engine.Iterate() == SDL_APP_CONTINUE, "Choice frame failed");
    SchoolDays::GameScene *choices = nullptr;
    for (auto *scene : app.runtime.GetActiveScenes())
      if (auto *game = dynamic_cast<SchoolDays::GameScene *>(scene)) choices = game;
    Require(choices != nullptr, "Custom choice scene is missing");
    event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_1;
    Require(!choices->handleEvent(event), "Retail choice accepted keyboard input");
    event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 640.0f;
    event.motion.y = 540.0f;
    Require(choices->handleEvent(event), "Custom choice mouse navigation failed");
    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 640.0f;
    event.button.y = 540.0f;
    Require(choices->handleEvent(event), "Custom choice activation failed");
    Require(!choices->handleEvent(event), "Committed choices are still interactive");
    // Run the invasive synthetic ownership/lifecycle probes last so their
    // deliberately stack-owned ORS events cannot perturb the ordinary fixture.
    SchoolDays::RuntimeMediaParityTestAccess::Run(app.runtime, app.engine.GetRenderer());
    std::cout << "SchoolDaysMediaTest: media, custom scenes, modal pause and choices passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "SchoolDaysMediaTest: " << error.what() << '\n';
    return 1;
  }
}
