// Reuse the fixed exhaustive-model/proof/state helpers and their full matrix.
// The additional population matrix stresses references surviving both ordered
// root-flush passes, the no-sort regime, and switches back to snapshot modes.
#define main vivify_count_cache_matrix
#include "test_vivify_count_cache.cpp"
#undef main

static void wide_query(Kernel &k) {
  const Arena arena = k.arena;
  const auto values = k.values;
  std::vector<Ref> first, second;
  vivify_candidates(k, first);
  vivify_candidates(k, second);
  assert(first == second && k.arena == arena && k.values == values);
  const auto &rule = tactix_rules()[tactix_rule_id("Vivify")];
  assert(rule.guard(k));
  std::vector<uint64_t> instances(first.size() + 1);
  const unsigned n = rule.instances(k, instances.data(), instances.size());
  assert(n == first.size());
  hash(n);
  for (unsigned i = 0; i < n; ++i) {
    assert(instances[i] == first[i]);
    hash(first[i]);
  }
}

static void wide_state(const Kernel &k) {
  state(k);
  for (uint32_t word : k.watches.arena) hash(word);
  for (const auto &head : k.watches.heads) {
    hash(head.offset); hash(head.size);
  }
  hash(k.stats.vivify_rounds);
}

static void population(unsigned scope, bool large, bool reuse, bool flush,
                       unsigned root_mode, unsigned proof_mode) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  k.vivify_scope = scope;
  k.vivify_reuse = reuse;
  k.vivify_flush = flush;
  k.tier1 = 2;
  k.tier2 = 6;
  for (unsigned i = 0; i < 84; ++i) {
    std::vector<Lit> lits;
    const unsigned size = 3 + i % 5;
    for (unsigned j = 0; j < size; ++j)
      lits.push_back(internalize(1 + (i + 3 * j) % 8));
    const bool red = i >= (large ? 82 : 42);
    const Ref ref = kernel_add_large_clause(k, lits.data(), size, red,
        red ? 1 + i % 9 : 0, ++k.next_clause_id);
    clause_at(k.arena, ref)->set_vivify(i % 3 == 0);
  }
  if (root_mode) {
    k.unit_ids[7] = ++k.next_clause_id;
    kernel_assign_unit(k, internalize(root_mode == 1 ? -8 : 8));
  }
  const auto original_models = models(k);
  assert(!original_models.empty());
  char path[] = "vivify-snapshot-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  Proof proof;
  if (proof_mode) {
    assert(proof.open(path, proof_mode == 1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof = &proof;
  }
  hash(scope); hash(large); hash(reuse); hash(flush);
  hash(root_mode); hash(proof_mode);
  wide_query(k);
  for (uint64_t budget : {UINT64_C(1), UINT64_C(0), UINT64_C(0)}) {
    k.work_budget = budget;
    const auto result = vivify_apply(k);
    assert(result == ApplyResult::Ok || result == ApplyResult::NoOp);
    hash(static_cast<unsigned>(result));
    kernel_check_invariants(k);
    assert(models(k) == original_models);
    wide_state(k);
    wide_query(k);
  }
  // Scratch capacity from the count-only path must not substitute stale keys
  // in a later mixed/flushing query, nor vice versa on the next population.
  k.vivify_scope = 0;
  k.vivify_flush = true;
  k.vivify_reuse = true;
  wide_query(k);
  k.proof = nullptr;
  if (proof_mode) proof.flush();
  FILE *input = std::fopen(path, "rb");
  assert(input);
  for (int ch; (ch = std::fgetc(input)) != EOF;) {
    hash(ch); ++proof_bytes;
  }
  assert(!std::ferror(input));
  std::fclose(input);
  assert(std::remove(path) == 0);
}

int main() {
  vivify_count_cache_matrix();
  unsigned cases = 0;
  for (unsigned scope : {0u, 2u, 3u})
    for (bool large : {false, true})
      for (bool reuse : {false, true})
        for (bool flush : {false, true})
          for (unsigned root_mode = 0; root_mode < 3; ++root_mode)
            for (unsigned mode = 0; mode < 3; ++mode) {
              population(scope, large, reuse, flush, root_mode, mode);
              ++cases;
            }
  std::printf("vivify-snapshot population-cases=%u digest=%016llx proof-bytes=%llu\n",
      cases, static_cast<unsigned long long>(digest),
      static_cast<unsigned long long>(proof_bytes));
}
