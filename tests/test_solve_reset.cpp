// Per-solve reset identity: a reused policy object must be indistinguishable
// from a fresh one, or a run depends on episode order — and the §5.6 learned
// policy solves many instances per process. For fresh kernels A, B of one
// formula, `DefaultPolicy p; p.solve(A); p.solve(B)` must give solve #2 the
// trajectory and the exact proof bytes of `DefaultPolicy q; q.solve(B)`. Both
// references re-base every limit on the solver's current counters at search
// entry: kissat search.c:45-70 with kimits.c:99-136, cadical
// internal.cpp:499-608.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace tactix;

// A deterministic random 3-SAT at the phase transition, big enough that every
// schedule fires and small enough for ctest.
static void build(Kernel &k, uint32_t nvars, uint32_t nclauses) {
  kernel_reserve_vars(k, nvars);
  uint64_t rng = 0x1234567890abcdefull;
  const auto next = [&rng]() {
    rng += 0x9E3779B97F4A7C15ull;
    uint64_t z = rng;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  };
  std::vector<Lit> scratch;
  for (uint32_t c = 0; c < nclauses; c++) {
    scratch.clear();
    while (scratch.size() < 3) {
      const int v = static_cast<int>(next() % nvars) + 1;
      bool dup = false;
      for (const Lit l : scratch)
        dup |= (var_of(l) == static_cast<Var>(v - 1));
      if (dup)
        continue;
      scratch.push_back(internalize((next() & 1) ? v : -v));
    }
    ingest_clause(k, scratch);
  }
  finish_input(k);
}

// The trajectory signature: the counters the schedule drives, plus the proof
// bytes. Two solves down different paths cannot agree on all of them.
struct Trace {
  uint64_t conflicts = 0, decisions = 0, propagations = 0, ticks = 0;
  uint64_t restarts = 0, reductions = 0, deleted = 0, learned = 0;
  uint64_t switched = 0, rephased = 0, walks = 0, retiered = 0;
  uint64_t random_sequences = 0, tier2_band_dynamic = 0;
  int result = 0;
  std::string proof;

  bool operator==(const Trace &o) const {
    return conflicts == o.conflicts && decisions == o.decisions &&
           propagations == o.propagations && ticks == o.ticks &&
           restarts == o.restarts && reductions == o.reductions &&
           deleted == o.deleted && learned == o.learned &&
           switched == o.switched && rephased == o.rephased &&
           walks == o.walks && retiered == o.retiered &&
           random_sequences == o.random_sequences &&
           tier2_band_dynamic == o.tier2_band_dynamic && result == o.result &&
           proof == o.proof;
  }
};

static void print(const char *tag, const Trace &t) {
  std::printf("  %-9s res=%d confl=%llu dec=%llu props=%llu ticks=%llu "
              "restart=%llu reduce=%llu del=%llu learn=%llu switch=%llu "
              "rephase=%llu walk=%llu retier=%llu randseq=%llu band=%llu "
              "proof=%zuB\n",
              tag, t.result, (unsigned long long)t.conflicts,
              (unsigned long long)t.decisions,
              (unsigned long long)t.propagations, (unsigned long long)t.ticks,
              (unsigned long long)t.restarts, (unsigned long long)t.reductions,
              (unsigned long long)t.deleted, (unsigned long long)t.learned,
              (unsigned long long)t.switched, (unsigned long long)t.rephased,
              (unsigned long long)t.walks, (unsigned long long)t.retiered,
              (unsigned long long)t.random_sequences,
              (unsigned long long)t.tier2_band_dynamic, t.proof.size());
  std::fflush(stdout); // the interesting output is the one before an abort
}

