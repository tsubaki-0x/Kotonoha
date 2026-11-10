"""Stage a privately owned LxH patch in an existing School Days Windows L4.

No proprietary content is bundled with this tool. This only copies 7 user-
supplied ending scripts and their 21 media assets into the existing local
Display Options overlay. Retail GPKs, saves and other overlay files stay intact.
Run again with --disable to remove only files installed by this tool.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path

ENDINGS = (
    "05-A2-F00",
    "05-A2-Z00",
    "05-B0-F00",
    "05-G0-L03",
    "05-KC-F00",
    "05-SB-E01",
    "05-SC-L03",
)
MANIFEST = ".school-days-lxh-local-manifest.json"


def paths() -> tuple[str, ...]:
    entries: list[str] = []
    for ending in ENDINGS:
        entries.extend(
            (
                f"Script/English/05/{ending}.ENG.ORS",
                f"LxH/{ending}-LXH.png",
                f"LxH/Media/{ending}-LXH.wmv",
                f"LxH/Media/{ending}-LXH.ogg",
            )
        )
    return tuple(entries)


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            sha.update(chunk)
    return sha.hexdigest()


def check_release(release: Path) -> Path:
    if not (release / "SchoolDays.exe").is_file():
        raise RuntimeError("Not a SchoolDays.exe Release directory: " + str(release))
    if not (release / "Packs/Script.GPK").is_file():
        raise RuntimeError("School Days retail Packs are missing beside L4")
    target = release / "display-options"
    for name in ("Option_Display.png", "Option_Display.cmap"):
        if not (target / "System/Option" / name).is_file():
            raise RuntimeError("L4 Display Options overlay is incomplete: " + name)
    return target


def read_manifest(target: Path) -> dict[str, str] | None:
    file = target / MANIFEST
    if not file.exists():
        return None
    data = json.loads(file.read_text(encoding="utf-8"))
    if (data.get("format") != 1 or
            set(data.get("files", {})) != set(paths()) or
            not all(isinstance(v, str) and len(v) == 64
                    for v in data["files"].values())):
        raise RuntimeError("Unrecognized local LxH manifest; refusing changes")
    return data["files"]


def check_installed(target: Path, files: dict[str, str]) -> None:
    for name in paths():
        file = target / name
        if not file.is_file() or digest(file) != files[name]:
            raise RuntimeError("Installed LxH file changed or missing: " + name)


def enable(target: Path, source: Path) -> None:
    old = read_manifest(target)
    if old is not None:
        check_installed(target, old)
        print("LxH already enabled; 28/28 files verified")
        return
    expected: dict[str, str] = {}
    for name in paths():
        file = source / name
        if not file.is_file() or file.stat().st_size < 8:
            raise RuntimeError("Private LxH patch is missing file: " + name)
        target_file = target / name
        if target_file.exists():
            raise RuntimeError("Refusing to overwrite unmanaged asset: " + str(target_file))
        expected[name] = digest(file)

    copied: list[Path] = []
    try:
        for name in paths():
            destination = target / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            pending = destination.with_name(destination.name + ".lxh-copying")
            if pending.exists():
                raise RuntimeError("Unmanaged LxH staging file exists: " + str(pending))
            shutil.copyfile(source / name, pending)
            if digest(pending) != expected[name]:
                pending.unlink()
                raise RuntimeError("LxH copy failed integrity validation: " + name)
            os.replace(pending, destination)
            copied.append(destination)
        record = target / MANIFEST
        pending_record = target / (MANIFEST + ".tmp")
        if pending_record.exists() or record.exists():
            raise RuntimeError("Local LxH manifest staging collision")
        pending_record.write_text(
            json.dumps({"format": 1, "files": expected},
                       indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        os.replace(pending_record, record)
    except Exception:
        for file in reversed(copied):
            if file.is_file() and digest(file) == expected[file.relative_to(target).as_posix()]:
                file.unlink()
        raise
    check_installed(target, expected)
    print("LxH enabled in L4: 7 ORS + 7 PNG + 7 WMV + 7 OGG verified")
    print("Display Options, GPKs and Save were not modified")


def disable(target: Path) -> None:
    files = read_manifest(target)
    if files is None:
        print("LxH already disabled; no managed files present")
        return
    check_installed(target, files)
    for name in paths():
        (target / name).unlink()
    for name in ("LxH/Media", "LxH", "Script/English/05",
                 "Script/English", "Script"):
        try:
            (target / name).rmdir()
        except OSError:
            pass
    (target / MANIFEST).unlink()
    print("LxH disabled in L4; original English GPK scripts are active again")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", required=True, type=Path,
                        help="L4 games/school-days/Release folder")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--enable", metavar="PRIVATE_OVERLAY", type=Path)
    group.add_argument("--disable", action="store_true")
    group.add_argument("--status", action="store_true")
    args = parser.parse_args()
    target = check_release(args.release.resolve())
    if args.enable:
        enable(target, args.enable.resolve())
    elif args.disable:
        disable(target)
    else:
        manifest = read_manifest(target)
        if manifest is None:
            print("LxH: OFF (no local patch installed)")
        else:
            check_installed(target, manifest)
            print("LxH: ON (7 ORS + 21 media files verified)")


if __name__ == "__main__":
    main()
