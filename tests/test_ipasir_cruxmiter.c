#undef NDEBUG
#include "api_test_support.h"
#include "tactix_ipasir.h"
#include "strategy/presets/cruxmiter.h"
#include "tactix_chvatal.h"
#include "tactix_search_demo.h"
#include "tactix_scripted_default.h"
#include <stdio.h>
#include <stdlib.h>

static void input(void *s,const int *clause) {
  do {ipasir_add(s,*clause);}while(*clause++);
}
static tactix_ipasir_strategy_counters counters(void *s) {
  tactix_ipasir_strategy_counters c={0};c.version=1;c.size=sizeof c;
  assert(!tactix_ipasir_get_strategy_counters(s,&c));return c;
}
static void save(void *s,const char *prefix,unsigned index) {
  if(!prefix)return;
  tactix_ipasir_result *r=tactix_ipasir_result_acquire(s);assert(r);
  const char *suffixes[]={".json",".cnf",".proof",".model",".core","-core.cnf","-core.proof"};
  for(unsigned i=0;i<sizeof suffixes/sizeof *suffixes;++i) {
    char from[4096],to[4096];snprintf(from,sizeof from,"%s%s",tactix_ipasir_result_bundle(r),suffixes[i]);
    FILE *in=fopen(from,"rb");if(!in)continue;
    snprintf(to,sizeof to,"%s-%u%s",prefix,index,suffixes[i]);FILE *out=fopen(to,"wb");assert(out);
    char bytes[4096];size_t n;
    while((n=fread(bytes,1,sizeof bytes,in)))assert(fwrite(bytes,1,n,out)==n);
    assert(!ferror(in)&&!fclose(in)&&!fclose(out));
  }
  tactix_ipasir_result_release(r);
}
static int delegate(void *state,void *session,tactix *view,unsigned cause) {
  (void)session;(void)cause;const int result=tactix_solve(view);
  *(int *)state=(int)tactix_stat(view,TACTIX_STAT_STABLE);return result;
}
static int flag(void *state) {return *(int *)state;}

/* Exercise the shared program's suspension point with a real query repair.
 * BVE is an explicit test-client action; it is not added to the crux schedule. */
typedef struct {void *session;unsigned calls;int paused;} hook_state;
static int boundary(void *state) {
  hook_state *h=state;++h->calls;
  if(h->calls==3&&!h->paused) {h->paused=1;return 1;}
  tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;
  assert(!tactix_ipasir_next(h->session,&q));
  return q.action==TACTIX_QUERY_RESTORE||q.action==TACTIX_QUERY_INTERRUPTED;
}
static void continuation(void) {
  void *s=ipasir_init();assert(s);input(s,(const int[]){1,2,0});ipasir_assume(s,-1);
  tactix *view=NULL;assert(tactix_ipasir_begin(s,&view)==1);
  cruxmiter_program p;assert(cruxmiter_begin(&p));hook_state h={s,0,0};
  assert(!cruxmiter_run(&p,view,boundary,&h,NULL)&&h.paused);
  const unsigned stage=p.stage;const uint64_t requests=p.requests;assert(requests==1);
  assert(tactix_apply_instance(view,rule("BVE"),1)>=0);
  tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;assert(!tactix_ipasir_next(s,&q)&&q.action==TACTIX_QUERY_RESTORE);
  assert(tactix_ipasir_repair(s,&view)==1&&p.stage==stage&&p.requests==requests);
  assert(!tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE));
  int result=cruxmiter_run(&p,view,boundary,&h,NULL);
  if(result==CRUXMITER_HANDOFF)result=tactix_solve(view);
  assert(result==10&&tactix_ipasir_finish(s,result)==10&&ipasir_val(s,1)==-1&&ipasir_val(s,2)==2);
  ipasir_release(s);
}
typedef struct {void *session;chvatal_program *program;int paused;} twin_hook;
static int twin_boundary(void *state) {
  twin_hook *h=state;
  if(!h->paused&&h->program->revision>1) {h->paused=1;return 1;}
  tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;
  assert(!tactix_ipasir_next(h->session,&q));
  return q.action==TACTIX_QUERY_RESTORE||q.action==TACTIX_QUERY_INTERRUPTED;
}
static int twin_stop(void *state) {return ++*(unsigned *)state==2;}
static void chvatal_continuation(void) {
  void *s=ipasir_init();assert(s);
  const int clauses[][4]={{-3,1,0},{-3,2,0},{3,-1,-2,0},{-4,1,0},{-4,2,0},{4,-1,-2,0}};
  for(unsigned i=0;i<6;++i)input(s,clauses[i]);
  ipasir_assume(s,-1);
  tactix *view=NULL;assert(tactix_ipasir_begin(s,&view)==1);
  int literals[]={-3,1,-3,2,3,-1,-2,-4,1,-4,2,4,-1,-2};size_t starts[]={0,2,4,7,9,11,14};
  const struct chvatal_formula formula={literals,starts,6,14,4};
  chvatal_program p={0};unsigned stopped=0;
  assert(!chvatal_begin(&p,&formula,twin_stop,&stopped)&&stopped==2&&!p.pairs&&!p.pair_round);
  assert(chvatal_begin(&p,&formula,NULL,NULL));twin_hook h={s,&p,0};
  assert(!chvatal_run(&p,view,twin_boundary,&h,NULL)&&h.paused&&p.revision==2&&p.index==1&&p.round_accepted==1);
  const chvatal_program before=p;
  assert(tactix_apply_instance(view,rule("BVE"),1)>=0);
  tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;
  assert(!tactix_ipasir_next(s,&q)&&q.action==TACTIX_QUERY_RESTORE);
  assert(tactix_ipasir_repair(s,&view)==1&&!memcmp(&p,&before,sizeof p));
  assert(!tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE));
  int result=chvatal_run(&p,view,twin_boundary,&h,NULL);
  if(result==CHVATAL_HANDOFF)result=tactix_solve(view);
  assert(result==10&&tactix_ipasir_finish(s,10)==10&&ipasir_val(s,1)==-1&&ipasir_val(s,3)==-3&&ipasir_val(s,4)==-4);
  chvatal_release(&p);ipasir_release(s);
}
typedef struct {void *session;tactix *view;tactix_search_demo *program;int paused;
  tactix_scripted_default *scripted;} demo_hook;
