// SCHEDULING REGRESSION TESTS.
//
// WHY THIS FILE EXISTS. The suite was strong where the solver can be WRONG
// (soundness: brute-force oracles, model checks on SAT, proof checks on UNSAT,
// ASan/UBSan) and silent where it can be BADLY SCHEDULED. That is not a
// hypothetical gap. Of the defects found on 2026-08-04, the soundness ones
// (Factor's N1 breach, the LRAT root-unit miss) were caught by tests; the
// scheduling ones were caught by reading reference source or by staring at a
// telemetry probe, and not one of them would have failed a single test:
//
//   SCHED-B1  a batch entry's budget pre-check was UNFIREABLE by construction
//   SCHED-B5  the mode growth key ran one switch ahead of kissat's
//   SCHED-B4  C1/C2's effort meter under-priced the work by up to 2712x
//   SCHED-B3  instances() re-emitted its head every round and starved the tail
//
// A defect found by measurement costs a cluster window and an argument. The
// same defect found here costs 4 ms. Scheduling is deterministic and the policy
// exposes its transitions "so tests can pose schedules directly", so there was
// never a technical reason these did not exist.
//
// Each test below pins the REFERENCE'S OWN identity, not our current behaviour,
// so it fails when we drift from kissat/cadical rather than when we change.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tactix;

// --- SCHED-B5: the mode growth key ---------------------------------------
//
// kissat mode.c:90 `const uint64_t count = (statistics->switched + 1) / 2;`,
// scaled by kissat_nlogpown(count, 4) = count * (log10(count+9))^4
// (kimits.c:27). `switched` and `limits.mode.count` are incremented at the SAME
// site (mode.c:204-205) and this branch runs only on EVEN mode.count, so the
// key is exactly mode.count/2 -- and the FIRST focused re-entry has key 1,
// where log10(1+9)^4 = 1 exactly. So kissat's first focused interval is
// `modeint` ON THE NOSE, an integer identity that cannot be fudged.
//
// We shipped `(mode.count / 2) + 1`, i.e. key 2, i.e. 2*log10(11)^4 ~= 2.35x
// the reference interval, growing in the same direction forever after. This
// test fails on that exact bug.
static void test_mode_growth_key_matches_kissat(void) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  DefaultPolicy p;

  // Pose the switch sequence directly: count 1 = entered stable (ticks branch),
  // count 2 = entered focused (the conflicts branch under test).
  p.mode.count = 2;
  k.stats.conflicts = 500;
  p.update_mode_limits(k);
  const uint64_t first = p.mode.conflicts_limit - k.stats.conflicts;
  assert(first == DefaultPolicy::MODE_INTERVAL &&
         "kissat's FIRST focused interval is exactly modeint (key 1, "
         "log10(10)^4 == 1). A larger value means the growth key is ahead.");

  // And the whole ladder, not just its first rung.
  for (uint64_t c = 4; c <= 12; c += 2) {
    p.mode.count = c;
    k.stats.conflicts = 1000 * c;
    p.update_mode_limits(k);
    const double key = static_cast<double>(c / 2);
    const double l = std::log10(key + 9);
    const uint64_t want = static_cast<uint64_t>(
        DefaultPolicy::MODE_INTERVAL * key * l * l * l * l);
    const uint64_t got = p.mode.conflicts_limit - k.stats.conflicts;
    assert(got == want && "focused interval must be modeint * nlogpown(c/2, 4)");
  }
  std::printf("  mode growth key: matches kissat's (switched+1)/2 ladder\n");
}

