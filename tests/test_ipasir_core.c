#undef NDEBUG
#include "tactix_ipasir.h"
#include "ipasir_test_support.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void snapshot(void *s,const char *prefix,unsigned index,int reduced) {
  if(prefix)copy_query_bundle(tactix_ipasir_bundle(s),prefix,index,20,
                             reduced?TEST_CORE_REQUIRED:TEST_CORE_NONE);
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *prefix=argc==2?argv[1]:NULL;
  void *s=ipasir_init();assert(s);ipasir_add(s,1);ipasir_add(s,0);
  assert(tactix_ipasir_set_core_budget(s,1000000)==0);
  ipasir_assume(s,7);ipasir_assume(s,-1);assert(ipasir_solve(s)==20);
  assert(!ipasir_failed(s,7)&&ipasir_failed(s,-1));snapshot(s,prefix,1,1);
  const char *view=tactix_ipasir_bundle(s);char copy[4096];snprintf(copy,sizeof copy,"%s",view);
  assert(tactix_ipasir_set_core_budget(s,0)==0);
  assert(ipasir_failed(s,-1)&&!strcmp(copy,tactix_ipasir_bundle(s)));
  ipasir_assume(s,7);ipasir_assume(s,-1);assert(ipasir_solve(s)==20);
  assert(ipasir_failed(s,7)&&ipasir_failed(s,-1));snapshot(s,prefix,2,0);
  assert(ipasir_solve(s)==10&&ipasir_val(s,1)==1);ipasir_release(s);
  s=ipasir_init();assert(s);assert(tactix_ipasir_set_core_budget(s,1000000)==0);
  ipasir_assume(s,7);ipasir_assume(s,9);ipasir_assume(s,-9);assert(ipasir_solve(s)==20);
  assert(!ipasir_failed(s,7)&&ipasir_failed(s,9)&&ipasir_failed(s,-9));snapshot(s,prefix,3,1);ipasir_release(s);
  s=ipasir_init();assert(s);ipasir_add(s,1);ipasir_add(s,0);ipasir_add(s,-1);ipasir_add(s,0);
  assert(tactix_ipasir_set_core_budget(s,1000000)==0);ipasir_assume(s,7);assert(ipasir_solve(s)==20);
  assert(!ipasir_failed(s,7));snapshot(s,prefix,4,1);ipasir_release(s);
  puts("IPASIR cores: strict reduction, full-cube fallback, empty/clashing cores and setter lifetimes pass");
}
