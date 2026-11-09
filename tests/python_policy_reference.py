"""Python reference policies for C/Python scheduling equivalence tests."""
from tactix import capi
import math
import ctypes as C
from dataclasses import dataclass

_U64 = (1 << 64) - 1


@dataclass
class InprocessClock:
    next_conflicts: int = 0
    rounds: int = 0
    reductions_mark: int = 0
    marks_mark: int = 0
    fixed_mark: int = 0
    material_mark: int = 0
    blocked: bool = False
    blocked_episodes: int = 0
    anchor: int = 0


@dataclass
class PhaseOutcome:
    status: int = 0
    bve_ran: bool = False
    maximum_bound: bool = False
    eliminate_limit_reached: bool = False


@dataclass
class InprocessSlot:
    rule: str
    group: str = 'Probe'
    permille: int = 0
    budget_aware: bool = False
    doses: int = 0
    forward_pair: bool = False
    limit: int = 0
    wrapped: int = 0
    enabled: bool = True
    preprocess: bool = False
    defer_preprocess: bool = False
    max_applies: int = 0
    applies: int = 0
    followup: str = 'None'
    followup_mode: str = 'None'
    decline: str = ''
    handed: int = 0
    spent: int = 0


class NativeInprocessSchedule:
    """Independent native phase clocks; not yet a complete DefaultPolicy.

    Configuration comes from the C reference's native defaults. The Python
    caller executes phase actions and chooses entry/exit and reduction events.
    No native scheduling delegate or trace replay is used.
    """
    def __init__(self, config):
        self.config = config
        self.initialized = False
        self.unusable = False
        self.phase = None
        self.last_reduce_conflicts = _U64
        self.forward_pair_limit = 0
        self.clocks = {group: InprocessClock() for group in ('Probe', 'Eliminate')}

    def _cleanup(self, action, primary=None):
        try:
            return action()
        except BaseException as error:
            self.unusable = True
            if primary is None:
                raise
            primary.phase_cleanup_errors = getattr(primary, 'phase_cleanup_errors', []) + [error]

    def begin_round(self, view=None):
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        if view is not None:
            result = view.tactix_support(capi.TACTIX_SUPPORT_BEGIN_FORWARD_ROUND, 0)
            if result < 0:
                self.unusable = True
                raise RuntimeError('forward round support failed')
        self.forward_pair_limit = 0

    def reset(self, slots):
        if self.phase is not None:
            raise RuntimeError('cannot reset an active phase')
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        self.initialized = False
        self.last_reduce_conflicts = _U64
        self.forward_pair_limit = 0
        self.clocks = {group: InprocessClock() for group in self.clocks}
        for slot in slots:
            slot.limit = slot.applies = slot.doses = slot.wrapped = 0
            slot.handed = slot.spent = 0
            slot.decline = ''

    def refresh_entry_limit(self, slot, observation, meter, outer, **flags):
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        if outer < 0:
            raise ValueError('invalid phase round')
        if slot.rule == 'Substitute' or slot.group == 'Probe' or not outer or slot.forward_pair:
            self.set_entry_limit(slot, observation, meter, **flags)
        return slot.limit

    def batch_admissible(self, slot, spent):
        """Check before Factor, closure, delay and rule guards, as native does."""
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        if slot.rule != 'Substitute' and spent > slot.limit:
            slot.decline = 'budget'
            return False
        return True

    def run_budgeted_batch(self, view, slot, meter, apply):
        """Run an admitted, guarded batch; apply owns typed rule parameters.

        Call batch_admissible before stateful entry guards; recheck here before
        installing wires. The full slot runner must preserve that ordering.

        Factor matching, Substitute rounds, guidance publication and completion
        marks remain caller-owned. Work wires follow native ClearBudget scope.
        """
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        before = meter(view)
        if not self.batch_admissible(slot, before):
            return 1
        remaining = max(0, slot.limit - before)
        if before > slot.limit:
            slot.wrapped = (slot.wrapped + 1) & _U64
        handed = remaining if slot.rule == 'Substitute' else max(1, remaining)
        saved = capi.tactix_work_settings()
        if view.tactix_read_work_settings(C.byref(saved)) < 0:
            raise RuntimeError('cannot read work settings')
        fields = capi.TACTIX_WORK_BUDGET | capi.TACTIX_WORK_MIDROUND
        posed = capi.tactix_work_settings(fields=fields, budget=handed,
            midround=self.config['forward_midround_budget'] and
                     slot.rule in ('SubsumeForward', 'Strengthen'))
        primary = None
        try:
            if view.tactix_install_work_settings(C.byref(posed)) < 0:
                raise RuntimeError('cannot install work settings')
            result = apply(view)
            if result < 0:
                raise RuntimeError('batch rule failed: ' + slot.rule)
        except BaseException as error:
            primary = error
            self.unusable = True
            raise
        finally:
            posed.budget, posed.midround = 0, saved.midround
            def clear():
                if view.tactix_install_work_settings(C.byref(posed)) < 0:
                    raise RuntimeError('cannot clear batch work settings')
            self._cleanup(clear, primary)
        spent = (meter(view) - before) & _U64
        slot.handed = (slot.handed + handed) & _U64
        slot.spent = (slot.spent + spent) & _U64
        if spent:
            slot.doses = (slot.doses + 1) & ((1 << 32) - 1)
        return result

    def run_typed_batch(self, view, slot, meter, parameters):
        """One caller-selected typed canonical application, with owned feedback.

        The independent continuation copies the program and is released on
        every exit. The caller still handles interruption, query rejection,
        completion marks and propagation from the returned execution packet.
        The action code follows public application codes, derived from the
        published status; packet.base.rule_result retains its native enum value.
        A budget decline returns (1, None), without creating a continuation.
        """
        packet = None
        def apply(view):
            nonlocal packet
            rule = next((i for i in range(view.tactix_rule_count())
                         if view.tactix_rule_name(i).decode() == slot.rule), None)
            if rule is None:
                raise ValueError('unknown batch rule: ' + slot.rule)
            step = capi.tactix_online_step(step_id=rule + 1, rule_id=rule,
                parameters=parameters, granularity=capi.TACTIX_ONLINE_GRANULARITY_CANONICAL,
                repeat_mode=capi.TACTIX_ONLINE_REPEAT_ONCE, budget_unlimited=1,
                rule_budget_mode=capi.TACTIX_ONLINE_RULE_BUDGET_INHERIT)
            program = capi.tactix_online_program(C.pointer(step), 1,
                                                capi.TACTIX_ONLINE_REPEAT_ONCE, 0)
            executor = None
            try:
                executor = view.tactix_online_executor_create()
                if not executor:
                    raise RuntimeError('cannot create typed batch continuation')
                if view.tactix_online_executor_configure(executor, 1, C.byref(program), 0,
                        capi.TACTIX_ONLINE_RECONFIGURE_RESET) < 0:
                    raise ValueError('invalid typed batch: ' + slot.rule)
                packet = capi.tactix_online_execution_result()
                if view.tactix_online_executor_advance(executor, None, C.byref(packet)) < 0:
                    raise RuntimeError('typed batch advance failed')
                if packet.base.status >= capi.TACTIX_ONLINE_INVALID_REQUEST:
                    error = RuntimeError('typed batch status: ' + str(packet.base.status))
                    error.execution_result = packet
                    raise error
                return {capi.TACTIX_ONLINE_SAT: 10, capi.TACTIX_ONLINE_UNSAT: 20,
                        capi.TACTIX_ONLINE_CONFLICT: 2}.get(packet.base.status,
                            0 if packet.base.made_rule_progress else 1)
            finally:
                if executor:
                    view._library.tactix_online_executor_release(executor)
        result = self.run_budgeted_batch(view, slot, meter, apply)
        return result, packet

    def _fast_phase(self, requested):
        if self.phase is not None:
            current = self.phase[1]
            if requested is not None and requested != current:
                raise ValueError('fast-elimination mode disagrees with active phase')
            return current
        return bool(requested)

    def admit_slot(self, slot, observation, *, congruence_changed=False,
                   sweep_changed=False, phase_unsat=False, initial_fast_elimination=None):
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        initial_fast_elimination = self._fast_phase(initial_fast_elimination)
        slot.followup_mode, slot.decline = 'None', ''
        startup = not observation['CONFLICTS']
        if startup and slot.followup != 'None' and self.config['preprocess_followups']:
            if slot.followup == 'AfterCongruence':
                slot.followup_mode = 'Eager' if congruence_changed else 'None'
            elif slot.followup == 'AfterSweepOrFinal':
                slot.followup_mode = ('Eager' if sweep_changed else
                                      'None' if congruence_changed else 'Final')
            else:
                raise ValueError('invalid followup')
            if slot.followup_mode == 'None':
                slot.decline = 'preprocess'
                return False
        fast_bve = initial_fast_elimination and slot.rule == 'BVE'
        if not slot.enabled or phase_unsat:
            slot.decline = 'disabled'
        elif startup and not slot.preprocess and not fast_bve:
            slot.decline = 'preprocess'
        elif slot.max_applies and slot.applies >= slot.max_applies:
            slot.decline = 'cap'
        elif startup and slot.defer_preprocess and not fast_bve:
            slot.decline = 'preprocess'
        else:
            slot.applies = (slot.applies + 1) & ((1 << 32) - 1)
            return True
        return False

    def set_entry_limit(self, slot, observation, meter, *, initial_fast_elimination=None):
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        permille = (self.config['initial_fast_elimination_bve_permille']
                    if self._fast_phase(initial_fast_elimination) and slot.rule == 'BVE' else slot.permille)
        if not 0 <= permille <= 1000 or not 0 <= meter <= _U64:
            raise ValueError('invalid effort budget')
        now, anchor = observation['SEARCH_TICKS'], self.clocks[slot.group].anchor
        reference = max(0, now - anchor)
        if now < anchor:
            slot.wrapped = (slot.wrapped + 1) & _U64
        if not observation['CONFLICTS']:
            reference = self.config['preprocess_initial_ticks']
        elif slot.rule != 'Substitute':
            reference = max(reference, self.config['minimum_effort_ticks'])
        slot.limit = (meter + reference * permille // 1000) & _U64
        if slot.forward_pair:
            if self.forward_pair_limit:
                slot.limit = self.forward_pair_limit
            else:
                self.forward_pair_limit = slot.limit
        if not slot.doses and slot.budget_aware and slot.rule == 'Factor':
            slot.limit = (meter + self.config['factor_initial_ticks']) & _U64
        return slot.limit

    @staticmethod
    def observe(view):
        return {name: view.tactix_stat(getattr(capi, 'TACTIX_STAT_' + name))
                for name in ('CONFLICTS', 'REDUCTIONS', 'BVE_MARKS', 'ROOT_FIXED',
                             'SUBSUME_MATERIAL', 'IRREDUNDANT_CLAUSES', 'SEARCH_TICKS')}

    def note_reduce(self, observation):
        self.last_reduce_conflicts = observation['CONFLICTS']

    def enter_phase(self, observation, group):
        """Record entry before root preparation, without consuming material."""
        self.forward_pair_limit = 0
        clock = self.clocks[group]
        clock.reductions_mark = observation['REDUCTIONS']
        clock.marks_mark = observation['BVE_MARKS']
        clock.fixed_mark = observation['ROOT_FIXED']
        clock.blocked = False

    def leave_phase(self, observation, group, *, bve_ran=False, maximum_bound=False,
                    initial_fast_elimination=False, unsat=False, reschedule_bve=None):
        """Finish the ordinary phase exit; early terminal paths skip this call.

        An advancing BVE bound requires the caller's rescheduling callback,
        before clearing retry marks. Raise on API errors, accept a normal
        no-op. Return whether it ran. maximum_bound describes the bound used
        by this phase, before advancing. Only that exit consumes material.
        The initial-fast wrapper separately calls finish_initial_fast_elimination.
        """
        clock = self.clocks[group]
        reschedule = False
        if group == 'Eliminate' and bve_ran and not initial_fast_elimination and not unsat:
            if maximum_bound:
                clock.marks_mark = observation['BVE_MARKS']
                clock.fixed_mark = observation['ROOT_FIXED']
                clock.material_mark = observation['SUBSUME_MATERIAL']
            else:
                if reschedule_bve is None:
                    raise ValueError('advancing a BVE bound requires rescheduling')
                reschedule_bve()
                clock.marks_mark = clock.fixed_mark = 0
                reschedule = True
        if not initial_fast_elimination and (group != 'Probe' or observation['CONFLICTS']):
            clock.anchor = observation['SEARCH_TICKS']
        if not unsat and not initial_fast_elimination:
            clock.rounds = (clock.rounds + 1) & _U64
            self.update_limit(observation, group)
        return reschedule

    @staticmethod
    def finish_initial_fast_elimination(reschedule_bve):
        """Rearm candidates after the fast phase, including terminal exits."""
        reschedule_bve()

    def run_rounds(self, view, group, slots, run_slot, *, stopped=lambda: False,
                   initial_fast_elimination=None):
        """Native outer fixpoint; run_slot owns each admitted rule application.

        The callback receives (view, slot, outer) and returns PhaseOutcome.
        It owns candidate discovery, guards, budgets, publication, propagation
        and the BVE bound. This loop owns round order and stopping only.
        """
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        if self.phase is None:
            raise RuntimeError('round execution requires an active phase')
        initial_fast_elimination = self._fast_phase(initial_fast_elimination)
        if self.phase is not None and self.phase[0] != group:
            raise ValueError('round group disagrees with active phase')
        if group not in self.clocks or initial_fast_elimination and group != 'Eliminate':
            raise ValueError('invalid round group')
        slots = tuple(slots)
        startup = not view.tactix_stat(capi.TACTIX_STAT_CONFLICTS)
        rounds = (self.config['initial_fast_elimination_rounds'] if initial_fast_elimination
                  else 1 if startup else self.config['phase_rounds'])
        active = view.tactix_stat(capi.TACTIX_STAT_ACTIVE_VARIABLES)
        outcome = PhaseOutcome()
        try:
            for outer in range(rounds):
                if stopped():
                    break
                self.begin_round(view)
                for slot in slots:
                    if slot.group != group or initial_fast_elimination and slot.rule != 'BVE':
                        continue
                    if stopped():
                        break
                    result = run_slot(view, slot, outer)
                    if not isinstance(result, PhaseOutcome) or result.status not in (0, 20):
                        raise ValueError('invalid slot outcome')
                    outcome.bve_ran |= result.bve_ran
                    outcome.maximum_bound |= result.maximum_bound
                    outcome.eliminate_limit_reached |= result.eliminate_limit_reached
                    if result.status == 20 or view.tactix_status() == 20:
                        outcome.status = 20
                        return outcome
                if outcome.eliminate_limit_reached:
                    break
                pending = (group == 'Eliminate' and self.config['hold_subsume_until_eliminate']
                           and (not view.tactix_stat(capi.TACTIX_STAT_SUBSUME_COMPLETE)
                                or not view.tactix_stat(capi.TACTIX_STAT_STRENGTHEN_COMPLETE)))
                current = view.tactix_stat(capi.TACTIX_STAT_ACTIVE_VARIABLES)
                if current == active and not pending:
                    break
                active = current
            return outcome
        except BaseException:
            self.unusable = True
            raise

    def execute_pipeline(self, view, group, run_rounds, *, run_startup=None, factor_bound=0,
                         initial_fast_elimination=False):
        """Execute the ordinary phase bracket through public C operations.

        run_rounds owns rule selection, budgets and BVE-bound progression and
        returns PhaseOutcome. Optional Probe startup runs before occurrences
        and returns 0/10/20. Parameters and fast-entry candidate selection
        remain caller-owned; this is not the full native policy.
        """
        if self.phase is not None:
            raise RuntimeError('inprocessing phase is already active')
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        if (group not in self.clocks or not 0 <= factor_bound <= (1 << 32) - 1 or
                run_startup is not None and group != 'Probe' or
                initial_fast_elimination and group != 'Eliminate'):
            raise ValueError('invalid phase configuration')
        def support(name, argument=0):
            result = view.tactix_support(getattr(capi, 'TACTIX_SUPPORT_' + name), argument)
            if result < 0:
                raise RuntimeError('phase support failed: ' + name)
            return result
        def reschedule():
            return support('RESCHEDULE_BVE')
        def cleanup(name, primary=None):
            return self._cleanup(lambda: support(name), primary)
        self.enter_phase(self.observe(view), group)
        self.phase = (group, bool(initial_fast_elimination))
        primary = None
        try:
            if group == 'Eliminate' and not initial_fast_elimination:
                support('BEGIN_BVE_DEFINITION_SESSION')
            if view.tactix_status() == 20:
                return 20
            flags = capi.TACTIX_ROOT_PREPARATION_CONSUME_ROOT_PREFIX
            flags |= (capi.TACTIX_ROOT_PREPARATION_PROBING if group == 'Probe' else
                      capi.TACTIX_ROOT_PREPARATION_CAPTURE_TARGET)
            if support('ROOT_PREPARATION', flags) == 20:
                return 20
            argument = factor_bound << capi.TACTIX_PHASE_FACTOR_BOUND_SHIFT
            if group == 'Eliminate':
                argument |= capi.TACTIX_PHASE_ELIMINATE
            support('ENTER_INPROCESS_PHASE', argument)
            startup_status = 0
            body_error = None
            try:
                if run_startup is not None:
                    startup_status = run_startup(view)
                    if startup_status not in (0, 10, 20):
                        raise ValueError('invalid startup status')
                    if startup_status == 0:
                        startup_status = view.tactix_status()
                if startup_status:
                    outcome = PhaseOutcome(startup_status)
                else:
                    support('OPEN_OCCURRENCE_SESSION')
                    outcome = run_rounds(view)
                    if not isinstance(outcome, PhaseOutcome) or outcome.status not in (0, 20):
                        raise ValueError('ordinary phase requires a non-SAT PhaseOutcome')
            except BaseException as error:
                body_error = error
                raise
            finally:
                exit_status = cleanup('LEAVE_INPROCESS_PHASE', body_error)
            status = view.tactix_status()
            if startup_status == 10:
                if status != 10:
                    raise ValueError('startup SAT disagrees with solver')
                return 10
            unsat = status == 20
            if not unsat and (outcome.status == 20 or exit_status == 20):
                raise ValueError('phase UNSAT disagrees with solver')
            self.leave_phase(self.observe(view), group, bve_ran=outcome.bve_ran,
                             maximum_bound=outcome.maximum_bound,
                             initial_fast_elimination=initial_fast_elimination,
                             unsat=unsat, reschedule_bve=reschedule)
            return 20 if unsat else 0
        except BaseException as error:
            primary = error
            self.unusable = True
            raise
        finally:
            try:
                if initial_fast_elimination:
                    cleanup('RESCHEDULE_BVE', primary)
            finally:
                self.phase = None

    def want_phase(self, observation, group):
        if self.unusable:
            raise RuntimeError('failed phase; discard the schedule')
        clock = self.clocks[group]
        conflicts = observation['CONFLICTS']
        if (not self.initialized or
            self.config['preprocess_only'] and conflicts or
            group == 'Eliminate' and not conflicts or
            self.last_reduce_conflicts == conflicts):
            return False
        startup = group == 'Probe' and not clock.rounds and conflicts == clock.next_conflicts
        early = (conflicts <= clock.next_conflicts if group == 'Probe' else
                 conflicts < clock.next_conflicts)
        if not startup and early:
            return False
        blocked = self.config['watermarks'] and clock.rounds and (
            observation['REDUCTIONS'] == clock.reductions_mark if group == 'Probe' else
            observation['BVE_MARKS'] <= clock.marks_mark and
            observation['ROOT_FIXED'] <= clock.fixed_mark and
            observation['SUBSUME_MATERIAL'] <= clock.material_mark)
        if blocked:
            if not clock.blocked:
                clock.blocked = True
                clock.blocked_episodes = (clock.blocked_episodes + 1) & _U64
            return False
        return True

    def update_limit(self, observation, group):
        clock = self.clocks[group]
        split = self.config['split_clocks']
        rounds = max(1, clock.rounds if split else
                     max(c.rounds for c in self.clocks.values()))
        lg = math.log10(float(rounds) + self.config['logarithm_offset'])
        if split and group == 'Probe':
            base = float(self.config['probe_interval']) * float(rounds) * lg
        else:
            base = float(self.config['eliminate_interval']) * float(rounds) * lg * lg
        scale = (math.log10(float(observation['IRREDUNDANT_CLAUSES']) + 1.0 +
                            self.config['logarithm_offset']) - self.config['scale_log_shift'])
        delta = int((self.config['scale_quadratic'] * scale * scale +
                     self.config['scale_constant']) * float(int(base)))
        clock.next_conflicts = (observation['CONFLICTS'] + delta) & _U64


class Smooth:
    """The scripted C example's bias-corrected moving average."""
    def __init__(self, window):
        self.alpha=1.0/window;self.beta=1.0-self.alpha
        self.value=self.biased=0.0;self.exp=1.0

    def update(self, value):
        self.biased+=self.alpha*(value-self.biased)
        if self.exp:
            old=self.exp;self.exp*=self.beta
            if self.exp==old:self.exp=0.0
        self.value=self.biased/(1.0-self.exp) if self.exp else self.biased


class ScriptedDefault:
    """Independent demo3 search scheduler; not the private native DefaultPolicy.

    demo3 explicitly disables its inprocessing slot (UINT64_MAX). This mirror
    retains that scope, all search scheduling and the canonical Lucky attempt.
    It does not use a native scheduling delegate or generated strategy code.
    """
    settings=()

    def __init__(self):self.begin()

    def begin(self):
        self.trace=[];self.requests=self.eligible=self.effective=self.rounds=0
        self.averages=[None,None];self.restart_next=1;self.reduce_next=2000
        self.mode_count=0;self.mode_conflicts_limit=1000
        self.mode_ticks_limit=self.focus_entry=0
        self.rel_u=self.rel_v=1;self.rel_wait=1024;self.rel_trigger=False
        self.rephase_next=1000;self.lucky_tried=False

    def run(self, view, rules, boundary=None):
        def stat(name):return view.tactix_stat(getattr(capi,'TACTIX_STAT_'+name))
        def apply(name, instance=None):
            code=(view.tactix_apply(rules[name]) if instance is None else
                  view.tactix_apply_instance(rules[name],instance))
            self.trace.append((rules[name],code,int(instance is not None),instance or 0));return code
        def restart_limit():
            restarts=stat('RESTARTS')
            self.restart_next=stat('CONFLICTS')+1+(int(math.log10(float(restarts)+9)-1.0) if restarts else 0)
        if view.tactix_status()==20:return 20
        while True:
            if boundary is not None:
                action=boundary()
                if action>=0:return action
            code=apply('Propagate')
            if code==20:return 20
            if not self.lucky_tried and code!=2:
                self.lucky_tried=True
                lucky=apply('Lucky',capi.TACTIX_INSTANCE_CANONICAL)
                if lucky in (10,20):return lucky
            if code==2:
                if apply('ConflictAnalysis')==20:return 20
                stable=bool(stat('STABLE'))
                if self.averages[stable] is None:self.averages[stable]=(Smooth(33.0),Smooth(1e5))
                for average in self.averages[stable]:average.update(float(stat('LAST_LEARNED_GLUE')))
                if stable and not self.rel_trigger:
                    self.rel_wait-=1
                    if not self.rel_wait:
                        if self.rel_u & -self.rel_u == self.rel_v:self.rel_u+=1;self.rel_v=1
                        else:self.rel_v*=2
                        self.rel_wait=self.rel_v*1024
                        if self.rel_wait>1<<20:self.rel_u=self.rel_v=1;self.rel_wait=1024
                        self.rel_trigger=True
                continue
            if boundary is not None and code==0:continue
            if not stat('UNASSIGNED'):return 10
            conflicts=stat('CONFLICTS')
            if conflicts>=self.reduce_next and apply('Reduce')!=-1:
                self.reduce_next=stat('CONFLICTS')+2000+300*stat('REDUCTIONS');continue
            stable=bool(stat('STABLE'))
            want_switch=(stat('TICKS')>=self.mode_ticks_limit if self.mode_count&1 else
                         conflicts>=self.mode_conflicts_limit)
            if want_switch and apply('SwitchMode')!=-1:
                self.mode_count+=1
                if not stable:self.rel_u=self.rel_v=1;self.rel_wait=1024;self.rel_trigger=False
                ticks=stat('TICKS')
                if self.mode_count&1:self.mode_ticks_limit=ticks+(ticks-self.focus_entry)
                else:
                    n=self.mode_count//2+1
                    self.mode_conflicts_limit=stat('CONFLICTS')+int(1000*float(n)*math.pow(math.log10(float(n)+9),4))
                    self.focus_entry=ticks;restart_limit()
                continue
            fast,slow=self.averages[0] or (None,None)
            want_restart=conflicts>=self.restart_next and (self.rel_trigger if stable else
                1.10*(slow.value if slow else 0.0)<=(fast.value if fast else 0.0))
            if want_restart and apply('Restart')!=-1:
                self.rel_trigger=False
                if not stable:restart_limit()
                continue
            if stable and conflicts>self.rephase_next:
                apply('Restart',0)
                rotation=(0,0,1,0,0,2)
                if apply('Rephase',rotation[stat('REPHASED')%6])!=-1:
                    n=stat('REPHASED');lg=math.log10(float(n)+9)
                    self.rephase_next=stat('CONFLICTS')+int(1000*float(n)*lg*lg*lg);continue
            assert apply('Decide')!=-1

    def __call__(self, execution, restored):
        if not restored:self.begin()
        assert execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE)==0
        def boundary():
            while True:
                action=execution.next()
                if action==capi.TACTIX_QUERY_ASSUME:execution.assume_next()
                elif action==capi.TACTIX_QUERY_BACKTRACK:execution.apply('Restart',0)
                elif action in (10,20):return action
                elif action in (capi.TACTIX_QUERY_RESTORE,capi.TACTIX_QUERY_INTERRUPTED,
                                capi.TACTIX_QUERY_INVALID,capi.TACTIX_QUERY_BUSY):return 0
                else:return -1
        rules={execution.c.tactix_rule_name(i).decode():i
               for i in range(execution.c.tactix_rule_count())}
        return self.run(execution.c,rules,boundary)

