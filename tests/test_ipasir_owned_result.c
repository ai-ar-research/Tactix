#undef NDEBUG
#include "tactix_ipasir.h"
#include "ipasir_test_support.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int stop(void *unused) {(void)unused;return 1;}
static int exists(const char *prefix) {
  char path[4096];snprintf(path,sizeof path,"%s.json",prefix);
  FILE *f=fopen(path,"rb");if(!f)return 0;assert(!fclose(f));return 1;
}
static void save(const tactix_ipasir_result *r,const char *prefix,unsigned index) {
  if(prefix)copy_query_bundle(tactix_ipasir_result_bundle(r),prefix,index,
                             tactix_ipasir_result_status(r),TEST_CORE_OPTIONAL);
}
int main(int argc,char **argv) {
  assert(argc==1||argc==2);const char *out=argc==2?argv[1]:NULL;
  assert(!tactix_ipasir_result_acquire(NULL));tactix_ipasir_result_release(NULL);
  assert(!tactix_ipasir_result_status(NULL)&&!tactix_ipasir_result_variables(NULL));
  assert(!tactix_ipasir_result_val(NULL,1)&&!tactix_ipasir_result_failed(NULL,1));
  assert(!tactix_ipasir_result_assumptions(NULL)&&!tactix_ipasir_result_assumption(NULL,0));
  assert(!tactix_ipasir_result_core_size(NULL)&&!tactix_ipasir_result_core_literal(NULL,0));
  assert(!tactix_ipasir_result_bundle(NULL));
  void *s=ipasir_init();assert(s&&!tactix_ipasir_result_acquire(s));
  ipasir_add(s,1);ipasir_add(s,0);assert(!tactix_ipasir_set_core_budget(s,1000000));
  ipasir_assume(s,7);ipasir_assume(s,-1);ipasir_assume(s,-1);assert(ipasir_solve(s)==20);
  tactix_ipasir_result *core=tactix_ipasir_result_acquire(s),*copy=tactix_ipasir_result_acquire(s);
  assert(core&&copy&&core!=copy&&tactix_ipasir_result_status(core)==20);
  assert(tactix_ipasir_result_assumptions(core)==3);
  assert(tactix_ipasir_result_assumption(core,0)==7);
  assert(tactix_ipasir_result_assumption(core,1)==-1&&tactix_ipasir_result_assumption(core,2)==-1);
  assert(!tactix_ipasir_result_assumption(core,3));
  assert(tactix_ipasir_result_core_size(core)==1&&tactix_ipasir_result_core_literal(core,0)==-1);
  assert(!tactix_ipasir_result_core_literal(core,1)&&tactix_ipasir_result_failed(core,-1));
  assert(!tactix_ipasir_result_failed(core,1)&&!tactix_ipasir_result_failed(core,INT_MIN));
  assert(!tactix_ipasir_result_variables(core)&&!tactix_ipasir_result_val(core,1));
  char old[4096];snprintf(old,sizeof old,"%s",tactix_ipasir_result_bundle(core));
  ipasir_assume(s,-9);assert(!tactix_ipasir_result_acquire(s)&&!ipasir_failed(s,-1));
  assert(tactix_ipasir_result_failed(core,-1)&&exists(old));
  assert(ipasir_solve(s)==10);
  tactix_ipasir_result *sat=tactix_ipasir_result_acquire(s);assert(sat);
  assert(tactix_ipasir_result_variables(sat)==9&&tactix_ipasir_result_status(sat)==10);
  assert(tactix_ipasir_result_val(sat,1)==1&&tactix_ipasir_result_val(sat,-1)==1);
  assert(tactix_ipasir_result_val(sat,9)==-9&&tactix_ipasir_result_val(sat,-9)==-9);
  assert(!tactix_ipasir_result_val(sat,INT_MIN)&&!tactix_ipasir_result_val(sat,10));
  assert(!tactix_ipasir_result_core_size(sat)&&!tactix_ipasir_result_failed(sat,-1));
  char sat_files[4096],discarded[4096]="";
  snprintf(sat_files,sizeof sat_files,"%s",tactix_ipasir_result_bundle(sat));
  for(unsigned i=0;i<8;++i) {
    ipasir_assume(s,-9);assert(ipasir_solve(s)==10);
    if(*discarded)assert(exists(discarded)); // public session-lifetime contract
    snprintf(discarded,sizeof discarded,"%s",tactix_ipasir_bundle(s));
    assert(exists(old)&&exists(sat_files)&&exists(discarded));
  }
  ipasir_add(s,-1);assert(!tactix_ipasir_result_acquire(s));ipasir_add(s,0);
  assert(ipasir_solve(s)==20);tactix_ipasir_result *hard=tactix_ipasir_result_acquire(s);
  assert(hard&&!tactix_ipasir_result_core_size(hard)&&exists(discarded));
  char hard_files[4096],directory[4096];
  snprintf(hard_files,sizeof hard_files,"%s",tactix_ipasir_result_bundle(hard));
  snprintf(directory,sizeof directory,"%s",old);*strrchr(directory,'/')=0;
  ipasir_set_terminate(s,NULL,stop);assert(!ipasir_solve(s)&&!tactix_ipasir_result_acquire(s));
  ipasir_release(s);
  assert(!exists(discarded)); // no owned result retained these query files
  assert(exists(old)&&!strcmp(old,tactix_ipasir_result_bundle(core)));
  assert(tactix_ipasir_result_failed(core,-1)&&tactix_ipasir_result_val(sat,9)==-9);
  save(core,out,1);save(sat,out,2);save(hard,out,3);
  tactix_ipasir_result_release(core);assert(exists(old));
  tactix_ipasir_result_release(copy);assert(!exists(old)&&exists(sat_files)&&exists(hard_files));
  tactix_ipasir_result_release(sat);assert(!exists(sat_files)&&exists(hard_files));
  tactix_ipasir_result_release(hard);assert(!exists(hard_files));
  struct stat remaining;assert(stat(directory,&remaining)!=0);
  puts("Owned IPASIR results: each result retains only its query; the last owner cleans the directory");
}
