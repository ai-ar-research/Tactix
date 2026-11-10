"""Experimental persistent SAT sessions over the reusable Tactix C library.

Python strategies execute public rule actions on the same native query. Results
own their values and certificate files independently of the session lifetime.
Use Result.export() to retain independently checkable artifacts on disk.
"""
from __future__ import annotations
import ctypes as C
from dataclasses import dataclass, field
import hashlib
import json
import os
from pathlib import Path
import shutil
from . import capi

SAT, UNSAT, UNKNOWN = 10, 20, 0

_Query = capi.tactix_ipasir_query_state
_StrategyCounters = capi.tactix_ipasir_strategy_counters
_Strategy = capi.tactix_ipasir_strategy
_Terminate = capi.SIGNATURES['ipasir_set_terminate'][1][2]
_Learn = capi.SIGNATURES['ipasir_set_learn'][1][3]

def _literal(value):
    if not isinstance(value, int) or value == 0 or not -(2**31-1) <= value <= 2**31-1:
        raise ValueError('expected a nonzero signed DIMACS literal')
    return value

class _Library:
    def __init__(self, path):
        self.dll = capi.load_library(path)
        self._callback_depth = 0
        self.rules = {self.dll.tactix_rule_name(i).decode(): i
                      for i in range(self.dll.tactix_rule_count())}

class _OwnedResult:
    def __init__(self, library, session=None, *, handle=None):
        self._lib = library
        self._handle = handle if handle is not None else library.dll.tactix_ipasir_result_acquire(session)
        if not self._handle:
            raise RuntimeError('cannot acquire published terminal result')

    def __del__(self):
        if getattr(self, '_handle', None):
            self._lib.dll.tactix_ipasir_result_release(self._handle)
            self._handle = None

class _QueryInterrupted(Exception):
    """Unwind a strategy when termination arrives between public actions."""

@dataclass(frozen=True)
class Result:
    status: int
    assumptions: tuple[int, ...]
    model: tuple[int, ...] = ()
    core: tuple[int, ...] = ()
    bundle: str | None = None
    _owner: object = field(default=None, repr=False, compare=False)
    _revision: int = field(default=-1, repr=False, compare=False)
    _native: object = field(default=None, repr=False, compare=False)

    @property
    def input_sha256(self):
        """Exact permanent-input identity, independent of A and session lifetime."""
        if self._native is None:return None
        return self._native._lib.dll.tactix_ipasir_result_input_sha256(self._native._handle).decode()

    def export(self, prefix):
        """Copy this query's immutable bundle, including after session closure."""
        if self.bundle is None:
            raise ValueError('UNKNOWN has no terminal certificate bundle')
        prefix = Path(prefix)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        manifest = json.loads(Path(self.bundle + '.json').read_text())
        for suffix, expected in manifest['sha256'].items():
            if suffix not in ('.cnf','.proof','.model','.core','-core.cnf','-core.proof'):
                raise ValueError('unknown certificate member')
            source = Path(self.bundle + suffix)
            with source.open('rb') as stream:
                digest = hashlib.sha256()
                for block in iter(lambda: stream.read(1024*1024), b''):
                    digest.update(block)
                digest = digest.hexdigest()
            if digest != expected:
                raise ValueError('certificate member hash mismatch')
            destination = Path(str(prefix) + suffix)
            if source.resolve() != destination.resolve():
                shutil.copyfile(source, destination)
        if Path(self.bundle+'.json').resolve() != Path(str(prefix)+'.json').resolve():
            shutil.copyfile(self.bundle + '.json', str(prefix) + '.json')
        return str(prefix)

@dataclass(frozen=True)
class CoreReduction:
    source: Result
    core: tuple[int, ...]
    context: tuple[int, ...]
    certificate: Result
    minimal: bool
    probes: tuple[Result, ...]
    # Each retained literal has a SAT witness for its deletion; later successful
    # removals only weaken those witnessed cubes.
    witnesses: tuple[tuple[int, Result], ...]

