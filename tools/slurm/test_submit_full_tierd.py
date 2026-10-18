import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent))
from submit_full_tierd import array_indices, array_partition


class ArrayAdmissionTest(unittest.TestCase):
    def test_full_and_declared_repair(self):
        self.assertEqual(array_indices("#SBATCH --array=0-1165%8\n", {}),
                         list(range(1166)))
        repair = {"repair_of": {"missing_indices": [12, 13, 15, 41]}}
        self.assertEqual(array_indices("#SBATCH --array=12-13,15,41%8\n", repair),
                         [12, 13, 15, 41])

    def test_partial_or_mismatched_array_cannot_bypass_monitor_contract(self):
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12-13\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165%0\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=0-1165%126\n", {})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12-13,15%8\n",
                          {"repair_of": {"missing_indices": [12, 13, 15, 41]}})
        with self.assertRaises(RuntimeError):
            array_indices("#SBATCH --array=12,12%8\n",
                          {"repair_of": {"missing_indices": [12]}})

    def test_prepared_continuation_can_be_a_strict_repair_subset(self):
        repair = {"repair_of": {"missing_indices": [12, 13, 15, 41]}}
        self.assertEqual(
            array_indices("#SBATCH --array=13,41%125\n", repair, [13, 41]),
            [13, 41])
        with self.assertRaisesRegex(RuntimeError, "prepared missing rows"):
            array_indices("#SBATCH --array=13,15,41%125\n", repair, [13, 41])

    def test_cpu_q_is_mandatory(self):
        self.assertEqual(array_partition("#SBATCH --partition=cpu-q\n"),
                         "cpu-q")
        with self.assertRaisesRegex(RuntimeError, "must use cpu-q"):
            array_partition("#SBATCH --partition=hwu\n")


if __name__ == "__main__":
    unittest.main()
