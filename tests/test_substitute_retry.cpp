// Substitute's removal advice: a canonical binary disappearance or a retained
// binary-collapse endpoint re-arms BVE retry, independently of proof mode,
// duplicate copies and the caller's instance choice; surviving rewrites do not.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/substitute/substitute.hpp"

// Opens a discarded DSR stream when `mode` is set.
static void attach_proof(Kernel &k, Proof &proof, int mode) {
  if (!mode) return;
  assert(proof.open("/dev/null", ProofFormat::DsrAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
}

// Applies Substitute canonically or through its first instance; then a no-op
// reapplication synthesizes no fresh removal events and level 1 fails the guard.
static void apply(Kernel &k, bool single) {
  const auto &rule = tactix_rules()[tactix_rule_id("Substitute")];
  assert(rule.guard(k));
  uint64_t instances[8];
  assert(rule.instances(k, instances, 8) > 0);
  assert((single ? rule.apply_instance(k, instances[0]) : rule.apply(k)) ==
         ApplyResult::Ok);
}
static void reapply_is_quiet(Kernel &k) {
  const auto &rule = tactix_rules()[tactix_rule_id("Substitute")];
  const uint64_t after = k.stats.bve_marks;
  assert(rule.apply(k) == ApplyResult::NoOp && k.stats.bve_marks == after);
  k.level = 1; assert(!rule.guard(k)); k.level = 0;
}

static void disappearance(unsigned copies, int mode, bool single, bool reverse) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  Proof proof;
  attach_proof(k, proof, mode);
  for (Var v : reverse ? std::vector<Var>{1, 0, 2, 3}
                       : std::vector<Var>{0, 1, 2, 3})
    kernel_record_import(k, v);
  for (unsigned i = 0; i < copies; ++i) {
    kernel_add_binary_clause(k, internalize(-1), internalize(2), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  }
  // A surviving rewrite is not a disappearance and must not re-arm x3/x4.
  kernel_add_binary_clause(k, internalize(2), internalize(3), ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(3), internalize(4), ++k.next_clause_id);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto removed = k.removed;
  const uint64_t before = k.stats.bve_marks;
  apply(k, single);
  assert(k.value(internalize(1)) == 0 && k.value(internalize(2)) == 0);
  assert(kernel_var_retired(k, reverse ? 0 : 1));
  assert(!kernel_var_retired(k, reverse ? 1 : 0));
  assert(!kernel_binary_live(k, internalize(-1), internalize(2)));
  assert(!kernel_binary_live(k, internalize(1), internalize(-2)));
  assert(kernel_binary_live(k, internalize(reverse ? 2 : 1), internalize(3)));
  assert(kernel_binary_live(k, internalize(3), internalize(4)));
  assert(k.bve_retry[0] && k.bve_retry[1] && !k.bve_retry[2] && !k.bve_retry[3]);
  assert(k.removed[0] > removed[0] && k.removed[1] > removed[1] &&
         k.removed[2] == removed[2] && k.removed[3] == removed[3]);
  assert(k.stats.bve_marks - before == 4 * copies);
  reapply_is_quiet(k);
  k.proof = nullptr;
}

// A large clause over the class collapses to a binary (retry on x3) unless it
// is redundant or keeps a fourth literal; the solved model is checked.
static void collapse(unsigned copies, bool redundant, bool large, int mode,
                     bool single) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  Proof proof;
  attach_proof(k, proof, mode);
  kernel_add_binary_clause(k, internalize(-1), internalize(2), ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  const Lit source[] = {internalize(1), internalize(2), internalize(3), internalize(4)};
  for (unsigned i = 0; i < copies; ++i)
    kernel_add_large_clause(k, source, large ? 4 : 3, redundant,
                            redundant ? 2 : 0, ++k.next_clause_id);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto before = k.stats.bve_marks;
  apply(k, single);
  assert(kernel_var_retired(k, 1));
  assert(k.bve_retry[2] == (!redundant && !large) && !k.bve_retry[3]);
  if (!redundant && !large) {
    assert(kernel_binary_live(k, internalize(1), internalize(3)));
    assert(k.stats.bve_marks - before == 4 + 3 * copies);
  }
  reapply_is_quiet(k);
  kernel_check_invariants(k);
  k.proof = nullptr;
  // The redundant controls supply an arbitrary learned clause; make it
  // entailed by surviving endpoints before checking an original-formula model.
  if (redundant) {
    const Lit support[] = {internalize(1), internalize(3), internalize(4)};
    if (large) kernel_add_large_clause(k, support, 3, false, 0, ++k.next_clause_id);
    else kernel_add_binary_clause(k, support[0], support[1], ++k.next_clause_id);
  }
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  const auto val = [&](int x) { return k.value(internalize(x)) > 0; };
  assert((val(-1) || val(2)) && (val(1) || val(-2)));
  assert(val(1) || val(2) || val(3) || (large && val(4)));
}

int main() {
  for (unsigned copies : {1u, 2u})
    for (int mode : {0, 1})
      for (bool single : {false, true}) {
        for (bool reverse : {false, true})
          disappearance(copies, mode, single, reverse);
        for (bool redundant : {false, true})
          for (bool large : {false, true})
            collapse(copies, redundant, large, mode, single);
      }
  std::puts("PASS: Substitute removal-advice disappearance and collapse fixtures");
}
