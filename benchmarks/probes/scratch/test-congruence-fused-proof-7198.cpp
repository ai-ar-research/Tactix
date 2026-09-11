// Actual dense gate closure, model-set preservation and a gate-output conflict.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/propagate/propagate.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tactix;

static bool holds(Lit lit, unsigned mask) {
  return bool((mask >> var_of(lit)) & 1) != negated(lit);
}

int main(int argc, char **argv) {
  assert(argc == 9);
  const unsigned arity = std::atoi(argv[1]), position = std::atoi(argv[2]);
  const bool negative = std::atoi(argv[3]), reverse = std::atoi(argv[4]);
  const bool public_call = std::atoi(argv[5]), lrat = std::atoi(argv[6]);
  assert((arity == 2 || arity == 7 || arity == 65) && position < 3);
  Kernel k;
  kernel_reserve_vars(k, arity + 2);
  for (unsigned i = 0; i < k.nvars; i++) kernel_record_import(k, reverse ? k.nvars - 1 - i : i);
  const Lit a = make_lit(arity, negative), b = make_lit(arity + 1, negative);
  std::vector<std::vector<Lit>> clauses;
  for (Lit output : {a, b}) {
    std::vector<Lit> base;
    for (unsigned i = 0; i < arity; i++) {
      const Lit input = make_lit(i, bool(i & 1));
      clauses.push_back({negate(output), input});
      base.push_back(negate(input));
    }
    const unsigned at = position == 0 ? 0 : position == 1 ? arity / 2 : arity;
    base.insert(base.begin() + at, output);
    clauses.push_back(base);
  }
  FILE *cnf = std::fopen(argv[7], "wx"); assert(cnf);
  std::fprintf(cnf, "p cnf %u %zu\n", k.nvars, clauses.size() + 2);
  for (const auto &clause : clauses) {
    for (Lit lit : clause) std::fprintf(cnf, "%d ", kernel_externalize(k, lit));
    std::fprintf(cnf, "0\n");
    const uint64_t id = ++k.next_clause_id;
    if (clause.size() == 2) kernel_add_binary_clause(k, clause[0], clause[1], id);
    else kernel_add_large_clause(k, clause.data(), clause.size(), false, 0, id);
  }
  for (Lit lit : {a, negate(b)}) {
    std::fprintf(cnf, "%d 0\n", kernel_externalize(k, lit));
    k.unit_ids[var_of(lit)] = ++k.next_clause_id;
  }
  std::fclose(cnf);
  std::vector<unsigned> models;
  if (arity <= 7)
    for (unsigned mask = 0; mask < (1u << k.nvars); mask++) {
      bool model = true;
      for (const auto &clause : clauses)
        model &= std::any_of(clause.begin(), clause.end(), [&](Lit lit) { return holds(lit, mask); });
      if (model) models.push_back(mask);
    }
  OccIndex occ;
  kernel_dense_connect(k, occ, false);
  Proof proof;
  assert(proof.open(argv[8], lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  std::vector<CongGate> extracted;
  congruence_extract(k, extracted);
  assert(extracted.size() >= 2); // the modified dense branch is exercised
  k.level = 1; assert(!congruence_guard(k)); k.level = 0;
  assert(congruence_guard(k));
  const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instance = 0;
  assert(rule.instances(k, &instance, 1) == 1);
  assert((public_call ? rule.apply_instance(k, instance) : congruence_apply(k)) == ApplyResult::Ok);
  assert(k.stats.congruence_merges && !k.stats.congruence_unjustified);
  assert(!k.inconsistent && k.nvars == arity + 2);
  if (arity <= 7) {
    std::vector<unsigned> after;
    for (unsigned mask = 0; mask < (1u << k.nvars); mask++) {
      bool model = true;
      for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
        if (k.value(lit) > 0) model &= holds(lit, mask);
        for (const WatchEntry w : k.watches.entries(lit))
          if (w.binary()) model &= holds(lit, mask) || holds(w.other(), mask);
      }
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *c = clause_at(k.arena, ref);
        ref += clause_actual_words(c);
        if (!c->garbage()) model &= std::any_of(c->lits, c->lits + c->size, [&](Lit lit) { return holds(lit, mask); });
      }
      if (model) after.push_back(mask);
    }
    assert(!models.empty() && models == after);
  }
  kernel_dense_disconnect(k);
  kernel_assign_unit(k, a);
  kernel_assign_unit(k, negate(b));
  assert(propagate_apply(k) == ApplyResult::Unsat && k.inconsistent);
  proof.flush(); k.proof = nullptr;
  std::printf("gate-proof %u %u %d %d %d %d: gates=%llu merges=%llu ticks=%llu models=%zu\n",
      arity, position, negative, reverse, public_call, lrat,
      (unsigned long long)k.stats.congruence_gates,
      (unsigned long long)k.stats.congruence_merges,
      (unsigned long long)k.stats.congruence_ticks, models.size());
}
