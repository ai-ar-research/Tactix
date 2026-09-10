// Actual repeated scheduling and canonical transactions, not a comparator
// mirror. The frozen-parent control compares this executable's full digest.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/vivify/vivify.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace tactix;

static uint64_t digest = UINT64_C(1469598103934665603);
static uint64_t proof_bytes = 0, flushed = 0;
static void hash(uint64_t value) {
  digest = (digest ^ value) * UINT64_C(1099511628211);
}
static bool truth(Lit lit, unsigned mask) {
  return bool(mask & (1u << var_of(lit))) != negated(lit);
}
static std::vector<unsigned> models(const Kernel &k) {
  std::vector<unsigned> out;
  for (unsigned mask = 0; mask != 256; ++mask) {
    bool okay = true;
    for (Lit lit : k.trail) okay &= truth(lit, mask);
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage()) {
        bool satisfied = false;
        for (Lit lit : *c) satisfied |= truth(lit, mask);
        okay &= satisfied;
      }
      ref += clause_actual_words(c);
    }
    for (Lit a = 0; a != 16; ++a)
      for (Lit b = a + 1; b != 16; ++b)
        if (kernel_binary_live(k, a, b))
          okay &= truth(a, mask) || truth(b, mask);
    if (okay) out.push_back(mask);
  }
  return out;
}
static void state(const Kernel &k) {
  for (uint32_t word : k.arena) hash(word);
  for (int value : k.values) hash(value);
  for (Lit lit : k.trail) hash(lit);
  for (Lit a = 0; a != 16; ++a)
    for (Lit b = a + 1; b != 16; ++b) hash(kernel_binary_live(k, a, b));
  for (auto bit : k.bve_retry) hash(bit);
  hash(k.stats.vivify_subsumed); hash(k.stats.vivify_shrunken);
  hash(k.stats.vivify_implied); hash(k.stats.vivify_asymmetric);
  hash(k.stats.vivify_instantiated); hash(k.stats.vivify_deleted);
  hash(k.stats.vivify_noop); hash(k.stats.ticks); hash(k.stats.bve_marks);
}
static void query(Kernel &k) {
  const Arena before = k.arena;
  const auto values = k.values;
  std::vector<Ref> first, again;
  vivify_candidates(k, first); vivify_candidates(k, again);
  assert(first == again && k.arena == before && k.values == values);
  hash(first.size());
  for (Ref ref : first) hash(ref);
  const auto &rule = tactix_rules()[tactix_rule_id("Vivify")];
  assert(rule.guard(k));
  uint64_t instances[64];
  const unsigned count = rule.instances(k, instances, 64);
  assert(count == first.size());
  for (unsigned i = 0; i < count; ++i) assert(instances[i] == first[i]);
}
static void check(unsigned scope, bool reuse, bool flush, unsigned priority,
                  bool root_false, unsigned mode) {
  Kernel k; kernel_reserve_vars(k, 8);
  k.vivify_scope = scope; k.vivify_reuse = reuse; k.vivify_flush = flush;
  const std::vector<std::vector<int>> clauses = {
    {1,2,3}, {1,2,3,4}, {3,4,5}, {3,4,5}, {3,4,5,6},
    {4,5,6,7}, {5,6,7,8}, {2,4,6,8}, {1,3,5,7}, {2,3,5,8}};
  for (unsigned i = 0; i < clauses.size(); ++i) {
    std::vector<Lit> lits;
    for (int lit : clauses[i]) lits.push_back(internalize(lit));
    const bool red = i >= 5;
    const Ref ref = kernel_add_large_clause(k, lits.data(), lits.size(), red,
        red ? (i % 3 == 0 ? 2 : i % 3 == 1 ? 5 : 9) : 0, ++k.next_clause_id);
    clause_at(k.arena, ref)->set_vivify(priority == 1 || (priority == 2 && i % 2));
  }
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  if (root_false) {
    k.unit_ids[7] = ++k.next_clause_id;
    kernel_assign_unit(k, internalize(-8));
  }
  const auto before = models(k); assert(!before.empty());
  char path[] = "vivify-count-cache-XXXXXX";
  const int fd = mkstemp(path); assert(fd >= 0); close(fd);
  Proof proof;
  if (mode) {
    assert(proof.open(path, mode == 1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof = &proof;
  }
  hash(scope); hash(reuse); hash(flush); hash(priority); hash(root_false); hash(mode);
  query(k);
  // A cut round and a complete repeat exercise stale scratch, rewritten keys,
  // priority leftovers and re-arming after the consumed set becomes empty.
  for (uint64_t budget : {UINT64_C(1), UINT64_C(0), UINT64_C(0)}) {
    k.work_budget = budget;
    const ApplyResult result = vivify_apply(k);
    assert(result == ApplyResult::Ok || result == ApplyResult::NoOp);
    hash(static_cast<unsigned>(result));
    assert(k.level == 0 && !k.conflict_valid);
    kernel_check_invariants(k);
    assert(models(k) == before);
    state(k); query(k);
  }
  flushed += k.stats.vivify_subsumed;
  k.proof = nullptr; if (mode) proof.flush();
  FILE *input = std::fopen(path, "rb"); assert(input);
  for (int ch; (ch = std::fgetc(input)) != EOF;) { hash(ch); ++proof_bytes; }
  assert(!std::ferror(input)); std::fclose(input);
  assert(std::remove(path) == 0);
}
int main() {
  unsigned cases = 0;
  for (unsigned scope = 0; scope != 6; ++scope)
    for (bool reuse : {false, true}) for (bool flush : {false, true})
      for (unsigned priority = 0; priority != 3; ++priority)
        for (bool root_false : {false, true}) for (unsigned mode = 0; mode != 3; ++mode) {
          check(scope, reuse, flush, priority, root_false, mode); ++cases;
        }
  // Empty smaller Kernel follows larger populated scratch in the same thread.
  Kernel empty; kernel_reserve_vars(empty, 1);
  std::vector<Ref> out; vivify_candidates(empty, out); assert(out.empty());
  assert(proof_bytes && flushed);
  std::printf("vivify-count-cache cases=%u digest=%016llx proof-bytes=%llu subsumed=%llu\n",
      cases, (unsigned long long)digest, (unsigned long long)proof_bytes,
      (unsigned long long)flushed);
}
