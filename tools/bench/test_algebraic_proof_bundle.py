#!/usr/bin/env python3
"""Positive and fail-closed controls for the algebraic proof-bundle checker."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tools/bench/check_algebraic_proof_bundle.py"
FIXTURE = ROOT / "benchmarks/reports/2026-10-17-xor-lemma-mixed-composition"
CMS_PINS = ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-anon/pins"
CHECKER_BINS = ROOT / "tools/checkers/bin"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(bundle):
    return [
        sys.executable, str(CHECKER), "--bundle", str(bundle),
        "--frat-xor", str(CMS_PINS / "frat_xor"),
        "--cake-xlrup", str(CMS_PINS / "cake_xlrup"),
        "--proof-oracle", str(ROOT / "tools/bench/proof_oracle.py"),
        "--dsr-trim", str(CHECKER_BINS / "dsr-trim"),
        "--dsr-compress", str(CHECKER_BINS / "dsr-compress"),
        "--srcheck", str(CHECKER_BINS / "srcheck"),
        "--timeout", "30", "--jobs", "2",
    ]


def run(bundle):
    return subprocess.run(command(bundle), capture_output=True, text=True,
                          timeout=60)


class AlgebraicProofBundleControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="algebraic-bundle-test-",
                                                dir="/tmp")
        self.work = Path(self.temp.name)
        for name in ("original.cnf", "source.cnf", "lemmas.txt",
                     "challenge.frat", "consumer.dsr", "proof-bundle.json"):
            shutil.copyfile(FIXTURE / name, self.work / name)

    def tearDown(self):
        self.temp.cleanup()

    @property
    def bundle_path(self):
        return self.work / "proof-bundle.json"

    def manifest(self):
        return json.loads(self.bundle_path.read_text())

    def write_manifest(self, value):
        self.bundle_path.write_text(json.dumps(value, indent=2,
                                               sort_keys=True) + "\n")

    def assert_rejected(self):
        checked = run(self.bundle_path)
        self.assertNotEqual(checked.returncode, 0, checked.stdout + checked.stderr)
        self.assertNotIn("s VERIFIED TACTIX ALGEBRAIC PROOF BUNDLE",
                         checked.stdout)

    def test_positive_real_backends(self):
        checked = run(self.bundle_path)
        self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
        self.assertEqual(checked.stdout.strip(),
                         "s VERIFIED TACTIX ALGEBRAIC PROOF BUNDLE")

    def test_changed_lemma_with_updated_digest_is_rejected(self):
        path = self.work / "lemmas.txt"
        text = path.read_text()
        self.assertIn("1 2 5 -6 0", text)
        path.write_text(text.replace("1 2 5 -6 0", "1 2 5 6 0"))
        manifest = self.manifest()
        manifest["lemmas"]["sha256"] = sha(path)
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_changed_consumer_with_updated_digest_is_rejected(self):
        path = self.work / "consumer.dsr"
        path.write_text("not a proof\n")
        manifest = self.manifest()
        manifest["consumer"]["certificate"]["sha256"] = sha(path)
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_source_outside_original_with_updated_digest_is_rejected(self):
        path = self.work / "source.cnf"
        text = path.read_text()
        self.assertIn("-1 2 3 4 0", text)
        path.write_text(text.replace("-1 2 3 4 0", "1 2 3 4 0"))
        manifest = self.manifest()
        manifest["batches"][0]["source"]["sha256"] = sha(path)
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_wrong_digest_is_rejected(self):
        manifest = self.manifest()
        manifest["original"]["sha256"] = "0" * 64
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_missing_or_duplicate_lemma_ownership_is_rejected(self):
        manifest = self.manifest()
        manifest["batches"][0]["lemma_ids"] = [1]
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_path_traversal_is_rejected(self):
        manifest = self.manifest()
        manifest["original"]["path"] = "../original.cnf"
        self.write_manifest(manifest)
        self.assert_rejected()

    def test_symbolic_link_is_rejected(self):
        target = self.work / "original.cnf"
        link = self.work / "linked.cnf"
        link.symlink_to(target.name)
        manifest = self.manifest()
        manifest["original"] = {"path": link.name, "sha256": sha(target)}
        self.write_manifest(manifest)
        self.assert_rejected()


if __name__ == "__main__":
    unittest.main()
