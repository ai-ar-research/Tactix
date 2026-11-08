#include "cruxmiter.h"
#include <inttypes.h>
#include <string.h>

enum { START, INITIAL_PROPAGATE, ROUND_BEGIN, DOSE_APPLY, DOSE_PROPAGATE,
       ROUND_END, HANDOFF };

static int rule_id(const char *name) {
  for(int i=0;i<tactix_rule_count();++i) {
    const char *candidate=tactix_rule_name(i);
    if(candidate&&!strcmp(candidate,name))return i;
  }
  fprintf(stderr,"tactix-cruxmiter-policy: missing rule '%s'\n",name);
  return -1;
}
int cruxmiter_begin(cruxmiter_program *p) {
  memset(p,0,sizeof *p);
  p->propagate=rule_id("Propagate");p->congruence=rule_id("Congruence");
  p->substitute=rule_id("Substitute");p->backbone=rule_id("Backbone");
  return p->propagate>=0&&p->congruence>=0&&p->substitute>=0&&p->backbone>=0;
}
static int terminal(int code) {return code==10||code==20;}
static int apply(cruxmiter_program *p,tactix *s,int rule) {
  /* Per-action deltas survive a restored/replayed representation. Sampling
   * across re-entry would subtract counters from a different kernel epoch. */
  const uint64_t m=tactix_stat(s,TACTIX_STAT_CONGRUENCE_MERGES);
  const uint64_t v=tactix_stat(s,TACTIX_STAT_SUBSTITUTE_VARS);
  ++p->requests;
  const int code=tactix_apply(s,rule);
  p->eligible+=code>=0;p->effective+=code==0;
  p->round_merges+=tactix_stat(s,TACTIX_STAT_CONGRUENCE_MERGES)-m;
  p->round_substitutions+=tactix_stat(s,TACTIX_STAT_SUBSTITUTE_VARS)-v;
  return code;
}
static void next_phase(cruxmiter_program *p) {
  p->count=0;
  if(p->phase==0) {p->phase=1;p->stage=DOSE_APPLY;}
  else p->stage=p->phase==1?ROUND_END:HANDOFF;
}
int cruxmiter_run(cruxmiter_program *p,tactix *s,
    int (*hook)(void *),void *context,FILE *log) {
  for(;;) {
    if(hook&&hook(context))return 0;
    switch(p->stage) {
    case START:
      if(tactix_status(s)==20)return 20;
      p->stage=INITIAL_PROPAGATE;break;
    case INITIAL_PROPAGATE: {
      const int code=apply(p,s,p->propagate);
      p->stage=ROUND_BEGIN;
      if(terminal(code))return code;
      if(code==2)return 20;
      break;
    }
    case ROUND_BEGIN:
      p->round_applications=p->applications;p->round_merges=p->round_substitutions=0;
      p->phase=0;p->count=0;p->stage=DOSE_APPLY;break;
    case DOSE_APPLY: {
      const unsigned cap=p->phase==0?4:p->phase==1?24:512;
      if(p->count==cap) {next_phase(p);break;}
      const int rule=p->phase==0?p->congruence:p->phase==1?p->substitute:p->backbone;
      const int code=apply(p,s,rule);
      if(terminal(code))return code;
      if(code==2)return 20;
      if(code!=0) {next_phase(p);break;}
      ++p->applications;p->stage=DOSE_PROPAGATE;break;
    }
    case DOSE_PROPAGATE: {
      const int code=apply(p,s,p->propagate);
      ++p->count;p->stage=DOSE_APPLY;
      if(terminal(code))return code;
      if(code==2)return 20;
      break;
    }
    case ROUND_END:
      ++p->rounds;
      if(log) {
        fprintf(log,"c crux-gate-closure round=%u applications=%" PRIu64
            " merges=%" PRIu64 " substitutions=%" PRIu64 "\n",p->round+1,
            p->applications-p->round_applications,p->round_merges,p->round_substitutions);
        fflush(log);
      }
      if(++p->round==4 || (p->round>=2&&!p->round_merges&&!p->round_substitutions)) {
        p->phase=2;p->count=0;p->stage=DOSE_APPLY;
      } else p->stage=ROUND_BEGIN;
      break;
    case HANDOFF:return CRUXMITER_HANDOFF;
    default:return 0;
    }
  }
}
const char *cruxmiter_setting(size_t index,const char **value) {
  static const char *const settings[][2]={
    {"TACTIX_FORCE_MODE","stable"},{"TACTIX_REORDER","0"},
    {"TACTIX_NO_BVE","1"},{"TACTIX_CONFLICT_REUSE","0"},
    {"TACTIX_SLOT_PERMILLE","Vivify:200"},{"TACTIX_NO_FACTOR","1"},
    {"TACTIX_NO_SWEEP","1"},{"TACTIX_SLOT_DISABLE","Backbone"}};
  if(index>=sizeof settings/sizeof *settings)return NULL;
  *value=settings[index][1];return settings[index][0];
}
int cruxmiter_configure(tactix *s) {
  const char *value,*name;
  for(size_t i=0;(name=cruxmiter_setting(i,&value));++i)
    if(tactix_set_default_option(s,name,value)) {
      fprintf(stderr,"tactix-cruxmiter-policy: rejected setting %s=%s\n",name,value);
      return 0;
    }
  return 1;
}
