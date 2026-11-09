// Exact PHP definitions and public regional root inference, then full solve.
#include "tactix.h"
#include "php_input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static int rule(const char *name) {
  for(int id=0;id<tactix_rule_count();++id)
    if(!std::strcmp(tactix_rule_name(id),name))return id;
  return -1;
}
static uint64_t definitions(tactix *s, php_example::Matrix matrix,
                            php_example::Matrix parent_cols, bool prioritize) {
  uint64_t count=0;const int probe=rule("Probe"),sweep=rule("Sweep");
  if(tactix_regions_begin(s))std::exit(1);
  const auto define=[&](int kind,int a,int b) {
    int z=tactix_define_extension(s,kind,a,b);if(z<0)std::exit(1);++count;return z;
  };
  const auto append=[](auto &to,const auto &from){to.insert(to.end(),from.begin(),from.end());};
  const auto query=[&](int literal,std::vector<int> variables,bool local) {
    int id=tactix_region_add(s,variables.data(),int(variables.size()));
    if(id<0||tactix_region_queue(s,local?sweep:probe,literal,id,
          local?4096:0)||
       (prioritize&&tactix_bump_variable(s,std::abs(literal),1)))std::exit(1);
  };
  auto parent_rows=matrix;
 // Base column prefixes: derive their AMO witnesses directly from the input
 // encoding. This base work may be cubic, matching pairwise input size.
 using Matrix=std::vector<std::vector<int>>;
 Matrix prefix(matrix.size(),std::vector<int>(matrix[0].size())),capacity=prefix;
 for(size_t j=0;j<matrix[0].size();++j) {
  prefix[0][j]=matrix[0][j];
  for(size_t i=1;i<matrix.size();++i) {
   capacity[i][j]=define(TACTIX_EXTENSION_AND,prefix[i-1][j],matrix[i][j]);
   if(i+1<matrix.size())prefix[i][j]=define(TACTIX_EXTENSION_OR,prefix[i-1][j],matrix[i][j]);
   auto variables=parent_cols[j];
   for(size_t r=0;r<i;++r)variables.push_back(prefix[r][j]);
   variables.push_back(capacity[i][j]);
   query(capacity[i][j],std::move(variables),false);parent_cols[j].push_back(capacity[i][j]);
   if(i+1<matrix.size())parent_cols[j].push_back(prefix[i][j]);
  }
 }
 while(matrix[0].size()>1) {
  const size_t n=matrix[0].size();Matrix q(n,std::vector<int>(n-1)),conjunction=q,child_prefix=q,child_capacity=q;
  std::vector<std::vector<int>> child_cols(n-1),child_rows(n);
  for(size_t i=0;i<n;++i)for(size_t j=0;j+1<n;++j) {
   conjunction[i][j]=define(TACTIX_EXTENSION_AND,matrix[i][n-1],matrix[n][j]);
   q[i][j]=define(TACTIX_EXTENSION_OR,matrix[i][j],conjunction[i][j]);child_cols[j].push_back(q[i][j]);child_rows[i].push_back(q[i][j]);
  }
  for(size_t j=0;j+1<n;++j) {
   const int b=matrix[n][j];std::vector<int> guard(n),right(n),right_and(n),difference(n);
   child_prefix[0][j]=q[0][j];right[0]=q[0][j];right_and[0]=conjunction[0][j];
   for(size_t i=1;i+1<n;++i){child_prefix[i][j]=define(TACTIX_EXTENSION_OR,child_prefix[i-1][j],q[i][j]);child_cols[j].push_back(child_prefix[i][j]);}
   // Prefix-versus-last-pigeon exclusions, proved backward using one adjacent
   // prefix implication. The final one is the parent's existing AMO witness.
   guard[n-1]=capacity[n][j];
   for(size_t tail=n-1;tail>0;--tail) {
    size_t i=tail-1;guard[i]=define(TACTIX_EXTENSION_AND,prefix[i][j],b);
    std::vector<int> variables={prefix[i][j],prefix[i+1][j],b,guard[i],guard[i+1]};
    query(guard[i],std::move(variables),true);
   }
   for(size_t i=1;i<n;++i) {
    std::vector<int> common={child_prefix[i-1][j],q[i][j],conjunction[i][j],matrix[i][j],matrix[i][n-1],b,
      prefix[i][j],prefix[i-1][j],prefix[i][n-1],prefix[i-1][n-1],right[i-1],right_and[i-1]};
    if(difference[i-1])common.push_back(difference[i-1]);
    if(i+1<n) {
     // Only Qprefix -> transformed-parent-prefix is needed. Its violation
     // separates by b into two UP-refutable predicates, using the previously
     // established prefix implication rather than expanding old columns.
     right_and[i]=define(TACTIX_EXTENSION_AND,prefix[i][n-1],b);
     right[i]=define(TACTIX_EXTENSION_OR,prefix[i][j],right_and[i]);
     difference[i]=define(TACTIX_EXTENSION_AND,child_prefix[i][j],-right[i]);
     auto variables=common;append(variables,std::vector<int>{child_prefix[i][j],right_and[i],right[i],difference[i]});
     query(difference[i],std::move(variables),true);
    }
    int collision=define(TACTIX_EXTENSION_AND,child_prefix[i-1][j],q[i][j]);child_capacity[i][j]=collision;child_cols[j].push_back(collision);
    auto variables=common;append(variables,std::vector<int>{capacity[i][j],capacity[i][n-1],guard[i],collision});
    query(collision,std::move(variables),true);
   }
  }
  for(size_t i=0;i<q.size();++i) {
   const auto &row=q[i];int summary=row[0];
   if(row.size()==1){summary=define(TACTIX_EXTENSION_OR,summary,summary);child_rows[i].push_back(summary);}
   else for(size_t j=1;j<row.size();++j){summary=define(TACTIX_EXTENSION_OR,summary,row[j]);child_rows[i].push_back(summary);}
   std::vector<int> variables=parent_rows[i];append(variables,parent_rows.back());append(variables,parent_cols.back());append(variables,child_rows[i]);append(variables,conjunction[i]);
   query(-summary,std::move(variables),false);
  }
  matrix=std::move(q);prefix=std::move(child_prefix);capacity=std::move(child_capacity);parent_cols=std::move(child_cols);parent_rows=std::move(child_rows);
 }
  return count;
}
int main(int argc,char **argv) {
  if(argc<2||argc>4){std::fprintf(stderr,"usage: %s INPUT.cnf [PROOF] [--prioritize]\n",argv[0]);return 1;}
  const char *proof=nullptr;bool prioritize=false;
  for(int i=2;i<argc;++i) {
    if(!std::strcmp(argv[i],"--prioritize")&&!prioritize)prioritize=true;
    else if(!proof&&argv[i][0]!='-')proof=argv[i];else return 1;
  }
  std::unique_ptr<FILE,decltype(&std::fclose)> input(std::fopen(argv[1],"r"),std::fclose);
  if(!input)return 1;
  php_example::Matrix columns;auto matrix=php_example::read(input.get(),&columns);
  if(!matrix||std::fseek(input.get(),0,SEEK_SET)) {
    std::fputs("unsupported input: expected unary PHP with binary AMO implications\n",stderr);return 1;
  }
  std::unique_ptr<tactix,decltype(&tactix_release)> solver(tactix_init(),tactix_release);auto *s=solver.get();
  if((proof&&tactix_proof(s,proof,TACTIX_PROOF_PBP))||tactix_read_dimacs(s,input.get()))return 1;
  const auto count=definitions(s,std::move(*matrix),std::move(columns),prioritize);
  std::printf("c php-er definitions=%llu prioritized=%d\n",(unsigned long long)count,prioritize);std::fflush(stdout);
  const int result=tactix_solve(s);
  std::printf("c php-er queries=%llu units=%llu lemmas=%llu ticks=%llu\n",
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_QUERIES),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_UNITS),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_LEMMAS),
      (unsigned long long)tactix_stat(s,TACTIX_STAT_REGION_TICKS));
  std::puts(result==20?"s UNSATISFIABLE":result==10?"s SATISFIABLE":"s UNKNOWN");
  return result==10||result==20?result:1;
}
