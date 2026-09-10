"""Fixed accounting/transport fixtures; never submit or query a real job."""
import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('slurm_wait', Path(__file__).with_name('wait.py'))
wait = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wait)
NOW = '2026-09-09T00:19:08'
OLD = '2026-09-08T00:00:00'
def row(task, state='COMPLETED', submitted=NOW, exit_code='0:0'):
    return f'123_{task}|{state}|{exit_code}|{submitted}\n'

class AccountingTests(unittest.TestCase):
    def test_all_tasks_completed(self):
        self.assertEqual(wait.accounting_state(row(0) + row(1), '123'), 'COMPLETED')
    def test_later_failure_not_hidden(self):
        self.assertEqual(wait.accounting_state(row(0) + row(1, 'FAILED', exit_code='2:0'), '123'), 'FAILED')
    def test_live_accounting_is_not_terminal(self):
        self.assertIsNone(wait.accounting_state(row(0) + row(1, 'RUNNING'), '123'))
    def test_missing_accounting(self):
        self.assertIsNone(wait.accounting_state('', '123'))
    def test_recycled_id_requires_identity(self):
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            wait.accounting_state(row(1, 'FAILED', OLD) + row(0) + row(1), '123')
    def test_bound_rejects_old_incarnation(self):
        self.assertEqual(wait.accounting_state(row(1, 'FAILED', OLD) + row(0) + row(1), '123', NOW), 'COMPLETED')
    def test_duplicate_current_rows_rejected(self):
        with self.assertRaises(ValueError): wait.accounting_state(row(0) + row(0), '123', NOW)
    def test_wrong_job_rejected(self):
        with self.assertRaises(ValueError): wait.accounting_state(row(0), '456')
    def test_malformed_rows_rejected(self):
        for text in ('COMPLETED', '123_0|COMPLETED|0:0|Unknown',
                     '123_0|COMPLETED|0:0|2026-09-09', '123_0||0:0|' + NOW,
                     '123_0.batch|COMPLETED|0:0|' + NOW):
            with self.subTest(text=text), self.assertRaises(ValueError):
                wait.accounting_state(text, '123')
    def test_nonzero_exit_not_success(self):
        self.assertEqual(wait.accounting_state(row(0, exit_code='1:0'), '123'), 'NONZERO_EXIT')
    def test_unrecognized_state_is_unverified(self):
        with self.assertRaisesRegex(ValueError, 'unrecognized'):
            wait.accounting_state(row(0, 'OUT_OF_ME+'), '123')
    def test_transient_error_repolls_same_job(self):
        error = subprocess.CalledProcessError(1, ['squeue'])
        with patch.object(wait, 'output', side_effect=[error, '', row(0)]) as output, patch.object(wait.time, 'sleep') as sleep:
            self.assertEqual(wait.wait_for_job('123', 30, NOW), 'COMPLETED')
        self.assertEqual(output.call_args_list[0], output.call_args_list[1])
        self.assertIn(NOW, output.call_args_list[-1].args)
        sleep.assert_called_once_with(30)
    def test_missing_then_live_then_terminal_accounting(self):
        with patch.object(wait, 'output', side_effect=['', '', '', row(0, 'RUNNING'), '', row(0)]), patch.object(wait.time, 'sleep') as sleep:
            self.assertEqual(wait.wait_for_job('123', 30), 'COMPLETED')
        self.assertEqual(sleep.call_count, 2)
    def test_purged_queue_job_requires_terminal_accounting(self):
        error = subprocess.CalledProcessError(1, ['squeue'], output='',
            stderr='slurm_load_jobs error: Invalid job id specified\n')
        with patch.object(wait, 'output', side_effect=[error, row(0, 'FAILED', exit_code='2:0')]) as output, patch.object(wait.time, 'sleep') as sleep:
            self.assertEqual(wait.wait_for_job('123', 30, NOW), 'FAILED')
        self.assertEqual(output.call_args_list[1].args[0], 'sacct')
        self.assertIn(NOW, output.call_args_list[1].args)
        sleep.assert_not_called()
    def test_purged_queue_job_with_missing_accounting_keeps_waiting(self):
        error = subprocess.CalledProcessError(1, ['squeue'], output='',
            stderr='slurm_load_jobs error: Invalid job id specified')
        with patch.object(wait, 'output', side_effect=[error, '', error, row(0)]), patch.object(wait.time, 'sleep') as sleep:
            self.assertEqual(wait.wait_for_job('123', 30, NOW), 'COMPLETED')
        sleep.assert_called_once_with(30)
    def test_other_queue_errors_do_not_fall_through(self):
        for code, stdout, stderr in (
            (2, '', 'slurm_load_jobs error: Invalid job id specified'),
            (1, '123_0', 'slurm_load_jobs error: Invalid job id specified'),
            (1, '', 'Socket timed out'),
            (1, '', 'slurm_load_jobs error: Invalid job id specified\nSocket timed out'),
        ):
            error = subprocess.CalledProcessError(code, ['squeue'], output=stdout, stderr=stderr)
            with self.subTest(code=code, stdout=stdout, stderr=stderr), patch.object(wait, 'output', side_effect=[error, '', row(0)]) as output, patch.object(wait.time, 'sleep') as sleep:
                self.assertEqual(wait.wait_for_job('123', 30, NOW), 'COMPLETED')
                self.assertEqual(output.call_args_list[0], output.call_args_list[1])
                sleep.assert_called_once_with(30)

if __name__ == '__main__':
    unittest.main()
