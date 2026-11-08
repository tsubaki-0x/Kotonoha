#include <Kotonoha/SchoolDaysAssetMount.hpp>
#include <Kotonoha/SchoolDaysImageAssetResolver.hpp>
#include <Kotonoha/core/AssetProvider.hpp>
#include <Kotonoha/routing/SchoolDaysLxhContent.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>

namespace Kotonoha {
namespace {

std::mutex gMountMutex;
std::shared_ptr<const SchoolDaysGpkAssetStore> gMount;
std::shared_ptr<const SchoolDaysGpkArchive> gIntegratedLxh;
std::shared_ptr<const AssetOverlay> gOverlay;

std::shared_ptr<const SchoolDaysGpkAssetStore> CurrentMount() {
  std::lock_guard<std::mutex> guard(gMountMutex);
  return gMount;
}

std::shared_ptr<const SchoolDaysGpkArchive> CurrentIntegratedLxh() {
  std::lock_guard<std::mutex> guard(gMountMutex);
  return gIntegratedLxh;
}

class MountedRawAssetStream final : public AssetStream {
    std::shared_ptr<const void> mount_;
    std::ifstream file_;
    std::vector<uint8_t> prefix_;
    uint64_t bodyOffset_ = 0u;
    uint64_t logicalSize_ = 0u;
    uint64_t position_ = 0u;

public:
    MountedRawAssetStream(std::shared_ptr<const void> mount,
                          const std::filesystem::path& path,
                          std::vector<uint8_t> prefix, uint64_t bodyOffset,
                          uint64_t logicalSize)
        : mount_(std::move(mount)), file_(path, std::ios::binary),
          prefix_(std::move(prefix)), bodyOffset_(bodyOffset),
          logicalSize_(logicalSize) {
        if (!file_) throw std::runtime_error("School Days GPK raw stream could not open archive");
    }

    uint64_t size() const override { return logicalSize_; }
    uint64_t tell() const override { return position_; }

    std::size_t read(Uint8* buffer, std::size_t size) override {
        if (size == 0u || position_ >= logicalSize_) return 0u;
        if (buffer == nullptr) throw std::invalid_argument("School Days GPK raw stream buffer is null");
        const uint64_t requested = std::min<uint64_t>(
            static_cast<uint64_t>(size), logicalSize_ - position_);
        uint64_t done = 0u;
        if (position_ < prefix_.size()) {
            const uint64_t prefixBytes = std::min<uint64_t>(
                requested, prefix_.size() - static_cast<std::size_t>(position_));
            std::memcpy(buffer, prefix_.data() + static_cast<std::size_t>(position_),
                        static_cast<std::size_t>(prefixBytes));
            position_ += prefixBytes;
            done += prefixBytes;
        }
        const uint64_t remaining = requested - done;
        if (remaining == 0u) return static_cast<std::size_t>(done);
        const uint64_t bodyPosition = position_ - prefix_.size();
        const uint64_t physical = bodyOffset_ + bodyPosition;
        if (physical > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
            remaining > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
            throw std::runtime_error("School Days GPK raw stream range exceeds host I/O limits");
        }
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(physical), std::ios::beg);
        if (!file_) throw std::runtime_error("School Days GPK raw stream seek failed");
        file_.read(reinterpret_cast<char*>(buffer + done),
                   static_cast<std::streamsize>(remaining));
        const std::streamsize got = file_.gcount();
        if (got <= 0) throw std::runtime_error("School Days GPK raw stream read failed");
        position_ += static_cast<uint64_t>(got);
        done += static_cast<uint64_t>(got);
        return static_cast<std::size_t>(done);
    }

    bool seek(uint64_t position) override {
        if (position > logicalSize_) return false;
        position_ = position;
        return true;
    }
};

std::shared_ptr<const AssetOverlay> CurrentOverlay() {
  std::lock_guard<std::mutex> guard(gMountMutex);
  return gOverlay;
}

// Translated scripts name resources with their original Kotonoha-side layout
// ("Se/Se00/...", "Music/Bgm/...", "Music/Vocal/..."); the retail packs expose
// them as "Se00/...", "BGM/SD_BGM/..." and "BGM/VOCAL/...".
std::string CanonicalGpkLogical(std::string_view logicalPath) {
  std::string path(logicalPath);
  for (char &ch : path)
    if (ch == '\\')
      ch = '/';
  const auto startsWith = [&path](std::string_view prefix) {
    if (path.size() < prefix.size())
      return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(path[i])) !=
          std::tolower(static_cast<unsigned char>(prefix[i])))
        return false;
    }
    return true;
  };
  if (startsWith("Music/Bgm/"))
    return "BGM/SD_BGM/" + path.substr(10);
  if (startsWith("Music/Vocal/"))
    return "BGM/VOCAL/" + path.substr(12);
  if (startsWith("Se/Se"))
    return path.substr(3);
  return path;
}

