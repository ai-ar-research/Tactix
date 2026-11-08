"""Python reference policies for C/Python scheduling equivalence tests."""
from tactix import capi

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
            self.trace.append((rules[name],code));return code
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
