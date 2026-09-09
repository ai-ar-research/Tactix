// Fixed proof/model fixtures only; real benchmark executions use SLURM.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "rules/vivify/vivify.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;

static unsigned models(bool instantiate) {
  unsigned checked = 0;
  for (unsigned mask = 0; mask != 16; ++mask) {
    Kernel k; kernel_reserve_vars(k, 4);
    const auto truth = [mask](int x) { bool v = mask & (1u << (std::abs(x)-1)); return x > 0 ? v : !v; };
    if (!(truth(1) || truth(instantiate ? 2 : 3))) continue;
    if (instantiate ? (!(truth(-3) || truth(4)) || !(truth(-3) || truth(-4)))
                    : !(truth(1) || truth(-2))) continue;
    for (Var v = 0; v != 4; ++v) {
      k.values[make_lit(v, false)] = mask & (1u << v) ? 1 : -1;
      k.values[make_lit(v, true)] = -k.values[make_lit(v, false)];
    }
    kernel_extend_model(k);
    assert(k.value(internalize(1)) > 0 || k.value(internalize(2)) > 0 || k.value(internalize(3)) > 0);
    ++checked;
  }
  assert(checked > 0); return checked;
}
int main(int argc, char **argv) {
  assert(argc == 6);
  const bool instantiate = std::atoi(argv[1]), lrat = std::atoi(argv[2]), reuse = std::atoi(argv[3]);
  const unsigned checked = models(instantiate);
  std::vector<std::vector<int>> clauses = {{1, 2, 3}};
  if (instantiate) { clauses.push_back({-3, 4}); clauses.push_back({-3, -4}); }
  else clauses.push_back({1, -2});
  for (int p = 0; p < 4; ++p) clauses.push_back({5+3*p, 6+3*p, 7+3*p});
  for (int h = 0; h < 3; ++h) for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b)
    clauses.push_back({-(5+3*a+h), -(5+3*b+h)});
  FILE *cnf = std::fopen(argv[4], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf 16 %zu\n", clauses.size());
  for (const auto &clause : clauses) { for (int lit : clause) std::fprintf(cnf, "%d ", lit); std::fputs("0\n", cnf); }
  std::fclose(cnf);
  Kernel k; k.vivify_reuse = reuse;
  Proof proof; assert(proof.open(argv[5], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii)); k.proof = &proof;
  FILE *input = std::fopen(argv[4], "r"); assert(input); assert(!parse_dimacs(input, k)); std::fclose(input);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto before = k.stats.bve_marks;
  assert(clause_at(k.arena, 0)->size == 3);
  assert(vivify_try(k, 0) == ApplyResult::Ok);
  assert(k.stats.bve_marks - before == 3);
  assert(k.bve_retry[0] && k.bve_retry[1] && k.bve_retry[2]);
  assert(kernel_binary_live(k, internalize(1), internalize(instantiate ? 2 : 3)));
  assert(default_solve(k) == 20);
  k.proof = nullptr; proof.flush();
  std::printf("PASS: %u exhaustive image models and explicit binary-Vivify proof fixture\n", checked);
}
