"""Python-owned DefaultPolicy structured-root scheduling and ownership."""
import ctypes as C
import re
from dataclasses import dataclass, field
from types import MappingProxyType
from . import capi
from .program import DefaultStage
from .sequence import OnlineSequence, strategy_state, _U64


@dataclass
class StructuredOwnership:
    grew_root_trail: bool = False
    external_begin: int = 0
    external_end: int = 0
    symmetry_applied: bool = False
    coupled_factor_ranges: list = field(default_factory=list)


@dataclass
class StageTelemetry:
    runs: int = 0
    advances: int = 0
    applications: int = 0
    effective: int = 0
    sequence_passes: int = 0
    terminal: int = capi.TACTIX_ONLINE_COMPLETED


@dataclass
class RootTelemetry(StageTelemetry):
    sr_discovery_ticks: int = 0
    sr_discovery_progress_window: int = 0
    sr_discovery_renewals: int = 0
    sr_discovery_overflow: bool = False
    stages: list = field(default_factory=list)


class StructuredRoot:
    """Run the native authored stages using Python scheduling decisions.

    Call at a materialized root, within the caller's Startup scope if needed,
    or at Probe entry before its occurrence session opens.
    An instance consumes the structured opportunity once. No native scheduler
    or solve handoff is used. Configuration defaults are resolved native data;
    callers may supply numeric inprocess settings; stage content stays native.
    Callers own placement/deferred-preprocessing gates, materialization,
    post-ULC BCE cleanup and terminal query acceptance.
    A rejected candidate returns zero with query_rejected_model set; the caller
    must restart it after closing any surrounding phase, as native DefaultPolicy
    does. Ownership and terminal telemetry retain the native candidate boundary.
    The optional observer receives actual atomic results and their before-call
    allowance metadata for completed advances. Interrupted dispatches may unwind
    before notification. It must not mutate the execution or controller.
    """
    def __init__(self, execution, *, config=None, observer=None):
        self.execution = execution
        self.definition = execution.default_definition
        config = dict(self.definition.section('inprocess') if config is None else config)
        for suffix in ('large_domain_limit', 'backtracking_domain_limit', 'sr_discovery_ticks',
                       'progress_minimum_ticks', 'progress_ticks_per_variable'):
            key = 'structured_root_' + suffix
            value = config.get(key)
            if type(value) is not int or not 0 <= value <= _U64:
                raise ValueError(f'{key} must be a uint64')
        for suffix in ('sr_discovery_unlimited', 'progress_feedback', 'large_domain_progress_feedback'):
            key = 'structured_root_' + suffix
            if type(config.get(key)) not in (int, bool) or config[key] not in (0, 1):
                raise ValueError(f'{key} must be boolean')
        self._config = MappingProxyType(config)
        self.observer = observer
        self.consumed = False
        self.ownership = StructuredOwnership()
        self.telemetry = RootTelemetry()
        self._depth = None
        self._running = False
        self._phase_open = False
        self._ownership_captured = False
        self.query_rejected_model = False

    @property
    def config(self): return self._config

    def _execution_state(self):
        state = capi.tactix_execution_state()
        if self.execution.c.tactix_read_execution_state(C.byref(state)):
            raise RuntimeError('cannot observe structured-root context')
        return state

    def _support(self, name, argument=0):
        code = self.execution.c.tactix_support(getattr(capi, 'TACTIX_SUPPORT_' + name), argument)
        if code < 0: raise RuntimeError(f'structured-root support {name} refused')

    def _phase_state(self):
        state = capi.tactix_inprocess_state()
        if self.execution.c.tactix_read_inprocess_state(C.byref(state)):
            raise RuntimeError('cannot observe inprocessing phase')
        return state

    def close(self):
        if self._running: raise RuntimeError('cannot close a running StructuredRoot')
        self._close()

    def _close(self):
        if self._depth is not None:
            state = self._execution_state()
            phase = self._phase_state()
            if (state.context_depth != self._depth or state.context_top != capi.TACTIX_CONTEXT_INPROCESSING
                    or bool(phase.phase_open) != self._phase_open or phase.occurrences_open
                    or phase.phase_eliminate):
                raise RuntimeError('close foreign phases/contexts before StructuredRoot')
            self._support('LEAVE_CONTEXT')
            self._depth = None

    def run(self):
        self.execution._check()
        if self._running: raise RuntimeError('structured-root re-entry')
        if self.consumed or self.execution.c.tactix_stat(capi.TACTIX_STAT_CONFLICTS): return 0
        state = self._execution_state()
        phase = self._phase_state()
        if (phase.occurrences_open or phase.phase_eliminate or self.execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL)
                or not self.execution.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
                or self.execution.c.tactix_stat(capi.TACTIX_STAT_COLD_STATE_DEFERRED)):
            raise RuntimeError('StructuredRoot requires a materialized root before occurrences')
        if self.execution.default_definition.text != self.definition.text:
            raise RuntimeError('native strategy settings changed; recreate StructuredRoot')
        before = strategy_state(self.execution)
        self._ownership_before = before
        self.consumed, self._running = True, True
        self._phase_open = bool(phase.phase_open)
        self.ownership = StructuredOwnership(external_begin=state.external_variables,
                                             external_end=state.external_variables)
        primary = None
        try:
            return self._run(state.context_depth)
        except BaseException as error:
            primary = error
            raise
        finally:
            try:
                if not self._ownership_captured: self._capture_ownership(before)
            except BaseException as cleanup:
                if primary is None: raise
                primary.structured_cleanup_error = cleanup
            finally:
                try:
                    self._close()
                except BaseException as cleanup:
                    if primary is None: raise
                    primary.structured_context_error = cleanup
                finally:
                    self._running = False

    def _capture_ownership(self, before):
        after = strategy_state(self.execution)
        self.ownership.grew_root_trail = after.trail_size > before.trail_size
        self.ownership.symmetry_applied = after.sr_applied > before.sr_applied
        self.ownership.external_end = self._execution_state().external_variables
        self._ownership_captured = True

    def _run(self, depth):
        config, total = self._config, self.telemetry
        total.runs = (total.runs + 1) & _U64
        variables = self.execution.c.tactix_stat(capi.TACTIX_STAT_VARIABLES)
        large = variables > config['structured_root_large_domain_limit']
        bounded = variables >= config['structured_root_backtracking_domain_limit']
        family, suffix = ((capi.TACTIX_DEFAULT_STAGE_LARGE_DOMAIN, 'large_stage') if large else
            (capi.TACTIX_DEFAULT_STAGE_BOUNDED_BACKTRACKING, 'bounded_backtracking_stage') if bounded else
            (capi.TACTIX_DEFAULT_STAGE_REGULAR, 'stage'))
        prefix = 'inprocess.structured_root_' + suffix
        indices = sorted(int(key[len(prefix):]) for key in self.definition
                         if re.fullmatch(re.escape(prefix) + r'\d+', key))
        if indices != list(range(len(indices))): raise ValueError('noncontiguous native structured stages')
        self.family = family
        if not indices: return 0
        self._depth = depth + 1
        try:
            self._support('ENTER_CONTEXT', capi.TACTIX_CONTEXT_INPROCESSING)
        except BaseException:
            if self._execution_state().context_depth <= depth:
                self._depth = None
            raise
        hard_unlimited = bool(config['structured_root_sr_discovery_unlimited'])
        hard = config['structured_root_sr_discovery_ticks']
        window = 0
        if config['structured_root_progress_feedback'] and (not large or config['structured_root_large_domain_progress_feedback']):
            window = max(config['structured_root_progress_minimum_ticks'],
                         min(_U64, config['structured_root_progress_ticks_per_variable'] * variables))
        limited = bool(window and (hard_unlimited or hard > window))
        granted = window if limited else _U64 if hard_unlimited else hard
        pool = capi.tactix_online_discovery_budget(limit=window if limited else hard,
            unlimited=hard_unlimited and not limited, ticks_per_generator=window,
            hard_limit=_U64 if hard_unlimited else hard)
        total.sr_discovery_progress_window = window
        total.stages = [StageTelemetry() for _ in indices]
        before = strategy_state(self.execution)
        prior_progress = False
        for index in indices:
            current = strategy_state(self.execution)
            stage_stats = total.stages[index]
            stage_stats.runs += 1
            with DefaultStage(self.execution, family, index) as stage:
                steps = stage.steps_after_feedback(prior_progress,
                    current.unique_literal_amo_binaries > before.unique_literal_amo_binaries,
                    current.nonforcing_cuts_declined > before.nonforcing_cuts_declined)
                source = stage.program
                program = capi.tactix_online_program_v2(steps, len(steps), source.repeat_mode, source.repeat_limit)
                with OnlineSequence(self.execution, program, on_budget_exhaustion=stage.on_budget_exhaustion,
                        progress_followups=stage.progress_followups, before_progress=stage.before_progress) as sequence:
                    try:
                        while not sequence.complete:
                            dispatched = sequence.dispatches[:] if self.observer else None
                            spent = [sequence.consumed(i) for i in range(len(steps))] if self.observer else None
                            shared_before, granted_before = pool.consumed, granted
                            event = sequence.advance(pool)
                            r = event.base
                            if self.observer:
                                for position, count in enumerate(sequence.dispatches):
                                    if count != dispatched[position]:
                                        step = steps[position].base
                                        allowance = 0 if step.budget_unlimited else max(0, step.budget-spent[position])
                                        self.observer(index, step.step_id, event, bool(step.budget_unlimited),
                                                      allowance, shared_before, granted_before)
                            total.sr_discovery_ticks = pool.consumed
                            total.sr_discovery_overflow = bool(pool.accounting_overflow)
                            for stats in (stage_stats, total):
                                stats.advances = (stats.advances + 1) & _U64
                                stats.applications = (stats.applications + r.applications) & _U64
                                stats.effective = (stats.effective + bool(r.made_rule_progress)) & _U64
                                stats.terminal = r.status
                            units = min(_U64, event.discovery_progress_units + bool(r.made_rule_progress)
                                        + bool(event.discovery_entry_required))
                            if window and units and not pool.accounting_overflow:
                                renewed = min(_U64, max(granted, pool.consumed) + min(_U64, window * units))
                                if not hard_unlimited: renewed = min(renewed, hard)
                                if renewed > granted:
                                    granted = pool.limit = renewed
                                    pool.unlimited = 0
                                    total.sr_discovery_renewals = (total.sr_discovery_renewals + 1) & _U64
                            if event.query_rejected_model:
                                # Native captures the candidate trail before its
                                # caller rejects it and restarts. Keep that
                                # ownership fact without proposing query SAT.
                                self._capture_ownership(self._ownership_before)
                                stage_stats.terminal = total.terminal = capi.TACTIX_ONLINE_SAT
                                self.query_rejected_model = True
                                return 0
                            if r.status in (capi.TACTIX_ONLINE_SAT, capi.TACTIX_ONLINE_UNSAT):
                                return 10 if r.status == capi.TACTIX_ONLINE_SAT else 20
                            if r.status in (capi.TACTIX_ONLINE_COMPLETED, capi.TACTIX_ONLINE_NO_CHANGE,
                                            capi.TACTIX_ONLINE_SCOPED_FIXPOINT): continue
                            if r.status == capi.TACTIX_ONLINE_BUDGET_EXHAUSTED:
                                if (stage.on_budget_exhaustion == capi.TACTIX_SEQUENCE_BUDGET_CONTINUE or
                                        stage.on_budget_exhaustion == capi.TACTIX_SEQUENCE_BUDGET_FINISH_ROUND and not sequence.complete):
                                    continue
                                return 0
                            if r.status == capi.TACTIX_ONLINE_CONFLICT:
                                raise RuntimeError('root preprocessing returned a conflict')
                            return 0
                    finally:
                        stage_stats.sequence_passes = sequence.rounds
                        total.sequence_passes = (total.sequence_passes + sequence.rounds) & _U64
            prior_progress |= bool(stage_stats.effective)
        return self.execution.c.tactix_status() if self._phase_state().inconsistent else 0
