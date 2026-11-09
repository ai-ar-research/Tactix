"""Owned clock integration in a Python CDCL policy, not full DefaultPolicy parity."""
from pathlib import Path
import gc
import importlib.util
import itertools
import json
import sys
import tempfile

library, root = map(Path, sys.argv[1:])
sys.path.insert(0, str(root/'python'))
sys.path.insert(0, str(root/'tools/bench'))
from tactix import Session, RestartClock, SAT, UNSAT, UNKNOWN, capi
from proof_oracle import certificate_route
spec = importlib.util.spec_from_file_location('clock_runner', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
for tool in (runner.DRAT_TRIM, runner.CAKE_LPR):
    if not Path(tool).is_file(): sys.exit(77)


def raises(kind, call):
    try: call()
    except kind: return
    raise AssertionError('expected '+kind.__name__)


class ClockedCDCL:
    """Python selects decisions, mode transitions, and clock-triggered restarts."""
    def __init__(self):
        self.clock = None
        self.views = []
        self.events = set()
        self.restarts = self.switches = self.analyses = 0

    def event(self, execution, event, glue=0):
        self.clock.update(execution, event, glue)
        self.events.add(event)

    def __call__(self, execution, restored):
        self.views.append(execution)
        if self.clock is None:
            self.clock = RestartClock(execution)
            profile = self.clock.parameters
            profile.fast_ema_window, profile.slow_ema_window = 3, 7
            profile.glue_margin = 0.1
            profile.reluctant_period = 1
            self.clock.configure(execution, profile)
        self.clock.reset(execution)
        before = bytes(self.clock.observe(execution))
        profile = self.clock.parameters
        profile.glue_margin = float('nan')
        raises(ValueError, lambda: self.clock.configure(execution, profile))
        for event in (-1, 99, 2**32, 'reset'):
            raises(ValueError, lambda: self.clock.update(execution, event))
        raises(ValueError, lambda: self.clock.update(execution, capi.TACTIX_RESTART_CLOCK_RESET, 1))
        for glue in (-1, 2**32):
            raises(ValueError, lambda: self.clock.update(execution, capi.TACTIX_RESTART_CLOCK_GLUE, glue))
        assert bytes(self.clock.observe(execution)) == before
        stat = lambda name: execution.c.tactix_stat(getattr(capi, 'TACTIX_STAT_'+name))
        switch_at = stat('CONFLICTS')+2
        for _ in range(100_000):
            action = execution.next()
            if action in (SAT, UNSAT): return action
            if action == capi.TACTIX_QUERY_ASSUME: execution.assume_next()
            elif action == capi.TACTIX_QUERY_PROPAGATE: execution.apply('Propagate')
            elif action == capi.TACTIX_QUERY_ANALYZE:
                self.event(execution, capi.TACTIX_RESTART_CLOCK_DECISION_RATE)
                learned, glue_sum = stat('LEARNED'), stat('LEARNED_GLUE_SUM')
                execution.apply('ConflictAnalysis', 0)
                self.analyses += 1
                if stat('LEARNED') != learned:
                    glue = stat('LEARNED_GLUE_SUM')-glue_sum
                    self.event(execution, capi.TACTIX_RESTART_CLOCK_GLUE, glue)
                    if stat('STABLE'): self.event(execution, capi.TACTIX_RESTART_CLOCK_RELUCTANT)
            elif action == capi.TACTIX_QUERY_BACKTRACK:
                execution.apply('Restart', 0)
                self.event(execution, capi.TACTIX_RESTART_CLOCK_RESTARTED)
            elif action == capi.TACTIX_QUERY_READY:
                if stat('CONFLICTS') >= switch_at:
                    execution.apply('SwitchMode')
                    self.event(execution, capi.TACTIX_RESTART_CLOCK_MODE_ENTERED)
                    self.switches += 1
                    switch_at = stat('CONFLICTS')+2
                if self.clock.observe(execution).due and stat('LEVEL'):
                    execution.apply('Restart', 0)
                    self.event(execution, capi.TACTIX_RESTART_CLOCK_RESTARTED)
                    self.restarts += 1
                else: execution.decide(-abs(execution.candidates()[0]))
            elif action in (capi.TACTIX_QUERY_RESTORE, capi.TACTIX_QUERY_INTERRUPTED): return UNKNOWN
            else: raise AssertionError(action)
        raise AssertionError('clocked policy made no progress')


# This hook applies only while solving. Certificate checking below runs external
# oracles separately; strategy execution itself must not launch a compiler/process.
solving = False
def audit(event, args):
    if solving and event in ('subprocess.Popen', 'os.system', 'os.posix_spawn'):
        raise AssertionError('Python strategy spawned a process')
sys.addaudithook(audit)

policy = ClockedCDCL()
certificates = []
with tempfile.TemporaryDirectory(prefix='python-restart-clock-') as temporary:
    for case, omitted in enumerate(((), (1, 1, 1, 1, 1, 1))):
        clauses = [tuple(i+1 if sign < 0 else -i-1 for i, sign in enumerate(values))
                   for values in itertools.product((-1, 1), repeat=6) if values != omitted]
        with Session(library=library, strategy=policy) as session:
            for clause in clauses: session.add(clause)
            for query, assumptions in enumerate(((), (-1,), (1,), ())):
                solving = True
                try: result = session.solve(assumptions)
                finally: solving = False
                expected = [values for values in itertools.product((-1, 1), repeat=6)
                            if all(values[abs(x)-1] == (1 if x > 0 else -1) for x in assumptions)
                            and all(any(values[abs(x)-1] == (1 if x > 0 else -1) for x in clause)
                                    for clause in clauses)]
                assert result.status == (SAT if expected else UNSAT)
                if result.status == SAT:
                    model = set(result.model)
                    assert all(any(x in model for x in clause) for clause in clauses)
                    assert all(x in model for x in assumptions)
                prefix = Path(temporary)/f'{case}-{query}'
                result.export(prefix)
                certificates.append((prefix, result.status))
        assert policy.clock.parameters.glue_margin == 0.1
        raises(RuntimeError, lambda: policy.clock.observe(policy.views[-1]))
        raises(RuntimeError, lambda: policy.clock.reset(policy.views[-1]))
        artifacts = Path(result.bundle).parent
        assert artifacts.is_dir()
        del result
        gc.collect()
        assert not artifacts.exists()
    # Certificate artifacts and clock remain independently owned after close.
    for prefix, status in certificates:
        if status == SAT: continue
        proof = Path(str(prefix)+'.proof')
        assert certificate_route(proof) == 'drat'
        checked = runner.check_proof('dsr', str(prefix)+'.cnf', str(proof), 30, route='dsr-cake_lpr')
        assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
    assert policy.analyses > 0 and policy.restarts > 0 and policy.switches > 0
    assert policy.events == {capi.TACTIX_RESTART_CLOCK_DECISION_RATE, capi.TACTIX_RESTART_CLOCK_GLUE,
                             capi.TACTIX_RESTART_CLOCK_RELUCTANT, capi.TACTIX_RESTART_CLOCK_RESTARTED,
                             capi.TACTIX_RESTART_CLOCK_MODE_ENTERED}
    policy.clock.close()
    policy.clock.close()
    raises(RuntimeError, lambda: policy.clock.parameters)
print(json.dumps(dict(status='PASS', queries=len(certificates), analyses=policy.analyses,
                      scheduled_restarts=policy.restarts, mode_switches=policy.switches,
                      scope='Python-owned CDCL with shared restart clock; no full DefaultPolicy parity claim')))
