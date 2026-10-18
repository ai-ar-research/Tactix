import tempfile
import unittest
from pathlib import Path

from workspace_quota import WorkspaceQuotaMonitor, used_gib


class WorkspaceQuotaTest(unittest.TestCase):
    def test_usage_and_limit(self):
        self.assertEqual(used_gib('NAME USED QUOTA\nhwu 750G 1T\n', 'hwu'), 750)
        with tempfile.TemporaryDirectory() as directory:
            table = Path(directory) / 'quota'
            triggered = []
            monitor = WorkspaceQuotaMonitor(triggered.append, quota_path=table,
                                            user='hwu')
            table.write_text('NAME USED QUOTA\nhwu 750G 1T\n')
            monitor.poll()
            self.assertEqual(triggered, [])
            table.write_text('NAME USED QUOTA\nhwu 751G 1T\n')
            monitor.poll()
            self.assertEqual(triggered[0]['reason'], 'quota_limit')

    def test_unavailable_quota_stops_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            triggered = []
            monitor = WorkspaceQuotaMonitor(
                triggered.append, quota_path=Path(directory) / 'absent', user='hwu')
            monitor.poll()
            self.assertEqual(triggered[0]['reason'], 'quota_unavailable')


if __name__ == '__main__':
    unittest.main()
