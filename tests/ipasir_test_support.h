#ifndef TACTIX_TESTS_IPASIR_TEST_SUPPORT_H
#define TACTIX_TESTS_IPASIR_TEST_SUPPORT_H
#include <assert.h>
#include <stdio.h>

enum { TEST_CORE_NONE, TEST_CORE_OPTIONAL, TEST_CORE_REQUIRED };

/* Preserve the certificate oracle's prefix-index naming. Required members
 * must exist even when the caller only checks ownership without exporting. */
static inline void copy_query_bundle(const char *bundle,const char *prefix,
    unsigned index,int status,int core) {
  assert(bundle&&(status==10||status==20));
  const char *suffixes[]={".cnf",".proof",".json",status==10?".model":".core",
                          "-core.cnf","-core.proof"};
  for(unsigned i=0;i<(core==TEST_CORE_NONE?4u:6u);++i) {
    char from[4096],to[4096];
    int count=snprintf(from,sizeof from,"%s%s",bundle,suffixes[i]);
    assert(count>0&&(size_t)count<sizeof from);
    FILE *in=fopen(from,"rb");
    if(!in) {assert(i>=4&&core==TEST_CORE_OPTIONAL);continue;}
    if(prefix) {
      count=snprintf(to,sizeof to,"%s-%u%s",prefix,index,suffixes[i]);
      assert(count>0&&(size_t)count<sizeof to);
      FILE *out=fopen(to,"wb");assert(out);char bytes[4096];size_t n;
      while((n=fread(bytes,1,sizeof bytes,in)))assert(fwrite(bytes,1,n,out)==n);
      assert(!ferror(in));assert(!fclose(out));
    }
    assert(!fclose(in));
  }
}
#endif
