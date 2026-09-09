// Fixed twelve-variable public-rule control, not a solver/fuzz benchmark.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace tactix;

static bool satisfied(int lit, unsigned mask) {
  const bool value = (mask >> (std::abs(lit) - 1)) & 1u;
  return lit > 0 ? value : !value;
}

int main(int argc, char **argv) {
  assert(argc == 6);
  const int mode = std::atoi(argv[1]), position = std::atoi(argv[2]), missing = std::atoi(argv[3]);
  assert(mode >= 0 && mode < 2 && position >= 0 && position < 4 && missing >= 0 && missing < 2);
  constexpr unsigned variables = 12, assignments = 1u << variables;
  const std::vector<int> padding = {5, 6, 7, 8, 9, 10, 11, 12};
  std::vector<std::vector<int>> input;
  for (int output : {1, 4})
    for (unsigned signs = 0; signs < 8; ++signs) {
      if (__builtin_popcount(signs) % 2 != 1) continue;
      if (output == 4 && missing && signs == 7) continue;
      std::vector<int> clause = {output, 2, 3};
      for (unsigned i = 0; i < 3; ++i) if (signs & (1u << i)) clause[i] = -clause[i];
      if (output == 4) clause.insert(clause.begin() + position, padding.begin(), padding.end());
      input.push_back(clause);
    }
  input.push_back({1, 4}); input.push_back({-1, -4});
  for (int lit : padding) input.push_back({-lit});
  Kernel k; kernel_reserve_vars(k, variables);
  std::ofstream cnf(std::string(argv[4]) + ".cnf"); assert(cnf);
  cnf << "p cnf " << variables << ' ' << input.size() << '\n';
  for (const auto &row : input) {
    std::vector<Lit> lits;
    for (int lit : row) { cnf << lit << ' '; lits.push_back(internalize(lit)); }
    cnf << "0\n";
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 1) { k.unit_ids[var_of(lits[0])] = id; kernel_assign_unit(k, lits[0]); }
    else if (lits.size() == 2) kernel_add_binary_clause(k, lits[0], lits[1], id);
    else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  cnf.close(); assert(!kernel_propagate(k));
  const auto arena = k.arena;
  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, true);
  assert(k.arena == arena); // pure extraction must not retire any clause
  const bool padded_gate = std::any_of(gates.begin(), gates.end(), [](const CongGate &g) {
    return g.kind == GateKind::Xor && var_of(g.output) == 3;
  });
  assert(padded_gate == !missing);
  std::vector<unsigned> original;
  for (unsigned mask = 0; mask < assignments; ++mask) {
    bool model = true;
    for (const auto &row : input) {
      bool clause = false;
      for (int lit : row) clause |= satisfied(lit, mask);
      model &= clause;
    }
    if (model) original.push_back(mask);
  }
  assert(!original.empty() == bool(missing));
  Proof proof; assert(proof.open(argv[5], mode ? ProofFormat::Lrat : ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof = &proof;
  const int id = tactix_rule_id("Congruence"); assert(id >= 0);
  const auto &rule = tactix_rules()[id]; uint64_t instance = 0;
  assert(rule.instances(k, &instance, 1) == 1);
  const auto result = rule.apply_instance(k, instance);
  if (!missing) assert(result == ApplyResult::Unsat && k.inconsistent);
  else {
    assert(result != ApplyResult::Unsat && !k.inconsistent);
    std::vector<unsigned> after;
    for (unsigned mask = 0; mask < assignments; ++mask) {
      bool model = true;
      for (Lit lit = 0; lit < 2 * variables; ++lit) {
        if (k.value(lit) > 0) model &= satisfied(kernel_externalize(k, lit), mask);
        for (const WatchEntry w : k.watches.entries(lit)) if (w.binary())
          model &= satisfied(kernel_externalize(k, lit), mask) || satisfied(kernel_externalize(k, w.other()), mask);
      }
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *clause = clause_at(k.arena, ref);
        if (!clause->garbage()) {
          bool holds = false;
          for (Lit lit : *clause) holds |= satisfied(kernel_externalize(k, lit), mask);
          model &= holds;
        }
        ref += clause_actual_words(clause);
      }
      if (model) after.push_back(mask);
    }
    assert(after == original);
  }
  proof.flush(); k.proof = nullptr;
  std::printf("PASS mode=%d position=%d missing=%d models=%zu padded_gate=%d assignments=%u\n",
              mode, position, missing, original.size(), padded_gate, assignments);
}