// One solve on a fresh kernel; the proof is closed before the bytes are read.
static Trace run(DefaultPolicy &p, uint32_t nvars, uint32_t nclauses,
                 const char *path) {
  Trace t;
  {
    Kernel k;
    ScopedHeuristics hg(k);
    Proof proof;
    assert(proof.open(path, ProofFormat::DratAscii));
    k.proof = &proof;
    build(k, nvars, nclauses);
    t.result = p.solve(k);
    t.conflicts = k.stats.conflicts;
    t.decisions = k.stats.decisions;
    t.propagations = k.stats.propagations;
    t.ticks = k.stats.ticks;
    t.restarts = k.stats.restarts;
    t.reductions = k.stats.reductions;
    t.deleted = k.stats.deleted;
    t.learned = k.stats.learned;
    t.switched = k.stats.switched;
    t.rephased = k.stats.rephased;
    t.walks = k.stats.walks;
    t.retiered = k.stats.retiered;
    t.random_sequences = k.stats.random_sequences;
    t.tier2_band_dynamic = k.stats.tier2_band_dynamic;
    k.proof = nullptr;
  }
  FILE *f = std::fopen(path, "rb");
  assert(f);
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
    t.proof.append(buf, n);
  std::fclose(f);
  std::remove(path);
  return t;
}

// The shipped single-solve path must not move: there the reset is a no-op,
// because every limit is re-based on a kernel counter still zero at solve
// entry and so lands on its constructor default. A re-based field whose
// zero-counter value differs from its default fails here.
static void assert_reset_is_a_noop_on_a_fresh_kernel(uint32_t nvars,
                                                     uint32_t nclauses) {
  Kernel k;
  ScopedHeuristics hg(k);
  build(k, nvars, nclauses);
  assert(k.stats.conflicts == 0 && k.stats.ticks == 0 &&
         "solve entry is no longer at zero counters — the no-op argument for "
         "the shipped single-solve path has to be re-made, not assumed");
  DefaultPolicy p, pristine;
  p.reset_schedule(k);
#define SAME(field) assert(p.field == pristine.field && #field)
  SAME(restart.next_conflicts);
  SAME(reduce.next_conflicts);
  SAME(mode.count);
  SAME(mode.conflicts_limit);
  SAME(mode.ticks_limit);
  SAME(mode.ticks_at_focus_entry);
  SAME(reluctant.u);
  SAME(reluctant.v);
  SAME(reluctant.wait);
  SAME(reluctant.trigger);
  SAME(rephase.next_conflicts);
  SAME(rephase.count);
  SAME(reorder.next_conflicts);
  SAME(reorder.count);
  SAME(averages[0].initialized);
  SAME(averages[1].initialized);
#undef SAME
  assert(p.inprocess().initialized() == pristine.inprocess().initialized());
  assert(p.inprocess().probe_clock.next_conflicts ==
         pristine.inprocess().probe_clock.next_conflicts);
  assert(p.inprocess().probe_clock.rounds ==
         pristine.inprocess().probe_clock.rounds);
  assert(p.inprocess().elim_clock.next_conflicts ==
         pristine.inprocess().elim_clock.next_conflicts);
  assert(p.inprocess().elim_clock.rounds ==
         pristine.inprocess().elim_clock.rounds);
  assert(p.inprocess().entries.empty() &&
         "the slot is built AFTER the reset on solve #1, so there is no "
         "per-entry state for it to touch there");
}