bool Fail(std::string *error, const char *text) {
  if (error != nullptr)
    *error = text != nullptr ? text : "School Days asset error";
  return false;
}

bool Fail(std::string *error, const std::string &text) {
  if (error != nullptr) *error = text;
  return false;
}

SchoolDaysAssetResolutionStatus
MountedResolverLookup(const std::string &logicalPath, std::string *reference) {
  if (reference != nullptr)
    reference->clear();
  // These authored paths belong exclusively to Packs/LxH.GPK; never a
  // leftover loose overlay or an original commercial archive.
  if (SchoolDaysLxhContent::IsPackResource(logicalPath)) {
    const auto status = LookupMountedSchoolDaysAsset(logicalPath);
    if (status == SchoolDaysGpkLookupStatus::Present && reference != nullptr)
      *reference = MakeSchoolDaysGpkUri(CanonicalGpkLogical(logicalPath));
    return status == SchoolDaysGpkLookupStatus::Present
               ? SchoolDaysAssetResolutionStatus::Present
               : status == SchoolDaysGpkLookupStatus::Collision
                     ? SchoolDaysAssetResolutionStatus::Collision
                     : status == SchoolDaysGpkLookupStatus::Invalid
                           ? SchoolDaysAssetResolutionStatus::Invalid
                           : SchoolDaysAssetResolutionStatus::Missing;
  }
  std::string physical;
  if (ResolveSchoolDaysOverlayFile(logicalPath, &physical)) {
    if (reference != nullptr)
      *reference = physical;
    return SchoolDaysAssetResolutionStatus::Present;
  }
  const auto status = LookupMountedSchoolDaysAsset(logicalPath);
  switch (status) {
  case SchoolDaysGpkLookupStatus::Present:
    if (reference != nullptr)
      *reference = MakeSchoolDaysGpkUri(CanonicalGpkLogical(logicalPath));
    return SchoolDaysAssetResolutionStatus::Present;
  case SchoolDaysGpkLookupStatus::Missing:
    return SchoolDaysAssetResolutionStatus::Missing;
  case SchoolDaysGpkLookupStatus::Collision:
    return SchoolDaysAssetResolutionStatus::Collision;
  case SchoolDaysGpkLookupStatus::Invalid:
    return SchoolDaysAssetResolutionStatus::Invalid;
  }
  return SchoolDaysAssetResolutionStatus::Invalid;
}

bool MountedResolverIsReference(const std::string &reference) {
  return IsSchoolDaysGpkUri(reference);
}

bool MountedResolverRead(const std::string &reference,
                         std::vector<uint8_t> *bytes, std::string *error) {
  std::string logical;
  if (!SchoolDaysLogicalFromGpkUri(reference, &logical))
    return Fail(error, "School Days GPK reference is invalid");
  return ReadMountedSchoolDaysAsset(logical, bytes, error);
}

