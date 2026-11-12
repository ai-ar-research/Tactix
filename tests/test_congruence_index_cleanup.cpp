// Native candidate indexing retires visited root-satisfied records even when
// no gate or equivalence exists. Public extraction must remain read-only.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "test_io.hpp"
#include "temporary_file.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;

static void fixture(unsigned width, unsigned position, int value, bool negative,
                    bool redundant, unsigned copies, ProofFormat format) {
  Kernel k;
  kernel_reserve_vars(k, width);
  std::vector<Lit> lits;
  for (unsigned v = 1; v <= width; v++)
    lits.push_back(internalize(negative ? -static_cast<int>(v) : static_cast<int>(v)));
  std::vector<Ref> refs;
  for (unsigned i = 0; i < copies; i++)
    refs.push_back(kernel_add_large_clause(k, lits.data(), width, redundant, 0, ++k.next_clause_id));
  if (value) {
    const Lit unit = value > 0 ? lits[position] : negate(lits[position]);
    k.unit_ids[var_of(unit)] = ++k.next_clause_id;
    kernel_assign_unit(k, unit);
    assert(!kernel_propagate(k));
  }
  const auto original = k.arena;
  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, true);
  assert(gates.empty() && k.arena == original);
  k.level = 1;
  assert(!congruence_guard(k));
  k.level = 0;
  assert(congruence_guard(k));
  const int id = tactix_rule_id("Congruence");
  assert(id >= 0);
  const auto &rule = tactix_rules()[id];
  uint64_t instance = 0;
  assert(rule.instances(k, &instance, 1) == 1);
  const uint64_t proof_line = k.next_clause_id;
  bool deleted = !redundant && value == 1 && position <= 5;
  TemporaryFile file;
  const char *path = file.path.c_str();
  {
    Proof proof;
    assert(proof.open(path, format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    assert(rule.apply_instance(k, instance) == ApplyResult::NoOp);
    assert(!k.inconsistent && !k.stats.congruence_merges);
    for (Ref ref : refs) assert(clause_at(k.arena, ref)->garbage() == deleted);
    assert(k.live_irredundant_large == (redundant || deleted ? 0 : copies));
    assert(congruence_apply(k) == ApplyResult::NoOp); // no duplicate deletion
    proof.flush();
    k.proof = nullptr;
  }
  // No lemma was added, so every deletion line carries `proof_line` and the
  // deletions are the only admissible proof events of both applications.
  assert(k.next_clause_id == proof_line);
  // The entry rebuild selects the watched pair as Kissat's sparse resume does,
  // so each deletion names the retired record's literal sequence, a
  // permutation of the original clause.
  std::string expected;
  if (deleted)
    for (Ref ref : refs) {
      const Clause *c = clause_at(k.arena, ref);
      assert(c->size == width);
      assert(std::is_permutation(c->lits, c->lits + c->size, lits.begin()));
      expected += "d ";
      for (Lit lit : *c) expected += std::to_string(externalize(lit)) + " ";
      expected += "0\n";
  }
  const std::string actual = read_file(path);
  if (actual != expected)
    std::fprintf(stderr, "case width=%u position=%u value=%d negative=%d redundant=%d copies=%u\nexpected:\n%s---\nactual:\n%s---\n",
                 width, position, value, static_cast<int>(negative), static_cast<int>(redundant), copies,
                 expected.c_str(), actual.c_str());
  assert(actual == expected);
  assert(k.value(lits[position]) == value && k.nvars == width);
  // The original clause still evaluates true in every admitted deletion case.
  if (deleted) assert(k.value(lits[position]) == 1);
  if (!negative && !redundant && copies == 1)
    std::printf("%u %u %d %u\n", width, position, value, static_cast<unsigned>(deleted));
}

int main() {
  setvbuf(stdout, nullptr, _IOLBF, 0); // progress survives an assertion abort
  for (unsigned width = 3; width <= 7; width++)
    for (unsigned position = 0; position < width; position++)
      for (int value = -1; value <= 1; value++)
        for (bool negative : {false, true})
          for (bool redundant : {false, true})
            for (unsigned copies : {1u, 2u})
              fixture(width, position, value, negative, redundant, copies, ProofFormat::DsrAscii);
  std::puts("PASS:600 actual Congruence cleanup cases, pure extraction and exact proof deletions");
}