// Every entry here is state a second solve would inherit. Asserting the dirt
// keeps the identity check above from going vacuously green.
static void assert_every_group_is_dirty_after_a_solve(const DefaultPolicy &p) {
  unsigned dirty = 0, total = 0;
  const auto note = [&](bool b, const char *what) {
    total++;
    if (b)
      dirty++;
    else
      std::printf("  NOT DIRTY: %s\n", what);
  };
  note(p.averages[0].initialized, "averages[focused]");
  note(p.restart.next_conflicts != DefaultPolicy::RESTART_INTERVAL,
       "restart limit");
  note(p.reduce.next_conflicts != DefaultPolicy::REDUCE_INTERVAL_INIT,
       "reduce limit");
  note(p.mode.count != 0 ||
           p.mode.conflicts_limit != DefaultPolicy::MODE_INITIAL_CONFLICTS,
       "mode alternation");
  note(p.reluctant.u != 1 || p.reluctant.v != 1 ||
           p.reluctant.wait != DefaultPolicy::RELUCTANT_PERIOD,
       "reluctant sequence");
  note(p.rephase.count != 0 ||
           p.rephase.next_conflicts != DefaultPolicy::REPHASE_INTERVAL_INIT,
       "rephase schedule");
  note(p.inprocess().initialized(), "inprocessing prime flag");
  note(p.inprocess().probe_clock.rounds != 0, "probe clock");
  note(p.inprocess().elim_clock.rounds != 0 ||
           p.inprocess().elim_clock.next_conflicts != 0,
       "elim clock");
  // The phase effort anchors moved from the entries to the clocks; a reused
  // policy that keeps them would price its first phase against the previous
  // solve's whole tick count.
  note(p.inprocess().probe_clock.anchor != 0 || p.inprocess().elim_clock.anchor != 0, "phase anchors");
  uint64_t entry_state = 0;
  for (const DefaultInprocessScheduler::SlotEntry &e : p.inprocess().entries)
    entry_state += e.limit + e.applies + e.doses + e.cursor +
                   e.tm.occasions + e.binaries_mark + e.delay_current;
  note(entry_state != 0, "slot-entry runtime state");
  std::printf("  state groups dirty after a solve: %u/%u\n", dirty, total);
  assert(dirty == total &&
         "a state group a real solve leaves PRISTINE makes the identity "
         "assertion vacuous for it — shrink the formula's excuse, not the set");
}

int main(int argc, char **argv) {
  // 160 vars at ratio 4.7 refutes past the first mode switch, reduce, rephase,
  // walk and retier. Both arguments are overridable for a soak.
  const uint32_t nvars =
      argc > 1 ? static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10))
               : 160;
  const uint32_t nclauses =
      argc > 2 ? static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10))
               : nvars * 47 / 10;

  assert_reset_is_a_noop_on_a_fresh_kernel(nvars, nclauses);

  // --- the identity: solve #2 on a reused policy == solve #1 on a fresh one --
  DefaultPolicy p;
  run(p, nvars, nclauses, "reset_first.drat"); // dirties p
  const Trace reused = run(p, nvars, nclauses, "reset_reused.drat");
  DefaultPolicy q;
  const Trace fresh = run(q, nvars, nclauses, "reset_fresh.drat");
  print("reused#2", reused);
  print("fresh#1", fresh);
  assert(fresh.result == 10 || fresh.result == 20);
  assert(!fresh.proof.empty() &&
         "the proof halves of the comparison below must not both be empty");
  assert(reused == fresh &&
         "solve #2 on a REUSED policy diverged from a FRESH one: scheduling "
         "state leaked across the solve boundary (SCHED-H2)");
  assert(p.solves == 2 && q.solves == 1 &&
         "the per-solve reset did not run exactly once per solve");

  // --- and the leak really had something to carry ---------------------------
  DefaultPolicy r;
  const Trace once = run(r, nvars, nclauses, "reset_dirty.drat");
  assert(once == fresh);
  assert_every_group_is_dirty_after_a_solve(r);

  // --- posed state is re-initialised too ------------------------------------
  // Reorder is off by default, so pose it dirty and check the reset takes it
  // back (kissat kimits.c:113). Only `reset_schedule_per_solve` may exempt a
  // posed schedule.
  DefaultPolicy posed;
  posed.reorder.next_conflicts = 12345;
  posed.reorder.count = 7;
  const Trace after = run(posed, nvars, nclauses, "reset_posed.drat");
  assert(after == fresh &&
         "posed scheduling state survived into the solve it preceded");

  // ... and the opt-out really opts out, or it is not an escape hatch.
  DefaultPolicy honoured;
  honoured.reset_schedule_per_solve = false;
  honoured.reduce.next_conflicts = 1; // reduce from the first conflict on
  const Trace kept = run(honoured, nvars, nclauses, "reset_optout.drat");
  assert(!(kept == fresh) &&
         "reset_schedule_per_solve=false did not honour the posed schedule");

  std::printf("test_solve_reset: OK\n");
  return 0;
}
