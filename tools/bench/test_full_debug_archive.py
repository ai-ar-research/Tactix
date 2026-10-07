#!/usr/bin/env python3
"""Focused non-solving tests for the archived full-Debug harness."""

import json
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from full_debug_archive import ArchiveHarnessError, prepare_clean_build, validate_test_commands
from full_debug_archive_campaign import packaged_dependency_paths
import full_debug_archive_campaign as campaign


class FullDebugArchiveTests(unittest.TestCase):
    def test_reference_bundle_includes_sources_but_excludes_build_products(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            names = ["cadical/src/options.hpp", "cadical/src/probe.cpp",
                     "kissat/src/options.h", "kissat/src/probe.c"]
            for name in names + ["cadical/src/probe.o", "kissat/src/kissat"]:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name + "\n")
            source = root / "archive"
            with patch.object(campaign, "ROOT", root):
                copied = campaign.copy_reference_sources(source)
            self.assertEqual(sorted(copied), sorted(names))
            for name in names:
                self.assertEqual((source / name).read_bytes(), (root / name).read_bytes())
            self.assertFalse((source / "cadical/src/probe.o").exists())
            self.assertFalse((source / "kissat/src/kissat").exists())

    def test_reference_bundle_rejects_missing_required_inventory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            header = root / "cadical/src/options.hpp"
            header.parent.mkdir(parents=True)
            header.write_text("reference\n")
            with patch.object(campaign, "ROOT", root):
                with self.assertRaisesRegex(ArchiveHarnessError, "kissat/src/options.h"):
                    campaign.copy_reference_sources(root / "archive")
            self.assertFalse((root / "archive").exists())

    def make_pin(self, root):
        archive = root / "source.tar"
        payload = b"committed source\n"
        with tarfile.open(archive, "w") as stream:
            entry = tarfile.TarInfo("src/example.cpp")
            entry.size = len(payload)
            stream.addfile(entry, io.BytesIO(payload))
        pin = root / "PIN.json"
        data = {"schema": "tactix-campaign-pin-v1", "source_commit": "fixed-commit",
                "source_archive": "source.tar", "source_archive_sha256": campaign.sha_file(archive),
                "source_sha256": {"src/example.cpp": campaign.sha_bytes(payload)}}
        pin.write_text(json.dumps(data))
        return pin, data

    def test_pin_uses_archive_without_reading_changed_worktree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            pin, data = self.make_pin(root)
            (root / "src").mkdir()
            (root / "src/example.cpp").write_text("unfinished development\n")
            destination = root / "snapshot"
            destination.mkdir()
            with patch.object(campaign, "ROOT", root), patch.object(campaign, "copy_dependencies", return_value=[]) as dependencies:
                metadata, packaged = campaign.pinned_source(pin, destination)
            self.assertEqual((destination / "src/example.cpp").read_bytes(), b"committed source\n")
            self.assertEqual(metadata["source_head"], data["source_commit"])
            self.assertEqual(metadata["source_authority"]["manifest_sha256"], campaign.sha_file(pin))
            self.assertEqual(packaged, [])
            dependencies.assert_called_once_with(destination)

    def test_pin_rejects_changed_archive_before_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            pin, data = self.make_pin(root)
            with (root / "source.tar").open("ab") as archive:
                archive.write(b"changed")
            destination = root / "snapshot"
            destination.mkdir()
            with patch.object(campaign, "ROOT", root), patch.object(campaign, "copy_dependencies") as dependencies:
                with self.assertRaisesRegex(ArchiveHarnessError, "archive changed"):
                    campaign.pinned_source(pin, destination)
                dependencies.assert_not_called()

    def test_pin_rejects_source_hash_mismatch_before_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            pin, data = self.make_pin(root)
            data["source_sha256"]["src/example.cpp"] = "0" * 64
            pin.write_text(json.dumps(data))
            destination = root / "snapshot"
            destination.mkdir()
            with patch.object(campaign, "ROOT", root), patch.object(campaign, "copy_dependencies") as dependencies:
                with self.assertRaisesRegex(ArchiveHarnessError, "input changed"):
                    campaign.pinned_source(pin, destination)
                dependencies.assert_not_called()

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
