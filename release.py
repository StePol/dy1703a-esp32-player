#!/usr/bin/env python3
"""Create the next semantic-version Git tag without remembering the current version."""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SEMVER = re.compile(r"^v?(\d+)\.(\d+)\.(\d+)$")


def git(*args, check=True):
    result = subprocess.run(
        ["git", *args],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        message = result.stderr.strip() or result.stdout.strip() or "Git command failed."
        raise SystemExit(message)
    return result.stdout.strip()


def latest_version():
    tags = git("tag", "--list", "v[0-9]*.[0-9]*.[0-9]*", "--sort=-version:refname").splitlines()
    for tag in tags:
        match = SEMVER.fullmatch(tag.strip())
        if match:
            return tag.strip(), tuple(map(int, match.groups()))
    return None, (0, 0, 0)


def main():
    bump = sys.argv[1].lower() if len(sys.argv) > 1 else "patch"
    if bump not in {"patch", "minor", "major"}:
        raise SystemExit("Použitie: python release.py [patch|minor|major]")

    if git("status", "--porcelain"):
        raise SystemExit("Pracovný strom nie je čistý. Najprv commitni alebo odlož zmeny.")

    git("fetch", "--tags")
    old_tag, (major, minor, patch) = latest_version()

    if bump == "major":
        major, minor, patch = major + 1, 0, 0
    elif bump == "minor":
        minor, patch = minor + 1, 0
    else:
        patch += 1

    new_tag = f"v{major}.{minor}.{patch}"
    print(f"Aktuálna verzia: {old_tag or 'žiadny semantický tag'}")
    print(f"Nová verzia:     {new_tag}")

    answer = input(f"Vytvoriť tag {new_tag} a odoslať ho na origin? [a/N]: ").strip().lower()
    if answer not in {"a", "ano", "á", "áno", "y", "yes"}:
        print("Zrušené.")
        return

    git("tag", "-a", new_tag, "-m", f"Release {new_tag}")
    pushed = subprocess.run(["git", "push", "origin", new_tag], cwd=ROOT)
    if pushed.returncode != 0:
        print(f"Tag {new_tag} bol vytvorený lokálne, ale push zlyhal.", file=sys.stderr)
        print(f"Po oprave spojenia spusti: git push origin {new_tag}", file=sys.stderr)
        raise SystemExit(pushed.returncode)

    print(f"Hotovo: {new_tag}")


if __name__ == "__main__":
    main()