class Cruxmiter:
    """Python transcription of the C Cruxmiter root program, not a native delegate."""
    settings = (('TACTIX_FORCE_MODE','stable'),('TACTIX_REORDER','0'),
                ('TACTIX_NO_BVE','1'),('TACTIX_CONFLICT_REUSE','0'),
                ('TACTIX_SLOT_PERMILLE','Vivify:200'),('TACTIX_NO_FACTOR','1'),
                ('TACTIX_NO_SWEEP','1'),('TACTIX_SLOT_DISABLE','Backbone'))

    def __init__(self): self.begin()

    def configure(self, session):
        for name,value in self.settings: session.set_default_option(name,value)

    def begin(self):
        self.stage='start';self.phase=self.count=self.round=0
        self.requests=self.eligible=self.effective=self.rounds=0
        self.applications=self.round_applications=self.round_merges=self.round_substitutions=0
        self.trace=[]

    def run(self, view, rules, boundary=lambda:False):
        def apply(name):
            before_m=view.tactix_stat(capi.TACTIX_STAT_CONGRUENCE_MERGES)
            before_v=view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_VARS)
            self.requests+=1;code=view.tactix_apply(rules[name])
            self.eligible+=code>=0;self.effective+=code==0
            self.round_merges+=(view.tactix_stat(capi.TACTIX_STAT_CONGRUENCE_MERGES)-before_m)&((1<<64)-1)
            self.round_substitutions+=(view.tactix_stat(capi.TACTIX_STAT_SUBSTITUTE_VARS)-before_v)&((1<<64)-1)
            self.trace.append((rules[name],code,0,0));return code
        def next_phase():
            self.count=0
            if self.phase==0:self.phase=1;self.stage='apply'
            else:self.stage='end' if self.phase==1 else 'handoff'
        while True:
            if boundary():return 0
            if self.stage=='start':
                if view.tactix_status()==20:return 20
                self.stage='initial'
            elif self.stage=='initial':
                code=apply('Propagate');self.stage='begin'
                if code in (10,20):return code
                if code==2:return 20
            elif self.stage=='begin':
                self.round_applications=self.applications;self.round_merges=self.round_substitutions=0
                self.phase=self.count=0;self.stage='apply'
            elif self.stage=='apply':
                if self.count==(4,24,512)[self.phase]:next_phase();continue
                code=apply(('Congruence','Substitute','Backbone')[self.phase])
                if code in (10,20):return code
                if code==2:return 20
                if code!=0:next_phase();continue
                self.applications+=1;self.stage='propagate'
            elif self.stage=='propagate':
                code=apply('Propagate');self.count+=1;self.stage='apply'
                if code in (10,20):return code
                if code==2:return 20
            elif self.stage=='end':
                self.rounds+=1;self.round+=1
                if self.round==4 or self.round>=2 and not self.round_merges and not self.round_substitutions:
                    self.phase=2;self.count=0;self.stage='apply'
                else:self.stage='begin'
            elif self.stage=='handoff':return 30
            else:raise AssertionError(self.stage)

    def __call__(self, execution, restored):
        if not restored:self.begin()
        assert execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE)==0
        rules={execution.c.tactix_rule_name(i).decode():i
               for i in range(execution.c.tactix_rule_count())}
        result=self.run(execution.c,rules,
                        lambda:execution.next() in (capi.TACTIX_QUERY_RESTORE,
                            capi.TACTIX_QUERY_INTERRUPTED,capi.TACTIX_QUERY_INVALID,capi.TACTIX_QUERY_BUSY))
        return execution.default_solve() if result==30 else result