bool MountedResolverListDirectory(const std::string &directoryReference,
                                  std::vector<std::string> *children,
                                  std::string *error) {
  if (children == nullptr)
    return Fail(error, "School Days GPK directory output is null");
  children->clear();
  std::string logical;
  if (!SchoolDaysLogicalFromGpkUri(directoryReference, &logical))
    return Fail(error, "School Days GPK directory reference is invalid");
  const auto mount = CurrentMount();
  const auto integrated = CurrentIntegratedLxh();
  if (mount == nullptr || integrated == nullptr)
    return Fail(error, "School Days integrated GPK set is not mounted");
  std::vector<std::string> logicalChildren;
  const auto status = mount->ListDirectory(logical, &logicalChildren);
  std::vector<std::string> integratedChildren;
  const auto integratedStatus = integrated->ListDirectory(logical, &integratedChildren);
  if (status == SchoolDaysGpkLookupStatus::Collision ||
      status == SchoolDaysGpkLookupStatus::Invalid ||
      integratedStatus == SchoolDaysGpkLookupStatus::Collision ||
      integratedStatus == SchoolDaysGpkLookupStatus::Invalid)
    return Fail(error, "School Days GPK directory path is invalid or ambiguous");
  if (status != SchoolDaysGpkLookupStatus::Present &&
      integratedStatus != SchoolDaysGpkLookupStatus::Present) {
    if (error != nullptr) {
      switch (status) {
      case SchoolDaysGpkLookupStatus::Missing:
        *error = "School Days GPK directory is missing";
        break;
      case SchoolDaysGpkLookupStatus::Collision:
        *error = "School Days GPK directory has a case-insensitive collision";
        break;
      default:
        *error = "School Days GPK directory is invalid";
        break;
      }
    }
    return false;
  }
  if (integratedStatus == SchoolDaysGpkLookupStatus::Present) {
    logicalChildren.insert(logicalChildren.end(), integratedChildren.begin(),
                           integratedChildren.end());
    std::sort(logicalChildren.begin(), logicalChildren.end());
    logicalChildren.erase(std::unique(logicalChildren.begin(), logicalChildren.end()),
                          logicalChildren.end());
  }
  children->reserve(logicalChildren.size());
  for (const auto &child : logicalChildren)
    children->push_back(MakeSchoolDaysGpkUri(child));
  return true;
}

bool ReadFilesystem(std::string_view reference, std::vector<uint8_t> *out,
                    std::string *error) {
  if (out == nullptr)
    return Fail(error, "School Days asset output is null");
  out->clear();
  std::ifstream file(std::filesystem::path(std::string(reference)),
                     std::ios::binary | std::ios::ate);
  if (!file)
    return Fail(error, "School Days asset file could not be opened");
  const std::streamoff end = file.tellg();
  if (end < 0 ||
      static_cast<uintmax_t>(end) >
          static_cast<uintmax_t>(std::numeric_limits<size_t>::max()) ||
      static_cast<uintmax_t>(end) >
          static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
    return Fail(error, "School Days asset file exceeds host read range");
  }
  try {
    out->resize(static_cast<size_t>(end));
  } catch (...) {
    return Fail(error, "School Days asset buffer allocation failed");
  }
  file.seekg(0, std::ios::beg);
  if (!out->empty() && !file.read(reinterpret_cast<char *>(out->data()),
                                  static_cast<std::streamsize>(out->size()))) {
    out->clear();
    return Fail(error, "School Days asset file read failed");
  }
  return true;
}

} // namespace

bool MountSchoolDaysRetailGpks(const std::filesystem::path &gameOrPacksRoot,
                               std::string *error) {
  if (error != nullptr)
    error->clear();
  std::filesystem::path packs = gameOrPacksRoot;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(packs / "BGM.GPK", ec)) {
    ec.clear();
    packs /= "Packs";
  }

  auto candidate = std::make_shared<SchoolDaysGpkAssetStore>();
  if (!candidate->OpenRetailDirectory(packs, error))
    return false;
  auto integrated = std::make_shared<SchoolDaysGpkArchive>();
  std::string integratedError;
  if (!integrated->Open(packs / "LxH.GPK", &integratedError))
    return Fail(error, std::string("Mandatory integrated LxH.GPK missing or invalid: ") +
                           integratedError);
  const auto expected = SchoolDaysLxhContent::PackEntries();
  if (integrated->EntryCount() != expected.size())
    return Fail(error, "Integrated LxH.GPK must contain exactly 28 files");
  for (const auto& logical : expected) {
    size_t entryIndex = 0;
    if (integrated->Lookup(logical, &entryIndex) != SchoolDaysGpkLookupStatus::Present ||
        entryIndex >= integrated->Index().Entries().size())
      return Fail(error, "Integrated LxH.GPK missing or ambiguous file: " + logical);
    const auto& entry = integrated->Index().Entries()[entryIndex];
    if (!entry.IsRaw() || entry.uncompressedLength != entry.storedLength ||
        entry.storedLength == 0u)
      return Fail(error, "Integrated LxH.GPK must use non-empty seekable raw entry: " + logical);
  }
  {
    std::lock_guard<std::mutex> guard(gMountMutex);
    gMount = std::move(candidate);
    gIntegratedLxh = std::move(integrated);
  }
  return true;
}

