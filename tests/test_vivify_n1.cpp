// N1: every band Vivify implements must be reachable through the public API.
// `Kernel::vivify_scope` 0 means all bands, 3/4/5 the redundant tier1/tier2/
// tier3 bands; the rule side must admit a tier3 candidate at 0 and 5, and the
// setter must store 3/4/5 rather than substitute a band. A removed capability
// is not the carve-out, which only lets the built-in policy decline a choice.
// See the implementation notes (`vivify_scope`).
#include "unit_test.hpp"
#include "rule_test_utils.hpp"
#include "rules/registry.hpp"
#include "rules/vivify/vivify.hpp"

// A vivifiable clause parked in tier3: probing ¬1 propagates 2, so (1 2 3)
// distills to (1 2) whatever its glue — a NoOp can only be a closed band.
static Ref build_tier3_vivifiable(Kernel &k) {
  kernel_reserve_vars(k, 3);
  const Ref ref = add_external_clause(k, {1, 2, 3}, /*redundant=*/true, /*glue=*/9);
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
  assert(k.stats.vivify_subsumed == 1 &&
         "apply reported Ok but removed no subsumed candidate");
  assert(k.stats.vivify_tier3 == 1 && "the tier3 witness did not fire, so the "
                                      "candidate was not really in tier3");
  std::printf("  scope %u: tier3 candidate enumerated and applied\n", scope);
}

// The canonical/v1 schedule may select one band, but that policy choice must
// not remove the other legal clause instances from the additive semantic API.
TEST(semantic_surface_bypasses_schedule_band) {
  Kernel k;
  const Ref ref = build_tier3_vivifiable(k);
  k.vivify_scope = 3; // tier1 only: v1 should decline the tier3 witness

  const int id = tactix_rule_id("Vivify");
  assert(id >= 0);
  const RuleVTable &v = tactix_rules()[id];
  assert(v.semantic_instances && v.apply_semantic_instance);

  uint64_t legacy[4];
  assert(v.instances(k, legacy, 4) == 0);
  uint64_t semantic[4];
  const uint32_t count = v.semantic_instances(k, semantic, 4);
  assert(count == 1 && semantic[0] == ref &&
         "N1 BREACH: a policy band hid a legal semantic instance");

  const ApplyResult result = v.apply_semantic_instance(k, semantic[0]);
  assert(result == ApplyResult::Ok);
  assert(k.stats.vivify_subsumed == 1);
  assert(k.vivify_scope == 3 &&
         "semantic application did not restore online configuration");
  std::printf("  semantic clause selection bypasses and restores scope\n");
}

int main() {
  rule_side(0); // the unscoped N1 path
  rule_side(5); // the named tier3 band
  run_tests();
  std::printf("test_vivify_n1: all passed\n");
  return 0;
}
