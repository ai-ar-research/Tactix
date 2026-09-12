// CCE unit tests. Two properties are unreachable from BCE's suite: a clause
// blocked only once a covered literal is added, and model reconstruction over
// a sequence of witness segments. docs/calculus.md §X3.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/cce/cce.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace tactix;

namespace {

using Cnf = std::vector<std::vector<int>>;

void load(Kernel &k, uint32_t nvars, const Cnf &f) {
  kernel_reserve_vars(k, nvars);
  for (const auto &cl : f) {
    std::vector<Lit> l;
    for (const int e : cl)
      l.push_back(internalize(e));
    if (l.size() == 1)
      kernel_assign_unit(k, l[0]);
    else if (l.size() == 2)
      kernel_add_binary_clause(k, l[0], l[1]);
    else
      kernel_add_large_clause(k, l.data(), (uint32_t)l.size(), false, 0);
  }
}

bool satisfied(const Kernel &k, const std::vector<int> &cl) {
  for (const int e : cl)
    if (k.values[internalize(e)] > 0)
      return true;
  return false;
}

} // namespace

// The covered-literal path: (1∨2∨3) is blocked on nothing, but adding the
// covered literal 4 (found on pivot 1) makes (1∨2∨3∨4) blocked on 2.
static void test_covered_then_blocked() {
  Kernel k;
  load(k, 4, {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}});
  assert(cce_guard(k));
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("CCE")];
  assert(rule.apply_semantic_instance(k, cce_large_instance(0)) ==
         ApplyResult::Ok);
  assert(k.stats.cce_deleted == 1);
  assert(k.stats.cce_cla == 1 && "the covered literal 4 was added");
  assert(k.stats.cce_blocked == 1 && k.stats.cce_ata == 0);
  // one segment per CLA step, plus the blocked exit's
  assert(!k.reconstruction.empty());
}

// The negative case: every literal of (1∨2∨3) has two opposite occurrences
// sharing no literal, so nothing is covered and nothing is blocked.
static void test_empty_intersection() {
  Kernel k;
  load(k, 7, {{1, 2, 3},
              {-1, 4, 6}, {-1, 5, 7},
              {-2, 4, 6}, {-2, 5, 7},
              {-3, 4, 6}, {-3, 5, 7}});
  // the single-literal entry: only (1∨2∨3) is a candidate here
  assert(cce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.cce_deleted == 0);
  assert(k.stats.cce_tried == 1 && "the candidate was actually extended");
}

// The asymmetric-tautology exit: the candidate is RUP against the rest, so it
// is deleted with no witness segment at all.
static void test_asymmetric_tautology() {
  Kernel k;
  load(k, 3, {{1, 2, 3}, {1, 2}});
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("CCE")];
  assert(rule.apply_semantic_instance(k, cce_large_instance(0)) ==
         ApplyResult::Ok);
  assert(k.stats.cce_deleted == 1 && k.stats.cce_ata == 1);
  assert(k.reconstruction.empty() && "an ATA deletion needs no witness");
}

// Redundant clauses are not candidates (deviation 1): Reduce may drop them for
// free, so witnessing one would only put needless segments on ρ.
static void test_redundant_skipped() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/true, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(4));
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("CCE")];
  assert(rule.apply_semantic_instance(k, cce_large_instance(0)) ==
         ApplyResult::NoOp);
  assert(k.stats.cce_deleted == 0);
}

// Instance surface (N1): the single-literal entry reaches the round's own
// elimination, and an unrelated payload is a NoOp.
static void test_instances() {
  Kernel k;
  load(k, 4, {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}});
  std::vector<Lit> lits;
  cce_literals(k, lits);
  assert(lits.size() == 8); // both polarities of four variables
  const int id = tactix_rule_id("CCE");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.guard(k));
  uint64_t insts[16];
  assert(r.instances(k, insts, 16) == 8);
  assert(r.apply_instance(k, lit_instance(internalize(1))) == ApplyResult::Ok);
  assert(k.stats.cce_deleted == 1);
  // totality: a payload naming a variable outside the kernel is a NoOp
  assert(r.apply_instance(k, lit_instance(internalize(99))) ==
         ApplyResult::NoOp);
}