bool SetSchoolDaysAssetOverlay(const std::filesystem::path &directory,
                               std::string *error) {
  try {
    auto mounted = std::make_shared<AssetOverlay>(directory);
    std::lock_guard<std::mutex> guard(gMountMutex);
    gOverlay = std::move(mounted);
    return true;
  } catch (const std::exception &e) {
    return Fail(error, e.what());
  }
}
bool SchoolDaysAssetOverlaySet() {
  auto overlay = CurrentOverlay();
  return overlay && !overlay->root().empty();
}
bool ResolveSchoolDaysOverlayFile(std::string_view logical,
                                  std::string *physical) {
  // Keep all integrated epilogues in the extra GPK, even when a historical
  // display-options overlay still contains loose copies of those 28 files.
  // Only the packaged installation is authoritative. Extracted developer
  // fixtures can still supply synthetic grafts through a loose overlay.
  if (CurrentIntegratedLxh() && SchoolDaysLxhContent::IsPackResource(logical))
    return false;
  auto overlay = CurrentOverlay();
  if (!overlay)
    return false;
  auto file = overlay->resolve(std::string(logical));
  // Compatibility names are mapped onto the same indexed engine overlay;
  // there is no separate filesystem script mount.
  if (file.empty()) {
    auto key = AssetOverlay::key(std::string(logical));
    const std::string prefix = "script/english/";
    if (key.compare(0, prefix.size(), prefix) == 0) {
      std::filesystem::path relative(key.substr(prefix.size()));
      auto name = relative.filename().string();
      name.resize(name.find('.') == name.npos ? name.size() : name.find('.'));
      auto stem = (relative.parent_path() / name).generic_string();
      for (const auto &candidate : overlay->list(stem)) {
        std::filesystem::path path(candidate);
        if (path.parent_path() == relative.parent_path() &&
            path.extension() == ".txt") {
          file = overlay->resolve(candidate);
          break;
        }
      }
    }
  }

  if (file.empty())
    return false;
  if (physical)
    *physical = std::move(file);
  return true;
}

void UnmountSchoolDaysRetailGpks() {
  std::lock_guard<std::mutex> guard(gMountMutex);
  gMount.reset();
  gIntegratedLxh.reset();
}

void InstallMountedSchoolDaysAssetResolver() {
  InstallSchoolDaysVirtualAssetLookup(&MountedResolverLookup);
  InstallSchoolDaysVirtualAssetIo(&MountedResolverIsReference,
                                  &MountedResolverRead,
                                  &MountedResolverListDirectory);
}

void UninstallMountedSchoolDaysAssetResolver() {
  if (gSchoolDaysVirtualAssetLookup == &MountedResolverLookup) {
    InstallSchoolDaysVirtualAssetLookup(nullptr);
    InstallSchoolDaysVirtualAssetIo(nullptr, nullptr, nullptr);
  }
}

bool SchoolDaysRetailGpksMounted() {
  const auto mount = CurrentMount();
  const auto integrated = CurrentIntegratedLxh();
  return mount != nullptr && mount->IsOpen() && integrated != nullptr &&
         integrated->IsOpen();
}

size_t MountedSchoolDaysGpkEntryCount() {
  const auto mount = CurrentMount();
  const auto integrated = CurrentIntegratedLxh();
  return mount != nullptr && integrated != nullptr
             ? mount->EntryCount() + integrated->EntryCount() : 0u;
}

SchoolDaysGpkLookupStatus
LookupMountedSchoolDaysAsset(std::string_view logicalPath) {
  if (SchoolDaysLxhContent::IsPackResource(logicalPath)) {
    const auto integrated = CurrentIntegratedLxh();
    return integrated ? integrated->Lookup(CanonicalGpkLogical(logicalPath))
                      : SchoolDaysGpkLookupStatus::Missing;
  }
  if (ResolveSchoolDaysOverlayFile(logicalPath, nullptr))
    return SchoolDaysGpkLookupStatus::Present;
  const auto mount = CurrentMount();
  return mount != nullptr ? mount->Lookup(CanonicalGpkLogical(logicalPath))
                          : SchoolDaysGpkLookupStatus::Missing;
}

