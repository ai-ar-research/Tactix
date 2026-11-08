#ifndef TACTIX_CHVATAL_PROGRAM_H
#define TACTIX_CHVATAL_PROGRAM_H
#include "tactix.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Original-input twin candidates, independently validated by public SR.
 * The program owns pairs and continuation only, never a borrowed solver view.
 * Zero-initialize before first begin; release when its owner is destroyed. */
struct chvatal_formula {
  int *literals;
  size_t *starts;
  size_t clauses,literal_count;
  int variables;
};
struct chvatal_pair {int representative,twin;};
enum {CHVATAL_HANDOFF=30};
typedef struct chvatal_program {
  struct chvatal_pair *pairs;
  unsigned *pair_round;
  size_t pair_count,begin,end,index;
  unsigned rounds,stage,direction,count,after_collapse,after_analysis;
  int propagate,analyze,substitute,subsume,sr,result,reported;
  long long accepted,round_accepted;
  uint64_t revision,requests,eligible,effective,completed_rounds;
} chvatal_program;

static inline int chvatal_grow(void **array,size_t *capacity,size_t count,size_t size) {
  if(count<*capacity)return 1;
  const size_t next=*capacity?2 * *capacity:1024;
  void *grown=realloc(*array,next*size);
  if(!grown)return 0;
  *array=grown;*capacity=next;return 1;
}
static inline int chvatal_begin(chvatal_program *,const struct chvatal_formula *,int (*stop)(void *),void *);
/* Reset the schedule for the same permanent input. SR still validates each
 * candidate against this query's actual representation. */
static inline void chvatal_reset(chvatal_program *);
static inline void chvatal_release(chvatal_program *);
/* A nonzero hook suspends before the next public action. Root preprocessing
 * precedes A; the explicit default handoff installs assumptions. */
static inline int chvatal_run(chvatal_program *,tactix *,int (*hook)(void *),void *,FILE *log);