static void test_binary_semantic_instance() {
  for (bool duplicated : {false, true}) {
    Kernel k;
    load(k, 2, duplicated ? Cnf{{1, 2}, {1, 2}, {-1, -2}}
                          : Cnf{{1, 2}, {-1, -2}});
    const RuleVTable &rule = tactix_rules()[tactix_rule_id("CCE")];
    assert(rule.semantic_instances && rule.apply_semantic_instance);
    uint64_t instances[8];
    const uint32_t count = rule.semantic_instances(k, instances, 8);
    assert(count == 2);
    const uint64_t selected =
        cce_binary_instance(internalize(1), internalize(2));
    assert(std::find(instances, instances + count, selected) !=
           instances + count);
    assert(rule.apply_semantic_instance(k, selected) == ApplyResult::Ok);
    assert(!kernel_binary_live(k, internalize(1), internalize(2)));
    assert(kernel_binary_live(k, internalize(-1), internalize(-2)));
    assert(k.stats.cce_deleted == 1);
    assert(k.stats.binaries_deleted == (duplicated ? 2 : 1));

    const auto solver_values = k.values;
    for (unsigned bits = 0; bits < 4; ++bits) {
      for (Var variable = 0; variable < 2; ++variable) {
        const Lit positive = make_lit(variable, false);
        const int8_t value = bits & (1u << variable) ? 1 : -1;
        k.values[positive] = value;
        k.values[negate(positive)] = -value;
      }
      if (k.value(internalize(-1)) < 0 && k.value(internalize(-2)) < 0)
        continue;
      k.model_values = k.values;
      k.model_extended = false;
      kernel_extend_model(k);
      assert(k.model_values[internalize(1)] > 0 ||
             k.model_values[internalize(2)] > 0);
      assert(k.model_values[internalize(-1)] > 0 ||
             k.model_values[internalize(-2)] > 0);
    }
    k.values = solver_values;
    kernel_check_invariants(k);
  }
}

// The round's repetition filter and its resume: a filter that skips nothing is
// inert, and one that skips everything has wedged.
static void test_filter_and_resume() {
  { // a first round works and ADVANCES the watermark
    Kernel k;
    load(k, 4, {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}});
    assert(k.touch_filter);
    assert(cce_apply(k) == ApplyResult::Ok);
    assert(k.stats.cce_tried > 0);
    assert(k.cce_touch_mark > 0 && k.stats.cce_mark_advances == 1);
  }
  Kernel k;
  load(k, 4, {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}});
  // (a) a completed round leaves the watermark at the current epoch
  k.cce_touch_mark = k.touch_epoch;
  assert(cce_apply(k) == ApplyResult::NoOp);
  assert(k.stats.cce_tried == 0 && "(a) nothing changed, nothing tried");
  assert(k.stats.cce_mark_skipped > 0);

  // (b) any producer re-offers the candidates: a stamp has no clear pass.
  const uint64_t skipped = k.stats.cce_mark_skipped;
  for (Var v = 0; v < k.nvars; v++)
    kernel_touch_var(k, v);
  assert(cce_apply(k) == ApplyResult::Ok);
  assert(k.stats.cce_tried > 0 && "(b) the filter re-opens");
  assert(k.stats.cce_mark_skipped == skipped);

  // (c) resume. A round cut by the dose re-arms whatever it did not reach.
  Kernel k2;
  load(k2, 8, {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}, {5, 6, 7}, {-5, 8},
               {-8, -6}, {-7, 8}});
  k2.work_budget = 1; // one candidate's worth of ticks, then stop
  assert(cce_apply(k2) != ApplyResult::Unsat);
  assert(k2.stats.cce_rearmed > 0 && "(c) the unreached tail must be re-armed");
  assert(k2.cce_touch_mark > 0 && k2.stats.cce_mark_advances == 1);
  size_t armed = 0;
  for (Ref r = 0; r < k2.arena.size();) {
    const Clause *c = clause_at(k2.arena, r);
    if (!c->garbage())
      for (const Lit l : *c)
        if (kernel_changed_since(k2, var_of(l), k2.cce_touch_mark)) {
          armed++;
          break;
        }
    r += clause_actual_words(c);
  }
  assert(armed > 0 && "the next round must still see the tail");
}

