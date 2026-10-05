#!/usr/bin/env python3
"""Safety checks shared by archived full-Debug campaign harnesses."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import stat


class ArchiveHarnessError(RuntimeError):
    """A prepared Debug archive is incomplete or unsafe to use."""


def inside(path: Path, root: Path) -> bool:
    """Return whether an existing path resolves below an existing root."""
    try:
        path.resolve(strict=True).relative_to(root.resolve(strict=True))
        return True
    except (FileNotFoundError, ValueError):
        return False


def prepare_clean_build(build: Path, build_root: Path, *, force: bool) -> None:
    """Require a fresh, specifically scoped build directory.

    Reusing a CMake build can make an interrupted or permission-damaged output
    look up to date.  A forced refresh therefore removes the exact campaign
    build, after proving it is a direct child of the repository build root.
    """
    resolved_root = build_root.resolve(strict=True)
    resolved_build = build.resolve(strict=False)
    if resolved_build.parent != resolved_root:
        raise ArchiveHarnessError(f"build is not a direct child of {resolved_root}: {resolved_build}")
    if build.is_symlink():
        raise ArchiveHarnessError(f"refusing symlink build directory: {build}")
    if build.exists():
        if not force:
            raise ArchiveHarnessError(f"build exists; pass --force to replace it: {build}")
        if not build.is_dir():
            raise ArchiveHarnessError(f"build path is not a directory: {build}")
        shutil.rmtree(build)
    build.mkdir(parents=False)


def validate_test_commands(inventory: dict, build: Path) -> list[Path]:
    """Require every build-local CTest command to be a real executable.

    CTest's JSON inventory accepts zero-byte and non-executable paths.  Hashing
    that inventory can consequently freeze a broken build.  This check runs
    after building, while finalizing, and immediately before the Slurm test.
    """
    tests = inventory.get("tests")
    if not isinstance(tests, list) or not tests:
        raise ArchiveHarnessError("CTest inventory has no tests")
    checked: list[Path] = []
    seen: set[Path] = set()
    for test in tests:
        name = test.get("name", "<unnamed>")
        command = test.get("command")
        if not isinstance(command, list) or not command:
            raise ArchiveHarnessError(f"CTest test has no command: {name}")
        executable = Path(command[0])
        if not executable.is_absolute() or not inside(executable, build):
            continue
        resolved = executable.resolve(strict=True)
        mode = resolved.stat().st_mode
        if not stat.S_ISREG(mode):
            raise ArchiveHarnessError(f"CTest command is not a regular file: {name}: {resolved}")
        if resolved.stat().st_size <= 0:
            raise ArchiveHarnessError(f"CTest command is empty: {name}: {resolved}")
        if mode & 0o111 == 0 or not os.access(resolved, os.X_OK):
            raise ArchiveHarnessError(f"CTest command is not executable: {name}: {resolved}")
        if resolved not in seen:
            seen.add(resolved)
            checked.append(resolved)
    if not checked:
        raise ArchiveHarnessError("CTest inventory has no build-local executable commands")
    return checked
