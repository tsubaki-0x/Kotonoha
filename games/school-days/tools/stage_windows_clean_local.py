#!/usr/bin/env python3
"""Stage reproducible, LOCAL-ONLY Windows QA previews from pinned builds.

This is NOT a public-release authorizer.  It positively selects files rather
than invoking CPack, copying Packs, or reading private save/translation roots.
It does not run SchoolDays.exe or perform any Git/network operation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys
import zipfile


COMMIT = "eebeee1012472fe59dc9d4f128a80b507b087715"
EXE_SHA256 = {
    "dev": "20EE0BF45FB1F370B41ED70808A29FE632DE21E7F52A5D78B261C70553D9F9F3",
    "dist": "96CD55BCBDD24A1965FE9DDB7D6AB095D7A2138FE348642CB8D1BE565E85D52B",
}
FONT_SHA256 = "7C8597677E9FAC0F54D7848AD18BC6A708DFB5BAA4EBF4BD91E66EFCCA313BF3"
OFL_SHA256 = "F05E84C3000FAF09CF8E445D35018B01FC0D6026953840E9398AEAB501B86DA2"
BINARIES = ("SchoolDays.exe", "SDL3.dll", "SDL3_ttf.dll", "imgui.dll", "kotonoha.dll")
SHADERS = {
    "Anime4K": ("LICENSE-Anime4K.txt", "SOURCE.txt"),
    "ArtCNN": ("LICENSE-ArtCNN.txt", "SOURCE.txt"),
}
THIRD_PARTIES = {
    "SDL3": "lib/submodules/SDL/LICENSE.txt",
    "SDL3_ttf": "lib/submodules/SDL_ttf/LICENSE.txt",
    "ImGui": "lib/submodules/imgui/LICENSE.txt",
    "Kotonoha": "lib/LICENSE",
}
VCPKG_REQUIRED = {
    "brotli", "bzip2", "ffmpeg", "freetype", "fribidi", "harfbuzz",
    "json-c", "libass", "libiconv", "libpng", "plutosvg", "plutovg", "zlib",
}
FORBIDDEN_EXTENSIONS = {
    ".gpk", ".ors", ".wmv", ".ogg", ".dat", ".pdb", ".lib", ".pem",
    ".pfx", ".key", ".keystore", ".iso", ".zip",
}
PROHIBITED_DIRS = {"packs", "save", "saves", "userdata", "translations"}
SECRET_SIGNATURE = re.compile(
    rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|"
    rb"ghp_[A-Za-z0-9]{32,}|"
    rb"github_pat_[A-Za-z0-9_]{20,}|"
    rb"sk-[A-Za-z0-9]{32,}"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def ordinary_file(path: Path) -> None:
    """Reject symlink/reparse-point final paths rather than following them."""
    st = path.lstat()
    if not stat.S_ISREG(st.st_mode) or path.is_symlink() or (
        getattr(st, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    ):
        raise ValueError(f"Not an ordinary input file: {path}")


def font_metadata(path: Path) -> dict[int, set[str]]:
    """Read TrueType name records directly: no font packages/network needed."""
    ordinary_file(path)
    with path.open("rb") as f:
        signature, n_tables, _, _, _ = struct.unpack(">4sHHHH", f.read(12))
        if signature not in (b"\x00\x01\x00\x00", b"OTTO", b"true"):
            raise ValueError("Unrecognized OpenType/TrueType font signature")
        names: tuple[int, int] | None = None
        for _ in range(n_tables):
            tag, _, start, length = struct.unpack(">4sIII", f.read(16))
            if tag == b"name":
                names = (start, length)
        if names is None:
            raise ValueError("Font lacks OpenType name table")
        start, length = names
        if length > 1024 * 1024:
            raise ValueError("Unexpected font name table size")
        f.seek(start)
        _, count, string_offset = struct.unpack(">HHH", f.read(6))
        records = [struct.unpack(">HHHHHH", f.read(12)) for _ in range(count)]
        result: dict[int, set[str]] = {}
        for platform, _, _, name_id, size, offset in records:
            if name_id not in (0, 1, 6, 13, 14) or size > 4096:
                continue
            f.seek(start + string_offset + offset)
            raw = f.read(size)
            try:
                value = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman")
            except UnicodeError:
                continue
            result.setdefault(name_id, set()).add(value)
    return result


def validate_font(font: Path, ofl: Path) -> str:
    if sha256(font) != FONT_SHA256:
        raise ValueError("Unapproved font bytes; refuse substitution or proprietary Windows font")
    if sha256(ofl) != OFL_SHA256:
        raise ValueError("OFL 1.1 license text checksum mismatch")
    names = font_metadata(font)
    if "Noto Sans JP" not in names.get(1, set()):
        raise ValueError("Unrecognized Noto font family")
    if "NotoSansJP-Regular" not in names.get(6, set()):
        raise ValueError("Unrecognized PostScript font name")
    if not any("SIL Open Font License, Version 1.1" in s for s in names.get(13, set())):
        raise ValueError("Font does not declare SIL OFL v1.1")
    copyright_notice = next(
        (s for s in names.get(0, set()) if "2014-2021 Adobe" in s), None
    )
    if copyright_notice is None:
        raise ValueError("Expected copyright attribution not found")
    return copyright_notice


class Stage:
    def __init__(self, destination: Path):
        self.destination = destination
        self.entries: list[dict[str, str | int]] = []

    def copy(self, source: Path, relative: str, role: str) -> None:
        ordinary_file(source)
        if Path(relative).is_absolute() or ".." in Path(relative).parts or "\\" in relative:
            raise ValueError(f"Unsafe output path: {relative}")
        target = self.destination / relative
        if target.exists():
            raise FileExistsError(f"Duplicate output: {relative}")
        target.parent.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha256()
        with source.open("rb") as reader, target.open("xb") as writer:
            for block in iter(lambda: reader.read(1024 * 1024), b""):
                writer.write(block)
                digest.update(block)
        if sha256(target) != digest.hexdigest().upper():
            raise ValueError(f"Copied hash differs for {relative}")
        self.entries.append({"path": relative, "bytes": target.stat().st_size,
                             "sha256": digest.hexdigest().upper(), "role": role})

    def text(self, relative: str, content: str, role: str) -> None:
        target = self.destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        self.entries.append({"path": relative, "bytes": target.stat().st_size,
                             "sha256": sha256(target), "role": role})


def guard_payload(destination: Path) -> None:
    paths = list(destination.rglob("*"))
    for p in paths:
        if p.is_symlink():
            raise ValueError(f"Symlink in staged output: {p}")
        if p.is_dir():
            if p.name.casefold() in PROHIBITED_DIRS:
                raise ValueError(f"Forbidden directory in payload: {p}")
            continue
        ordinary_file(p)
        rel = p.relative_to(destination)
        if p.suffix.casefold() in FORBIDDEN_EXTENSIONS:
            raise ValueError(f"Forbidden file in payload: {rel}")
        if p.suffix.casefold() == ".exe" and rel.as_posix() != "SchoolDays.exe":
            raise ValueError(f"Unexpected executable: {rel}")
        if p.suffix.casefold() == ".dll" and p.name not in BINARIES:
            raise ValueError(f"Unexpected DLL: {rel}")
        if p.suffix.casefold() in (".txt", ".json", ".md", ".ass", ".glsl"):
            if p.stat().st_size <= 4 * 1024 * 1024 and SECRET_SIGNATURE.search(p.read_bytes()):
                raise ValueError(f"Potential secret in staged text: {rel}")


def deterministic_zip(stage: Path, archive: Path) -> str:
    if archive.exists():
        raise FileExistsError(f"Archive already exists: {archive}")
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for file in sorted(stage.rglob("*")):
            if not file.is_file():
                continue
            relative = file.relative_to(stage).as_posix()
            info = zipfile.ZipInfo(relative, (2020, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            out.writestr(info, file.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                         compresslevel=9)
    return sha256(archive)


def stage(args: argparse.Namespace) -> None:
    root = args.source_release.resolve()
    repository = args.source_repo.resolve()
    destination = args.destination.absolute()
    archive = destination.parent / (destination.name + ".zip")
    if destination.exists() or archive.exists():
        raise FileExistsError("Output stage or archive already exists; refusing overwrite")
    if destination == root or root in destination.parents or destination in root.parents:
        raise ValueError("Staging must be separate from the source Release tree")
    if not args.private_local_preview:
        raise ValueError("Only private/local preview mode is permitted until rights review")
    font_notice = validate_font(args.font, args.ofl)

    # Fail all read-only source preconditions before creating the destination.
    for binary in BINARIES:
        ordinary_file(root / binary)
    if sha256(root / "SchoolDays.exe") != EXE_SHA256[args.profile]:
        raise ValueError(f"Wrong SchoolDays.exe for pinned {args.profile} candidate")
    for relative in ("game-assets/styles.ass", "game-assets/school-days-hq.ktnroute"):
        ordinary_file(root / relative)
    for kind, notices in SHADERS.items():
        shader = root / "assets" / kind
        if not shader.is_dir() or shader.is_symlink():
            raise ValueError(f"Missing shader directory: {shader}")
        sources = sorted(shader.glob("*.glsl"))
        if not 1 <= len(sources) <= 80:
            raise ValueError(f"Unexpected {kind} shader list")
        for file in sources:
            ordinary_file(file)
        for name in notices:
            ordinary_file(shader / name)
    for relative in THIRD_PARTIES.values():
        ordinary_file(repository / relative)
    share = args.vcpkg_share.resolve()
    ports = {
        p.name: p / "copyright"
        for p in share.iterdir() if p.is_dir() and (p / "copyright").is_file()
    }
    if not VCPKG_REQUIRED.issubset(ports):
        raise ValueError(f"Missing notices: {sorted(VCPKG_REQUIRED - ports.keys())}")
    for p in ports.values():
        ordinary_file(p)
    ordinary_file(args.font)
    ordinary_file(args.ofl)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.mkdir()
    incomplete = destination / "INCOMPLETE-DO-NOT-USE.txt"
    incomplete.write_text("Incomplete preview staging; do not use or distribute.\n",
                          encoding="utf-8")
    st = Stage(destination)
    for name in BINARIES:
        st.copy(root / name, name, "compiled Windows runtime")
    for kind, notices in SHADERS.items():
        shader = root / "assets" / kind
        for p in sorted(shader.glob("*.glsl")):
            st.copy(p, f"assets/{kind}/{p.name}", "MIT shader")
        for name in notices:
            st.copy(shader / name, f"assets/{kind}/{name}", "shader license/source")
    st.copy(root / "game-assets/styles.ass", "game-assets/styles.ass",
            "School Days style data; rights review pending")
    st.copy(root / "game-assets/school-days-hq.ktnroute",
            "game-assets/school-days-hq.ktnroute",
            "retail-derived routing metadata; PRIVATE LOCAL USE ONLY")
    st.copy(args.font, "assets/fonts/NotoSansJP-Regular.ttf",
            "Noto Sans JP font: SIL OFL v1.1 (unmodified bytes)")
    st.text("THIRD-PARTY-LICENSES/NotoSansJP-OFL-1.1.txt",
            font_notice + "\n\n" + args.ofl.read_text(encoding="utf-8"),
            "Noto copyright + full SIL OFL")
    for label, rel in sorted(THIRD_PARTIES.items()):
        st.copy(repository / rel, f"THIRD-PARTY-LICENSES/{label}.txt",
                "upstream project/license notice")
    for name, path in sorted(ports.items()):
        st.copy(path, f"THIRD-PARTY-LICENSES/vcpkg/{name}.txt",
                "installed vcpkg port copyright/license")
    st.text("LOCAL-ONLY-README.txt",
            "KOTONOHA SCHOOL DAYS HQ - LOCAL WINDOWS QA PREVIEW\n"
            f"Profile: {args.profile.upper()} | Pinned executable source: {COMMIT}\n"
            "DO NOT PUBLISH OR DISTRIBUTE THIS PACKAGE. The included 1.7MB route "
            "contains retail-derived metadata pending rights review.\n"
            "No retail GPK, save, Config.DAT, translation GPK/JSON, launcher HD artwork "
            "or original executable is bundled.\n"
            "User must supply legally held Packs outside the package. Run with "
            "--school-days <owned game folder> and --documents <new isolated profile> "
            "--config <new isolated settings.json>.\n"
            "No software publisher authorizes distribution of third-party artwork. "
            "The missing HD launcher/Display Options visuals require separate rights.\n"
            "The unmodified Noto Sans JP font is SIL OFL 1.1, see THIRD-PARTY-LICENSES. "
            "Japanese coverage checked; simplified Chinese is not fully covered.\n"
            "The engine links FFmpeg and other third-party libraries statically. LGPL "
            "source/relinking compliance and further licensing review remain OPEN.\n"
            "Windows MSVC 2015-2022 x64 runtime is an external system dependency.\n"
            "Do not confuse a headless synthetic smoke with visual/retail QA.\n",
            "private local preview instructions")
    incomplete.unlink()
    guard_payload(destination)
    manifest = {
        "schema": 1,
        "type": "PRIVATE-LOCAL-QA-NOT-PUBLIC-DISTRIBUTABLE",
        "pinned_binary_commit": COMMIT,
        "profile": args.profile,
        "font_source_sha256": FONT_SHA256,
        "license_text_sha256": OFL_SHA256,
        "assets_commercial_retail": False,
        "public_release_gate": "NO-GO - retail route, artwork, static FFmpeg/LGPL, visual QA",
        "files": sorted(st.entries, key=lambda e: str(e["path"])),
    }
    raw = json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    (destination / "manifest.json").write_text(raw, encoding="utf-8", newline="\n")
    digest = sha256(destination / "manifest.json")
    (destination / "manifest.sha256").write_text(digest + "  manifest.json\n",
                                                   encoding="ascii", newline="\n")
    guard_payload(destination)
    zip_hash = deterministic_zip(destination, archive)
    print(json.dumps({"profile": args.profile, "destination": str(destination),
                      "zip": str(archive), "archive_sha256": zip_hash,
                      "manifest_sha256": digest, "files": len(st.entries) + 2},
                     ensure_ascii=False, sort_keys=True))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True, choices=sorted(EXE_SHA256))
    parser.add_argument("--source-release", type=Path, required=True)
    parser.add_argument("--source-repo", type=Path, required=True)
    parser.add_argument("--vcpkg-share", type=Path, required=True)
    parser.add_argument("--font", type=Path, required=True,
                        help="Known-SHA original NotoSansJP, not a Windows proprietary font")
    parser.add_argument("--ofl", type=Path, default=Path(__file__).parent
                        / "licensing" / "SIL-OFL-1.1.txt")
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--private-local-preview", action="store_true",
                        help="Explicitly acknowledge no public distribution rights")
    args = parser.parse_args()
    try:
        stage(args)
    except (OSError, ValueError, struct.error) as exc:
        parser.exit(1, f"STAGING BLOCKED: {exc}\n")


if __name__ == "__main__":
    main()
