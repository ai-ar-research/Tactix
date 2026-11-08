// PHP bucket definitions, then ordinary DefaultPolicy search. Public API only.
#include "tactix.h"
#include "php_input.h"
#include <cstdio>
#include <cstring>
#include <memory>

int main(int argc, char **argv) {
  if (argc<2 || argc>4) {
    std::fprintf(stderr,"usage: %s INPUT.cnf [PROOF] [--prioritize]\n",argv[0]);
    return 1;
  }
  const char *proof=nullptr;
  bool prioritize=false;
  for (int i=2;i<argc;++i) {
    if (!std::strcmp(argv[i],"--prioritize")&&!prioritize)prioritize=true;
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
  std::printf("c php-er definitions=%u prioritized=%d\n",definitions,prioritize);
  const int result=tactix_solve(s);
  std::printf("c php-er decisions=%llu\n",(unsigned long long)tactix_stat(s,TACTIX_STAT_DECISIONS));
  std::puts(result==20 ? "s UNSATISFIABLE":result==10 ? "s SATISFIABLE":"s UNKNOWN");
  return result==10||result==20 ? result:1;
}