class Execution:
    """Borrowed public execution view, valid only inside the strategy call."""
    def __init__(self, library, session, view):
        self._lib, self._session, self._view = library, session, view
        self._active = True
        self.c = capi.SolverView(library.dll, view, self._check)

    def _check(self):
        if not self._active:
            raise RuntimeError('strategy execution view has expired')
        if self._lib._callback_depth:
            raise RuntimeError('execution view is unavailable inside a solver callback')

    def _check_query(self):
        self._check()
        if self._session is None:
            raise RuntimeError("query operations require a persistent Session strategy")

    def next(self):
        self._check_query()
        query = _Query(version=1, size=C.sizeof(_Query))
        if self._lib.dll.tactix_ipasir_next(self._session, C.byref(query)):
            raise RuntimeError('invalid query boundary')
        return query.action

    @property
    def rejected_models(self):
        """Rejected model checks in this epoch; candidates may be checked repeatedly."""
        self._check_query()
        count = self._lib.dll.tactix_ipasir_rejected_models(self._session)
        if count == 2**64-1:
            raise RuntimeError('no active query execution view')
        return count

    def assume_next(self):
        self._check_query()
        code = self._lib.dll.tactix_ipasir_assume_next(self._session)
        if code == -2:
            raise _QueryInterrupted()
        if code:
            raise RuntimeError('assumption not ready')

    def apply(self, rule, instance=None):
        self._check()
        rid = self._lib.rules[rule]
        code = (self._lib.dll.tactix_apply(self._view, rid) if instance is None else
                self._lib.dll.tactix_apply_instance(self._view, rid, instance))
        if code < 0:
            raise RuntimeError(f'{rule} is not applicable')
        return code

    def candidates(self, limit=64):
        self._check()
        if not isinstance(limit, int) or not 1 <= limit <= 2**31-1:
            raise ValueError('candidate limit must be positive')
        buffer = (C.c_int * limit)()
        count = self._lib.dll.tactix_decide_candidates(self._view, buffer, limit)
        return tuple(buffer[:count])

    def decide(self, literal):
        self._check()
        if self._lib.dll.tactix_decide(self._view, _literal(literal)):
            raise RuntimeError('decision not applicable')

    def default_solve(self):
        """An explicit strategy handoff to configured DefaultPolicy."""
        self._check()
        return self._lib.dll.tactix_solve(self._view)

class Solver(Execution):
    """Owned plain C session for Python-authored public-interface strategies.

    default_frontend reproduces tactix_public_main's setup. The caller still
    selects every policy action, including cold materialization and H lifetime.
    Session remains the persistent IPASIR frontend.
    """
    def __init__(self, library=None, *, default_frontend=False):
        library = _Library(library)
        handle = library.dll.tactix_init_configured(
            capi.TACTIX_INIT_DEFAULT_FRONTEND if default_frontend else 0)
        if not handle: raise RuntimeError('cannot initialize Tactix solver')
        super().__init__(library, None, handle)

    def close(self):
        if getattr(self, '_active', False):
            self._active = False
            self._lib.dll.tactix_release(self._view)

    def __del__(self): self.close()
    def __enter__(self): self._check();return self
    def __exit__(self, *_): self.close()

    def add(self, clause):
        self._check()
        clause = tuple(_literal(literal) for literal in clause)
        for literal in (*clause, 0):
            if self.c.tactix_add_checked(literal):
                raise RuntimeError('plain solver input has already closed')

    def proof(self, path, format=capi.TACTIX_PROOF_AUTO):
        if self.c.tactix_proof(os.fsencode(path), format):
            raise ValueError('invalid or late proof attachment')

    def read_dimacs(self, path):
        """Use the CLI parser, including its advisory header.

        On malformed input ValueError leaves a partial solver; close it.
        """
        self._check()
        libc = C.CDLL(None, use_errno=True)
        libc.fopen.argtypes, libc.fopen.restype = [C.c_char_p, C.c_char_p], C.c_void_p
        libc.fclose.argtypes, libc.fclose.restype = [C.c_void_p], C.c_int
        stream = libc.fopen(os.fsencode(path), b'r')
        if not stream: raise OSError(C.get_errno(), os.strerror(C.get_errno()), str(path))
        try:
            if self.c.tactix_read_dimacs(stream): raise ValueError('invalid DIMACS or nonempty solver')
        finally:
            libc.fclose(stream)

    @property
    def model(self):
        if not self.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED):
            raise RuntimeError('solver input is still open')
        if self.c.tactix_status() != SAT: raise RuntimeError('solver has no SAT model')
        count = self.c.tactix_stat(capi.TACTIX_STAT_OUTPUT_VARIABLES)
        return tuple(self.c.tactix_value(v) for v in range(1, count+1))


