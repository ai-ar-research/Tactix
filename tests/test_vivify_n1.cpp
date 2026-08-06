// N1 REGRESSION: every band Vivify implements must be reachable through the
// public API, and the UNSCOPED path must not exclude one.
//
// This is the Factor breach (tests/test_factor_n1.cpp) in a second rule. The
// tier3 band (redundant, glue > the live tier2) exists and DefaultPolicy
// vivifies it, but `tactix_set_vivify_scope` clamped `scope <= 2 ? scope : 0`,
// so an external policy asking for scope 3/4/5 silently got scope 0 — and
// scope 0 was itself bounded by tier2. The result was that no API caller could
// vivify a tier3 clause at all: the guard reported applicable, `instances()`
// never enumerated the Ref, and `apply_instance` on it no-opped. That is a
// capability REMOVED, which the N1 carve-out does not permit; the carve-out
// lets the built-in POLICY decline a choice.
//
// Two halves, and both fail on the pre-fix build:
//   1. the rule side — scope 0 (and scope 5) must admit a tier3 candidate;
//   2. the API side  — the setter must STORE 3/4/5 rather than substitute 0.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/vivify/vivify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// The C API declared by hand: including tactix.h here is impossible (its
// `typedef struct tactix_solver tactix` collides with `namespace tactix`, the
// reason api.cpp does not include it either), and the setter is exactly what
// this test has to pin. tests/test_api_c.c remains the header/ABI sync check.
extern "C" {
struct tactix_solver *tactix_init(void);
void tactix_release(struct tactix_solver *);
void tactix_set_vivify_scope(struct tactix_solver *, unsigned);
uint64_t tactix_config(struct tactix_solver *, int);
}
#define CFG_VIVIFY_SCOPE 5 // TACTIX_CFG_VIVIFY_SCOPE

using namespace tactix;

// A genuinely vivifiable clause parked in tier3: C = (1 2 3) redundant with
// glue 9 > the default tier2, plus the binary (1 2). Probing ¬1 propagates 2,
// so C distills to (1 2) — the outcome test_vivify's tier2 fixture asserts,
// moved one band up. The clause is vivifiable independently of its glue, so a
// NoOp here can only be the band being closed.
static Ref build_tier3_vivifiable(Kernel &k) {
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref ref = kernel_add_large_clause(k, c, 3, /*redundant=*/true,
                                          /*glue=*/9);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  return ref;
}

// Drive the REGISTRY surface — guard, instances(), apply_instance — because
// that is what tactix_rules_applicable / tactix_apply_instance route an
// external policy through. Calling vivify_apply directly would miss the point:
// the breach was that the advertised band held no advertisable instance.
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

// Each named band still selects only its own clauses: making scope 0 mean ALL
// bands must not collapse the 3/4/5 split SCHED-G3 needs to spend three
// separate shares.
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

// The API half: the setter must store what it was asked for. A clamp that
// substitutes a DIFFERENT band is worse than a rejection — the caller has no
// way to see it happened, and the pose reads as configured.
static void api_side() {
  struct tactix_solver *s = tactix_init();
  for (unsigned scope = 0; scope <= 5; scope++) {
    tactix_set_vivify_scope(s, scope);
    assert(tactix_config(s, CFG_VIVIFY_SCOPE) == scope &&
           "N1 BREACH: tactix_set_vivify_scope silently substituted a band");
  }
  // Out of range leaves the pose untouched rather than substituting one.
  tactix_set_vivify_scope(s, 5);
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
