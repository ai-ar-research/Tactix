#undef NDEBUG
#include "tactix_ipasir.h"
#include "ipasir_test_support.h"
#include "../tools/fuzz/public_random_session.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  tactix_random_session *random;
  tactix_ipasir_result *source;
  unsigned calls,restored;
} driver;
static int run(void *opaque,void *session,struct tactix_solver *view,unsigned entry) {
  driver *d=opaque;++d->calls;d->restored+=entry==TACTIX_IPASIR_ENTER_RESTORED;
  if(d->source)assert(!tactix_ipasir_minimize_core(session,d->source,NULL,0,1));
  return tactix_random_session_run(d->random,session,view,entry);
}
static int stop(void *p) {return *(int *)p;}
static unsigned exported=0;
static void export_result(const tactix_ipasir_result *r,const char *prefix) {
  assert(r);
  if(prefix)copy_query_bundle(tactix_ipasir_result_bundle(r),prefix,++exported,
                             tactix_ipasir_result_status(r),TEST_CORE_OPTIONAL);
}
static void exercise(unsigned mode,const char *prefix) {
  void *s=ipasir_init();assert(s);driver d={0};
  if(mode) {
    d.random=tactix_random_session_new(42);assert(d.random);
    tactix_random_session_variables(d.random,7);
    assert(!tactix_ipasir_set_strategy(s,&d,run));
  }
  ipasir_add(s,1);ipasir_add(s,0);
  ipasir_assume(s,7);ipasir_assume(s,-1);ipasir_assume(s,7);
  assert(ipasir_solve(s)==20);d.source=tactix_ipasir_result_acquire(s);assert(d.source);
  assert(strlen(tactix_ipasir_result_input_sha256(d.source))==64);
  void *foreign=ipasir_init();assert(foreign);
  assert(!tactix_ipasir_minimize_core(foreign,d.source,NULL,0,10));ipasir_release(foreign);
  const int bad=8;assert(!tactix_ipasir_minimize_core(s,d.source,&bad,1,10));
  // Source may precede the currently published SAT result. Probes preserve
  // that borrowed result, its value getters and the actual string address.
  assert(ipasir_solve(s)==10);tactix_ipasir_result *sat=tactix_ipasir_result_acquire(s);assert(sat);
  assert(!tactix_ipasir_minimize_core(s,sat,NULL,0,10));
  const char *before=tactix_ipasir_bundle(s);assert(before&&ipasir_val(s,1)==1);
  tactix_ipasir_reduction *zero=tactix_ipasir_minimize_core(s,d.source,NULL,0,0);assert(zero);
  assert(!tactix_ipasir_reduction_minimal(zero)&&!tactix_ipasir_reduction_probes(zero));
  assert(tactix_ipasir_reduction_core_size(zero)==2);tactix_ipasir_reduction_release(zero);
  const unsigned calls=d.calls;
  tactix_ipasir_reduction *r=tactix_ipasir_minimize_core(s,d.source,NULL,0,UINT64_MAX);assert(r);
  assert(tactix_ipasir_reduction_minimal(r)&&tactix_ipasir_reduction_core_size(r)==1);
  assert(tactix_ipasir_reduction_core_literal(r,0)==-1&&tactix_ipasir_reduction_probes(r)==2);
  assert(tactix_ipasir_reduction_probe_status(r,0)==20&&tactix_ipasir_reduction_probe_status(r,1)==10);
  assert(tactix_ipasir_reduction_witnesses(r)==1&&tactix_ipasir_reduction_witness_literal(r,0)==-1);
  assert(tactix_ipasir_reduction_witness_probe(r,0)==1);
  if(mode)assert(d.calls==calls+2); // Selected random driver also owns probes.
  assert(before==tactix_ipasir_bundle(s)&&ipasir_val(s,1)==1&&!ipasir_failed(s,-1));
  tactix_ipasir_result *certificate=tactix_ipasir_reduction_certificate(r);
  tactix_ipasir_result *witness=tactix_ipasir_reduction_probe_result(r,1);
  assert(certificate&&witness&&tactix_ipasir_result_status(witness)==10);
  assert(tactix_ipasir_result_assumptions(certificate)==1&&tactix_ipasir_result_assumption(certificate,0)==-1);
  assert(!tactix_ipasir_result_assumptions(witness));
  const int context=-1;
  tactix_ipasir_reduction *fixed=tactix_ipasir_minimize_core(s,d.source,&context,1,UINT64_MAX);assert(fixed);
  assert(tactix_ipasir_reduction_minimal(fixed)&&!tactix_ipasir_reduction_core_size(fixed));
  assert(tactix_ipasir_reduction_context_size(fixed)==1&&tactix_ipasir_reduction_context_literal(fixed,0)==-1);
  tactix_ipasir_reduction_release(fixed);
  int interrupted=1;ipasir_set_terminate(s,&interrupted,stop);
  tactix_ipasir_reduction *partial=tactix_ipasir_minimize_core(s,d.source,NULL,0,10);assert(partial);
  assert(!tactix_ipasir_reduction_minimal(partial)&&tactix_ipasir_reduction_probes(partial)==1);
  assert(!tactix_ipasir_reduction_probe_status(partial,0)&&!tactix_ipasir_reduction_probe_result(partial,0));
  tactix_ipasir_result *fallback=tactix_ipasir_reduction_certificate(partial);assert(fallback);
  assert(!strcmp(tactix_ipasir_result_bundle(fallback),tactix_ipasir_result_bundle(d.source)));
  tactix_ipasir_result_release(fallback);tactix_ipasir_reduction_release(partial);
  ipasir_set_terminate(s,NULL,NULL);assert(before==tactix_ipasir_bundle(s));
  // Invalid pending inputs must be rejected without silently consuming them.
  ipasir_assume(s,-1);assert(!tactix_ipasir_minimize_core(s,d.source,NULL,0,10));
  assert(ipasir_solve(s)==20);
  ipasir_add(s,2);assert(!tactix_ipasir_minimize_core(s,d.source,NULL,0,10));
  ipasir_add(s,0);assert(!tactix_ipasir_minimize_core(s,d.source,NULL,0,10));
  assert(ipasir_solve(s)==10&&ipasir_val(s,2)==2);
  ipasir_release(s);tactix_random_session_delete(d.random);
  // Reduction, source, certificate and witnesses independently retain files.
  export_result(d.source,prefix);export_result(certificate,prefix);export_result(witness,prefix);
  tactix_ipasir_reduction_release(r);tactix_ipasir_result_release(d.source);
  export_result(certificate,prefix);export_result(witness,prefix);
  tactix_ipasir_result_release(certificate);tactix_ipasir_result_release(witness);tactix_ipasir_result_release(sat);
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  exercise(0,prefix);exercise(1,prefix);
  puts("Native minimization: Default/public-random probes, fixed context, UNKNOWN, ownership and stale/pending rejection pass");
}
