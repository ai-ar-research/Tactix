#!/usr/bin/env python3
"""Exercise the actual timing-summary script against v1/v2 record boundaries."""
import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
real = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'

class SummarySchemaTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sort-summary-fixture-', dir=root / 'benchmarks/probes/scratch')
        self.base = Path(self.tmp.name)
        self.c = self.base / real.relative_to(root)
        (self.c / 'provisional').mkdir(parents=True)
        (self.c / 'results').mkdir()
        self.manifest = json.loads((real / 'manifest.json').read_text())
        self.manifest['pin_sha256'] = {} # pin integrity is covered by live sealing, not this schema fixture
        old = root / self.manifest['predecessor_rank4_record']['path']
        target = self.base / self.manifest['predecessor_rank4_record']['path']
        target.parent.mkdir(parents=True)
        target.write_bytes(old.read_bytes())
        assert sha(target) == self.manifest['predecessor_rank4_record']['sha256']
        (self.c / 'manifest.json').write_text(json.dumps(self.manifest))
        (self.c / 'matrix.tsv').write_bytes((real / 'matrix.tsv').read_bytes())
        self.final = [json.loads((real / f'results/{i:03d}.json').read_text()) for i in range(6)]
        self.provisional = [json.loads((real / f'provisional/{i:03d}.json').read_text()) for i in range(6)]
        source = Path(__file__).with_name('seal-learned-sort-timing-6178.py')
        code = source.read_text()
        anchor = 'root = Path(__file__).resolve().parents[3]'
        assert code.count(anchor) == 1
        self.code = compile(code.replace(anchor, f'root = Path({str(self.base)!r})'), str(source), 'exec')

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, final_count=6):
        for i in range(6):
            (self.c / f'provisional/{i:03d}.json').write_text(json.dumps(self.provisional[i]))
            if i < final_count:
                (self.c / f'results/{i:03d}.json').write_text(json.dumps(self.final[i]))

    def run_summary(self):
        with contextlib.redirect_stdout(io.StringIO()):
            exec(self.code, {'__file__': __file__, '__name__': '__main__'})
        p = self.c / ('summary.json' if (self.c / 'summary.json').exists() else 'solver-checkpoint.json')
        return json.loads(p.read_text())

    def test_all_final(self):
        self.write(); self.assertEqual(self.run_summary()['checked_clean'], 6)

    def test_provisional_omits_final_identity(self):
        self.assertTrue(all('binary_sha256' not in r and 'command' not in r and 'cpu_affinity' not in r for r in self.provisional))
        self.write(0); s = self.run_summary()
        self.assertEqual(s['checked_clean'], 0)
        self.assertIsNone(s['waves'][0]['placement']['candidate']['affinity'])

    def test_mixed_records(self):
        self.write(3); self.assertEqual(self.run_summary()['checked_clean'], 3)

    def test_missing_final_digest_rejected(self):
        del self.final[0]['binary_sha256']; self.write()
        with self.assertRaises(KeyError): self.run_summary()

    def test_wrong_final_digest_rejected(self):
        self.final[0]['binary_sha256'] = 'wrong'; self.write()
        with self.assertRaises(AssertionError): self.run_summary()

    def test_changed_proof_rejected(self):
        self.provisional[0]['certificate_sha256'] = 'wrong'; self.write(0)
        with self.assertRaises(AssertionError): self.run_summary()

    def test_rejected_checker_not_provisional_pass(self):
        self.final[0]['proof_check']['result'] = 'rejected'; self.write()
        with self.assertRaises(AssertionError): self.run_summary()

if __name__ == '__main__': unittest.main()
