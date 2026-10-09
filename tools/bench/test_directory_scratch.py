"""DirectoryScratch keeps AnonymousScratch's release contract with named files.

Synthetic files and patched checker stages only; never a SAT benchmark. Runs
in the campaign validation job from the pinned copy as well.
"""
import errno
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import directory_scratch
from directory_scratch import DirectoryScratch


class DirectoryScratchTests(unittest.TestCase):
    def test_named_files_release_and_close(self):
        with tempfile.TemporaryDirectory() as root:
            s = DirectoryScratch(Path(root) / 'scratch', '0001-77')
            p = s / 'proof.out'
            p.write_bytes(b'certificate')
            self.assertEqual(p.parent, Path(root) / 'scratch/0001-77')
            self.assertTrue(p.is_file())
            self.assertTrue(s.contains('proof.out') and s.exists())
            self.assertEqual(s.accounted_bytes() > 0, True)
            self.assertTrue(s.release('proof.out'))
            self.assertFalse(p.exists())
            self.assertFalse(s.release('proof.out'))
            self.assertFalse(s.release('never-opened'))
            with self.assertRaises(RuntimeError):
                s / 'proof.out'
            self.assertFalse(s.exists())
            self.assertTrue(s.directory.is_dir())
            (s / 'leftover').write_text('x')
            (s.tmpdir / 'child-temporary').write_text('x')
            s.close()
            self.assertFalse(s.directory.exists())
            self.assertFalse(s.exists())
            status = s.resource_status()
            self.assertEqual(status['released_bytes'], {'proof.out': 11})
            self.assertTrue(status['removed'])
            self.assertEqual(sorted(p.name for p in (Path(root) / 'scratch').iterdir()), [])

    def test_names_and_roots_are_validated(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):
                DirectoryScratch('relative/root', 'x')
            with self.assertRaises(ValueError):
                DirectoryScratch(root, 'a/b')
            s = DirectoryScratch(root, 'task')
            for bad in ('', 'a/b', '..', 'tmp'):
                with self.assertRaises(ValueError):
                    s / bad
            with self.assertRaises(TypeError):
                s.release(Path(root) / 'task/x')
            # A leftover task directory is never reused.
            with self.assertRaises(FileExistsError):
                DirectoryScratch(root, 'task')
            self.assertEqual(s.fresh_name('a'), 'a')
            self.assertEqual(s.fresh_name('a'), 'a.generation-1')
            s.close()

    def test_close_retries_transient_nfs_errors(self):
        calls = []
        real = directory_scratch.shutil.rmtree

        def flaky(path):
            calls.append(path)
            if len(calls) < 3:
                raise OSError(errno.ENOTEMPTY, 'silly-renamed file')
            real(path)
        with tempfile.TemporaryDirectory() as root:
            s = DirectoryScratch(root, 'task')
            with mock.patch.object(directory_scratch.shutil, 'rmtree', flaky), \
                    mock.patch.object(directory_scratch.time, 'sleep'):
                s.close()
            self.assertEqual(s.cleanup_retries, 2)
            self.assertFalse(s.directory.exists())

    def test_witness_free_route_deletes_files_at_checked_handoffs(self):
        from runtime_task import check_unsat_proof
        with tempfile.TemporaryDirectory() as root:
            s = DirectoryScratch(root, 'task')
            self.addCleanup(s.close)
            cnf, proof = s / 'input.cnf', s / 'proof.out'
            cnf.write_text('p cnf 1 2\n1 0\n-1 0\n')
            proof.write_bytes(b'original-certificate')

            def canonical(source, destination):
                destination.write_bytes(source.read_bytes())
                return dict(first_empty_clause=None, duplicate_literal_clauses=0,
                            clauses=2)

            def convert(source, destination):
                destination.write_bytes(source.read_bytes())
                return True

            def stage(_scratch, name, command, _timeout, produced=None, **_kwargs):
                if name == 'drat-trim':
                    self.assertFalse(cnf.exists())
                    self.assertFalse(proof.exists())
                    self.assertTrue(Path(command[2]).exists())
                    produced.write_bytes(b'elaborated-lrat')
                else:
                    self.assertEqual(name, 'cake_lpr')
                    self.assertFalse((s.directory / 'proof.drat').exists())
                return dict(name=name, result='verified', exit_code=0,
                            timed_out=False, output_tail='verified')
            with mock.patch('runtime_task.canonical_checker_cnf', canonical), \
                    mock.patch('runtime_task.certificate_route', return_value='drat'), \
                    mock.patch('runtime_task.dsr_proof_as_drat', convert), \
                    mock.patch('runtime_task.proof_stage', side_effect=stage):
                result = check_unsat_proof(
                    Path(root), 'auto', cnf, proof, s, 10, 'dsr-cake_lpr',
                    release_intermediates=True, cnf_scratch_name='input.cnf',
                    proof_scratch_name='proof.out')
            self.assertEqual(result['result'], 'verified')
            remaining = sorted(p.name for p in s.directory.iterdir())
            self.assertEqual(remaining, ['tmp'])
            self.assertFalse(s.exists())


if __name__ == '__main__':
    unittest.main()
