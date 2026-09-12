Import("env")

import os
import re
import subprocess
from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
generated_dir = build_dir / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)
header = generated_dir / "build_info.h"

def git(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=project_dir, stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return ""

tag = git("describe", "--tags", "--abbrev=0")
sha = git("rev-parse", "--short=8", "HEAD") or "unknown"
count = git("rev-list", "--count", "HEAD") or "0"

# Semantic version comes from the latest Git tag (for example v1.2.0).
# If no semantic-version tag exists, use a safe development version.
m = re.fullmatch(r"v?(\\d+)\\.(\\d+)\\.(\\d+)", tag)
version = f"{m.group(1)}.{m.group(2)}.{m.group(3)}" if m else "0.0.0-dev"

header.write_text(
    "#pragma once\\n"
    f'#define BUILD_VERSION "{version}"\\n'
    f'#define BUILD_GIT_SHA "{sha}"\\n'
    f'#define BUILD_NUMBER "{count}"\\n',
    encoding="utf-8"
)

env.Append(CPPDEFINES=[("BUILD_VERSION", '"%s"' % version), ("BUILD_GIT_SHA", '"%s"' % sha), ("BUILD_NUMBER", '"%s"' % count)])
env.Append(CPPPATH=[str(generated_dir)])
print(f"[BUILD INFO] version={version} git={sha} build={count}")
