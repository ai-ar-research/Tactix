#undef NDEBUG
#include "tactix_ipasir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned calls,limit; } terminator;
static int terminate(void *data) {
  terminator *t=data;++t->calls;
  return t->limit && t->calls>=t->limit;
}
static int cancel_null(void *data) {assert(!data);return -7;}
int main(void) {
  void *s=ipasir_init();assert(s);
  ipasir_add(s,1);ipasir_add(s,0);
  terminator stop={0,1},never={0,0};
  ipasir_assume(s,-1);
  ipasir_set_terminate(s,&stop,terminate);
  assert(ipasir_solve(s)==0&&stop.calls==1);
  assert(!tactix_ipasir_bundle(s)&&!ipasir_val(s,1)&&!ipasir_failed(s,-1));
  ipasir_set_terminate(s,NULL,NULL);
  assert(ipasir_solve(s)==10&&ipasir_val(s,1)==1); // -1 was consumed on UNKNOWN
  const char *bundle=tactix_ipasir_bundle(s);assert(bundle);
  char copy[4096];assert(snprintf(copy,sizeof copy,"%s",bundle)>0);
  ipasir_set_terminate(s,&never,terminate);
  assert(ipasir_val(s,1)==1&&!strcmp(copy,tactix_ipasir_bundle(s)));
  ipasir_assume(s,-1);
  assert(ipasir_solve(s)==20&&never.calls>0&&ipasir_failed(s,-1));
  bundle=tactix_ipasir_bundle(s);assert(bundle);
  assert(snprintf(copy,sizeof copy,"%s",bundle)>0);
  stop.calls=0;ipasir_set_terminate(s,&stop,terminate);
  assert(ipasir_failed(s,-1)&&!strcmp(copy,tactix_ipasir_bundle(s)));
  assert(ipasir_solve(s)==0&&stop.calls==1); // setter overrides previous callback
  ipasir_set_terminate(s,&never,NULL); // NULL disables irrespective of state
  assert(ipasir_solve(s)==10&&ipasir_val(s,1)==1);
  ipasir_set_terminate(s,NULL,cancel_null);
  assert(ipasir_solve(s)==0); // NULL data and every nonzero return are supported
  ipasir_set_terminate(s,NULL,NULL);
  assert(ipasir_solve(s)==10&&ipasir_val(s,1)==1);
  ipasir_add(s,2);ipasir_add(s,3);ipasir_add(s,0);
  ipasir_assume(s,-2);stop.calls=0;stop.limit=2;
  ipasir_set_terminate(s,&stop,terminate);
  assert(ipasir_solve(s)==0&&stop.calls==2); // native admission is interruptible
  ipasir_set_terminate(s,NULL,NULL);
  assert(ipasir_solve(s)==10); // appended F survived; the temporary cube did not
  ipasir_assume(s,-2);ipasir_assume(s,-3);
  assert(ipasir_solve(s)==20&&ipasir_failed(s,-2)&&ipasir_failed(s,-3));
  ipasir_release(s);
  s=ipasir_init();assert(s);
  for(int i=0;i<32;++i) {ipasir_add(s,1);ipasir_add(s,-1);ipasir_add(s,0);}
  stop.calls=0;stop.limit=16;ipasir_set_terminate(s,&stop,terminate);
  assert(ipasir_solve(s)==0&&stop.calls==16); // interrupted fresh input replay
  ipasir_set_terminate(s,NULL,NULL);assert(ipasir_solve(s)==10);
  ipasir_release(s);
  puts("IPASIR termination: UNKNOWN consumption, admission, callback replacement and result lifetimes pass");
}