static inline uint64_t chvatal_mix(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

static inline int chvatal_compare_int(const void *left, const void *right) {
  const int a = *(const int *)left, b = *(const int *)right;
  return (a > b) - (a < b);
}

struct chvatal_keyed {
  uint64_t key;
  int variable;
};

static inline int chvatal_compare_keyed(const void *left, const void *right) {
  const struct chvatal_keyed *a = (const struct chvatal_keyed *)left;
  const struct chvatal_keyed *b = (const struct chvatal_keyed *)right;
  if (a->key != b->key) return a->key < b->key ? -1 : 1;
  return a->variable - b->variable;
}

/* Bottom-up twin rounds.  Returns the merge pairs in a valid application
 * order (every pair's children are merged in an earlier round). */
static inline struct chvatal_pair *chvatal_find_twins(const struct chvatal_formula *f, size_t *pair_count,
                               unsigned *rounds_out, unsigned **round_out,
                               int (*stop)(void *),void *context,int *interrupted) {
  const int n = f->variables;
  int *rep = (int *)malloc(((size_t)n + 1) * sizeof(int));
  uint64_t *signature = (uint64_t *)calloc((size_t)n + 1, sizeof(uint64_t));
  uint64_t *negatives = (uint64_t *)calloc((size_t)n + 1, sizeof(uint64_t));
  struct chvatal_keyed *keyed = (struct chvatal_keyed *)malloc(((size_t)n + 1) * sizeof *keyed);
  int *scratch = NULL;
  size_t scratch_capacity = 0, pair_capacity = 0, i;
  struct chvatal_pair *pairs = NULL;
  unsigned *pair_round = NULL;
  size_t round_capacity = 0;
  unsigned rounds = 0;
  int v;
  *pair_count = 0;
  if (!rep || !signature || !negatives || !keyed) goto done;
  for (v = 0; v <= n; ++v) rep[v] = v;
  for (;;) {
    size_t merged = 0, count = 0;
    memset(signature, 0, ((size_t)n + 1) * sizeof(uint64_t));
    memset(negatives, 0, ((size_t)n + 1) * sizeof(uint64_t));
    for (i = 0; i < f->clauses; ++i) {
      const size_t begin = f->starts[i], end = f->starts[i + 1];
      const size_t size = end - begin;
      size_t j, k;
      if (size > scratch_capacity) {
        scratch_capacity = 2 * size;
        scratch = (int *)realloc(scratch, scratch_capacity * sizeof(int));
        if (!scratch) goto done;
      }
      for (j = begin; j < end; ++j) {
        if(stop&&stop(context)) {*interrupted=1;goto done;}
        const int literal = f->literals[j];
        const int variable = literal < 0 ? -literal : literal;
        if (literal > 0) continue;
        /* The clause without -variable, with representatives mapped in. */
        {
          size_t m = 0;
          uint64_t h = UINT64_C(1469598103934665603);
          for (k = begin; k < end; ++k) {
            const int other = f->literals[k];
            const int ov = other < 0 ? -other : other;
            if (k == j) continue;
            scratch[m++] = other < 0 ? -rep[ov] : rep[ov];
          }
          qsort(scratch, m, sizeof(int), chvatal_compare_int);
          for (k = 0; k < m; ++k)
            if (!k || scratch[k] != scratch[k - 1])
              h = chvatal_mix(h ^ (uint64_t)(uint32_t)scratch[k]);
          signature[variable] += chvatal_mix(h);
          ++negatives[variable];
        }
      }
    }
    for (v = 1; v <= n; ++v)
      if (rep[v] == v && negatives[v]) {
        keyed[count].key = chvatal_mix(signature[v] ^ chvatal_mix(negatives[v]));
        keyed[count].variable = v;
        ++count;
      }
    qsort(keyed, count, sizeof *keyed, chvatal_compare_keyed);
    for (i = 0; i < count;) {
      size_t j = i + 1;
      while (j < count && keyed[j].key == keyed[i].key) ++j;
      for (size_t k = i + 1; k < j; ++k) {
        if (!chvatal_grow((void **)&pairs, &pair_capacity, *pair_count,
                  sizeof *pairs))
          goto done;
        if (!chvatal_grow((void **)&pair_round, &round_capacity, *pair_count,
                  sizeof *pair_round))
          goto done;
        pairs[*pair_count].representative = keyed[i].variable;
        pairs[*pair_count].twin = keyed[k].variable;
        pair_round[*pair_count] = rounds;
        ++*pair_count;
        ++merged;
      }
      i = j;
    }
    /* Map after the round so every pair of this round sees only earlier
     * merges, exactly the order in which they are applied. */
    for (i = *pair_count - merged; i < *pair_count; ++i)
      rep[pairs[i].twin] = pairs[i].representative;
    if (!merged) break;
    ++rounds;
  }
done:
  *rounds_out = rounds;
  *round_out = pair_round;
  free(rep);
  free(signature);
  free(negatives);
  free(keyed);
  free(scratch);
  return pairs;
}

enum {CHVATAL_START,CHVATAL_INITIAL_PROPAGATE,CHVATAL_INITIAL_ANALYZE,CHVATAL_ROUND_BEGIN,CHVATAL_MERGE,CHVATAL_ROUND_PROPAGATE,
      CHVATAL_ROUND_COLLAPSE,CHVATAL_SUBSTITUTE,CHVATAL_SUBSTITUTE_PROPAGATE,CHVATAL_SUBSUME,CHVATAL_SPARSE_COLLECT,
      CHVATAL_COLLAPSE_PROPAGATE,CHVATAL_COLLECT,CHVATAL_ANALYZE,CHVATAL_ROUND_END,CHVATAL_REPORT,CHVATAL_FINAL_COLLAPSE,CHVATAL_LOG,CHVATAL_SEARCH};

static inline int chvatal_rule_id(const char *name) {
  for(int i=0;i<tactix_rule_count();++i)
    if(!strcmp(tactix_rule_name(i),name))return i;
  return -1;
}
static inline void chvatal_release(chvatal_program *p) {
  free(p->pairs);free(p->pair_round);memset(p,0,sizeof *p);
}
static inline void chvatal_reset(chvatal_program *p) {
  p->begin=p->end=p->index=0;p->stage=CHVATAL_START;
  p->direction=p->count=p->after_collapse=p->after_analysis=0;
  p->result=p->reported=0;p->accepted=p->round_accepted=0;
  p->requests=p->eligible=p->effective=p->completed_rounds=0;
}
static inline int chvatal_begin(chvatal_program *p,const struct chvatal_formula *f,int (*stop)(void *),void *context) {
  const uint64_t revision=p->revision?p->revision:1;
  chvatal_release(p);p->revision=revision;
  p->propagate=chvatal_rule_id("Propagate");p->analyze=chvatal_rule_id("ConflictAnalysis");
  p->substitute=chvatal_rule_id("Substitute");p->subsume=chvatal_rule_id("SubsumeForward");
  p->sr=chvatal_rule_id("SubstitutionRedundancy");
  int interrupted=0;
  p->pairs=chvatal_find_twins(f,&p->pair_count,&p->rounds,&p->pair_round,stop,context,&interrupted);
  if(interrupted) {chvatal_release(p);return 0;}
  return p->propagate>=0&&p->analyze>=0&&p->substitute>=0&&p->subsume>=0&&p->sr>=0&&
      (!p->pair_count||(p->pairs&&p->pair_round));
}
static inline int chvatal_terminal(int code) {return code==10||code==20;}
static inline int chvatal_apply(chvatal_program *p,tactix *s,int rule) {
  ++p->requests;const int code=tactix_apply(s,rule);
  p->eligible+=code>=0;p->effective+=code==0;return code;
}
static inline void chvatal_finish_root(chvatal_program *p,int code) {
  p->result=code;p->stage=p->reported?CHVATAL_LOG:CHVATAL_REPORT;
}
static inline void chvatal_analyze_then(chvatal_program *p,unsigned next) {
  p->after_analysis=next;p->stage=CHVATAL_ANALYZE;
}
static inline void chvatal_collapse_then(chvatal_program *p,unsigned next) {
  p->after_collapse=next;p->count=0;p->stage=CHVATAL_SUBSTITUTE;
}
/* Forward clauses precede reverse clauses within each bottom-up round. The
 * online program copies its one instance before the stack storage expires. */
static inline int chvatal_merge_one(chvatal_program *p,tactix *s) {
  const struct chvatal_pair pair=p->pairs[p->index];
  int storage[3]={p->direction?pair.twin:pair.representative,
                 p->direction?-pair.representative:-pair.twin,0};
  storage[2]=storage[0];
  tactix_online_sr_instance instance;memset(&instance,0,sizeof instance);
  instance.clause=storage;instance.clause_count=2;
  instance.witness=storage+2;instance.witness_count=1;
  tactix_online_step_v2 step;memset(&step,0,sizeof step);
  step.base.step_id=1;step.base.rule_id=p->sr;
  step.base.parameters.kind=TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.base.parameters.value.substitution_redundancy.maximum_validation_ticks=UINT64_C(1)<<22;
  step.base.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
  step.base.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;step.base.repeat_limit=1;
  step.base.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;step.base.budget_unlimited=1;
  step.base.post_application=TACTIX_ONLINE_POST_NONE;
  step.sr_instances=&instance;step.sr_instance_count=1;
  tactix_online_program_v2 program;memset(&program,0,sizeof program);program.steps=&step;program.step_count=1;
  program.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;program.repeat_limit=1;
  tactix_online_result result;memset(&result,0,sizeof result);++p->requests;
  if(p->revision==UINT64_MAX||tactix_online_configure_v2(s,&program,++p->revision,
        TACTIX_ONLINE_RECONFIGURE_RESET)||tactix_online_advance(s,&result))return -1;
  ++p->eligible;p->effective+=!!result.made_rule_progress;
  return !!result.made_rule_progress;
}
static inline int chvatal_run(chvatal_program *p,tactix *s,int (*hook)(void *),void *context,FILE *log) {
  for(;;) {
    if(hook&&hook(context))return 0;
    switch(p->stage) {
    case CHVATAL_START:
      if(tactix_status(s)==20)return 20;
      p->stage=CHVATAL_INITIAL_PROPAGATE;break;
    case CHVATAL_INITIAL_PROPAGATE: {
      const int code=chvatal_apply(p,s,p->propagate);
      p->stage=code==2?CHVATAL_INITIAL_ANALYZE:CHVATAL_ROUND_BEGIN;
      if(chvatal_terminal(code))return code;
      break;
    }
    case CHVATAL_INITIAL_ANALYZE: {
      const int code=chvatal_apply(p,s,p->analyze);p->stage=CHVATAL_ROUND_BEGIN;
      if(chvatal_terminal(code))return code;
      break;
    }
    case CHVATAL_ROUND_BEGIN:
      if(p->begin==p->pair_count) {p->stage=CHVATAL_REPORT;break;}
      p->end=p->begin;
      while(p->end<p->pair_count&&p->pair_round[p->end]==p->pair_round[p->begin])++p->end;
      p->index=p->begin;p->direction=0;p->round_accepted=0;p->stage=CHVATAL_MERGE;break;
    case CHVATAL_MERGE: {
      if(p->index==p->end) {
        if(!p->direction) {p->direction=1;p->index=p->begin;break;}
        p->accepted+=p->round_accepted;p->stage=CHVATAL_ROUND_PROPAGATE;break;
      }
      const int accepted=chvatal_merge_one(p,s);
      if(accepted<0) {p->stage=CHVATAL_REPORT;break;}
      p->round_accepted+=accepted;++p->index;break;
    }
    case CHVATAL_ROUND_PROPAGATE: {
      const int code=chvatal_apply(p,s,p->propagate);
      if(chvatal_terminal(code))chvatal_finish_root(p,code);
      else if(code==2)chvatal_analyze_then(p,CHVATAL_ROUND_COLLAPSE);
      else p->stage=CHVATAL_ROUND_COLLAPSE;
      break;
    }
    case CHVATAL_ROUND_COLLAPSE:chvatal_collapse_then(p,CHVATAL_ROUND_END);break;
    case CHVATAL_SUBSTITUTE: {
      if(p->count==64) {p->stage=CHVATAL_SUBSUME;break;}
      const int code=chvatal_apply(p,s,p->substitute);
      if(chvatal_terminal(code))chvatal_finish_root(p,code);
      else p->stage=code==0?CHVATAL_SUBSTITUTE_PROPAGATE:CHVATAL_SUBSUME;
      break;
    }
    case CHVATAL_SUBSTITUTE_PROPAGATE: {
      const int code=chvatal_apply(p,s,p->propagate);++p->count;
      if(chvatal_terminal(code))chvatal_finish_root(p,code);
      else if(code==2)chvatal_analyze_then(p,p->after_collapse);
      else p->stage=CHVATAL_SUBSTITUTE;
      break;
    }
    case CHVATAL_SUBSUME: {
      const int code=chvatal_apply(p,s,p->subsume);
      if(chvatal_terminal(code))chvatal_finish_root(p,code);else p->stage=CHVATAL_SPARSE_COLLECT;
      break;
    }
    case CHVATAL_SPARSE_COLLECT:
      (void)tactix_support(s,TACTIX_SUPPORT_SPARSE_COLLECT,0);p->stage=CHVATAL_COLLAPSE_PROPAGATE;break;
    case CHVATAL_COLLAPSE_PROPAGATE: {
      const int code=chvatal_apply(p,s,p->propagate);
      if(chvatal_terminal(code))chvatal_finish_root(p,code);
      else if(code==2)chvatal_analyze_then(p,p->after_collapse);
      else p->stage=CHVATAL_COLLECT;
      break;
    }
    case CHVATAL_COLLECT:
      (void)tactix_support(s,TACTIX_SUPPORT_COLLECT_GARBAGE,0);p->stage=p->after_collapse;break;
    case CHVATAL_ANALYZE: {
      const int code=chvatal_apply(p,s,p->analyze);
      if(chvatal_terminal(code))chvatal_finish_root(p,code);else p->stage=p->after_analysis;
      break;
    }
    case CHVATAL_ROUND_END:
      ++p->completed_rounds;p->begin=p->end;p->stage=CHVATAL_ROUND_BEGIN;break;
    case CHVATAL_REPORT:
      if(log) {fprintf(log,"c chvatal-twins pairs=%zu rounds=%u accepted=%lld\n",
          p->pair_count,p->rounds,p->accepted);fflush(log);}
      p->reported=1;p->stage=p->result?CHVATAL_LOG:CHVATAL_FINAL_COLLAPSE;break;
    case CHVATAL_FINAL_COLLAPSE:chvatal_collapse_then(p,CHVATAL_LOG);break;
    case CHVATAL_LOG:
      if(log) {fprintf(log,"c chvatal-twins substituted=%" PRIu64 "\n",
          tactix_stat(s,TACTIX_STAT_SUBSTITUTE_VARS));fflush(log);}
      p->stage=CHVATAL_SEARCH;break;
    case CHVATAL_SEARCH: {
      const int result=p->result;p->result=0;
      return result?result:CHVATAL_HANDOFF;
    }
    default:return 0;
    }
  }
}

#ifdef __cplusplus
}
#endif
#endif