static int demo_boundary(void *state) {
  demo_hook *h=state;
  const int progressed=h->scripted?
      h->scripted->avg[0].initialized||h->scripted->avg[1].initialized:
      h->program->steps&&(h->program->kind!=TACTIX_DEMO_DPLL||h->program->pending_purge);
  if(!h->paused&&progressed) {
    h->paused=1;return 0;
  }
  for(;;) {
    tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;
    assert(!tactix_ipasir_next(h->session,&q));
    if(q.action==TACTIX_QUERY_ASSUME)assert(!tactix_ipasir_assume_next(h->session));
    else if(q.action==TACTIX_QUERY_BACKTRACK)assert(tactix_apply_instance(h->view,rule("Restart"),0)>=0);
    else if(q.action==TACTIX_QUERY_SAT||q.action==TACTIX_QUERY_UNSAT)return q.action;
    else if(q.action==TACTIX_QUERY_RESTORE||q.action==TACTIX_QUERY_INTERRUPTED)return 0;
    else return -1;
  }
}
static int demo_run(demo_hook *h) {
  return h->scripted?tactix_scripted_default_run(h->scripted,h->view,demo_boundary,h):
      tactix_search_demo_run(h->program,h->view,demo_boundary,h);
}
static void demo_continuation(int kind) {
  void *s=ipasir_init();assert(s);
  input(s,(const int[]){1,2,0});input(s,(const int[]){1,-2,0});input(s,(const int[]){3,4,0});
  ipasir_assume(s,-3);tactix *view=NULL;assert(tactix_ipasir_begin(s,&view)==1);
  assert(!tactix_decide(view,-1)); // Client-selected wrong branch before A.
  tactix_search_demo p;assert(tactix_search_demo_begin(&p,kind==3?TACTIX_DEMO_NO_RESTARTS:kind));
  tactix_scripted_default scripted;tactix_scripted_default_begin(&scripted);
  // Suppress the optional Lucky shortcut only in this continuation fixture,
  // so the scripted scheduler observes a real conflict before suspension.
  scripted.lucky_tried=1;
  demo_hook h={s,view,&p,0,kind==3?&scripted:NULL};
  assert(!demo_run(&h)&&h.paused);
  const uint64_t steps=p.steps;const int purge=p.pending_purge;
  assert((kind==3||steps)&&purge==(kind==TACTIX_DEMO_DPLL));
  const tactix_scripted_default before=scripted;
  assert(tactix_apply_instance(view,rule("BVE"),3)>=0);
  tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;
  assert(!tactix_ipasir_next(s,&q)&&q.action==TACTIX_QUERY_RESTORE);
  assert(tactix_ipasir_repair(s,&view)==1&&p.steps==steps&&p.pending_purge==purge);
  assert(!memcmp(&before,&scripted,sizeof scripted));
  h.view=view;
  assert(demo_run(&h)==10&&!p.pending_purge);
  assert(tactix_ipasir_finish(s,10)==10&&ipasir_val(s,1)==1&&ipasir_val(s,3)==-3&&ipasir_val(s,4)==4);
  ipasir_release(s);
}
static void plain(const char *mode,const char *prefix,const char *scenario,const char *name) {
  const int crux=!strcmp(name,"cruxmiter");
  tactix *s=tactix_init();assert(s&&(!crux||cruxmiter_configure(s)));
  char path[4096];
  if(strcmp(mode,"none")) {
    snprintf(path,sizeof path,"%s.proof",prefix);
    const int format=!strcmp(mode,"dsr")?TACTIX_PROOF_DSR_ASCII:
        !strcmp(mode,"pbp")?TACTIX_PROOF_PBP:TACTIX_PROOF_AUTO;
    assert(!tactix_proof(s,path,format));
  }
  int variables;
  snprintf(path,sizeof path,"%s.cnf",prefix);FILE *cnf=fopen(path,"w");assert(cnf);
  if(!strcmp(scenario,"sat")) {
    const int clauses[][4]={{-3,1,0},{-3,2,0},{3,-1,-2,0},{-4,1,0},{-4,2,0},{4,-1,-2,0}};
    variables=4;assert(fprintf(cnf,"p cnf 4 6\n")>0);
    for(unsigned i=0;i<6;++i) {
      add_clause(s,clauses[i]);for(const int *l=clauses[i];*l;++l)assert(fprintf(cnf,"%d ",*l)>0);
      assert(fputs("0\n",cnf)>=0);
    }
  } else {
    assert(!strcmp(scenario,"unsat"));variables=12;assert(fprintf(cnf,"p cnf 12 22\n")>0);
    for(int pigeon=0;pigeon<4;++pigeon) {
      int clause[4]={3*pigeon+1,3*pigeon+2,3*pigeon+3,0};add_clause(s,clause);
      assert(fprintf(cnf,"%d %d %d 0\n",clause[0],clause[1],clause[2])>0);
    }
    for(int hole=0;hole<3;++hole)for(int a=0;a<4;++a)for(int b=a+1;b<4;++b) {
      int clause[3]={-(3*a+hole+1),-(3*b+hole+1),0};add_clause(s,clause);
      assert(fprintf(cnf,"%d %d 0\n",clause[0],clause[1])>0);
    }
  }
  assert(!fclose(cnf));
  cruxmiter_program p={0};int result;
  if(crux) {
    assert(!tactix_set_guidance_publication(s,TACTIX_GUIDANCE_NATIVE));
    assert(cruxmiter_begin(&p));result=cruxmiter_run(&p,s,NULL,NULL,NULL);
    if(result==CRUXMITER_HANDOFF)result=tactix_solve(s);
  } else if(!strcmp(name,"chvatal")) {
    int literals[128],literal;size_t starts[64]={0};unsigned clauses;
    struct chvatal_formula formula={literals,starts,0,0,0};
    FILE *file=fopen(path,"r");assert(file);
    assert(fscanf(file,"p cnf %d %u",&formula.variables,&clauses)==2);
    while(fscanf(file,"%d",&literal)==1) {
      if(literal) {assert(formula.literal_count<128);literals[formula.literal_count++]=literal;}
      else {assert(formula.clauses+1<64);starts[++formula.clauses]=formula.literal_count;}
    }
    assert(!ferror(file)&&!fclose(file)&&formula.clauses==clauses);
    assert(!tactix_set_guidance_publication(s,TACTIX_GUIDANCE_NATIVE));
    chvatal_program program={0};assert(chvatal_begin(&program,&formula,NULL,NULL));
    result=chvatal_run(&program,s,NULL,NULL,NULL);
    p.requests=program.requests;p.eligible=program.eligible;
    p.effective=program.effective;p.rounds=program.completed_rounds;
    chvatal_release(&program);
    if(result==CHVATAL_HANDOFF)result=tactix_solve(s);
  } else if(!strcmp(name,"scripted-default")) {
    tactix_scripted_default program;tactix_scripted_default_begin(&program);
    result=tactix_scripted_default_run(&program,s,NULL,NULL);
  } else {
    tactix_search_demo program;
    assert(tactix_search_demo_begin(&program,!strcmp(name,"dpll")?TACTIX_DEMO_DPLL:TACTIX_DEMO_NO_RESTARTS));
    result=tactix_search_demo_run(&program,s,NULL,NULL);
  }
  assert(result==(!strcmp(scenario,"sat")?10:20));
  if(result==10) {
    snprintf(path,sizeof path,"%s.model",prefix);FILE *model=fopen(path,"w");assert(model);
    for(int v=1;v<=variables;++v)assert(fprintf(model,"%d ",tactix_value(s,v)>0?v:-v)>0);
    assert(fputs("0\n",model)>=0&&!fclose(model));
  }
  printf("plain result=%d requests=%llu eligible=%llu effective=%llu rounds=%llu conflicts=%llu decisions=%llu\n",result,
      (unsigned long long)p.requests,(unsigned long long)p.eligible,(unsigned long long)p.effective,
      (unsigned long long)p.rounds,(unsigned long long)tactix_stat(s,TACTIX_STAT_CONFLICTS),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_DECISIONS));
  tactix_release(s);
}
int main(int argc,char **argv) {
  if(argc==4||argc==5) {plain(argv[1],argv[2],argv[3],argc==5?argv[4]:"cruxmiter");return 0;}
  assert(argc>=1&&argc<=3);const char *prefix=argc>=2?argv[1]:NULL;
  const char *name=argc==3?argv[2]:"cruxmiter";const int crux=!strcmp(name,"cruxmiter");
  const int chvatal=!strcmp(name,"chvatal");
  void *s=ipasir_init();assert(s);
  assert(!strcmp(tactix_ipasir_strategy_name(s),"default"));
  assert(tactix_ipasir_set_strategy_name(s,"missing")==-1);
  assert(!tactix_ipasir_set_default_option(s,"TACTIX_FORCE_MODE","focused"));
  assert(!tactix_ipasir_set_strategy_name(s,name));
  assert(!strcmp(tactix_ipasir_strategy_name(s),name));
  assert(!tactix_ipasir_set_core_budget(s,1000000));
  input(s,(const int[]){-3,1,0});input(s,(const int[]){-3,2,0});input(s,(const int[]){3,-1,-2,0});
  input(s,(const int[]){-4,1,0});input(s,(const int[]){-4,2,0});input(s,(const int[]){4,-1,-2,0});
  ipasir_assume(s,-3);ipasir_assume(s,4);assert(ipasir_solve(s)==20);save(s,prefix,1);
  const char *published=tactix_ipasir_bundle(s);assert(published);
  assert(tactix_ipasir_set_strategy_name(s,"missing")==-1&&published==tactix_ipasir_bundle(s));
  ipasir_assume(s,3);assert(ipasir_solve(s)==10&&ipasir_val(s,4)==4);save(s,prefix,2);
  ipasir_assume(s,-4);assert(ipasir_solve(s)==10&&ipasir_val(s,3)==-3);save(s,prefix,3);
  input(s,(const int[]){3,0});ipasir_assume(s,7);ipasir_assume(s,-4);
  assert(ipasir_solve(s)==20&&ipasir_failed(s,-4)&&!ipasir_failed(s,7));save(s,prefix,4);
  assert(ipasir_solve(s)==10);save(s,prefix,5);
  tactix_ipasir_strategy_counters c=counters(s);
  assert(c.query_entries==5);
  if(crux||chvatal)assert(c.root_requests>0&&c.root_eligible>0&&c.root_effective>0&&c.root_rounds>0&&c.default_handoffs>0);
  else assert(!c.root_requests&&!c.root_eligible&&!c.root_effective&&!c.root_rounds&&!c.default_handoffs);
  int stop=1;ipasir_set_terminate(s,&stop,flag);ipasir_assume(s,-4);assert(!ipasir_solve(s));
  ipasir_set_terminate(s,NULL,NULL);assert(ipasir_solve(s)==10);
  int stable=-1;assert(!tactix_ipasir_set_strategy(s,&stable,delegate));
  assert(!strcmp(tactix_ipasir_strategy_name(s),"callback"));
  assert(ipasir_solve(s)==10&&stable==0); // Preset's stable-mode option did not leak.
  assert(tactix_ipasir_get_strategy_counters(s,&c)==-1);
  assert(!tactix_ipasir_set_strategy_name(s,NULL)&&!strcmp(tactix_ipasir_strategy_name(s),"default"));
  ipasir_release(s);
  if(crux)continuation();
  else if(chvatal)chvatal_continuation();
  else demo_continuation(!strcmp(name,"scripted-default")?3:
      !strcmp(name,"dpll")?TACTIX_DEMO_DPLL:TACTIX_DEMO_NO_RESTARTS);
  printf("%s: entries=%llu requests=%llu eligible=%llu effective=%llu rounds=%llu handoffs=%llu; cores and option isolation pass\n",
      name,(unsigned long long)c.query_entries,(unsigned long long)c.root_requests,(unsigned long long)c.root_eligible,
      (unsigned long long)c.root_effective,(unsigned long long)c.root_rounds,(unsigned long long)c.default_handoffs);
}
