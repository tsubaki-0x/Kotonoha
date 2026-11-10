// Opt-in Windows QA: reads seven authentic user-owned retail ending saves
// against the installed L4 packs, but never mutates save files, the user's
// GlobalFlag, configuration or retail GPKs.
#include "SchoolDaysAssets.hpp"

#include <Kotonoha/SchoolDaysAssetMount.hpp>
#include <Kotonoha/routing/SchoolDaysGpkLanguageProfile.hpp>
#include <Kotonoha/routing/SchoolDaysKtrfGameplayBridge.hpp>
#include <Kotonoha/routing/SchoolDaysKtrfSession.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

struct Kotonoha_Game {};
namespace fs = std::filesystem;

namespace {
void Check(bool ok, const std::string &message) {
  if (!ok) throw std::runtime_error(message);
}

std::vector<uint8_t> Read(const fs::path &file) {
  std::ifstream stream(file, std::ios::binary);
  Check(static_cast<bool>(stream), "Cannot read saved slot: " + file.string());
  return {std::istreambuf_iterator<char>(stream), {}};
}

struct Ending {
  const char *filename;
  const char *scene;
};
constexpr std::array<Ending, 7> Endings{{
    {"SaveFile000.DAT", "05/05-A2-Z00"},
    {"SaveFile001.DAT", "05/05-A2-F00"},
    {"SaveFile002.DAT", "05/05-G0-L03"},
    {"SaveFile0012.DAT", "05/05-KC-F00"},
    {"SaveFile0013.DAT", "05/05-SC-L03"},
    {"SaveFile0017.DAT", "05/05-SB-E01"},
    {"SaveFile0019.DAT", "05/05-B0-F00"},
}};

// Replay the actual transactional bridge LOAD without a renderer or game GUI.
// Its presenter factory only records the selected scene path; the mounted
// archive still owns real ORS/media resolution, and no save writes are made.
struct ProbeScene final : Kotonoha::SchoolDaysScenePresentation {
  explicit ProbeScene(std::string source) : path(std::move(source)) {}
  std::string path;
  void RestoreChoice(int64_t) override {}
  void SetPaused(bool) override {}
};

Kotonoha::Gameplay *CreateProbeScene(const char *path, Kotonoha_Game *,
                                     void *) {
  return path && *path ? new ProbeScene(path) : nullptr;
}
void DestroyProbeScene(Kotonoha::Gameplay *scene, void *) { delete scene; }
}

int main(int argc, char **argv) {
  try {
    Check(argc == 5,
          "Usage: SchoolDaysLxhRetailSaveProbe RELEASE_DIR ROUTE SAVE_DIR "
          "LOCALE (en|pt-BR|es)");
    const fs::path release = argv[1];
    const fs::path route = argv[2];
    const fs::path saves = argv[3];
    const std::string locale(argv[4]);
    Check(locale == "en" || locale == "pt-BR" || locale == "es",
          "Only certified GPK locales are supported");
    Check(fs::is_regular_file(route), "Route does not exist");
    Check(fs::is_directory(saves), "Save directory does not exist");
    const auto settings = (release / "settings.json").string();
    const Kotonoha::SchoolDaysTranslatedGpkSelection selection{
        locale,
        Kotonoha::SchoolDaysGpkLanguageProfile::ScriptPath(settings, locale),
        Kotonoha::SchoolDaysGpkLanguageProfile::LxhPath(settings, locale)};
    std::string error;
    Check(Kotonoha::ConfigureSchoolDaysTranslatedGpks(selection, &error),
          "Locale is not installable: " + error);

    {
      SchoolDays::Assets assets(release);
      Check(Kotonoha::SchoolDaysRetailGpksMounted(),
            "Mandatory retail/LxH packs were not mounted");
      Kotonoha::SchoolDaysKtrfSession session;
      Kotonoha_KtrfError routeError{};
      Check(session.Open(route.string().c_str(), release.string().c_str(),
                         &routeError),
            std::string("KTRF cannot open: ") + routeError.message);
      Kotonoha_Game game;
      Kotonoha::SchoolDaysKtrfGameplayBridge bridge;
      const Kotonoha::SchoolDaysKtrfGameplayBridge::Factory factory{
          &CreateProbeScene, &DestroyProbeScene, nullptr};
      routeError = Kotonoha_KtrfError{};
      const auto routeString = route.string();
      const auto releaseString = release.string();
      Check(bridge.Open(routeString.c_str(), releaseString.c_str(), &game,
                        factory, &routeError),
            std::string("Cannot create private LOAD bridge: ") +
                routeError.message);
      for (const auto &save : Endings) {
        const fs::path path = saves / save.filename;
        const auto bytes = Read(path);
        Kotonoha::SchoolDaysKtrfSession::LoadPlan load;
        routeError = Kotonoha_KtrfError{};
        Check(session.PlanLoadSave(bytes, &load, &routeError),
              std::string("Save plan failed for ") + save.filename + ": " +
                  routeError.message);
        Check(load.scene.sceneKey == save.scene,
              std::string("Save resolved to another scene: ") + save.filename +
                  " -> " + load.scene.sceneKey);
        const auto asset = assets.resolve(load.scene.orsPath);
        Check(Kotonoha::IsSchoolDaysGpkUri(asset),
              std::string("LxH save scene did not resolve through mounted GPK: ") +
                  load.scene.orsPath);
        const auto ors = assets.read(asset);
        const std::string text(ors.begin(), ors.end());
        const std::string marker =
            "LxH/Media/" + std::string(save.scene).substr(3) + "-LXH";
        Check(text.find(marker) != std::string::npos,
              std::string("Saved ending did not resolve to LxH extended ORS: ") +
                  load.scene.sceneKey);
        routeError = Kotonoha_KtrfError{};
        Kotonoha::SchoolDaysKtrfGameplayBridge::StepResult restored;
        Check(bridge.LoadSave(bytes, &restored, &routeError),
              std::string("Actual bridge LOAD failed for ") + save.filename +
                  ": " + routeError.message);
        Check(restored.scene.sceneKey == save.scene &&
                  bridge.CurrentScene().sceneKey == save.scene &&
                  bridge.CurrentGameplay() != nullptr,
              std::string("Actual LOAD did not activate saved ending: ") +
                  save.filename);
        std::cout << "LXH_RETAIL_SAVE_OK " << save.filename << " => "
                  << load.scene.sceneKey << " bytes=" << ors.size() << '\n';
      }
    }
    Kotonoha::ClearSchoolDaysTranslatedGpks();
    std::cout << "LXH_RETAIL_SAVE_PROBE_PASS 7/7 locale=" << locale << '\n';
    return 0;
  } catch (const std::exception &e) {
    Kotonoha::UnmountSchoolDaysRetailGpks();
    Kotonoha::ClearSchoolDaysTranslatedGpks();
    std::cerr << "LXH_RETAIL_SAVE_PROBE_FAIL " << e.what() << '\n';
    return 1;
  }
}
