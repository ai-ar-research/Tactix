#!/usr/bin/env python3
"""Fixed metadata controls for frozen checker selection; executes no solver."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import fuzz


class CheckerCollectionControls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="checker-collection-")
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.expected = {}
        self.records = []
        for name in fuzz.CORE_CHECKERS:
            data = ("metadata fixture: " + name + "\n").encode()
            (self.directory / name).write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            self.expected[name] = digest
            self.records.append(digest + "  " + name + "\n")
        self.manifest = self.directory / "SHA256SUMS"
        self.manifest.write_text("".join(self.records))
        for attr, value in (("CHECKER_DIR", str(self.directory)),
                            ("CHECKER_MANIFEST", str(self.manifest))):
            change = patch.object(fuzz, attr, value)
            change.start()
            self.addCleanup(change.stop)

    def test_valid_collection_matches_all_six_digests(self):
        self.assertEqual(fuzz.verify_checker_collection(), self.expected)

    def test_duplicate_name_is_rejected_even_with_matching_digest(self):
        self.manifest.write_text("".join(self.records) + self.records[0])
        with self.assertRaisesRegex(SystemExit, "duplicate checker"):
            fuzz.verify_checker_collection()

    def test_changed_binary_is_rejected(self):
        (self.directory / fuzz.CORE_CHECKERS[0]).write_bytes(b"changed")
        with self.assertRaisesRegex(SystemExit, "has sha256"):
            fuzz.verify_checker_collection()

    def test_missing_manifest_member_is_rejected(self):
        self.manifest.write_text("".join(self.records[:-1]))
        with self.assertRaisesRegex(SystemExit, "expected"):
            fuzz.verify_checker_collection()

    def test_symlink_is_rejected_even_when_target_digest_matches(self):
        member = self.directory / fuzz.CORE_CHECKERS[0]
        target = self.directory / "target"
        member.rename(target)
        member.symlink_to(target)
        with self.assertRaisesRegex(SystemExit, "not a regular file"):
            fuzz.verify_checker_collection()


if __name__ == "__main__":
    unittest.main()
