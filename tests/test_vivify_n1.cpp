// N1: every band Vivify implements must be reachable through the public API.
// `Kernel::vivify_scope` 0 means all bands, 3/4/5 the redundant tier1/tier2/
// tier3 bands; the rule side must admit a tier3 candidate at 0 and 5, and the
// setter must store 3/4/5 rather than substitute a band. A removed capability
// is not the carve-out, which only lets the built-in policy decline a choice.
// See docs/implementation-notes.md (`vivify_scope`).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/vivify/vivify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// The C API is declared by hand because `typedef struct tactix_solver tactix`
// collides with `namespace tactix`, which is also why api.cpp does not include
// tactix.h. tests/test_api_c.c remains the header/ABI sync check.
extern "C" {
struct tactix_solver *tactix_init(void);
void tactix_release(struct tactix_solver *);
void tactix_set_vivify_scope(struct tactix_solver *, unsigned);
uint64_t tactix_config(struct tactix_solver *, int);
}
#define CFG_VIVIFY_SCOPE 5 // TACTIX_CFG_VIVIFY_SCOPE

using namespace tactix;

// A vivifiable clause parked in tier3: probing ¬1 propagates 2, so (1 2 3)
// distills to (1 2) whatever its glue — a NoOp can only be a closed band.
static Ref build_tier3_vivifiable(Kernel &k) {
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref ref = kernel_add_large_clause(k, c, 3, /*redundant=*/true,
                                          /*glue=*/9);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  return ref;
}

// Drive the registry surface, which is what `tactix_rules_applicable` and
// `tactix_apply_instance` route an external policy through.
static void rule_side(uint8_t scope) {
  Kernel k;
  const Ref ref = build_tier3_vivifiable(k);
  assert(k.tier2 < 9 && "test is vacuous unless glue 9 is really above tier2");
  k.vivify_scope = scope;

  const int id = tactix_rule_id("Vivify");
  assert(id >= 0);
  const RuleVTable *v = &tactix_rules()[id];
  assert(v->guard(k));

  uint64_t inst[8];
  const uint32_t n = v->instances(k, inst, 8);
  assert(n == 1 && inst[0] == ref &&
         "N1 BREACH: instances() does not offer the tier3 candidate");

  const ApplyResult r = v->apply_instance(k, inst[0]);
  assert(r == ApplyResult::Ok &&
         "N1 BREACH: a legal tier3 vivification is not applicable through the "
         "rule API");
  assert(k.stats.vivified == 1 && "apply reported Ok but distilled nothing");
  assert(k.stats.vivify_tier3 == 1 && "the tier3 witness did not fire, so the "
                                      "candidate was not really in tier3");
  std::printf("  scope %u: tier3 candidate enumerated and applied\n", scope);
}

// Each named band still selects only its own clauses: scope 0 meaning all
// bands must not collapse the 3/4/5 split the policy budgets separately.
static void bands_stay_disjoint() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit t1[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref r1 = kernel_add_large_clause(k, t1, 3, true, /*glue=*/2);
  const Lit t2[3] = {internalize(1), internalize(4), internalize(5)};
  const Ref r2 = kernel_add_large_clause(k, t2, 3, true, /*glue=*/5);
  const Lit t3[3] = {internalize(2), internalize(4), internalize(6)};
  const Ref r3 = kernel_add_large_clause(k, t3, 3, true, /*glue=*/9);
  std::vector<Ref> cands;
  const auto only = [&](uint8_t scope, Ref want) {
    k.vivify_scope = scope;
    vivify_candidates(k, cands);
    assert(cands.size() == 1 && cands[0] == want);
  };
  only(3, r1);
  only(4, r2);
  only(5, r3);
  k.vivify_scope = 1; // the legacy redundant band still stops at tier2
  vivify_candidates(k, cands);
  assert(cands.size() == 2);
  k.vivify_scope = 0; // ... and the unscoped path now spans all three
  vivify_candidates(k, cands);
  assert(cands.size() == 3 &&
         "N1 BREACH: the unscoped path still excludes tier3");
  std::printf("  bands 1/3/4/5 unchanged, scope 0 spans all three tiers\n");
}

// The setter stores what it was asked for: a clamp that substitutes a different
// band is worse than a rejection, since the caller cannot see it happened.
static void api_side() {
  struct tactix_solver *s = tactix_init();
  for (unsigned scope = 0; scope <= 5; scope++) {
    tactix_set_vivify_scope(s, scope);
    assert(tactix_config(s, CFG_VIVIFY_SCOPE) == scope &&
           "N1 BREACH: tactix_set_vivify_scope silently substituted a band");
  }
  tactix_set_vivify_scope(s, 5); // out of range leaves the pose untouched
  tactix_set_vivify_scope(s, 6);
  assert(tactix_config(s, CFG_VIVIFY_SCOPE) == 5);
  tactix_release(s);
  std::printf("  API setter stores scopes 0-5, ignores out-of-range\n");
}

int main() {
  rule_side(0); // the unscoped N1 path
  rule_side(5); // the named tier3 band
  bands_stay_disjoint();
  api_side();
  std::printf("test_vivify_n1: all passed\n");
  return 0;
}
