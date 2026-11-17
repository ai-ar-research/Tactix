#ifndef TACTIX_TESTS_IPASIR_DRIVER_H
#define TACTIX_TESTS_IPASIR_DRIVER_H
#include "api_test_support.h"
#include "tactix_ipasir.h"

/* Shared boundary protocol; each fixture retains its branching policy. */
static inline tactix_ipasir_query_state next_query(void *s) {
  tactix_ipasir_query_state q={0};q.version=TACTIX_IPASIR_SESSION_VERSION;q.size=sizeof q;
  assert(tactix_ipasir_next(s,&q)==0);return q;
}
static inline int drive_query(void *session,tactix *view,unsigned *steps,
                              void (*branch)(tactix *)) {
  for(unsigned i=0;i<10000;++i) {
    if(steps)++*steps;
    const tactix_ipasir_query_state q=next_query(session);
    switch(q.action) {
    case TACTIX_QUERY_ASSUME: {
      const int code=tactix_ipasir_assume_next(session);
      if(code==TACTIX_IPASIR_ACTION_INTERRUPTED)return 0;
      assert(!code);break;
    }
    case TACTIX_QUERY_PROPAGATE:assert(tactix_apply(view,rule("Propagate"))>=0);break;
    case TACTIX_QUERY_ANALYZE:assert(tactix_apply_instance(view,rule("ConflictAnalysis"),0)>=0);break;
    case TACTIX_QUERY_BACKTRACK:assert(tactix_apply_instance(view,rule("Restart"),0)>=0);break;
    case TACTIX_QUERY_READY: {
      if(branch)branch(view);
      else {
        int literal=0;assert(tactix_decide_candidates(view,&literal,1)==1);
        assert(tactix_decide(view,literal)==0);
      }
      break;
    }
    case TACTIX_QUERY_SAT:case TACTIX_QUERY_UNSAT:return q.action;
    case TACTIX_QUERY_RESTORE:case TACTIX_QUERY_INTERRUPTED:return 0;
    default:assert(!"unexpected programmable-query boundary");
    }
  }
  assert(!"script did not terminate");return 0;
}
#endif
