// Fixed environment representation controls, also linked to the actual parent
// Sweep object. Scratch contents are not semantic state; all encoded fields,
// BFS order, proof identities, filtering statistics and model sets are checked.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/sweep/sweep.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace tactix;

static uint64_t digest = UINT64_C(1469598103934665603);
static uint64_t environments = 0, proof_bytes = 0, stripped = 0;
static void hash(uint64_t x) { digest = (digest ^ x) * UINT64_C(1099511628211); }
template <class T> static void vector_hash(const std::vector<T> &v) {
  hash(v.size());
  for (const auto &x : v) hash(x);
}
static bool value(Lit l, unsigned mask) {
  return bool(mask & (1u << var_of(l))) != negated(l);
}
static std::vector<unsigned> models(const Kernel &k) {
  std::vector<unsigned> result;
  for (unsigned mask = 0; mask < 64; ++mask) {
    bool okay = true;
    for (Lit l : k.trail) okay &= value(l, mask);
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage()) {
        bool satisfied = false;
        for (Lit l : *clause) satisfied |= value(l, mask);
        okay &= satisfied;
      }
      ref += clause_actual_words(clause);
    }
    for (Lit a = 0; a < 12; ++a)
      for (Lit b = a + 1; b < 12; ++b)
        if (kernel_binary_live(k, a, b)) okay &= value(a, mask) || value(b, mask);
    if (okay) result.push_back(mask);
  }
  return result;
}
static void run(unsigned root_mode, unsigned position, bool strip,
                unsigned proof_mode, bool tight) {
  Kernel k;
  kernel_reserve_vars(k, 6);
  unsigned index = 0;
  for (int z : {-3, 3}) for (int w : {-4, 4})
    for (bool reverse : {false, true}) {
      std::vector<Lit> lits = {internalize(reverse ? 1 : -1), internalize(reverse ? -2 : 2), internalize(z), internalize(w)};
      if (index++ % 3 == 0) lits.insert(lits.begin() + position, internalize(6));
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, ++k.next_clause_id);
    }
  for (const auto &clause : std::vector<std::vector<int>>{{5,6,2}, {6,5,-1}, {2,3,4}, {2,3,4}}) {
    std::vector<Lit> lits;
    for (int l : clause) lits.push_back(internalize(l));
    kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, ++k.next_clause_id);
  }
  for (unsigned i = 0; i < 2; ++i)
    kernel_add_binary_clause(k, internalize(5), internalize(1), ++k.next_clause_id);
  if (root_mode) {
    if (root_mode != 2) k.unit_ids[5] = ++k.next_clause_id;
    kernel_assign_unit(k, internalize(root_mode == 3 ? 6 : -6));
    if (root_mode == 4) {
      k.unit_ids[4] = ++k.next_clause_id;
      kernel_assign_unit(k, internalize(-5));
    }
  }
  const auto original_models = models(k);
  assert(!original_models.empty());
  char path[] = "sweep-rootfree-XXXXXX";
  const int fd = mkstemp(path); assert(fd >= 0); close(fd);
  Proof proof;
  if (proof_mode) {
    assert(proof.open(path, proof_mode == 1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof = &proof;
  }
  SweepLimits limits;
  limits.strip_false = strip;
  limits.max_env_vars = tight ? 3 : 64;
  limits.max_env_clauses = tight ? 3 : 64;
  limits.max_depth = tight ? 1 : 3;
  SweepRound round;
  assert(sweep_guard(k));
  const auto &rule = tactix_rules()[tactix_rule_id("Sweep")];
  const Arena original_arena = k.arena;
  uint64_t instances[16];
  const auto count = rule.instances(k, instances, 16);
  assert(k.arena == original_arena);
  hash(count);
  for (unsigned i = 0; i < count; ++i) hash(instances[i]);
  sweep_round_begin(k, round);
  hash(root_mode); hash(position); hash(strip); hash(proof_mode); hash(tight);
  // Reuse one environment across seeds, including fixed seeds and changes
  // between fully root-free and filtered last clauses.
  for (unsigned seed : {0u, 5u, 1u, 4u, 2u, 0u}) {
    const bool built = sweep_env_build(k, seed, limits, round);
    hash(seed); hash(built); environments += built;
    const auto &e = round.env;
    vector_hash(e.vars); vector_hash(e.sub_of_var); vector_hash(e.touched);
    vector_hash(e.depth); vector_hash(e.clause_ids); vector_hash(e.clause_lits);
    vector_hash(e.clause_off); vector_hash(e.strip_ids); vector_hash(e.strip_off);
    vector_hash(e.key_slots); hash(e.key_mask); hash(e.formula_epoch);
    hash(k.stats.sweep_ticks); hash(k.stats.sweep_ticks_dedup);
    hash(k.stats.sweep_env_lits); hash(k.stats.sweep_env_false);
    hash(k.stats.sweep_env_freefalse); hash(k.stats.sweep_env_stripped);
    hash(k.stats.sweep_env_units);
    vector_hash(k.arena); vector_hash(k.values); vector_hash(k.trail);
    vector_hash(k.watches.arena);
    assert(models(k) == original_models);
  }
  stripped += k.stats.sweep_env_stripped;
  k.proof = nullptr;
  if (proof_mode) proof.flush();
  FILE *input = std::fopen(path, "rb"); assert(input);
  for (int ch; (ch = std::fgetc(input)) != EOF;) { hash(ch); ++proof_bytes; }
  assert(!std::ferror(input)); std::fclose(input);
  assert(std::remove(path) == 0);
}
int main() {
  unsigned cases = 0;
  for (unsigned root_mode = 0; root_mode < 5; ++root_mode)
    for (unsigned position = 0; position < 5; ++position)
      for (bool strip : {false, true})
        for (unsigned mode = 0; mode < 3; ++mode)
          for (bool tight : {false, true}) {
            run(root_mode, position, strip, mode, tight); ++cases;
          }
  assert(environments && stripped && proof_bytes);
  std::printf("sweep-rootfree cases=%u environments=%llu stripped=%llu proof-bytes=%llu digest=%016llx\n",
      cases, (unsigned long long)environments, (unsigned long long)stripped,
      (unsigned long long)proof_bytes, (unsigned long long)digest);
  return 0;
}
