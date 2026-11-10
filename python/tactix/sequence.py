"""Caller-owned sequence scheduling over independent public atomic executors."""
import ctypes as C
from . import capi, _QueryInterrupted

_U64 = 2**64 - 1


def strategy_state(execution):
    state = capi.tactix_strategy_state()
    if execution.c.tactix_read_strategy_state(C.byref(state)):
        raise RuntimeError('cannot observe strategy state')
    return state


def _version(state):
    return state.arena_generation, state.touch_epoch, state.strategy_epoch


class OnlineSequence:
    """Python sequence policy with native atomic actions and owned payloads.

    Supports C program versions 1..17. Configuration copies the request into
    native continuations; input arrays can then be changed or released. Python
    owns admission, exhaustion/retry, round repetition and progress accounting.
    It never invokes DefaultPolicy. A result with query_rejected_model reports
    a rejected F-model candidate, not a certified query SAT verdict; callers
    must handle that boundary explicitly, as native DefaultPolicy does.
    """
    def __init__(self, execution, program=None, **options):
        execution._check()
        self.execution, self._lib = execution, execution._lib
        self._executors, self._ids = [], ()
        self._configured, self._closed = False, False
        self._revision, self.rounds = 0, 0
        self._complete, self._expected = False, None
        if program is not None:
            self.configure(program, **options)

    def _check(self):
        if self._closed:
            raise RuntimeError('sequence has been released')
        self.execution._check()

    def configure(self, program, *, version=2, revision=1,
                  mode=capi.TACTIX_ONLINE_RECONFIGURE_RESET,
                  on_budget_exhaustion=capi.TACTIX_SEQUENCE_BUDGET_CONTINUE,
                  progress_followups=(), before_progress=()):
        self._check()
        if not isinstance(version, int) or version not in range(1, 18):
            raise ValueError('unknown C program version')
        program_type = getattr(capi, 'tactix_online_program' + (f'_v{version}' if version > 1 else ''))
        if not isinstance(program, program_type):
            raise TypeError('program type does not match its C version')
        if not isinstance(revision, int) or not 0 <= revision <= _U64:
            raise ValueError('revision must be a uint64')
        if mode not in (capi.TACTIX_ONLINE_RECONFIGURE_RESET, capi.TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS):
            raise ValueError('unknown reconfiguration mode')
        if on_budget_exhaustion not in (capi.TACTIX_SEQUENCE_BUDGET_STOP,
                capi.TACTIX_SEQUENCE_BUDGET_CONTINUE, capi.TACTIX_SEQUENCE_BUDGET_FINISH_ROUND):
            raise ValueError('unknown exhaustion policy')
        if program.step_count < 0 or (program.step_count and not program.steps):
            raise ValueError('invalid program steps')
        ids = []
        for step in program.steps[:program.step_count]:
            while hasattr(step, 'base'):
                step = step.base
            ids.append(step.step_id)
        followups, initial = tuple(progress_followups), tuple(before_progress)
        if (len(set(followups)) != len(followups) or len(set(initial)) != len(initial)
                or not set(followups + initial) <= set(ids) or set(followups) & set(initial)):
            raise ValueError('admissions must name unique existing steps in disjoint groups')
        replacements = []
        scratch = self.execution.c.tactix_online_executor_create()
        if not scratch:
            raise RuntimeError('cannot create program validator')
        try:
            # Validate the whole request before publishing any replacement.
            if self.execution.c.tactix_online_executor_configure(
                    scratch, version, C.byref(program), revision, mode):
                raise ValueError('invalid online program')
            for index, identity in enumerate(ids):
                previous = (self._ids.index(identity) if identity in self._ids else None)
                if mode == capi.TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS and previous is not None:
                    handle = self._lib.dll.tactix_online_executor_clone(self._executors[previous])
                else:
                    handle = self.execution.c.tactix_online_executor_create()
                if not handle:
                    raise RuntimeError('cannot create atomic continuation')
                replacements.append(handle)
                one = program_type(C.pointer(program.steps[index]), 1, capi.TACTIX_ONLINE_REPEAT_ONCE, 1)
                if self.execution.c.tactix_online_executor_configure(
                        handle, version, C.byref(one), revision, mode):
                    raise ValueError('invalid atomic continuation')
        except BaseException:
            for handle in replacements:
                self._lib.dll.tactix_online_executor_release(handle)
            raise
        finally:
            self._lib.dll.tactix_online_executor_release(scratch)
        old, self._executors = self._executors, replacements
        for handle in old:
            self._lib.dll.tactix_online_executor_release(handle)
        self._ids, self._revision = tuple(ids), revision
        self._repeat, self._limit = program.repeat_mode, program.repeat_limit
        self._exhaustion, self._followups, self._initial = on_budget_exhaustion, set(followups), set(initial)
        self.rounds = self._generation = self._current = 0
        self.dispatches = [0] * len(ids)
        self._stopped = [None] * len(ids)
        self._credit = self._finish_round = self._any_incomplete = self._complete = False
        self._reset_round()
        self._expected, self._configured = None, True

    def _reset_round(self):
        self._current = 0
        self._step_progress = self._followup_admitted = self._initial_admitted = False
        self._progress = self._incomplete = False

    def _next(self, clear='all'):
        self._current += 1
        self._step_progress = False
        if clear != 'kept': self._followup_admitted = False
        if clear == 'all': self._initial_admitted = False

    def _result(self, state, status):
        result = capi.tactix_online_execution_result()
        r = result.base
        r.status, r.rule_result = status, capi.TACTIX_RULE_RESULT_NO_OP
        r.rule_id, r.instance = 2**32 - 1, capi.TACTIX_INSTANCE_CANONICAL
        r.configuration_revision = self._revision
        r.before_arena_generation = r.after_arena_generation = state.arena_generation
        r.before_touch_epoch = r.after_touch_epoch = state.touch_epoch
        r.before_strategy_epoch = r.after_strategy_epoch = state.strategy_epoch
        return result

    def _skip(self, state, status, clear):
        result = self._result(state, status)
        result.base.step_id = self._ids[self._current]
        result.base.selection_complete = status == capi.TACTIX_ONLINE_NO_CHANGE
        if status == capi.TACTIX_ONLINE_BUDGET_EXHAUSTED:
            result.base.budget_status = capi.TACTIX_BUDGET_STATUS_EXHAUSTED
        self._next(clear)
        return result

    def _boundary(self, state):
        if self.execution._session is not None and (state.stopped or state.query_status in (
                capi.TACTIX_EXECUTION_INTERRUPTED, capi.TACTIX_EXECUTION_RESTORE,
                capi.TACTIX_EXECUTION_INVALID_STATE, capi.TACTIX_EXECUTION_INVALID_MODEL,
                capi.TACTIX_EXECUTION_PROOF_FAILURE)):
            raise _QueryInterrupted()

    @property
    def complete(self):
        self._check()
        return self._complete

    def consumed(self, index):
        self._check()
        if not isinstance(index, int) or not 0 <= index < len(self._ids):
            raise IndexError('unknown sequence step')
        return self._lib.dll.tactix_online_executor_consumed(self._executors[index], self._ids[index])

    def advance(self, discovery=None):
        self._check()
        if discovery is not None and (not isinstance(discovery, capi.tactix_online_discovery_budget)
                or discovery.unlimited not in (0, 1) or discovery.accounting_overflow not in (0, 1)):
            raise ValueError('invalid shared discovery ledger')
        state = strategy_state(self.execution)
        if not self._configured:
            return self._result(state, capi.TACTIX_ONLINE_INVALID_REQUEST)
        if self._expected is not None and self._expected != _version(state):
            self._stopped = [None] * len(self._ids)
        self._expected = _version(state)
        while True:
            state = strategy_state(self.execution)
            self._boundary(state)
            if state.stopped:
                return self._result(state, capi.TACTIX_ONLINE_INTERRUPTED)
            if self._complete:
                return capi.tactix_online_execution_result.from_buffer_copy(self._terminal)
            if self._current < len(self._executors):
                identity = self._ids[self._current]
                if identity in self._initial and not self._initial_admitted:
                    if self._generation:
                        return self._skip(state, capi.TACTIX_ONLINE_NO_CHANGE, 'followup')
                    self._initial_admitted = True
                followup = identity in self._followups
                if followup and not self._followup_admitted:
                    if not self._credit:
                        return self._skip(state, capi.TACTIX_ONLINE_NO_CHANGE, 'kept')
                    self._followup_admitted = True
                if (self._exhaustion == capi.TACTIX_SEQUENCE_BUDGET_CONTINUE and
                        self._stopped[self._current] == self._generation):
                    self._incomplete = self._any_incomplete = True
                    return self._skip(state, capi.TACTIX_ONLINE_BUDGET_EXHAUSTED, 'all')
                self.dispatches[self._current] = (self.dispatches[self._current] + 1) & _U64
                handle = self._executors[self._current]
                result = capi.tactix_online_execution_result()
                if self.execution.c.tactix_online_executor_advance(
                        handle, C.byref(discovery) if discovery is not None else None, C.byref(result)):
                    raise RuntimeError('atomic continuation refused')
                self._boundary(strategy_state(self.execution))
                r = result.base
                self._expected = r.after_arena_generation, r.after_touch_epoch, r.after_strategy_epoch
                if result.discovery_progress_units or result.discovery_entry_required:
                    return result
                if not followup and r.made_rule_progress: self._credit = True
                self._progress |= bool(r.made_rule_progress)
                self._step_progress |= bool(r.made_rule_progress)
                self._generation = (self._generation + bool(r.made_rule_progress)) & _U64
                if r.status == capi.TACTIX_ONLINE_BUDGET_EXHAUSTED:
                    self._incomplete = self._any_incomplete = True
                    if self._exhaustion == capi.TACTIX_SEQUENCE_BUDGET_STOP:
                        return result
                    if self._exhaustion == capi.TACTIX_SEQUENCE_BUDGET_FINISH_ROUND:
                        self._finish_round = True
                    self._stopped[self._current] = None if self._step_progress else self._generation
                    self._next()
                else:
                    complete = self._lib.dll.tactix_online_executor_complete(handle)
                    if complete < 0: raise RuntimeError('atomic continuation expired')
                    if complete:
                        if followup: self._credit = False
                        self._next()
                return result
            self.rounds = (self.rounds + 1) & _U64
            fixpoint = self._repeat in (capi.TACTIX_ONLINE_REPEAT_FIXPOINT,
                                       capi.TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT)
            if fixpoint:
                limit = C.c_uint64()
                if self.execution.c.tactix_online_repeat_round_limit(self._repeat, self._limit, C.byref(limit)):
                    raise RuntimeError('repeat bound refused')
                repeat = self._progress and (not limit.value or self.rounds < limit.value)
            else:
                repeat = self._repeat == capi.TACTIX_ONLINE_REPEAT_COUNT and self.rounds < self._limit
            if repeat and not self._finish_round:
                for handle in self._executors:
                    if self.execution.c.tactix_online_executor_rearm(
                            handle, self._revision, capi.TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS):
                        raise RuntimeError('round rearm refused')
                self._reset_round()
                continue
            self._complete = True
            incomplete = self._incomplete if fixpoint else self._any_incomplete
            exhausted = incomplete or (fixpoint and self._progress)
            result = self._result(state, capi.TACTIX_ONLINE_BUDGET_EXHAUSTED if exhausted else
                capi.TACTIX_ONLINE_SCOPED_FIXPOINT if fixpoint else capi.TACTIX_ONLINE_COMPLETED)
            result.base.selection_complete = not incomplete
            if exhausted: result.base.budget_status = capi.TACTIX_BUDGET_STATUS_EXHAUSTED
            self._terminal = capi.tactix_online_execution_result.from_buffer_copy(result)
            return result

    def close(self):
        for handle in getattr(self, '_executors', ()):
            self._lib.dll.tactix_online_executor_release(handle)
        self._executors, self._closed = [], True

    def __enter__(self): self._check();return self
    def __exit__(self, *_): self.close()
    def __del__(self): self.close()
