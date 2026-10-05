#!/usr/bin/env python3
"""Focused non-solving tests for the archived full-Debug harness."""

import json
from pathlib import Path
import tempfile
import unittest

from full_debug_archive import ArchiveHarnessError, prepare_clean_build, validate_test_commands
from full_debug_archive_campaign import packaged_dependency_paths


class FullDebugArchiveTests(unittest.TestCase):
    def inventory(self, executable: Path) -> dict:
        return {"tests": [{"name": "sample", "command": [str(executable)]}]}

    def test_rejects_empty_and_non_executable_test_commands(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary) / "build"
            build.mkdir()
            executable = build / "test_sample"
            executable.write_bytes(b"")
            executable.chmod(0o750)
            with self.assertRaisesRegex(ArchiveHarnessError, "empty"):
                validate_test_commands(self.inventory(executable), build)
            executable.write_bytes(b"binary")
            executable.chmod(0o640)
            with self.assertRaisesRegex(ArchiveHarnessError, "not executable"):
                validate_test_commands(self.inventory(executable), build)

    def test_accepts_nonempty_executable_and_returns_binding(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary) / "build"
            build.mkdir()
            executable = build / "test_sample"
            executable.write_bytes(b"binary")
            executable.chmod(0o750)
            self.assertEqual(validate_test_commands(self.inventory(executable), build), [executable])

    def test_clean_build_rejects_reuse_without_force(self):
        with tempfile.TemporaryDirectory() as temporary:
            build_root = Path(temporary) / "build"
            build_root.mkdir()
            build = build_root / "campaign"
            build.mkdir()
            marker = build / "stale"
            marker.write_text(json.dumps({"broken": True}))
            with self.assertRaisesRegex(ArchiveHarnessError, "build exists"):
                prepare_clean_build(build, build_root, force=False)
            prepare_clean_build(build, build_root, force=True)
            self.assertTrue(build.is_dir())
            self.assertFalse(marker.exists())

    def test_clean_build_rejects_broad_or_symlink_target(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build_root = root / "build"
            build_root.mkdir()
            with self.assertRaisesRegex(ArchiveHarnessError, "direct child"):
                prepare_clean_build(root / "elsewhere", build_root, force=True)
            real = build_root / "real"
            real.mkdir()
            alias = build_root / "alias"
            alias.symlink_to(real)
            with self.assertRaisesRegex(ArchiveHarnessError, "symlink"):
                prepare_clean_build(alias, build_root, force=True)

    def test_packaged_dependencies_are_unique_files_inside_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            dependency = source / "pins/checker"
            dependency.parent.mkdir(parents=True)
            dependency.write_bytes(b"checker")
            self.assertEqual(packaged_dependency_paths(source, ["pins/checker"]),
                             {dependency})
            with self.assertRaisesRegex(ArchiveHarnessError, "duplicate"):
                packaged_dependency_paths(source, ["pins/checker", "pins/checker"])
            with self.assertRaisesRegex(ArchiveHarnessError, "invalid"):
                packaged_dependency_paths(source, ["pins/missing"])
            outside = root / "outside"
            outside.write_bytes(b"outside")
            (source / "pins/escape").symlink_to(outside)
            with self.assertRaisesRegex(ArchiveHarnessError, "invalid"):
                packaged_dependency_paths(source, ["pins/escape"])


if __name__ == "__main__":
    unittest.main()