class RestartClock:
    """Owned native restart scheduler; the Python policy chooses all actions.

    The clock retains no execution view and can survive queries or sessions.
    Use reset for a new schedule and update for explicitly selected events.
    """
    def __init__(self, execution, parameters=None):
        execution._check()
        self._lib, self._handle = execution._lib, None
        self._handle = execution.c.tactix_restart_clock_create(
            None if parameters is None else C.byref(parameters))
        if not self._handle:
            raise ValueError('invalid restart-clock parameters')

    def close(self):
        if self._handle:
            self._lib.dll.tactix_restart_clock_release(self._handle)
            self._handle = None

    def __del__(self):
        if getattr(self, '_handle', None): self.close()

    def _view(self, execution):
        execution._check()
        if not self._handle: raise RuntimeError('restart clock is closed')
        if execution._lib.dll._handle != self._lib.dll._handle:
            raise ValueError('restart clock belongs to another library')
        return execution.c

    @property
    def parameters(self):
        if not self._handle: raise RuntimeError('restart clock is closed')
        result = capi.tactix_restart_clock_parameters()
        if self._lib.dll.tactix_restart_clock_get_parameters(self._handle, C.byref(result)):
            raise RuntimeError('cannot read restart-clock parameters')
        return result

    def configure(self, execution, parameters=None):
        if self._view(execution).tactix_restart_clock_configure(self._handle,
                None if parameters is None else C.byref(parameters)):
            raise ValueError('invalid restart-clock parameters')

    def update(self, execution, event, glue=0):
        if not isinstance(event, int) or not capi.TACTIX_RESTART_CLOCK_RESET <= event <= capi.TACTIX_RESTART_CLOCK_MODE_ENTERED:
            raise ValueError('invalid restart-clock event')
        if not isinstance(glue, int) or not 0 <= glue <= 2**32-1:
            raise ValueError('glue must be a uint32 value')
        if self._view(execution).tactix_restart_clock_update(self._handle, event, glue):
            raise ValueError('invalid restart-clock event')

    def reset(self, execution):
        self.update(execution, capi.TACTIX_RESTART_CLOCK_RESET)

    def observe(self, execution):
        result = capi.tactix_restart_clock_state()
        if self._view(execution).tactix_restart_clock_observe(self._handle, C.byref(result)):
            raise RuntimeError('cannot observe restart clock')
        return result


class BveOrder:
    """Owned mutable BVE ordering; the Python policy selects every operation.

    Create inside an Eliminate occurrence session. Other solver mutations
    invalidate the order. Closing remains safe after the execution expires.
    """
    def __init__(self, execution, instances):
        execution._check()
        self._execution, self._handle = execution, None
        instances = tuple(instances)
        if len(instances) > 2**31-1:
            raise ValueError('too many BVE instances')
        for instance in instances: self._instance(instance)
        data = (C.c_uint64 * len(instances))(*instances)
        self._handle = execution.c.tactix_bve_order_create(data, len(instances))
        if not self._handle:
            raise RuntimeError('BVE ordering requires a quiescent Eliminate occurrence session')

    @staticmethod
    def _instance(instance):
        if not isinstance(instance, int) or not 0 <= instance <= 2**64-1:
            raise ValueError('instance must be uint64; use TACTIX_LIT_INSTANCE for signed literals')
        return instance

    def close(self):
        if self._handle:
            self._execution._lib.dll.tactix_bve_order_release(self._handle)
            self._handle = None

    def __del__(self):
        if getattr(self, '_handle', None): self.close()

    def __enter__(self): return self
    def __exit__(self, *_): self.close()

    def _view(self):
        self._execution._check()
        if not self._handle: raise RuntimeError('BVE ordering is closed')
        return self._execution.c

    def pop(self):
        instance, begin = C.c_uint64(), C.c_uint32()
        code = self._view().tactix_bve_order_next(self._handle, C.byref(instance), C.byref(begin))
        if code < 0: raise RuntimeError('BVE ordering is stale')
        return (instance.value, begin.value) if code else None

    def candidate(self, instance):
        result = capi.tactix_bve_candidate_state()
        if self._view().tactix_read_bve_candidate(self._instance(instance), C.byref(result)):
            raise ValueError('invalid BVE candidate')
        return result

    def apply(self, rule, instance=None):
        instance = capi.TACTIX_INSTANCE_CANONICAL if instance is None else self._instance(instance)
        code = self._view().tactix_bve_order_apply(self._handle,
            self._execution._lib.rules[rule], instance)
        if code < 0: raise RuntimeError('BVE ordering or selected operation is invalid')
        return code

    def propagate(self):
        """Select native root propagation, including fixpoint bookkeeping."""
        code = self._view().tactix_bve_order_propagate(self._handle)
        if code < 0: raise RuntimeError('BVE ordering or propagation state is invalid')
        return code

    def flush_units(self, propagation_begin):
        if not isinstance(propagation_begin, int) or not 0 <= propagation_begin <= 2**32-1:
            raise ValueError('propagation boundary must be uint32')
        if self._view().tactix_bve_order_flush_units(self._handle, propagation_begin):
            raise RuntimeError('BVE ordering or propagation boundary is invalid')


