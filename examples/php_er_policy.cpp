// PHP bucket definitions, then ordinary DefaultPolicy search. Public API only.
#include "tactix.h"
#include "php_input.h"
#include <cstdio>
#include <cstring>
#include <memory>

static int rule(const char *name) {
  for(int id=0;id<tactix_rule_count();++id)
    if(!std::strcmp(tactix_rule_name(id),name))return id;
  return -1;
}

static int cdcl(tactix *s) {
  const int propagate=rule("Propagate"),analyze=rule("ConflictAnalysis"),
      restart=rule("Restart"),reduce=rule("Reduce");
  // Stable EVSIDS with Kissat's reluctant-doubling period; ordinary learned
  // clause reduction follows the existing scripted-default example.
  if(tactix_status(s))return tactix_status(s);
  if(tactix_apply(s,rule("SwitchMode"))<0)return 0;
  uint64_t u=1,v=1,next_restart=1024,next_reduce=2000;
  for(;;) {
    int result=tactix_apply(s,propagate);
    if(result==20)return 20;
    if(result==2) {
      const int learned=tactix_apply(s,analyze);
      if(learned==20)return 20;
      if(learned<0)return 0;
      const auto conflicts=tactix_stat(s,TACTIX_STAT_CONFLICTS);
      if(conflicts>=next_restart) {
        if(tactix_apply_instance(s,restart,0)==20)return 20;
        if((u&-u)==v){++u;v=1;}else v*=2;
        next_restart=conflicts+1024*v;
      }
      if(conflicts>=next_reduce) {
        if(tactix_apply(s,reduce)==20)return 20;
        next_reduce=conflicts+300;
      }
    } else {
      if(const int status=tactix_status(s))return status;
      int literal=0;
      if(tactix_decide_candidates(s,&literal,1)!=1||tactix_decide(s,literal)<0)return 0;
    }
  }
}

int main(int argc, char **argv) {
  if (argc<2 || argc>5) {
    std::fprintf(stderr,"usage: %s INPUT.cnf [PROOF] [--prioritize] [--cdcl-only]\n",argv[0]);
    return 1;
  }
  const char *proof=nullptr;
  bool prioritize=false,cdcl_only=false;
  for (int i=2;i<argc;++i) {
    if (!std::strcmp(argv[i],"--prioritize")&&!prioritize)prioritize=true;
    else if (!std::strcmp(argv[i],"--cdcl-only")&&!cdcl_only)cdcl_only=true;
    else if (!proof&&argv[i][0]!='-')proof=argv[i];
    else return 1;
  }
  std::unique_ptr<FILE,decltype(&std::fclose)> input(std::fopen(argv[1],"r"),std::fclose);
  if (!input)return 1;
  auto matrix=php_example::read(input.get());
  if (!matrix || std::fseek(input.get(),0,SEEK_SET)) {
    std::fputs("unsupported input: expected unary PHP with binary AMO implications\n",stderr);
    std::puts("s UNKNOWN");return 1;
  }
  std::unique_ptr<tactix,decltype(&tactix_release)> solver(tactix_init(),tactix_release);
  auto *s=solver.get();
  if ((proof&&tactix_proof(s,proof,TACTIX_PROOF_PBP)) || tactix_read_dimacs(s,input.get()))return 1;
  // Treat the first two holes as one bucket. Use the same definition for
  // every pigeon, retaining pigeon symmetry and adding only O(n) variables.
  unsigned definitions=0;
  for (const auto &row:*matrix) {
    int output=tactix_define_extension(s,TACTIX_EXTENSION_OR,row[0],row[1]);
    if (output<0 || (prioritize&&tactix_bump_variable(s,output,1)))return 1;
    ++definitions;
  }
  std::printf("c php-er definitions=%u prioritized=%d cdcl-only=%d\n",definitions,prioritize,cdcl_only);
  const int result=cdcl_only ? cdcl(s):tactix_solve(s);
  std::printf("c php-er decisions=%llu\n",(unsigned long long)tactix_stat(s,TACTIX_STAT_DECISIONS));
  std::puts(result==20 ? "s UNSATISFIABLE":result==10 ? "s SATISFIABLE":"s UNKNOWN");
  return result==10||result==20 ? result:1;
}
