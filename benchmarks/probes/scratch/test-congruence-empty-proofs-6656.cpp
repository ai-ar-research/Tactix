// Fixed four-variable model-set equality plus final independently checkable proofs.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/strengthen/strengthen.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
using namespace tactix;
static bool holds(int lit, unsigned mask) {
  const bool value = (mask >> (std::abs(lit) - 1)) & 1;
  return lit > 0 ? value : !value;
}
int main(int argc, char **argv) {
  assert(argc == 10);
  const unsigned mode = std::atoi(argv[1]), merge = std::atoi(argv[2]);
  const unsigned entry = std::atoi(argv[3]), copies = std::atoi(argv[4]);
  const unsigned reverse = std::atoi(argv[5]), public_call = std::atoi(argv[6]);
  assert(mode < 2 && merge < 2 && entry < 2 && copies >= 1 && copies <= 2 && reverse < 2 && public_call < 2);
  Kernel k; kernel_reserve_vars(k, 7);
  for (unsigned i = 0; i < 7; i++) kernel_record_import(k, reverse ? 6 - i : i);
  std::vector<std::vector<int>> input;
  for (unsigned i = 0; i < copies; i++) {
    input.push_back({1, 2}); input.push_back({1, -2});
    if (merge) { input.push_back({-3, 4}); input.push_back({3, -4}); }
  }
  if (entry) input.push_back({1});
  const unsigned base = input.size();
  for (unsigned signs = 0; signs < 8; signs++) {
    std::vector<int> clause;
    for (unsigned v = 0; v < 3; v++) clause.push_back(signs & (1u << v) ? -int(v + 5) : int(v + 5));
    input.push_back(clause);
  }
  std::ofstream cnf(argv[7]); assert(cnf);
  cnf << "p cnf 7 " << input.size() << '\n';
  for (const auto &row : input) { for (int lit : row) cnf << lit << ' '; cnf << "0\n"; }
  cnf.close();
  for (unsigned i = 0; i < base; i++) {
    const auto &row = input[i];
    if (row.size() == 2) kernel_add_binary_clause(k, internalize(row[0]), internalize(row[1]), i + 1);
    else { k.unit_ids[0] = i + 1; kernel_assign_unit(k, internalize(1)); }
  }
  k.next_clause_id = input.size(); // all original input identities precede lemmas
  assert(!kernel_propagate(k));
  std::vector<CongGate> gates; congruence_extract_gates(k, gates, true); assert(gates.empty());
  unsigned before = 0;
  for (unsigned mask = 0; mask < 16; mask++) {
    bool model = true;
    for (unsigned i = 0; i < base; i++) {
      bool satisfied = false; for (int lit : input[i]) satisfied |= holds(lit, mask);
      model &= satisfied;
    }
    if (model) before |= 1u << mask;
  }
  Proof proof; assert(proof.open(argv[8], mode ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
  uint64_t instance = 0; assert(rule.instances(k, &instance, 1) == 1);
  const ApplyResult result = public_call ? rule.apply_instance(k, instance) : rule.apply(k);
  assert(result == (merge || !entry ? ApplyResult::Ok : ApplyResult::NoOp));
  assert(!k.stats.congruence_gates && !k.stats.congruence_unjustified);
  unsigned after = 0;
  for (unsigned mask = 0; mask < 16; mask++) {
    bool model = true;
    for (Lit lit = 0; lit < 8; lit++) {
      if (k.value(lit) > 0) model &= holds(kernel_externalize(k, lit), mask);
      for (const WatchEntry w : k.watches.entries(lit)) if (w.binary())
        model &= holds(kernel_externalize(k, lit), mask) || holds(kernel_externalize(k, w.other()), mask);
    }
    if (model) after |= 1u << mask;
  }
  assert(before && before == after);
  assert(k.stats.congruence_merges == merge);
  const unsigned retained = merge || entry ? 0 : 1;
  assert(kernel_binary_copies(k, internalize(1), internalize(2)) == retained);
  assert(kernel_binary_copies(k, internalize(1), internalize(-2)) == retained);
  if (merge) {
    assert(kernel_binary_copies(k, internalize(-3), internalize(4)) == 1);
    assert(kernel_binary_copies(k, internalize(3), internalize(-4)) == 1);
  }
  // The untouched disjoint input cube closes the proof after the observed
  // transformation. Its original IDs were reserved before any lemma; it was
  // deliberately absent from the gate-extraction fixture, not from the CNF.
  for (unsigned i = base; i < input.size(); i++) {
    Lit lits[3]; for (unsigned j = 0; j < 3; j++) lits[j] = internalize(input[i][j]);
    kernel_add_large_clause(k, lits, 3, false, 0, i + 1);
  }
  for (unsigned round = 0; round < 20 && !k.inconsistent; round++) strengthen_apply(k);
  assert(k.inconsistent);
  proof.flush(); k.proof = nullptr;
  std::ofstream effects(argv[9]);
  effects << "{\"models_before\":" << before << ",\"models_after\":" << after
          << ",\"merge\":" << merge << ",\"retained_unit_parents\":" << retained << "}\n";
}
