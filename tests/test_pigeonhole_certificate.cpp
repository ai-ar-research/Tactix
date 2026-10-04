#undef NDEBUG
#include "proof/pigeonhole_certificate.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <set>

using Matrix = std::vector<std::vector<int32_t>>;
struct Sink {
  int32_t extent;
  FILE *proof = nullptr;
  uint64_t maximum = UINT64_MAX, additions = 0, helpers = 0, literals = 0;
  bool stopped = false;
  bool fresh(int32_t &out) {
    if (stopped || extent == INT32_MAX) return !(stopped = true);
    out = ++extent; ++helpers; return true;
  }
  bool add(const std::vector<int32_t> &clause, bool, const char *) {
    if (stopped || additions == maximum) return !(stopped = true);
    std::set<int32_t> seen;
    for (int32_t lit : clause) {
      assert(lit && lit != INT32_MIN && !seen.count(-lit) && seen.insert(lit).second);
      if (proof) std::fprintf(proof, "%d ", lit);
    }
    if (proof) std::fputs("0\n", proof);
    ++additions; literals += clause.size(); return true;
  }
};
static Matrix matrix(unsigned holes) {
  Matrix x(holes+1, std::vector<int32_t>(holes));
  int32_t v = 0;
  for (auto &row : x) for (int32_t &lit : row) lit = ++v;
  return x;
}
int main(int argc, char **argv) {
  if (argc == 1) {
    for (unsigned h = 1; h <= 6; ++h) {
      Sink sink{static_cast<int32_t>(h*(h+1))};
      tactix::pigeonhole_certificate::Certificate<Sink> proof(sink);
      assert(proof.prove(matrix(h)) && sink.additions);
      Sink stopped{static_cast<int32_t>(h*(h+1))}; stopped.maximum = 2;
      tactix::pigeonhole_certificate::Certificate<Sink> limited(stopped);
      assert(!limited.prove(matrix(h)) && stopped.additions == 2);
    }
    std::puts("test_pigeonhole_certificate: passed"); return 0;
  }
  assert(argc == 3 || argc == 5);
  const unsigned holes = std::strtoul(argv[1], nullptr, 10);
  assert(holes && holes <= 1000);
  const bool guarded = std::strtoul(argv[2], nullptr, 10);
  const int32_t original = holes*(holes+1), guard = guarded ? original+1 : 0;
  const auto x = matrix(holes);
  FILE *cnf = nullptr, *out = nullptr;
  if (argc == 5) {
    cnf = std::fopen(argv[3],"w"); out = std::fopen(argv[4],"w"); assert(cnf && out);
    // Guarded premises encode the implication guard OR pigeonhole. A disjoint
    // unit-free gadget permits an every-addition audit after deriving guard.
    const uint64_t clauses = holes+1 + uint64_t(holes)*holes*(holes+1)/2 + (guarded ? 4 : 0);
    std::fprintf(cnf,"p cnf %d %llu\n", original+(guarded ? 3 : 0), (unsigned long long)clauses);
    for (const auto &row : x) {
      for (int32_t lit : row) std::fprintf(cnf,"%d ",lit);
      if (guard) std::fprintf(cnf,"%d ",guard);
      std::fputs("0\n",cnf);
    }
    for (unsigned h = 0; h < holes; ++h) for (unsigned a = 0; a <= holes; ++a)
      for (unsigned b = a+1; b <= holes; ++b) {
        std::fprintf(cnf,"%d %d ",-x[a][h],-x[b][h]);
        if (guard) std::fprintf(cnf,"%d ",guard);
        std::fputs("0\n",cnf);
      }
    if (guarded) for (int a : {1,-1}) for (int b : {1,-1})
      std::fprintf(cnf,"%d %d 0\n",a*(guard+1),b*(guard+2));
    std::fclose(cnf);
  }
  Sink sink{original+(guarded ? 3 : 0),out};
  tactix::pigeonhole_certificate::Certificate<Sink> proof(sink,guard);
  assert(proof.prove(x));
  if (out) {
    if (guarded) std::fprintf(out,"%d 0\n0\n",guard+1);
    assert(!std::ferror(out)); std::fclose(out);
  }
  std::printf("{\"holes\":%u,\"guarded\":%s,\"helpers\":%llu,\"additions\":%llu,\"literals\":%llu}\n",
      holes,guarded?"true":"false",(unsigned long long)sink.helpers,
      (unsigned long long)sink.additions,(unsigned long long)sink.literals);
}
