#ifndef TACTIX_SEARCH_DEMO_H
#define TACTIX_SEARCH_DEMO_H
#include "tactix.h"
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TACTIX_DEMO_DPLL=1, TACTIX_DEMO_NO_RESTARTS=2 };
typedef struct tactix_search_demo {
  int kind,propagate,analyze,decide,reduce,pending_purge;
  uint64_t steps,step_limit;
} tactix_search_demo;
/* Hook: -1 permits the next policy action; 0/10/20 suspends/finishes.
 * It must settle query assumptions/backtracking, or request restoration/stop.
 * Program state contains no kernel/view handles and survives restoration.
 * A zero step_limit is unlimited; toy examples may set their original limit. */

static inline int tactix_search_demo_rule_id(const char *name) {
  for(int i=0;i<tactix_rule_count();++i)
    if(!strcmp(tactix_rule_name(i),name))return i;
  return -1;
}
static inline int tactix_search_demo_has(const int *ids,int n,int id) {
  for(int i=0;i<n;++i)if(ids[i]==id)return 1;
  return 0;
}
static inline int tactix_search_demo_begin(tactix_search_demo *p,int kind) {
  memset(p,0,sizeof *p);p->kind=kind;
  p->propagate=tactix_search_demo_rule_id("Propagate");p->analyze=tactix_search_demo_rule_id("ConflictAnalysis");
  p->decide=tactix_search_demo_rule_id("Decide");p->reduce=tactix_search_demo_rule_id("Reduce");
  return (kind==TACTIX_DEMO_DPLL || kind==TACTIX_DEMO_NO_RESTARTS) &&
      p->propagate>=0&&p->analyze>=0&&p->decide>=0&&
      (kind!=TACTIX_DEMO_DPLL || p->reduce>=0);
}
static inline int tactix_search_demo_run(tactix_search_demo *p,tactix *s,
    int (*hook)(void *),void *context) {
  for(;;) {
    int boundary=hook?hook(context):-1;
    if(boundary>=0 && !(boundary==10&&p->pending_purge))return boundary;
    if(p->pending_purge) {
      (void)tactix_apply_instance(s,p->reduce,UINT64_MAX-1);p->pending_purge=0;
      if(boundary==10)return 10;
      continue;
    }
    if(p->step_limit && p->steps+1>=p->step_limit)return 0;
    ++p->steps;
    const int status=tactix_status(s);if(status)return status;
    if(p->kind==TACTIX_DEMO_DPLL) {
      int ids[32];const int n=tactix_rules_applicable(s,ids,32);
      if(n<=0)return 0;
      if(tactix_search_demo_has(ids,n,p->propagate)) {
        if(tactix_apply(s,p->propagate)==20)return 20;
        continue;
      }
      if(tactix_search_demo_has(ids,n,p->analyze)) {
        const int code=tactix_apply(s,p->analyze);
        if(code==20)return 20;
        p->pending_purge=1;
        // The next iteration performs the original post-analysis purge,
        // retaining it if a query boundary suspends this invocation.
        continue;
      }
      if(!tactix_search_demo_has(ids,n,p->decide))return 0;
      if(tactix_apply(s,p->decide)<0)return 0;
    } else {
      int code=tactix_apply(s,p->propagate);
      if(code==20)return 20;
      if(code==2) {
        code=tactix_apply(s,p->analyze);
        if(code==20)return 20;
        continue;
      }
      if(hook&&code==0)continue; // Re-enter A's boundary before another decision.
      if(tactix_status(s)==10)return 10;
      if(tactix_apply(s,p->decide)<0)return 0;
    }
  }
}

#ifdef __cplusplus
}
#endif
#endif
