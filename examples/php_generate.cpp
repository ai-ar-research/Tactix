// Streaming pairwise PHP generator. Memory is independent of clause count.
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int64_t dimension(const char *text) {
  char *end; errno=0;
  const long long value=strtoll(text,&end,10);
  return errno || !*text || *end || value<1 || value>INT_MAX ? 0 : value;
}
int main(int argc,char **argv) {
  if (argc<3 || argc>4) {
    fprintf(stderr,"usage: %s PIGEONS HOLES [OUTPUT.cnf]\n",argv[0]);return 1;
  }
  const int64_t pigeons=dimension(argv[1]),holes=dimension(argv[2]);
  if (!pigeons || !holes || pigeons*holes>INT_MAX) {
    fputs("invalid PHP dimensions\n",stderr);return 1;
  }
  FILE *output=argc==4 ? fopen(argv[3],"w") : stdout;
  if (!output) {perror(argv[3]);return 1;}
  std::vector<char> buffer(4*1024*1024);
  setvbuf(output,buffer.data(),_IOFBF,buffer.size());
  fprintf(output,"c Pigeonhole principle: %lld pigeons, %lld holes.\np cnf %lld %lld\n",
          (long long)pigeons,(long long)holes,(long long)(pigeons*holes),
          (long long)(pigeons+holes*pigeons*(pigeons-1)/2));
  for (int64_t p=0;p<pigeons;++p) {
    for (int64_t h=0;h<holes;++h) fprintf(output,"%lld ",(long long)(p*holes+h+1));
    fputs("0\n",output);
  }
  for (int64_t h=0;h<holes;++h)
    for (int64_t p=0;p<pigeons;++p)
      for (int64_t q=p+1;q<pigeons;++q)
        fprintf(output,"-%lld -%lld 0\n",(long long)(p*holes+h+1),(long long)(q*holes+h+1));
  const bool failed=ferror(output);
  const int closed=fclose(output);
  return failed || closed ? 1 : 0;
}
