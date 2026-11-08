// External PHP recognition: only the public C strategy API is used.
#include "php_input.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>

struct php_input {
  struct Edge { unsigned to, clause; };
  int variables = 0, holes = 0;
  std::vector<std::vector<int>> clauses;
  std::vector<std::vector<Edge>> edges;
  std::vector<int> matrix;
};
namespace {
unsigned node(int lit) { return 2u*unsigned(std::abs(lit)-1)+(lit<0); }
bool token(FILE *in, char word[64]) {
  for (;;) {
    if (fscanf(in,"%63s",word)!=1) return false;
    if (strcmp(word,"c")) return true;
    int c; do { c=fgetc(in); } while (c!=EOF && c!='\n');
  }
}
bool number(FILE *in, long long &n) {
  char word[64], *end;
  if (!token(in,word)) return false;
  errno=0; n=strtoll(word,&end,10);
  return !errno && *word && !*end;
}
struct Paths {
  const php_input &input;
  std::vector<unsigned> previous, reason, queue, stamp;
  unsigned generation = 0;
  explicit Paths(const php_input &p) : input(p), previous(p.edges.size()),
      reason(p.edges.size()), stamp(p.edges.size()) {}
  void run(int literal) {
    if (!++generation) { std::fill(stamp.begin(),stamp.end(),0); ++generation; }
    queue.clear(); const unsigned start=node(literal);
    queue.push_back(start); stamp[start]=generation; previous[start]=start;
    for (size_t head=0; head<queue.size(); ++head)
      for (auto e: input.edges[queue[head]]) if (stamp[e.to]!=generation) {
        stamp[e.to]=generation;previous[e.to]=queue[head];reason[e.to]=e.clause;
        queue.push_back(e.to);
      }
  }
  bool has(int literal) const { return stamp[node(literal)]==generation; }
};
bool recognize(php_input &p, const std::vector<unsigned> &rows, int h) {
  if (rows.size()!=size_t(h+1) || uint64_t(p.variables)+2*uint64_t(h)*(h+1)*(h-1)/3>INT_MAX) return false;
  std::vector<int> owner(p.variables,-1), matrix(size_t(h)*(h+1));
  for (int i=0;i<=h;++i) for (int lit:p.clauses[rows[i]]) {
    const auto v=unsigned(std::abs(lit)-1);
    if (owner[v]!=-1) return false;
    owner[v]=i;
  }
  std::copy(p.clauses[rows[0]].begin(),p.clauses[rows[0]].end(),matrix.begin());
  Paths paths(p);
  for (int j=0;j<h;++j) {
    paths.run(matrix[j]);
    for (int i=1;i<=h;++i) {
      int match=0;
      for (int lit:p.clauses[rows[i]]) if (paths.has(-lit)) {
        if (match) return false;
        match=lit;
      }
      if (!match) return false;
      matrix[i*h+j]=match;
    }
  }
  // A bijection per row, and every pair must be supported, not just pairs
  // with the anchor row. Missing collisions must never be assumed.
  for (int i=0;i<=h;++i) {
    std::vector<int> values(matrix.begin()+i*h,matrix.begin()+(i+1)*h);
    std::sort(values.begin(),values.end());
    if (std::adjacent_find(values.begin(),values.end())!=values.end()) return false;
    for (int j=0;j<h;++j) {
      paths.run(matrix[i*h+j]);
      for (int k=i+1;k<=h;++k) if (!paths.has(-matrix[k*h+j])) return false;
    }
  }
  p.holes=h;p.matrix=std::move(matrix);return true;
}
}
extern "C" php_input *php_input_read(FILE *in) {
  char word[64];long long variables, count;
  if (!token(in,word)||strcmp(word,"p")||!token(in,word)||strcmp(word,"cnf")||
      !number(in,variables)||!number(in,count)||variables<1||variables>INT_MAX||count<1||count>INT_MAX) return nullptr;
  auto *p=new php_input; p->variables=int(variables);
  for (long long i=0;i<count;++i) {
    std::vector<int> row; long long lit;
    for (;;) {
      if (!number(in,lit)||lit < -variables||lit > variables) {delete p;return nullptr;}
      if (!lit) break;
      row.push_back(int(lit));
    }
    if (row.empty()) {delete p;return nullptr;}
    std::sort(row.begin(),row.end());
    if (std::adjacent_find(row.begin(),row.end())!=row.end()) {delete p;return nullptr;}
    p->clauses.push_back(std::move(row));
  }
  if (token(in,word)||ferror(in)) {delete p;return nullptr;}
  p->edges.resize(2*size_t(variables));
  std::vector<std::vector<unsigned>> positive(size_t(variables)+1);
  for (unsigned cid=0;cid<p->clauses.size();++cid) {
    const auto &c=p->clauses[cid];
    // Positive binary pigeon rows (h=2) are not capacity premises.
    // Including them would make this already-UNSAT 2-SAT graph misleading.
    if (c.size()==2 && c.front()<0) {
      p->edges[node(-c[0])].push_back({node(c[1]),cid});
      p->edges[node(-c[1])].push_back({node(c[0]),cid});
    }
    if (c.size()>=2 && c.size()<positive.size() && c.front()>0)
      positive[c.size()].push_back(cid);
  }
  for (size_t h=2;h<positive.size();++h)
    if (positive[h].size()==h+1 && recognize(*p,positive[h],int(h))) return p;
  delete p;return nullptr;
}
extern "C" int php_input_holes(const php_input *p) { return p->holes; }
extern "C" int *php_input_matrix(const php_input *p) {
  auto *m=static_cast<int *>(malloc(p->matrix.size()*sizeof(int)));
  if (m) std::copy(p->matrix.begin(),p->matrix.end(),m);
  return m;
}
extern "C" int php_input_collisions(tactix *s,const php_input *p,uint64_t *derived) {
  Paths paths(*p);const int h=p->holes;
  for (int j=0;j<h;++j) for (int i=0;i<=h;++i) {
    const int a=p->matrix[i*h+j];paths.run(a);
    for (int k=i+1;k<=h;++k) {
      const int b=p->matrix[k*h+j];unsigned at=node(-b);
      if (!paths.has(-b)) return -1;
      // A one-edge implication is the existing pairwise collision itself.
      if (paths.previous[at]==node(a)) continue;
      std::vector<tactix_rup_premise> premises;
      while (at!=node(a)) {
        const auto &c=p->clauses[paths.reason[at]];
        premises.push_back({c.data(),int(c.size())});at=paths.previous[at];
      }
      // Forward path order lets local propagation traverse a chain in one
      // pass instead of one full rescan per implication.
      std::reverse(premises.begin(),premises.end());
      const int target[]={-a,-b};
      const int result=tactix_add_rup(s,target,2,premises.data(),int(premises.size()),UINT64_MAX);
      if (result<0) return -1;
      ++*derived;
      if (result==20) return 20;
    }
  }
  return 0;
}
extern "C" void php_input_release(php_input *p) { delete p; }
