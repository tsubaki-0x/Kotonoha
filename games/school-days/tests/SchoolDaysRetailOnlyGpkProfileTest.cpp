#include <Kotonoha/SchoolDaysAssetMount.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

void require(bool value, const std::string &message) {
  if (!value) throw std::runtime_error(message);
}

void assertProfile(const std::string &locale, const fs::path &script) {
  std::string error;
  require(Kotonoha::ConfigureSchoolDaysTranslatedGpks({locale, script, {}}, &error),
          locale + " unexpectedly requires LxH.GPK: " + error);
  require(Kotonoha::ClearSchoolDaysTranslatedGpks(&error),
          locale + " profile could not be cleared: " + error);
}
} // namespace

int main() {
#if !defined(KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY)
  std::cerr << "This test belongs only to retail-only builds.\n";
  return 1;
#else
  try {
    const fs::path root = fs::current_path() / "retail-only-gpk-profile-fixture";
    fs::create_directories(root);
    const auto brazilian = root / "Script - Brazilian.GPK";
    const auto spanish = root / "Script - Spanish.GPK";
    {
      std::ofstream file(brazilian, std::ios::binary | std::ios::trunc);
      file << 'x';
    }
    {
      std::ofstream file(spanish, std::ios::binary | std::ios::trunc);
      file << 'x';
    }
    // The selection gate checks identity and file existence; actual GPK
    // structure is verified later during mount against Script.GPK retail.
    assertProfile("en", {});
    assertProfile("pt-BR", brazilian);
    assertProfile("es", spanish);

    std::string error;
    require(!Kotonoha::ConfigureSchoolDaysTranslatedGpks(
                {"pt-BR", brazilian, root / "pt-BR-lxh.GPK"}, &error),
            "Retail-only mode incorrectly accepts an LxH translation");
    require(!Kotonoha::ConfigureSchoolDaysTranslatedGpks(
                {"es", brazilian, {}}, &error),
            "Spanish mode incorrectly accepts the Brazilian GPK name");
    fs::remove(brazilian);
    fs::remove(spanish);
    fs::remove(root);
    std::cout << "Retail-only EN/PT-BR/ES selection requires no LxH.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Retail-only GPK profile test: " << error.what() << '\n';
    return 1;
  }
#endif
}
