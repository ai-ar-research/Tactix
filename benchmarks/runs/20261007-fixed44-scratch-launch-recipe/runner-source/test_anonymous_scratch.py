"""Crash/timeout/kill controls use synthetic writers, never SAT benchmarks."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import anonymous_scratch
from anonymous_scratch import AnonymousScratch

class ScratchTests(unittest.TestCase):
    def test_missing_python_constant_keeps_anonymous_storage(self):
        original = getattr(os, 'O_TMPFILE', 0o20000000 | os.O_DIRECTORY)
        with mock.patch.dict(os.__dict__):
            os.__dict__.pop('O_TMPFILE', None)
            with mock.patch.object(anonymous_scratch.sys, 'platform', 'linux'), mock.patch.object(anonymous_scratch.platform, 'machine', return_value='x86_64'):
                self.assertEqual(anonymous_scratch.anonymous_open_flag(), original)
                self.test_reopen_and_close()

    def test_missing_constant_unknown_abi_fails_closed(self):
        with mock.patch.dict(os.__dict__):
            os.__dict__.pop('O_TMPFILE', None)
            with mock.patch.object(anonymous_scratch.platform, 'machine', return_value='unknown'):
                with self.assertRaises(OSError):
                    AnonymousScratch('/tmp') / 'proof'

    def test_reopen_and_close(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=AnonymousScratch(tmp)
            try:
                p=s/'input.cnf'; p.write_bytes(b'formula')
                self.assertEqual(p.stat().st_nlink,0)
                self.assertEqual(subprocess.check_output(['cat',str(p)]),b'formula')
                self.assertEqual(list(Path(tmp).iterdir()),[])
            finally:s.close()
            self.assertFalse(p.exists())
            s.close()

    def test_release_returns_blocks_and_preserves_other_handles(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            first, second = s/'first', s/'second'
            first.write_bytes(b'a' * (1 << 20))
            second.write_bytes(b'keep')
            self.assertTrue(s.release('first'))
            self.assertFalse(first.exists())
            self.assertEqual(second.read_bytes(), b'keep')
            self.assertFalse(s.release('first'))
            with self.assertRaises(TypeError):
                s.release(first)
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'first'
            replacement = s/'replacement'
            replacement.write_bytes(b'new')
            # Linux normally reuses the just-closed descriptor. A stale proc
            # path may therefore name the replacement, but it is never an
            # accepted release identity and duplicate logical cleanup is safe.
            self.assertEqual(replacement.read_bytes(), b'new')
            self.assertFalse(s.release('first'))
            self.assertEqual(replacement.read_bytes(), b'new')
            s.close()

    def test_release_closes_and_tombstones_after_truncate_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            artifact = s/'artifact'
            artifact.write_bytes(b'content')
            with mock.patch.object(os, 'ftruncate', side_effect=OSError('injected truncate failure')):
                with self.assertRaisesRegex(OSError, 'injected truncate failure'):
                    s.release('artifact')
            self.assertFalse(artifact.exists())
            self.assertFalse(s.contains('artifact'))
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'artifact'
            s.close()

    def test_capacity_admission_fails_before_opening_an_artifact(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                 f_bavail=399, f_frsize=1)), \
             mock.patch.object(os, 'open') as opened:
            with self.assertRaisesRegex(RuntimeError, 'scratch_admission') as caught:
                AnonymousScratch(
                    tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                    maximum_live_handles=6)
            self.assertEqual(caught.exception.scratch_reason,
                             'scratch_admission')
            opened.assert_not_called()

    def test_capacity_admission_accepts_exact_available_boundary(self):
        import resource
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                 f_bavail=400, f_frsize=1)), \
             mock.patch.object(resource, 'getrlimit', side_effect=[
                 (resource.RLIM_INFINITY, resource.RLIM_INFINITY),
                 (60, 60), (60, 60)]), \
             mock.patch.object(resource, 'setrlimit'):
            scratch = AnonymousScratch(
                tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                maximum_live_handles=6)
            self.assertEqual(scratch.resource_status()['available_bytes'], 400)
            scratch.close()

    def test_cooperative_lock_contention_crash_release_and_stable_inode(self):
        script = r'''import json,os,sys,time
from anonymous_scratch import AnonymousScratch, ScratchResourceError
root=sys.argv[1];lock=root+'/private-locks'
try:
 s=AnonymousScratch(root,minimum_free_bytes=1,maximum_file_bytes=4096,
   maximum_live_handles=2,cooperative_lock_directory=lock,
   cooperative_lock_name='managed.lock')
except ScratchResourceError as error:
 print(json.dumps({'reason':error.scratch_reason}),flush=True);sys.exit(23)
print(json.dumps(s.resource_status()['cooperative_lock']),flush=True)
if sys.argv[2]=='hold':time.sleep(60)
s.close()
'''
        environment = dict(os.environ, PYTHONPATH=str(Path(__file__).parent))
        with tempfile.TemporaryDirectory() as tmp:
            first = subprocess.Popen(
                [sys.executable, '-c', script, tmp, 'hold'], env=environment,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                import json
                identity = json.loads(first.stdout.readline())
                lock_path = Path(tmp)/'private-locks/managed.lock'
                self.assertTrue(lock_path.is_file())
                inode = lock_path.stat().st_ino
                self.assertEqual(identity['inode'], inode)
                contender = subprocess.run(
                    [sys.executable, '-c', script, tmp, 'once'], env=environment,
                    capture_output=True, text=True, timeout=5)
                self.assertEqual(contender.returncode, 23, contender.stderr)
                self.assertEqual(json.loads(contender.stdout),
                                 {'reason': 'scratch_node_busy'})
                self.assertEqual(lock_path.stat().st_ino, inode)
                first.kill()
                first.wait(timeout=5)
                resumed = subprocess.run(
                    [sys.executable, '-c', script, tmp, 'once'], env=environment,
                    capture_output=True, text=True, timeout=5)
                self.assertEqual(resumed.returncode, 0, resumed.stderr)
                self.assertEqual(json.loads(resumed.stdout)['inode'], inode)
                self.assertEqual(lock_path.stat().st_ino, inode)
            finally:
                if first.poll() is None:
                    first.kill()
                    first.wait()
                first.stdout.close()
                first.stderr.close()

    def test_cooperative_lock_rejects_nonprivate_directory_and_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            public = root/'public'
            public.mkdir(mode=0o755)
            with self.assertRaisesRegex(OSError, 'not private'):
                AnonymousScratch(
                    root, minimum_free_bytes=1, maximum_file_bytes=4096,
                    maximum_live_handles=2,
                    cooperative_lock_directory=public,
                    cooperative_lock_name='managed.lock')
            private = root/'private'
            private.mkdir(mode=0o700)
            target = root/'target'
            target.write_text('unsafe')
            (private/'managed.lock').symlink_to(target)
            with self.assertRaises(OSError):
                AnonymousScratch(
                    root, minimum_free_bytes=1, maximum_file_bytes=4096,
                    maximum_live_handles=2,
                    cooperative_lock_directory=private,
                    cooperative_lock_name='managed.lock')

    def test_cooperative_admission_releases_lock_before_anonymous_open(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                 f_bavail=399, f_frsize=1)), \
             mock.patch.object(anonymous_scratch, 'anonymous_open_flag',
                               side_effect=AssertionError('anonymous open')):
            lock_directory = Path(tmp)/'private-locks'
            with self.assertRaisesRegex(RuntimeError, 'scratch_admission'):
                AnonymousScratch(
                    tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                    maximum_live_handles=6,
                    cooperative_lock_directory=lock_directory,
                    cooperative_lock_name='managed.lock')
            lock_path = lock_directory/'managed.lock'
            inode = lock_path.stat().st_ino
            # Admission released the flock but retained the one stable inode.
            with mock.patch.object(os, 'statvfs', return_value=SimpleNamespace(
                    f_bavail=399, f_frsize=1)):
                with self.assertRaisesRegex(RuntimeError, 'scratch_admission'):
                    AnonymousScratch(
                        tmp, minimum_free_bytes=400, maximum_file_bytes=60,
                        maximum_live_handles=6,
                        cooperative_lock_directory=lock_directory,
                        cooperative_lock_name='managed.lock')
            self.assertEqual(lock_path.stat().st_ino, inode)

    def test_handle_cap_is_atomic_and_release_preserves_high_water(self):
        import resource
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(resource, 'getrlimit', side_effect=[
                 (resource.RLIM_INFINITY, resource.RLIM_INFINITY),
                 (1024, 1024), (1024, 1024)]), \
             mock.patch.object(resource, 'setrlimit'):
            s = AnonymousScratch(
                tmp, minimum_free_bytes=1, maximum_file_bytes=1024,
                maximum_live_handles=2)
            first, second = s/'first', s/'second'
            with self.assertRaisesRegex(RuntimeError, 'scratch_handle_limit'):
                s/'third'
            self.assertEqual(set(s.handles), {'first', 'second'})
            self.assertTrue(s.release('first'))
            third = s/'third'
            self.assertTrue(second.exists() and third.exists())
            status = s.resource_status()
            self.assertEqual(status['high_water_handles'], 2)
            self.assertEqual(status['active_handles'], 2)
            with self.assertRaisesRegex(RuntimeError, 'cannot be reopened'):
                s/'first'
            s.close()

    def test_repeated_stage_names_use_distinct_tombstoned_generations(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = AnonymousScratch(tmp)
            first_name = s.fresh_name('stage.stdout')
            first = s/first_name
            first.write_bytes(b'old')
            self.assertTrue(s.release(first_name))
            second_name = s.fresh_name('stage.stdout')
            second = s/second_name
            second.write_bytes(b'new')
            self.assertNotEqual(first_name, second_name)
            # The kernel may reuse the descriptor, so the stale proc path can
            # alias this generation. Only its distinct logical name is valid.
            self.assertFalse(s.release(first_name))
            self.assertEqual(second.read_bytes(), b'new')
            s.close()

    def test_file_limit_is_exact_and_inherited_by_children(self):
        script = r'''import os, resource, signal, subprocess, sys, tempfile
from anonymous_scratch import AnonymousScratch
with tempfile.TemporaryDirectory() as tmp:
 s=AnonymousScratch(tmp,minimum_free_bytes=1,maximum_file_bytes=4096,maximum_live_handles=2)
 assert resource.getrlimit(resource.RLIMIT_FSIZE)==(4096,4096)
 p=s/'parent-bounded';p.write_bytes(b'x'*4096);assert p.stat().st_size==4096;s.release('parent-bounded')
 p=s/'bounded'
 writer='import os,resource,sys;assert resource.getrlimit(resource.RLIMIT_FSIZE)==(4096,4096);n=int(sys.argv[2]);fd=os.open(sys.argv[1],os.O_WRONLY);os.write(fd,b"x"*min(n,4096));n>4096 and os.write(fd,b"x"*(n-4096));os.close(fd)'
 assert subprocess.run([sys.executable,'-c',writer,str(p),'4096']).returncode==0
 assert p.stat().st_size==4096
 s.release('bounded')
 q=s/'over'
 failed=subprocess.run([sys.executable,'-c',writer,str(q),'4097'])
 assert failed.returncode!=0,failed.returncode
 s.close()
'''
        environment = dict(os.environ, PYTHONPATH=str(Path(__file__).parent))
        completed = subprocess.run(
            [sys.executable, '-c', script], env=environment,
            capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_abnormal_exit(self):
        for mode in ['crash','timeout','term','kill']:
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                script='''import sys,os,time
from anonymous_scratch import AnonymousScratch
s=AnonymousScratch(sys.argv[1])
for name in ['input.cnf','proof.drat','solver.stdout']:
 p=s/name;p.write_bytes(b'x'*1048576)
 print(str(p),flush=True)
if sys.argv[2]=='crash':os._exit(7)
time.sleep(60)
'''
                env=dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parent))
                p=subprocess.Popen([sys.executable,'-c',script,tmp,mode],stdout=subprocess.PIPE,text=True,env=env)
                paths=[Path(p.stdout.readline().strip()) for _ in range(3)]
                try:
                    self.assertEqual(list(Path(tmp).iterdir()),[])
                    if mode=='timeout':
                        with self.assertRaises(subprocess.TimeoutExpired):p.wait(timeout=.05)
                        p.kill()
                    elif mode=='kill':p.kill()
                    elif mode=='term':p.terminate()
                    p.wait(timeout=5)
                    self.assertTrue(all(not path.exists() for path in paths))
                    self.assertEqual(list(Path(tmp).iterdir()),[])
                finally:
                    if p.poll() is None:p.kill();p.wait()
                    p.stdout.close()

    def _ledger(self, root, slots=4, identity=None):
        return anonymous_scratch.NodeScratchLedger(
            root, Path(root)/'private-ledger', 'slot-', slots, identity)

    def test_shared_ledger_lists_live_slots_and_refuses_when_full(self):
        with tempfile.TemporaryDirectory() as tmp:
            first = self._ledger(tmp, 2, 'job-a')
            second = self._ledger(tmp, 2, 'job-b')
            try:
                self.assertEqual((first.slot, second.slot), (0, 1))
                first.publish(5 << 20)
                live = {p['slot']: p for p in second.participants()}
                self.assertEqual(live[0]['used_bytes'], 5 << 20)
                self.assertEqual(live[0]['identity'], 'job-a')
                self.assertEqual(live[1]['state'], 'running')
                with self.assertRaisesRegex(RuntimeError, 'scratch_admission') as caught:
                    self._ledger(tmp, 2)
                self.assertTrue(caught.exception.scratch_details['ledger_full'])
                first.close()
                # The stale record remains on disk but its lock is gone.
                self.assertEqual([p['slot'] for p in second.participants()], [1])
                reused = self._ledger(tmp, 2, 'job-c')
                self.assertEqual(reused.slot, 0)
                reused.close()
            finally:
                first.close()
                second.close()
            self.assertEqual(sorted(x.name for x in (Path(tmp)/'private-ledger').iterdir()),
                             ['slot-000', 'slot-001'])

    def test_shared_ledger_rejects_nonprivate_directory_and_symlinked_slot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            public = root/'public'
            public.mkdir(mode=0o755)
            with self.assertRaisesRegex(OSError, 'not private'):
                anonymous_scratch.NodeScratchLedger(root, public, 'slot-', 2)
            private = root/'private'
            private.mkdir(mode=0o700)
            (root/'target').write_text('unsafe')
            (private/'slot-000').symlink_to(root/'target')
            with self.assertRaises(OSError):
                anonymous_scratch.NodeScratchLedger(root, private, 'slot-', 2)
            with self.assertRaisesRegex(ValueError, 'private scratch child'):
                anonymous_scratch.NodeScratchLedger(root, root/'a'/'b', 'slot-', 2)

    def test_shared_ledger_slot_is_released_by_sigkill(self):
        script = ("import sys,time\n"
                  "from anonymous_scratch import NodeScratchLedger\n"
                  "from pathlib import Path\n"
                  "l=NodeScratchLedger(sys.argv[1],Path(sys.argv[1])/'private-ledger','slot-',4,'holder')\n"
                  "l.publish(1234);print(l.slot,flush=True);time.sleep(60)\n")
        environment = dict(os.environ, PYTHONPATH=str(Path(__file__).parent))
        with tempfile.TemporaryDirectory() as tmp:
            holder = subprocess.Popen([sys.executable, '-c', script, tmp],
                                      env=environment, stdout=subprocess.PIPE, text=True)
            try:
                self.assertEqual(holder.stdout.readline().strip(), '0')
                observer = self._ledger(tmp, 4, 'observer')
                self.assertEqual(observer.slot, 1)
                self.assertEqual({p['slot']: p['used_bytes'] for p in observer.participants()},
                                 {0: 1234, 1: 0})
                holder.kill()
                holder.wait(timeout=5)
                self.assertEqual([p['slot'] for p in observer.participants()], [1])
                observer.close()
            finally:
                if holder.poll() is None:
                    holder.kill()
                    holder.wait()
                holder.stdout.close()

    def _admit(self, tmp, statvfs_values, wait=60, poll=15):
        import resource
        clock = [0.0]
        sleeps = []
        def sleep(seconds):
            sleeps.append(seconds)
            clock[0] += seconds
        limit = [resource.RLIM_INFINITY, resource.RLIM_INFINITY]
        values = iter(statvfs_values)
        def set_limit(_kind, value):
            limit[:] = value
        with mock.patch.object(os, 'statvfs', side_effect=lambda _p: SimpleNamespace(
                 f_bavail=next(values), f_frsize=1)), \
             mock.patch.object(resource, 'getrlimit', side_effect=lambda _k: tuple(limit)), \
             mock.patch.object(resource, 'setrlimit', side_effect=set_limit):
            result = anonymous_scratch.admit_shared_scratch(
                tmp, admission_free_bytes=100, maximum_file_bytes=60,
                maximum_live_handles=6, ledger_directory=Path(tmp)/'private-ledger',
                ledger_slot_prefix='slot-', ledger_slots=4,
                admission_wait_seconds=wait, admission_poll_seconds=poll,
                clock=lambda: clock[0], sleep=sleep)
        return result, sleeps

    def test_shared_admission_colocates_without_a_mutex(self):
        with tempfile.TemporaryDirectory() as tmp:
            (a, la, first), _ = self._admit(tmp, [100])
            (b, lb, second), _ = self._admit(tmp, [100])
            try:
                self.assertEqual((first['ledger_slot'], second['ledger_slot']), (0, 1))
                self.assertEqual(first['attempts'], 1)
                self.assertEqual(len(lb.participants()), 2)
            finally:
                for owner in (a, b, la, lb):
                    owner.close()

    def test_shared_admission_waits_then_admits_and_times_out_retryably(self):
        with tempfile.TemporaryDirectory() as tmp:
            (scratch, ledger, admission), sleeps = self._admit(tmp, [99, 99, 100])
            self.assertEqual(admission['attempts'], 3)
            self.assertEqual(sleeps, [15, 15])
            self.assertEqual(admission['waited_seconds'], 30)
            scratch.close(); ledger.close()
            with mock.patch.object(anonymous_scratch, 'anonymous_open_flag',
                                   side_effect=AssertionError('anonymous open')):
                with self.assertRaisesRegex(RuntimeError, 'scratch_admission') as caught:
                    self._admit(tmp, [99] * 10, wait=60, poll=15)
            details = caught.exception.scratch_details
            self.assertEqual(details['admission_attempts'], 5)
            self.assertEqual(details['admission_waited_seconds'], 60)
            self.assertTrue(details['retryable'])
            # The refused attempt released its ledger slot.
            probe = self._ledger(tmp, 4)
            self.assertEqual(probe.slot, 0)
            probe.close()

    def _monitor(self, scratch, ledger, available, triggers):
        return anonymous_scratch.ScratchFloorMonitor(
            scratch, ledger, directory='/unused', reserve_bytes=40,
            hard_floor_bytes=20, poll_seconds=0.01,
            on_trigger=triggers.append,
            statvfs=lambda _p: SimpleNamespace(f_bavail=available[0], f_frsize=1))

    def test_floor_monitor_stops_only_the_largest_participant(self):
        with tempfile.TemporaryDirectory() as tmp:
            small, large = AnonymousScratch(tmp), AnonymousScratch(tmp)
            (small/'proof').write_bytes(b'x' * (1 << 16))
            (large/'proof').write_bytes(b'x' * (1 << 20))
            small_ledger, large_ledger = self._ledger(tmp), self._ledger(tmp)
            available = [100]
            triggers = []
            small_monitor = self._monitor(small, small_ledger, available, triggers)
            large_monitor = self._monitor(large, large_ledger, available, triggers)
            try:
                self.assertIsNone(small_monitor.evaluate())
                self.assertIsNone(large_monitor.evaluate())
                available[0] = 39
                self.assertIsNone(small_monitor.evaluate())
                details = large_monitor.evaluate()
                self.assertEqual(details['trigger'], 'node_floor')
                self.assertEqual(details['ledger_slot'], large_ledger.slot)
                self.assertTrue(details['retryable'])
                self.assertGreaterEqual(details['accounted_bytes'], 1 << 20)
                self.assertIsNone(large_monitor.evaluate())  # at most once
                # While the victim is terminating, the other participant waits.
                self.assertIsNone(small_monitor.evaluate())
                large.close(); large_ledger.close()
                # After the victim's release, the floor still binds the next one.
                self.assertEqual(small_monitor.evaluate()['trigger'], 'node_floor')
                self.assertEqual(small_monitor.status()['minimum_available_bytes'], 39)
            finally:
                for owner in (small, large, small_ledger, large_ledger):
                    owner.close()

    def test_floor_monitor_hard_floor_stops_every_participant(self):
        with tempfile.TemporaryDirectory() as tmp:
            scratches = [AnonymousScratch(tmp), AnonymousScratch(tmp)]
            (scratches[1]/'proof').write_bytes(b'x' * (1 << 20))
            ledgers = [self._ledger(tmp), self._ledger(tmp)]
            available = [19]
            triggers = []
            try:
                monitors = [self._monitor(s, l, available, triggers)
                            for s, l in zip(scratches, ledgers)]
                self.assertEqual([m.evaluate()['trigger'] for m in monitors],
                                 ['hard_floor', 'hard_floor'])
            finally:
                for owner in (*scratches, *ledgers):
                    owner.close()

    def test_floor_monitor_thread_triggers_once_and_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            scratch, ledger = AnonymousScratch(tmp), self._ledger(tmp)
            available = [10]
            triggers = []
            monitor = self._monitor(scratch, ledger, available, triggers)
            monitor.start()
            monitor.thread.join(timeout=5)
            monitor.stop()
            self.assertEqual([t['trigger'] for t in triggers], ['hard_floor'])
            broken = anonymous_scratch.ScratchFloorMonitor(
                scratch, ledger, directory='/unused', reserve_bytes=40,
                hard_floor_bytes=20, poll_seconds=0.01, on_trigger=triggers.append,
                statvfs=mock.Mock(side_effect=OSError('statvfs failed')))
            broken.start()
            broken.thread.join(timeout=5)
            broken.stop()
            self.assertEqual(triggers[-1]['trigger'], 'monitor_failure')
            scratch.close(); ledger.close()

    def test_unsupported_fails_closed(self):
        with self.assertRaises(OSError):
            AnonymousScratch('/proc')/'proof'

if __name__=='__main__':unittest.main()