// The reconstruction obligation: every assignment satisfying what survives a
// CCE round must extend to a model of the original. A wrong witness, or a
// right one in the wrong order, surfaces here as a wrong SAT answer.
static void test_reconstruction_exhaustive() {
  const uint32_t nvars = 8;
  uint64_t rng = 0x9e3779b97f4a7c15ull;
  const auto next = [&rng]() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
  };
  uint64_t deletions = 0, cla = 0, blocked = 0, ata = 0;
  uint64_t models = 0, repaired = 0;
  for (int trial = 0; trial < 400; trial++) {
    Cnf f;
    const int nclauses = 8 + (int)(next() % 14);
    for (int i = 0; i < nclauses; i++) {
      const bool ternary = (next() % 3) != 0;
      std::vector<int> cl;
      while (cl.size() < (size_t)(ternary ? 3 : 2)) {
        const int v = 1 + (int)(next() % nvars);
        bool dup = false;
        for (const int e : cl)
          dup |= (e == v || e == -v);
        if (!dup)
          cl.push_back((next() & 1) ? v : -v);
      }
      f.push_back(cl);
    }
    Kernel k;
    load(k, nvars, f);
    if (k.inconsistent)
      continue;
    cce_apply(k);
    deletions += k.stats.cce_deleted;
    cla += k.stats.cce_cla;
    blocked += k.stats.cce_blocked;
    ata += k.stats.cce_ata;
    if (!k.stats.cce_deleted)
      continue;

    // what survives: every original binary, plus the live arena clauses
    Cnf surviving;
    for (const auto &cl : f)
      if (cl.size() < 3)
        surviving.push_back(cl);
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage()) {
        std::vector<int> cl;
        for (const Lit l : *c)
          cl.push_back(externalize(l));
        surviving.push_back(cl);
      }
      ref += clause_actual_words(c);
    }

    const std::vector<int8_t> saved(k.values.begin(), k.values.end());
    for (uint32_t bits = 0; bits < (1u << nvars); bits++) {
      for (uint32_t v = 0; v < nvars; v++) {
        const Lit p = internalize((int)v + 1);
        const int8_t val = (bits >> v) & 1 ? 1 : -1;
        k.values[p] = val;
        k.values[negate(p)] = (int8_t)-val;
      }
      bool ok = true;
      for (const auto &cl : surviving)
        ok &= satisfied(k, cl);
      if (!ok)
        continue;
      models++;
      bool needed = false;
      for (const auto &cl : f)
        needed |= !satisfied(k, cl);
      repaired += needed; // the model DID have to be repaired to hold
      k.model_extended = false;
      kernel_extend_model(k);
      for (const auto &cl : f)
        if (!satisfied(k, cl)) {
          std::printf("trial %d: reconstruction left a clause unsatisfied:",
                      trial);
          for (const int e : cl)
            std::printf(" %d", e);
          std::printf("\n");
          assert(false);
        }
    }
    std::copy(saved.begin(), saved.end(), k.values.begin());
  }
  // Reachability: a green run over zero eliminations, or over eliminations that
  // never added a covered literal, proves nothing about this rule.
  std::printf("test_cce: %llu deletions (%llu blocked / %llu ata), %llu covered "
              "literals added; %llu models checked, %llu of them needed ρ\n",
              (unsigned long long)deletions, (unsigned long long)blocked,
              (unsigned long long)ata, (unsigned long long)cla,
              (unsigned long long)models, (unsigned long long)repaired);
  assert(deletions > 0 && blocked > 0 && cla > 0);
  assert(models > 0 && repaired > 0);
}

// End to end: eliminate, let the solver answer, extend, and check the original.
static void test_model_reconstruction_solved() {
  const Cnf orig = {{1, 2, 3}, {-1, 4}, {-4, -2}, {-3, 4}};
  Kernel k;
  load(k, 4, orig);
  assert(cce_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &cl : orig)
    assert(satisfied(k, cl) &&
           "every original clause satisfied by the extended model");
}

static int binary_proof_witness(const char *input, const char *output,
                                const char *format) {
  Kernel k;
  Proof proof;
  assert(proof.open(output, std::string(format) == "lrat"
                                ? ProofFormat::Lrat
                                : ProofFormat::DratAscii));
  k.proof = &proof;
  FILE *cnf = std::fopen(input, "r");
  assert(cnf && !parse_dimacs(cnf, k));
  std::fclose(cnf);
  const RuleVTable &rule = tactix_rules()[tactix_rule_id("CCE")];
  const Lit one = kernel_internalize(k, 1);
  const Lit two = kernel_internalize(k, 2);
  assert(one != INVALID_LIT && two != INVALID_LIT);
  assert(rule.apply_semantic_instance(k, cce_binary_instance(one, two)) ==
         ApplyResult::Ok);
  assert(!kernel_binary_live(k, one, two));
  DefaultPolicy policy;
  const int result = policy.solve(k, nullptr);
  proof.flush();
  if (result == 10) {
    kernel_extend_model(k);
    std::puts("s SATISFIABLE");
    std::printf("v");
    for (Var variable = 0; variable < kernel_num_output_vars(k); ++variable) {
      const Lit literal = make_lit(variable, false);
      const int external = static_cast<int>(variable) + 1;
      std::printf(" %d", k.model_values[literal] > 0 ? external : -external);
    }
    std::puts(" 0");
  } else {
    assert(result == 20);
    std::puts("s UNSATISFIABLE");
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4)
    return binary_proof_witness(argv[1], argv[2], argv[3]);
  assert(argc == 1);
  test_covered_then_blocked();
  test_empty_intersection();
  test_asymmetric_tautology();
  test_redundant_skipped();
  test_instances();
  test_binary_semantic_instance();
  test_filter_and_resume();
  test_model_reconstruction_solved();
  test_reconstruction_exhaustive();
  std::printf("test_cce: all passed\n");
  return 0;
}
