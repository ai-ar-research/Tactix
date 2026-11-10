"""Independent demo3 search scheduler used by public API tests."""
from tactix import capi
import math


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
        if execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE) != 0:
            raise RuntimeError("cannot configure scripted guidance publication")
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
