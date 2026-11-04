"""Conservative Qt package pruning and size reports (Python standard library only)."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile


LANGUAGES = {"en", "en-us", "zh-cn", "zh-tw"}


def files(root):
    # Do not follow framework links or count their targets twice on macOS.
    return sorted(p for p in root.rglob("*") if p.is_file() and not p.is_symlink())


def removable(path, root):
    relative = path.relative_to(root)
    parts = [part.lower() for part in relative.parts]
    if "qmltooling" in parts:
        return True
    if path.suffix.lower() == ".pdb" or any(p.endswith(".dsym") for p in parts):
        return True
    if path.suffix.lower() == ".qm":
        # Qt modules have compound names, e.g. qt_help_zh_CN.qm.
        return re.search(r"_(?:en(?:_US)?|zh_CN|zh_TW)$", path.stem, re.I) is None
    if path.suffix.lower() == ".pak" and "qtwebengine_locales" in parts:
        return path.stem.lower().replace("_", "-") not in LANGUAGES
    if path.name == "locale.pak":
        locales = [p[:-6].replace("_", "-") for p in parts if p.endswith(".lproj")]
        return bool(locales) and locales[-1] not in LANGUAGES
    return False


def prune(root):
    before = sum(p.stat().st_size for p in files(root))
    removed = []
    for path in files(root):
        if removable(path, root):
            removed.append({"path": path.relative_to(root).as_posix(), "bytes": path.stat().st_size})
            path.unlink()
    return {"before_bytes": before, "after_bytes": sum(p.stat().st_size for p in files(root)),
            "removed": removed}


def fetch_core(root, platform, architecture, nightly):
    release = "download/nightly" if nightly else "latest/download"
    url = f"https://github.com/Mythologyli/zju-connect/releases/{release}/zju-connect-{platform}-{architecture}.zip"
    name = "zju-connect.exe" if platform == "windows" else "zju-connect"
    # Extract one named executable, never arbitrary ZIP paths or bundled extras.
    with tempfile.TemporaryDirectory(prefix="shou-core-") as directory:
        archive = Path(directory) / "core.zip"
        urllib.request.urlretrieve(url, archive)
        with zipfile.ZipFile(archive) as bundle:
            matches = [entry for entry in bundle.infolist()
                       if not entry.is_dir() and Path(entry.filename).name == name]
            if len(matches) != 1:
                raise RuntimeError(f"Expected exactly one {name} in {url}")
            with bundle.open(matches[0]) as source, (root / name).open("wb") as target:
                shutil.copyfileobj(source, target)
        (root / name).chmod(0o755)


def report(root, archive=None):
    entries = [{"path": p.relative_to(root).as_posix(), "bytes": p.stat().st_size}
               for p in files(root)]
    groups = {}
    for entry in entries:
        parts = entry["path"].split("/")
        if "translations" in parts or any(p.endswith(".lproj") for p in parts):
            group = "translations"
        elif "qmltooling" in parts:
            group = "qmltooling"
        elif "resources" in [p.lower() for p in parts]:
            group = "resources"
        elif "plugins" in [p.lower() for p in parts]:
            group = "plugins"
        else:
            group = parts[0] if len(parts) > 1 else "root-files"
        groups[group] = groups.get(group, 0) + entry["bytes"]
    cores = [p for p in files(root) if p.name in ("zju-connect", "zju-connect.exe")]
    core = cores[0] if len(cores) == 1 else None
    version = subprocess.run([str(core.resolve()), "-version"], capture_output=True,
                             text=True, timeout=15, check=True).stdout.strip() if core else None
    tools = {}
    for name in ("windeployqt", "macdeployqt", "linuxdeploy", "linuxdeploy-plugin-qt"):
        located = shutil.which(name)
        local = Path(name)
        path = Path(located) if located else local if local.is_file() else None
        if path:
            tools[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return {"root": root.name, "qt_version": os.environ.get("QT_VERSION"),
            "core_version": version,
            "core_sha256": hashlib.sha256(core.read_bytes()).hexdigest() if core else None,
            "deployment_tool_sha256": tools,
            "archive_bytes": archive.stat().st_size if archive else None,
            "total_bytes": sum(e["bytes"] for e in entries), "groups": groups,
            "largest": sorted(entries, key=lambda e: e["bytes"], reverse=True)[:20],
            "files": entries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prune", "report", "fetch-core"))
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--platform", choices=("windows", "darwin", "linux"))
    parser.add_argument("--architecture")
    parser.add_argument("--nightly", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    if not root.is_dir():
        parser.error("root must be a deployment directory")
    if args.action == "fetch-core":
        if not args.platform or not args.architecture:
            parser.error("fetch-core requires platform and architecture")
        fetch_core(root, args.platform, args.architecture, args.nightly)
        return
    data = prune(root) if args.action == "prune" else report(root, args.archive)
    serialized = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(serialized, encoding="utf-8")
    print(json.dumps({k: v for k, v in data.items() if k not in ("files", "removed")}, indent=2))


if __name__ == "__main__":
    main()