class ScriptedCDCL:
    """Example Python control policy; rule execution remains in the native engine."""
    def __init__(self, choose=None):
        self.choose = choose or (lambda candidates: candidates[0])
        self.calls = self.steps = self.restorations = 0

    def __call__(self, execution, restored):
        self.calls += 1
        self.restorations += bool(restored)
        while True:
            self.steps += 1
            action = execution.next()
            if action in (SAT, UNSAT): return action
            if action == 1: execution.assume_next()
            elif action == 2: execution.apply('Propagate')
            elif action == 3: execution.apply('ConflictAnalysis', 0)
            elif action == 4: execution.apply('Restart', 0)
            elif action == 0:
                candidates = execution.candidates()
                if not candidates: raise RuntimeError('ready query has no branch candidate')
                execution.decide(self.choose(candidates))
            elif action in (5, 7): return UNKNOWN
            else: raise RuntimeError(f'unsupported execution boundary {action}')

class Session:
    def __init__(self, library=None, strategy=None, core_budget=0):
        self._lib = _Library(library)
        self._handle = self._lib.dll.ipasir_init()
        if not self._handle: raise RuntimeError('cannot initialize Tactix session')
        self._busy = False
        self._variables = 0
        self._token, self._revision = object(), 0
        self._callback_error = None
        self._strategy_callback = _Strategy()
        self._terminate_callback = _Terminate()
        self._learn_callback = _Learn()
        self.strategy = None
        self.set_core_budget(core_budget)
        self.set_strategy(strategy)

    def _check(self):
        if not self._handle: raise RuntimeError('session is closed')
        if self._busy: raise RuntimeError('session already executing')

    def close(self):
        self._check()
        self._lib.dll.ipasir_release(self._handle)
        self._handle = None

    def __enter__(self): return self
    def __exit__(self, *_): self.close()

    def __del__(self):
        if getattr(self, '_handle', None) and not getattr(self, '_busy', False):
            self._lib.dll.ipasir_release(self._handle)
            self._handle = None

    def add(self, clause):
        self._check()
        clause = tuple(_literal(x) for x in clause)
        for x in clause:
            self._lib.dll.ipasir_add(self._handle, x)
            self._variables = max(self._variables, abs(x))
        self._lib.dll.ipasir_add(self._handle, 0)
        self._revision += 1

    def set_core_budget(self, ticks):
        self._check()
        if not isinstance(ticks, int) or not 0 <= ticks < 2**64:
            raise ValueError('core budget must fit uint64')
        if self._lib.dll.tactix_ipasir_set_core_budget(self._handle, ticks):
            raise RuntimeError('core budget rejected')

    def set_default_option(self, name, value):
        self._check()
        if self._lib.dll.tactix_ipasir_set_default_option(self._handle, name.encode(),
                None if value is None else str(value).encode()):
            raise ValueError('invalid default option')

    def set_strategy(self, strategy):
        self._check()
        if isinstance(strategy, str):
            if self._lib.dll.tactix_ipasir_set_strategy_name(self._handle,strategy.encode()):
                raise ValueError('unknown native strategy')
            self._strategy_callback,self.strategy = _Strategy(),strategy
            return
        if strategy is not None and not callable(strategy):
            raise TypeError('strategy must be a native name, callable or None')
        def run(_, session, view, cause):
            execution = Execution(self._lib, session, view)
            try:
                result = strategy(execution, cause == 1)
                # ctypes converts callback results after this try block has
                # returned. Reject invalid values here so conversion errors
                # reach solve()/minimize_core() through the callback channel.
                if not isinstance(result, int) or result not in (UNKNOWN, SAT, UNSAT):
                    raise ValueError('strategy must return UNKNOWN (0), SAT (10), or UNSAT (20)')
                return result
            except _QueryInterrupted:
                return UNKNOWN
            except BaseException as error:
                self._callback_error = error
                return -1
            finally:
                execution._active = False
        callback = _Strategy(run) if strategy is not None else _Strategy()
        if self._lib.dll.tactix_ipasir_set_strategy(self._handle, None, callback):
            raise RuntimeError('strategy rejected')
        self._strategy_callback, self.strategy = callback, strategy

    @property
    def strategy_name(self):
        self._check()
        return self._lib.dll.tactix_ipasir_strategy_name(self._handle).decode()

    def strategy_counters(self):
        """Reached native-program work since the most recent selection."""
        self._check()
        data = _StrategyCounters(version=1,size=C.sizeof(_StrategyCounters))
        if self._lib.dll.tactix_ipasir_get_strategy_counters(self._handle,C.byref(data)):
            raise ValueError('no counters for this driver')
        return {name:getattr(data,name) for name,_ in data._fields_[2:]}

    def set_terminate(self, terminate):
        self._check()
        def poll(_):
            self._lib._callback_depth += 1
            try: return int(bool(terminate()))
            except BaseException as error:
                self._callback_error = error
                return 1
            finally:
                self._lib._callback_depth -= 1
        callback = _Terminate(poll) if terminate is not None else _Terminate()
        self._lib.dll.ipasir_set_terminate(self._handle, None, callback)
        self._terminate_callback = callback

    def set_learn(self, learn, max_length, *, work_budget=None):
        """Observe F-licensed clauses; callbacks receive owned literal tuples.

        The native scratch buffer is valid only during delivery. Do not re-enter
        the session or an execution view from this callback. An optional work
        budget bounds licensing per query without changing SAT search/results.
        """
        self._check()
        if not isinstance(max_length, int) or not 0 <= max_length < 2**31:
            raise ValueError('maximum learned-clause length must fit a nonnegative int')
        ticks = 2**64-1 if work_budget is None else work_budget
        if not isinstance(ticks, int) or not 0 <= ticks < 2**64:
            raise ValueError('learning work budget must fit uint64')
        def notify(_, clause):
            self._lib._callback_depth += 1
            try:
                values = []
                for i in range(max_length+1):
                    value = clause[i]
                    if not value:
                        learn(tuple(values))
                        return
                    values.append(value)
                raise RuntimeError('unterminated learned clause')
            except BaseException as error:
                self._callback_error = error
            finally:
                self._lib._callback_depth -= 1
        callback = _Learn(notify) if learn is not None else _Learn()
        if self._lib.dll.tactix_ipasir_set_learn_budget(self._handle, ticks):
            raise RuntimeError('learning work budget rejected')
        self._lib.dll.ipasir_set_learn(self._handle, None, max_length, callback)
        self._learn_callback = callback

    def solve(self, assumptions=()):
        self._check()
        assumptions = tuple(_literal(x) for x in assumptions)
        for x in assumptions:
            self._lib.dll.ipasir_assume(self._handle, x)
        self._callback_error = None
        self._busy = True
        try: status = self._lib.dll.ipasir_solve(self._handle)
        finally: self._busy = False
        if self._callback_error is not None:
            raise RuntimeError('Tactix client callback failed') from self._callback_error
        native = _OwnedResult(self._lib, self._handle) if status in (SAT,UNSAT) else None
        return self._read_result(native,status,assumptions)

    def _read_result(self,native,status,assumptions=()):
        handle = native._handle if native else None
        if native and self._lib.dll.tactix_ipasir_result_status(handle) != status:
            raise RuntimeError('published result status mismatch')
        if native:
            assumptions=tuple(self._lib.dll.tactix_ipasir_result_assumption(handle,i)
                for i in range(self._lib.dll.tactix_ipasir_result_assumptions(handle)))
        model = tuple(self._lib.dll.tactix_ipasir_result_val(handle, v)
                      for v in range(1, self._lib.dll.tactix_ipasir_result_variables(handle)+1)) if status == SAT else ()
        core = tuple(dict.fromkeys(self._lib.dll.tactix_ipasir_result_core_literal(handle, i)
                     for i in range(self._lib.dll.tactix_ipasir_result_core_size(handle)))) if status == UNSAT else ()
        prefix = self._lib.dll.tactix_ipasir_result_bundle(handle) if native else None
        return Result(status, assumptions, model, core, prefix.decode() if prefix else None,
                      self._token, self._revision, native)

    def minimize_core(self, source, *, context=(), max_probes=None):
        """Deletion minimization via this session and its selected strategy.

        The best checked UNSAT cube survives UNKNOWN or a probe limit. No minimum
        cardinality/weight claim is made. Input changes and foreign results are
        rejected; source values and its certificate remain separately available.
        """
        self._check()
        if (not isinstance(source,Result) or source.status != UNSAT or source._owner is not self._token or
                source._revision != self._revision or source._native is None):
            raise ValueError('expected an UNSAT result from this permanent prefix')
        maximum=2**64-1 if max_probes is None else max_probes
        if not isinstance(maximum,int) or not 0<=maximum<2**64:
            raise ValueError('probe limit must fit uint64')
        context = tuple(dict.fromkeys(_literal(x) for x in context))
        if not set(context) <= set(source.assumptions):
            raise ValueError('fixed context must belong to the source assumptions')
        cube=(C.c_int*len(context))(*context)
        dll=self._lib.dll
        self._callback_error=None
        self._busy=True
        try:
            reduction=dll.tactix_ipasir_minimize_core(self._handle,source._native._handle,
                                                     cube,len(context),maximum)
        finally:self._busy=False
        if not reduction:raise ValueError('native minimization request rejected')
        try:
            if self._callback_error is not None:
                raise RuntimeError('Tactix client callback failed') from self._callback_error
            probes=[]
            for i in range(dll.tactix_ipasir_reduction_probes(reduction)):
                status=dll.tactix_ipasir_reduction_probe_status(reduction,i)
                assumptions=tuple(dll.tactix_ipasir_reduction_probe_assumption(reduction,i,j)
                    for j in range(dll.tactix_ipasir_reduction_probe_assumptions(reduction,i)))
                native=_OwnedResult(self._lib,handle=dll.tactix_ipasir_reduction_probe_result(reduction,i)) if status else None
                probes.append(self._read_result(native,status,assumptions))
            native=_OwnedResult(self._lib,handle=dll.tactix_ipasir_reduction_certificate(reduction))
            certificate=self._read_result(native,UNSAT)
            certificate=next((r for r in (source,*probes) if r.bundle==certificate.bundle),certificate)
            core=tuple(dll.tactix_ipasir_reduction_core_literal(reduction,i)
                       for i in range(dll.tactix_ipasir_reduction_core_size(reduction)))
            fixed=tuple(dll.tactix_ipasir_reduction_context_literal(reduction,i)
                        for i in range(dll.tactix_ipasir_reduction_context_size(reduction)))
            witnesses=tuple((dll.tactix_ipasir_reduction_witness_literal(reduction,i),
                             probes[dll.tactix_ipasir_reduction_witness_probe(reduction,i)])
                            for i in range(dll.tactix_ipasir_reduction_witnesses(reduction)))
            return CoreReduction(source,core,fixed,certificate,
                                 bool(dll.tactix_ipasir_reduction_minimal(reduction)),tuple(probes),witnesses)
        finally:dll.tactix_ipasir_reduction_release(reduction)
