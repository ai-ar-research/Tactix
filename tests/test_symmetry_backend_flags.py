#!/usr/bin/env python3
"""Keep DejaVu's exceptions and RTTI outside the Tactix solver core."""

import json
from pathlib import Path
import shlex
import sys


compile_commands = Path(sys.argv[1])
source_root = Path(sys.argv[2])
entries = json.loads(compile_commands.read_text())


def command_for(suffix: str) -> list[str]:
    matches = [entry for entry in entries if entry["file"].endswith(suffix)]
    assert len(matches) == 1, (suffix, len(matches))
    return shlex.split(matches[0]["command"])


def last_toggle(command: list[str], enabled: str, disabled: str) -> str:
    toggles = [arg for arg in command if arg in (enabled, disabled)]
    assert toggles, (enabled, disabled, command)
    return toggles[-1]


backend = command_for("src/symmetry_backend/dejavu_backend.cpp")
core = command_for("src/rules/substitution_redundancy/symmetry.cpp")
assert last_toggle(backend, "-fexceptions", "-fno-exceptions") == "-fexceptions"
assert last_toggle(backend, "-frtti", "-fno-rtti") == "-frtti"
assert "-fvisibility=hidden" in backend
assert last_toggle(core, "-fexceptions", "-fno-exceptions") == "-fno-exceptions"
assert last_toggle(core, "-frtti", "-fno-rtti") == "-fno-rtti"

dejavu_includes = []
for path in (source_root / "src").rglob("*.[ch]pp"):
    if path.name == "dejavu_backend.cpp":
        continue
    if '"dejavu.h"' in path.read_text(errors="replace"):
        dejavu_includes.append(str(path.relative_to(source_root)))
assert not dejavu_includes, dejavu_includes
print("DejaVu exception/RTTI boundary PASS")
