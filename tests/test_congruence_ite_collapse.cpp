// V6 typed ITE closure: exhaustive original/final models over all literal
// signs, both import orders and the direct/public action surfaces (collapse
// and valued-side representation), plus the ITE constant unit whose original
// ternaries need a condition split; no DefaultPolicy solve.
#include "unit_test.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"

#include <initializer_list>

// A seven-variable fixture: clauses are added under a literal sign pattern,
// closure runs through the direct or the public action, and every assignment's
// original and final model status must agree.
struct IteFixture {
  unsigned signs;
  Kernel k;
  std::vector<std::vector<int>> original;

  IteFixture(unsigned signs, bool reverse_import) : signs(signs) {
    kernel_reserve_vars(k, 7);
    for (Var v = 0; v < 7; ++v)
      kernel_record_import(k, reverse_import ? 6 - v : v);
  }
  int pose(int lit) const {
    return signs & (1u << (std::abs(lit) - 1)) ? -lit : lit;
  }
  Lit lit(int external) const { return internalize(pose(external)); }
  void add(std::initializer_list<int> clause) {
    original.emplace_back();
    std::vector<Lit> lits;
    for (int l : clause) {
      original.back().push_back(pose(l));
      lits.push_back(lit(l));
    }
    const uint64_t id = ++k.next_clause_id;
    if (lits.size() == 2)
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    else
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
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

TEST(collapse) {
  unsigned closed = 0;
  for (unsigned signs = 0; signs < 128; ++signs)
  for (bool reverse_import : {false, true})
  for (bool public_call : {false, true})
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    IteFixture f(signs, reverse_import);
    // 3=4 is discovered by Congruence, not present as an entry binary pair.
    f.and_gate(3, 1, 2); f.and_gate(4, 1, 2);
    switch (scenario) {
    case 0: f.ite(6, 3, 4, 5); f.and_gate(-7, -3, -5); break; // c==then: OR
    case 1: f.ite(6, 3, -4, 5); f.and_gate(7, -3, 5); break; // c==-then: AND
    case 2: f.ite(6, 3, 5, 4); f.and_gate(7, 3, 5); break; // c==else: AND
    case 3: f.ite(6, 3, 5, -4); f.and_gate(-7, 3, -5); break; // c==-else: OR
    case 4: f.ite(6, 5, 3, -4); f.xor_gate(-7, 5, 3); break; // then==-else: XOR
    case 5: f.ite(6, 3, 4, 5); f.ite(7, 3, 4, 5); break; // both ITEs degenerate
    case 6: f.ite(6, 1, 2, 5); f.ite(7, 1, 2, 5); break; // nondegenerate control
    }
    assert(f.equal_output_models() == 8);
    const auto equal = [&] {
      return kernel_binary_live(f.k, f.lit(-6), f.lit(7)) &&
             kernel_binary_live(f.k, f.lit(6), f.lit(-7));
    };
    assert(!equal());
    f.close(public_call);
    assert(equal());
    ++closed;
  }
  assert(closed == 3584);
  std::printf("ITE closure: %u cases; all 458752 original/final assignments agree\n", closed);
}

TEST(valued_sides) {
  unsigned closed = 0;
  for (unsigned signs = 0; signs < 128; ++signs)
  for (bool reverse_import : {false, true})
  for (bool public_call : {false, true})
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    IteFixture f(signs, reverse_import);
    // 3=4 is discovered by Congruence, not present as an entry binary pair.
    f.and_gate(3, 1, 2); f.and_gate(4, 1, 2);
    // The equal AND outputs force 5 only during closure, after extraction
    // retained the still-unvalued ITE relation.
    f.and_gate((scenario & 1) ? 5 : -5, 3, -4);
    if (scenario < 2) f.ite(6, 1, 5, 2);
    else f.ite(6, 1, 2, 5);
    switch (scenario) {
    case 0: f.and_gate(-7, -1, -2); break; // then true => OR
    case 1: f.and_gate(7, -1, 2); break;   // then false => AND
    case 2: f.and_gate(-7, 1, -2); break;  // else true => OR
    case 3: f.and_gate(7, 1, 2); break;    // else false => AND
    }
    assert(f.equal_output_models() == 4);
    // Whether from implies to in the binary implication graph.
    const auto implies = [&](Lit from, Lit to) {
      bool seen[14] = {};
      std::vector<Lit> todo{from};
      seen[from] = true;
      for (size_t i = 0; i < todo.size(); ++i) {
        if (todo[i] == to) return true;
        for (const WatchEntry w : f.k.watches.entries(negate(todo[i])))
          if (w.binary() && !seen[w.other()]) {
            seen[w.other()] = true;
            todo.push_back(w.other());
          }
      }
      return false;
    };
    const auto equal = [&] {
      return implies(f.lit(6), f.lit(7)) && implies(f.lit(7), f.lit(6));
    };
    assert(!equal());
    f.close(public_call);
    // The original ITE clauses with the derived root value of 5 already supply
    // these implications, so no redundant binary copies are added under 6.
    const int sides[4][3] = {{6, -1, -2}, {-6, -1, 2}, {6, 1, -2}, {-6, 1, 2}};
    for (unsigned side = 1; side <= 2; ++side)
      assert(!kernel_binary_live(f.k, f.lit(sides[scenario][0]),
                                 f.lit(sides[scenario][side])));
    assert(equal());
    ++closed;
  }
  assert(closed == 2048);
  std::printf("ITE valued-side representation: %u cases; all 262144 original/final assignments agree\n", closed);
}

