"""Python reference policies for C/Python scheduling equivalence tests."""
from tactix import capi
import math
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
        self.last_reduce_conflicts = _U64
        self.clocks = {group: InprocessClock() for group in ('Probe', 'Eliminate')}

    @staticmethod
    def observe(view):
        return {name: view.tactix_stat(getattr(capi, 'TACTIX_STAT_' + name))
                for name in ('CONFLICTS', 'REDUCTIONS', 'BVE_MARKS', 'ROOT_FIXED',
                             'SUBSUME_MATERIAL', 'IRREDUNDANT_CLAUSES', 'SEARCH_TICKS')}

    def note_reduce(self, observation):
        self.last_reduce_conflicts = observation['CONFLICTS']

    def enter_phase(self, observation, group):
        """Record entry before root preparation, without consuming material."""
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

    def execute_pipeline(self, view, group, run_rounds, *, run_startup=None, factor_bound=0,
                         initial_fast_elimination=False):
        """Execute the ordinary phase bracket through public C operations.

        run_rounds owns rule selection, budgets and BVE-bound progression and
        returns PhaseOutcome. Optional Probe startup runs before occurrences
        and returns 0/10/20. Parameters and fast-entry candidate selection
        remain caller-owned; this is not the full native policy.
        """
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
            try:
                return support(name)
            except BaseException as error:
                self.unusable = True
                if primary is None:
                    raise
                primary.phase_cleanup_errors = getattr(primary, 'phase_cleanup_errors', []) + [error]
        self.enter_phase(self.observe(view), group)
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
            if initial_fast_elimination:
                cleanup('RESCHEDULE_BVE', primary)

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