bool ReadMountedSchoolDaysAsset(std::string_view logicalPath,
                                std::vector<uint8_t> *out, std::string *error) {
  if (SchoolDaysLxhContent::IsPackResource(logicalPath)) {
    const auto integrated = CurrentIntegratedLxh();
    return integrated ? integrated->Read(CanonicalGpkLogical(logicalPath), out, error)
                      : Fail(error, "Mandatory integrated LxH.GPK not mounted");
  }
  std::string physical;
  if (ResolveSchoolDaysOverlayFile(logicalPath, &physical))
    return ReadFilesystem(physical, out, error);
  const auto mount = CurrentMount();
  if (mount == nullptr)
    return Fail(error, "School Days retail GPKs are not mounted");
  return mount->Read(CanonicalGpkLogical(logicalPath), out, error);
}

std::unique_ptr<AssetStream> OpenMountedSchoolDaysAssetStream(
    std::string_view logicalPath) {
    if (SchoolDaysLxhContent::IsPackResource(logicalPath)) {
      const auto integrated = CurrentIntegratedLxh();
      if (integrated == nullptr) return {};
      size_t idx = 0;
      if (integrated->Lookup(CanonicalGpkLogical(logicalPath), &idx) !=
              SchoolDaysGpkLookupStatus::Present ||
          idx >= integrated->Index().Entries().size()) return {};
      const auto& entry = integrated->Index().Entries()[idx];
      if (!entry.IsRaw() || static_cast<uint64_t>(entry.offset) +
              entry.PhysicalBodyLength() > integrated->PidxOffset()) return {};
      try {
        return std::make_unique<MountedRawAssetStream>(
            integrated, integrated->Path(), entry.inlinePrefix, entry.offset,
            static_cast<uint64_t>(entry.storedLength));
      } catch (...) { return {}; }
    }
    if (ResolveSchoolDaysOverlayFile(logicalPath, nullptr)) return {};
    const auto mount = CurrentMount();
    if (mount == nullptr) return {};
    size_t archiveIndex = 0u;
    size_t entryIndex = 0u;
    const std::string canonical = CanonicalGpkLogical(logicalPath);
    if (mount->Lookup(canonical, &archiveIndex, &entryIndex) !=
        SchoolDaysGpkLookupStatus::Present) {
        return {};
    }
    const SchoolDaysGpkArchive* archive = mount->Archive(archiveIndex);
    if (archive == nullptr || entryIndex >= archive->Index().Entries().size()) return {};
    const auto& entry = archive->Index().Entries()[entryIndex];
    if (!entry.IsRaw()) return {};
    const uint64_t bodySize = static_cast<uint64_t>(entry.PhysicalBodyLength());
    const uint64_t physicalEnd = static_cast<uint64_t>(entry.offset) + bodySize;
    if (physicalEnd < entry.offset || physicalEnd > archive->PidxOffset() ||
        entry.inlinePrefix.size() > entry.storedLength) {
        return {};
    }
    try {
        return std::make_unique<MountedRawAssetStream>(
            mount, archive->Path(), entry.inlinePrefix, entry.offset,
            static_cast<uint64_t>(entry.storedLength));
    } catch (...) {
        return {};
    }
}

std::string MakeSchoolDaysGpkUri(std::string_view logicalPath) {
  std::string result(kSchoolDaysGpkUriPrefix);
  result.reserve(result.size() + logicalPath.size());
  for (char ch : logicalPath)
    result.push_back(ch == '\\' ? '/' : ch);
  return result;
}

bool IsSchoolDaysGpkUri(std::string_view reference) {
  return reference.size() > kSchoolDaysGpkUriPrefix.size() &&
         reference.substr(0u, kSchoolDaysGpkUriPrefix.size()) ==
             kSchoolDaysGpkUriPrefix;
}

bool SchoolDaysLogicalFromGpkUri(std::string_view reference,
                                 std::string *logicalPath) {
  if (logicalPath == nullptr || !IsSchoolDaysGpkUri(reference))
    return false;
  logicalPath->assign(reference.substr(kSchoolDaysGpkUriPrefix.size()));
  return !logicalPath->empty();
}

bool ReadSchoolDaysAssetRef(std::string_view reference,
                            std::vector<uint8_t> *out, std::string *error) {
  if (error != nullptr)
    error->clear();
  std::string logical;
  if (SchoolDaysLogicalFromGpkUri(reference, &logical))
    return ReadMountedSchoolDaysAsset(logical, out, error);
  return ReadFilesystem(reference, out, error);
}

} // namespace Kotonoha