TEST(unit_proof) {
  unsigned checked = 0;
  for (unsigned signs = 0; signs < 64; ++signs)
  for (bool truth : {false, true})
  for (bool reverse : {false, true})
  for (int mode = 0; mode < 3; ++mode) {
    std::vector<std::vector<int>> rows;
    for (int output : {3, 4, 5}) {
      rows.push_back({-output, 1});
      rows.push_back({-output, 2});
      rows.push_back({output, -1, -2});
    }
    const int c = 3, t = truth ? 4 : -4, e = truth ? -5 : 5, o = 6;
    rows.push_back({c, -e, o}); rows.push_back({c, e, -o});
    rows.push_back({-c, -t, o}); rows.push_back({-c, t, -o});
    const auto pose = [&](int lit) {
      return signs & (1u << (std::abs(lit) - 1)) ? -lit : lit;
    };
    for (auto &row : rows) for (int &lit : row) lit = pose(lit);
    if (reverse) {
      std::reverse(rows.begin(), rows.end());
      for (auto &row : rows) std::reverse(row.begin(), row.end());
    }
    unsigned models = 0;
    for (unsigned bits = 0; bits < 64; ++bits) {
      const auto value = [&](int lit) {
        return bool(bits & (1u << (std::abs(lit) - 1))) == (lit > 0);
      };
      bool model = true;
      for (const auto &row : rows) {
        bool sat = false;
        for (int lit : row) sat |= value(lit);
        model &= sat;
      }
      if (model) {
        ++models;
        assert(value(pose(6)) == truth);
      }
    }
    assert(models == 4);
    Kernel k;
    Proof proof;
    if (mode) {
      const auto format = mode == 1 ? ProofFormat::DsrBinary : ProofFormat::DsrAscii;
      assert(proof.open("/dev/null", format));
      k.proof = &proof;
      proof.bind_variable_map(&k.internal_to_external);
    }
    FILE *input = std::tmpfile();
    assert(input);
    std::fprintf(input, "p cnf 6 %zu\n", rows.size());
    for (const auto &row : rows) {
      for (int lit : row) std::fprintf(input, "%d ", lit);
      std::fputs("0\n", input);
    }
    std::rewind(input);
    assert(!parse_dimacs(input, k));
    std::fclose(input);
    assert(!k.inconsistent && congruence_guard(k));
    const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
    uint64_t action = 0;
    assert(rule.instances(k, &action, 1) == 1 && action == INSTANCE_CANONICAL);
    assert(rule.apply_instance(k, action) == ApplyResult::Ok);
    assert(!k.inconsistent && !k.stats.congruence_unjustified);
    assert(k.value(internalize(pose(6))) == (truth ? 1 : -1));
    kernel_check_invariants(k);
    ++checked;
  }
  assert(checked == 768);
  std::printf("ITE unit proof: %u signed/order/mode cases with exhaustive models\n", checked);
}