// --- SCHED-B3: instances() must ROTATE, not replay its head ---------------
//
// The policy can only take `cap` instances per occasion. Emitting the first
// `cap` sources every round means a budget-truncated round re-derives the same
// list and starts from the front, so the tail is never reached -- which is why
// an honest (tighter) effort budget for V4 measured NEGATIVE twice: it bought
// starvation, not economy. kissat keeps a per-variable pending bit
// (`transitivekeep`, default 1) to the same end.
//
// N1 note: rotation is the N1-safe shape. It removes no capability, because
// apply_instance still accepts ANY source literal; only the enumeration ORDER
// moves. A cursor that HID instances would be an N1 breach.
static void test_transitive_instances_rotate(void) {
  Kernel k;
  kernel_reserve_vars(k, 128);
  // A source needs out-degree >= 2 in the binary implication graph and at
  // least one destination above it, so build ten FANS rather than a chain
  // (a chain gives every literal out-degree 1 and yields no sources at all --
  // which silently SKIPPED this test on the first attempt).
  for (uint32_t s = 0; s < 10; s++)
    for (uint32_t t = 0; t < 4; t++)
      kernel_add_binary_clause(k, make_lit(s, true),
                               make_lit(20 + s * 4 + t, true));

  const RuleVTable *v4 = nullptr;
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, "TransitiveReduce"))
      v4 = &r;
  assert(v4 && v4->instances && v4->apply_instance);
  if (!v4->guard(k)) {
    std::printf("  transitive rotation: SKIPPED (guard false)\n");
    return;
  }

  uint64_t a[4] = {0, 0, 0, 0}, b[4] = {0, 0, 0, 0};
  const uint32_t na = v4->instances(k, a, 4);
  // A skip here would make the test VACUOUS, which is worse than absent: it
  // reads green while proving nothing. Demand the fixture instead.
  assert(na >= 4 && "fixture must offer more sources than the cap, or the "
                    "rotation is not observable");
  // Applying advances the cursor, so the NEXT enumeration must not begin at the
  // same place. Without the cursor these two calls are identical.
  v4->apply_instance(k, a[0]);
  const uint32_t nb = v4->instances(k, b, 4);
  assert(nb > 0);
  assert(!(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]) &&
         "instances() replayed its head: a truncated round would starve the "
         "tail forever (SCHED-B3)");
  std::printf("  transitive rotation: enumeration advances after apply\n");
}

// --- SCHED-B4: C1/C2 must meter PARTNER VISITS, not candidates ------------
//
// cadical budgets subsumption in `stats.subchecks` (subsume.cpp:86), bumped once
// per candidate partner actually compared, because the cost of "checking a
// clause" is the length of its occurrence lists and not 1. Metering candidates
// under-prices exactly the expensive ones -- measured at 2712x on DivS_862_11,
// which is how C2 spent 97.8 s of it inside FOUR "rounds" without ever
// exceeding its budget.
//
// A hot literal shared by many clauses makes the gap structural: one candidate,
// many partners. If visits ever collapse back to ~checked, the meter has
// silently reverted to counting candidates.
static void test_subsume_meters_partner_visits(void) {
  Kernel k;
  kernel_reserve_vars(k, 96);
  // LARGE clauses (binaries are not forward-subsumption candidates), all
  // sharing one hot literal so a single candidate faces many partners.
  const Lit hot = make_lit(0, true);
  for (uint32_t i = 2; i < 60; i++) {
    const Lit c4[4] = {hot, make_lit(1, true), make_lit(i, true),
                       make_lit(i + 1, true)};
    kernel_add_large_clause(k, c4, 4, false, 0, 0);
  }
  const Lit c3[3] = {hot, make_lit(1, true), make_lit(2, true)};
  kernel_add_large_clause(k, c3, 3, false, 0, 0); // subsumes one of the above

  const RuleVTable *c1 = nullptr;
  for (const RuleVTable &r : tactix_rules())
    if (!std::strcmp(r.name, "SubsumeForward"))
      c1 = &r;
  assert(c1);
  if (!c1->guard(k)) {
    std::printf("  subsume meter: SKIPPED (guard false)\n");
    return;
  }
  c1->apply(k);
  assert(k.stats.forward_visits > 0 &&
         "forward_visits never moved -- the meter is not reachable, so a green "
         "budget test would be vacuous");
  assert(k.stats.forward_visits > k.stats.forward_checked &&
         "visits must exceed candidates on a formula with a shared literal; "
         "equality means the meter is counting candidates again (SCHED-B4)");
  std::printf("  subsume meter: %llu visits vs %llu candidates\n",
              (unsigned long long)k.stats.forward_visits,
              (unsigned long long)k.stats.forward_checked);
}

int main() {
  test_mode_growth_key_matches_kissat();
  test_transitive_instances_rotate();
  test_subsume_meters_partner_visits();
  std::printf("test_scheduling: all passed\n");
  return 0;
}
