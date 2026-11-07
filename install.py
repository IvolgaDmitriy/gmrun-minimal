#!/usr/bin/env python3
"""Install user files and restore their previous versions on uninstall."""

import argparse
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent
HOME = Path.home()
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME") or HOME / ".config")
STATE = Path(os.environ.get("XDG_STATE_HOME") or HOME / ".local/state") / "gmrun-minimal"
MANIFEST = STATE / "installed.json"


def save(records):
    temporary = MANIFEST.with_suffix(".tmp")
    temporary.write_text(json.dumps(records, indent=2) + "\n")
    temporary.replace(MANIFEST)


def install():
    files = [
        (ROOT / "build/appearance.so", HOME / ".local/lib/gmrun-minimal/appearance.so", False),
        (ROOT / "bin/gmrun-minimal", HOME / ".local/bin/gmrun-minimal", False),
        (ROOT / "config/gmrunrc", HOME / ".gmrunrc", True),
        (ROOT / "config/gtkrc", CONFIG / "gmrun-minimal/gtkrc", True),
    ]
    for source, _, _ in files:
        if not source.is_file():
            raise RuntimeError(f"Missing {source}. Run make first.")
    STATE.mkdir(mode=0o700, parents=True, exist_ok=True)
    records = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else []
    known = {record["path"] for record in records}
    for source, target, keep_edits in files:
        if str(target) in known and keep_edits and target.exists():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if str(target) not in known:
            backup = None
            if target.exists() or target.is_symlink():
                if target.is_dir():
                    raise RuntimeError(f"Expected a file: {target}")
                backup = STATE / f"original-{len(records)}"
                shutil.copy2(target, backup, follow_symlinks=False)
            records.append({"path": str(target), "backup": str(backup) if backup else None})
            save(records)
        # Replacing a symlink must not overwrite the file it points to.
        if target.is_symlink():
            target.unlink()
        shutil.copy2(source, target)
        if source.name == "gmrun-minimal":
            target.chmod(0o755)
    print(f"Installed: {HOME / '.local/bin/gmrun-minimal'}")
    print(f"Original files: {STATE}")


def uninstall():
    if not MANIFEST.exists():
        raise RuntimeError("No installation record found for this user.")
    for record in reversed(json.loads(MANIFEST.read_text())):
        target = Path(record["path"])
        if target.exists() or target.is_symlink():
            if target.is_dir():
                raise RuntimeError(f"Expected a file: {target}")
            target.unlink()
        if record["backup"]:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(record["backup"], target, follow_symlinks=False)
    shutil.rmtree(STATE)
    print("Removed installed files and restored original settings.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["install", "uninstall"])
    args = parser.parse_args()
    if os.getuid() == 0:
        parser.error("Run as your desktop user, without sudo.")
    try:
        {"install": install, "uninstall": uninstall}[args.action]()
    except (OSError, ValueError, RuntimeError) as error:
        print(f"gmrun-minimal: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
