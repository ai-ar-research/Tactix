#pragma once
// A seven-variable V6 congruence fixture: clauses are added under a literal
// sign pattern, closure runs through the direct or the public action, and every
// assignment's original and final model status must agree.
#ifdef NDEBUG
#error "congruence_ite_support.hpp asserts have side effects; #undef NDEBUG before including it"
#endif
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdlib>
#include <initializer_list>
#include <vector>

struct IteFixture {
  unsigned signs;
  tactix::Kernel k;
  std::vector<std::vector<int>> original;

  IteFixture(unsigned signs, bool reverse_import) : signs(signs) {
    tactix::kernel_reserve_vars(k, 7);
    for (tactix::Var v = 0; v < 7; ++v)
      tactix::kernel_record_import(k, reverse_import ? 6 - v : v);
  }
  int pose(int lit) const {
    return signs & (1u << (std::abs(lit) - 1)) ? -lit : lit;
  }
  tactix::Lit lit(int external) const { return tactix::internalize(pose(external)); }
  void add(std::initializer_list<int> clause) {
    original.emplace_back();
    std::vector<tactix::Lit> lits;
    for (int l : clause) {
      original.back().push_back(pose(l));
      lits.push_back(lit(l));
    }
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 2)
      tactix::kernel_add_binary_clause(k, lits[0], lits[1], id);
    else
      tactix::kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  void and_gate(int out, int a, int b) {
    add({-out, a}); add({-out, b}); add({out, -a, -b});
  }
  void ite(int out, int c, int t, int e) {
    add({c, -e, out}); add({c, e, -out});
    add({-c, -t, out}); add({-c, t, -out});
  }
  void xor_gate(int out, int a, int b) {
    add({-out, a, b}); add({out, -a, b});
    add({out, a, -b}); add({-out, -a, -b});
  }
  bool original_model(unsigned bits) const {
    for (const auto &clause : original) {
      bool satisfied = false;
      for (int l : clause) satisfied |= value(bits, l);
      if (!satisfied) return false;
    }
    return true;
  }
  bool final_model(unsigned bits) const {
    using namespace tactix;
    for (Lit l = 0; l < 2 * k.nvars; ++l) {
      if (k.value(l) > 0 && !value(bits, externalize(l))) return false;
      for (const WatchEntry w : k.watches.entries(l))
        if (w.binary() && !value(bits, externalize(l)) &&
            !value(bits, externalize(w.other())))
          return false;
    }
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage()) {
        bool satisfied = false;
        for (Lit l : *clause) satisfied |= value(bits, externalize(l));
        if (!satisfied) return false;
      }
      ref += clause_actual_words(clause);
    }
    return true;
  }
  // The number of original models, each of which gives outputs 6 and 7 equal values.
  unsigned equal_output_models() const {
    unsigned models = 0;
    for (unsigned bits = 0; bits < 128; ++bits)
      if (original_model(bits)) {
        ++models;
        assert(value(bits, pose(6)) == value(bits, pose(7)));
      }
    return models;
  }
  // One root-only canonical closure that preserves the model set exactly.
  void close(bool public_call) {
    using namespace tactix;
    assert(congruence_guard(k));
    k.level = 1;
    assert(!congruence_guard(k));
    k.level = 0;
    uint64_t action = 0;
    const int id = tactix_rule_id("Congruence");
    assert(id >= 0);
    const auto &rule = tactix_rules()[id];
    assert(rule.instances(k, nullptr, 0) == 1);
    assert(rule.instances(k, &action, 1) == 1 && action == INSTANCE_CANONICAL);
    const ApplyResult result =
        public_call ? rule.apply_instance(k, action) : congruence_apply(k);
    assert(result == ApplyResult::Ok && !k.inconsistent &&
           !k.stats.congruence_unjustified);
    for (unsigned bits = 0; bits < 128; ++bits)
      assert(original_model(bits) == final_model(bits));
    for (Var v = 0; v < 7; ++v) assert(!kernel_var_retired(k, v));
  }
};
