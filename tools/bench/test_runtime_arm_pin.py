"""Regression for an arm wrapper hash that disagrees with its file pin."""
import hashlib
from pathlib import Path
import tempfile
import unittest

from runtime_task import validate_runtime_pin_contract


class RuntimeArmPinTests(unittest.TestCase):
    def test_rejects_stale_arm_metadata_before_solver_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            pins = campaign / 'pins'
            pins.mkdir()
            for name in ('solver', 'checkmodel', 'modelcheck.py'):
                (pins / name).write_text(name)
            hashes = {f'pins/{path.name}': hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in pins.iterdir()}
            arm = {'path': 'pins/solver', 'certificate': 'none',
                   'sha256': '0' * 64}
            manifest = {'arms': {'candidate': arm}, 'pin_sha256': hashes}
            with self.assertRaisesRegex(RuntimeError,
                                        'runtime arm pin mismatch: candidate'):
                validate_runtime_pin_contract(campaign, manifest)
            arm['sha256'] = hashes[arm['path']]
            validate_runtime_pin_contract(campaign, manifest)


if __name__ == '__main__':
    unittest.main()
