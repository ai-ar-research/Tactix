// External PHP recognition: only the public C strategy API is used.
#include "php_input.h"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <numeric>

namespace {
struct Input {
  int variables = 0;
  std::vector<std::vector<int>> clauses;
  std::vector<std::vector<unsigned>> edges;
};
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
  const Input &input;
  std::vector<unsigned> queue, stamp;
  unsigned generation = 0;
  explicit Paths(const Input &p) : input(p), stamp(p.edges.size()) {}
  void run(int literal) {
    if (!++generation) { std::fill(stamp.begin(),stamp.end(),0); ++generation; }
    queue.clear(); const unsigned start=node(literal);
    queue.push_back(start); stamp[start]=generation;
    for (size_t head=0; head<queue.size(); ++head)
      for (auto e: input.edges[queue[head]]) if (stamp[e]!=generation) {
        stamp[e]=generation;queue.push_back(e);
      }
  }
  bool has(int literal) const { return stamp[node(literal)]==generation; }
};
std::optional<php_example::Matrix> recognize(const Input &p, const std::vector<unsigned> &rows, int h) {
  if (rows.size()!=size_t(h+1) || uint64_t(p.variables)+h+1>INT_MAX) return {};
  std::vector<int> owner(p.variables,-1), matrix(size_t(h)*(h+1));
  for (int i=0;i<=h;++i) for (int lit:p.clauses[rows[i]]) {
    const auto v=unsigned(std::abs(lit)-1);
    if (owner[v]!=-1) return {};
    owner[v]=i;
  }
  std::copy(p.clauses[rows[0]].begin(),p.clauses[rows[0]].end(),matrix.begin());
  Paths paths(p);
  for (int j=0;j<h;++j) {
    paths.run(matrix[j]);
    for (int i=1;i<=h;++i) {
      int match=0;
      for (int lit:p.clauses[rows[i]]) if (paths.has(-lit)) {
        if (match) return {};
        match=lit;
      }
      if (!match) return {};
      matrix[i*h+j]=match;
    }
  }
  // A bijection per row, and every row pair supported: missing collisions are
  // never assumed.
  for (int i=0;i<=h;++i) {
    std::vector<int> values(matrix.begin()+i*h,matrix.begin()+(i+1)*h);
    std::sort(values.begin(),values.end());
    if (std::adjacent_find(values.begin(),values.end())!=values.end()) return {};
    for (int j=0;j<h;++j) {
      paths.run(matrix[i*h+j]);
      for (int k=i+1;k<=h;++k) if (!paths.has(-matrix[k*h+j])) return {};
    }
  }
  php_example::Matrix result;
  for (int i=0;i<=h;++i)result.emplace_back(matrix.begin()+i*h,matrix.begin()+(i+1)*h);
  return result;
}
}
std::optional<php_example::Matrix> php_example::read(FILE *in, Matrix *column_support) {
  char word[64];long long variables, count;
  if (!token(in,word)||strcmp(word,"p")||!token(in,word)||strcmp(word,"cnf")||
      !number(in,variables)||!number(in,count)||variables<1||variables>INT_MAX||count<1||count>INT_MAX) return {};
  Input input;auto *p=&input;p->variables=int(variables);
  for (long long i=0;i<count;++i) {
    std::vector<int> row; long long lit;
    for (;;) {
      if (!number(in,lit)||lit < -variables||lit > variables) {return {};}
      if (!lit) break;
      row.push_back(int(lit));
    }
    if (row.empty()) {return {};}
    std::sort(row.begin(),row.end());
    if (std::adjacent_find(row.begin(),row.end())!=row.end()) {return {};}
    p->clauses.push_back(std::move(row));
  }
  if (token(in,word)||ferror(in)) {return {};}
  p->edges.resize(2*size_t(variables));
  std::vector<std::vector<unsigned>> positive(size_t(variables)+1);
  for (unsigned cid=0;cid<p->clauses.size();++cid) {
    const auto &c=p->clauses[cid];
    // Positive binary pigeon rows (h=2) are not capacity premises.
    // Including them would make this already-UNSAT 2-SAT graph misleading.
    if (c.size()==2 && c.front()<0) {
      p->edges[node(-c[0])].push_back(node(c[1]));
      p->edges[node(-c[1])].push_back(node(c[0]));
    }
    if (c.size()>=2 && c.size()<positive.size() && c.front()>0)
      positive[c.size()].push_back(cid);
  }
  for (size_t h=2;h<positive.size();++h)
    if (positive[h].size()==h+1)
      if (auto matrix=recognize(*p,positive[h],int(h))) {
        if (column_support) {
          std::vector<unsigned> parent(static_cast<size_t>(variables));
          std::iota(parent.begin(), parent.end(), 0u);
          const auto find=[&](unsigned v) {
            while (parent[v]!=v) { parent[v]=parent[parent[v]]; v=parent[v]; }
            return v;
          };
          for (unsigned l=0;l<p->edges.size();++l) for (unsigned other:p->edges[l]) {
            auto a=find(l/2), b=find(other/2); if(a!=b)parent[a]=b;
          }
          std::vector<int> column(size_t(variables),-1);
          column_support->assign(h,{});
          for (size_t j=0;j<h;++j)column[find(unsigned((*matrix)[0][j]-1))]=int(j);
          for (unsigned v=0;v<unsigned(variables);++v)
            if (int j=column[find(v)];j>=0)(*column_support)[j].push_back(int(v+1));
        }
        return matrix;
      }
  return {};
}
