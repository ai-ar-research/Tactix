// V6 typed ITE closure: exhaustive original/final models, all literal signs,
// both import orders, direct/public action surfaces; no DefaultPolicy solve.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tactix;

static bool value(unsigned bits, int lit) {
  const bool positive = (bits >> (std::abs(lit) - 1)) & 1;
  return lit > 0 ? positive : !positive;
}

int main() {
  unsigned closed = 0;
  for (unsigned signs = 0; signs < 128; ++signs)
  for (bool reverse_import : {false, true})
  for (bool public_call : {false, true})
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    const auto pose = [&](int lit) {
      return signs & (1u << (std::abs(lit) - 1)) ? -lit : lit;
    };
    Kernel k;
    kernel_reserve_vars(k, 7);
    for (Var v = 0; v < 7; ++v) kernel_record_import(k, reverse_import ? 6 - v : v);
    std::vector<std::vector<int>> original;
    auto add = [&](std::initializer_list<int> clause) {
      original.emplace_back();
      std::vector<Lit> lits;
      for (int lit : clause) {
        original.back().push_back(pose(lit));
        lits.push_back(internalize(pose(lit)));
      }
      const uint64_t id = ++k.next_clause_id;
      if (lits.size() == 2) kernel_add_binary_clause(k, lits[0], lits[1], id);
      else kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
    };
    auto and_gate = [&](int out, int a, int b) {
      add({-out, a}); add({-out, b}); add({out, -a, -b});
    };
    auto ite = [&](int out, int c, int t, int e) {
      add({c, -e, out}); add({c, e, -out});
      add({-c, -t, out}); add({-c, t, -out});
    };
    auto xor_gate = [&](int out, int a, int b) {
      add({-out, a, b}); add({out, -a, b});
      add({out, a, -b}); add({-out, -a, -b});
    };
    // 3=4 is discovered by Congruence, not present as an entry binary pair.
    and_gate(3, 1, 2); and_gate(4, 1, 2);
    switch (scenario) {
    case 0: ite(6, 3, 4, 5); and_gate(-7, -3, -5); break; // c==then: OR
    case 1: ite(6, 3, -4, 5); and_gate(7, -3, 5); break; // c==-then: AND
    case 2: ite(6, 3, 5, 4); and_gate(7, 3, 5); break; // c==else: AND
    case 3: ite(6, 3, 5, -4); and_gate(-7, 3, -5); break; // c==-else: OR
    case 4: ite(6, 5, 3, -4); xor_gate(-7, 5, 3); break; // then==-else: XOR
    case 5: ite(6, 3, 4, 5); ite(7, 3, 4, 5); break; // both ITEs degenerate
    case 6: ite(6, 1, 2, 5); ite(7, 1, 2, 5); break; // nondegenerate control
    }
    auto original_model = [&](unsigned bits) {
      for (const auto &clause : original) {
        bool satisfied = false;
        for (int lit : clause) satisfied |= value(bits, lit);
        if (!satisfied) return false;
      }
      return true;
    };
    unsigned before_models = 0;
    for (unsigned bits = 0; bits < 128; ++bits)
      if (original_model(bits)) {
        ++before_models;
        assert(value(bits, pose(6)) == value(bits, pose(7)));
      }
    assert(before_models == 8);
    const bool entry_equal = kernel_binary_live(k, internalize(pose(-6)), internalize(pose(7))) &&
                             kernel_binary_live(k, internalize(pose(6)), internalize(pose(-7)));
    assert(!entry_equal && congruence_guard(k));
    k.level = 1;
    assert(!congruence_guard(k));
    k.level = 0;
    uint64_t action = 0;
    const int id = tactix_rule_id("Congruence");
    assert(id >= 0);
    const auto &rule = tactix_rules()[id];
    assert(rule.instances(k, nullptr, 0) == 1);
    assert(rule.instances(k, &action, 1) == 1 && action == INSTANCE_CANONICAL);
    const ApplyResult result = public_call ? rule.apply_instance(k, action) : congruence_apply(k);
    assert(result == ApplyResult::Ok && !k.inconsistent && !k.stats.congruence_unjustified);
    auto final_model = [&](unsigned bits) {
      for (Lit l = 0; l < 2 * k.nvars; ++l) {
        if (k.value(l) > 0 && !value(bits, externalize(l))) return false;
        for (const WatchEntry w : k.watches.entries(l))
          if (w.binary() && !value(bits, externalize(l)) && !value(bits, externalize(w.other()))) return false;
      }
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *clause = clause_at(k.arena, ref);
        if (!clause->garbage()) {
          bool satisfied = false;
          for (Lit lit : *clause) satisfied |= value(bits, externalize(lit));
          if (!satisfied) return false;
        }
        ref += clause_actual_words(clause);
      }
      return true;
    };
    for (unsigned bits = 0; bits < 128; ++bits)
      assert(original_model(bits) == final_model(bits));
    const bool equal = kernel_binary_live(k, internalize(pose(-6)), internalize(pose(7))) &&
                       kernel_binary_live(k, internalize(pose(6)), internalize(pose(-7)));
    assert(equal);
    for (Var v = 0; v < 7; ++v) assert(!kernel_var_retired(k, v));
    closed += equal;

  }
  assert(closed == 3584);
  std::printf("ITE closure: %u cases; all 458752 original/final assignments agree\\n", closed);
}
