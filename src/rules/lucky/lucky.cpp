// Transliterated from kissat/src/lucky.c (MIT). Probes share the single
// propagations/ticks counters rather than kissat's separate probing-mode
// statistics, so a policy's tick budget includes them. Probes never update
// saved/target/best phases (the public probe entry suppresses assign-time
// saving, matching kissat's `solver->probing`).
#include "rules/lucky/lucky.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#ifdef TACTIX_ENABLE_EVENT_TRACE
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace tactix {

namespace {

#ifdef TACTIX_ENABLE_EVENT_TRACE
void lucky_stage_trace(const Kernel &k, const char *stage, size_t first = 0,
                       size_t second = 0, size_t third = 0) {
  static FILE *out = nullptr;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    const char *path = std::getenv("TACTIX_LUCKY_MEMORY_TRACE");
    if (path && *path) {
      out = std::fopen(path, "w");
      if (out)
        std::setvbuf(out, nullptr, _IOLBF, 0);
    }
  }
  if (!out)
    return;
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  unsigned long virtual_pages = 0;
  if (FILE *statm = std::fopen("/proc/self/statm", "r")) {
    (void)std::fscanf(statm, "%lu", &virtual_pages);
    std::fclose(statm);
  }
  const unsigned long virtual_kib =
      virtual_pages * static_cast<unsigned long>(sysconf(_SC_PAGESIZE) / 1024);
  std::fprintf(out,
               "%s\trss_kib=%ld\tvm_kib=%lu\tvars=%u\ttrail=%zu\t"
               "trail_capacity=%zu\tframes=%zu\tframes_capacity=%zu\t"
               "delayed=%zu\tdelayed_capacity=%zu\tfirst=%zu\tsecond=%zu\t"
               "third=%zu\n",
               stage, usage.ru_maxrss, virtual_kib, k.nvars, k.trail.size(),
               k.trail.capacity(), k.frames.size(), k.frames.capacity(),
               k.delayed.size(), k.delayed.capacity(), first, second, third);
}
#else
inline void lucky_stage_trace(const Kernel &, const char *, size_t = 0,
                              size_t = 0, size_t = 0) {}
#endif

} // namespace

RuleEligibility lucky_guard(const Kernel &k) {
  // quiescent root state: see lucky.hpp
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("root-level", k.level == 0);
}

// abandon an in-flight probe: clear the recorded conflict (its clause
// stays in F; the assignment that falsified it is being undone) and
// restore the entry state without touching phases
static void abandon_probe(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
}

// decision-free probe assumption (kissat kissat_internal_assume)
static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

// A conflict under the very first Lucky assumption proves its negation at the
// root. Kissat analyzes and commits that failed literal before continuing the
// same directional sweep. Keeping this consequence is essential: abandoning
// it left FmlaEquivChain with 30 extra active variables before Factor and
// changed the mutable Factor heap even though all later grid guards matched.
static ApplyResult commit_failed_literal(Kernel &k, Lit assumed) {
  assert(k.level == 1 && k.conflict_valid);
  const Lit unit = negate(assumed);
  const bool lrat = k.proof && k.proof->lrat();
  uint64_t closing = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (lrat) {
    std::vector<uint8_t> seen(k.nvars, 0);
    closing = kernel_conflict_reason_walk(k, seen, entries);
  }

  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  kernel_backtrack(k, 0, /*update_phases=*/false);

  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat) {
      kernel_build_lrat_chain(entries, closing, k.lrat_chain);
      k.proof->add_rup(id, &unit, 1, k.lrat_chain.data(),
                       static_cast<uint32_t>(k.lrat_chain.size()));
    } else {
      k.proof->add_rup(0, &unit, 1, nullptr, 0);
    }
  }
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  return propagate_service(k);
}

// kissat ACTIVE (flags.h:23, flags.c:60-103): a variable stays active until it
// is fixed at the root or retired by elimination or substitution. Lucky
// consults it on every identity it might assume and on both endpoints of a
// binary polarity scan, so this holds wherever kissat writes ACTIVE. A retired
// identity keeps its mapping whenever compaction is declined, so the
// INVALID_VAR holes compaction leaves cannot stand in for the test.
static bool lucky_active(const Kernel &k, Var v) {
  return !kernel_var_retired(k, v) &&
         kernel_root_value(k, make_lit(v, false)) == 0;
}

// True iff the constant assignment of polarity !want_negated satisfies every
// irredundant clause, i.e. none of them needs a `want_negated` literal (kissat
// no_all_negative_clauses / no_all_positive_clauses).
static bool no_all_polarity_clauses(const Kernel &k, bool want_negated) {
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant()) {
      bool ok = false;
      for (const Lit l : *c)
        if (negated(l) != want_negated && k.value(l) >= 0) {
          ok = true;
          break;
        }
      if (!ok)
        return false; // this clause needs the other polarity
    }
    ref += clause_words(c->size);
  }
  // binaries live only in watch lists: scan each variable's
  // `want_negated`-polarity literal for a partner of the same polarity. Both
  // endpoints must be active (kissat lucky.c:29 and :37): a binary that an
  // inactive endpoint already satisfies, or that a retired one has left the
  // formula with, no longer forces that polarity on the active clauses.
  for (Var v = 0; v < k.nvars; v++) {
    if (!lucky_active(k, v))
      continue;
    const Lit lit = make_lit(v, want_negated);
    for (const WatchEntry w : k.watches.entries(lit))
      if (w.binary() && negated(w.other()) == want_negated &&
          lucky_active(k, var_of(w.other())))
        return false;
  }
  return true;
}

// assign every unassigned variable to `phase` (+1/-1), propagating after
// each; kissat asserts no conflict is possible after the clause scan
static ApplyResult constant_probe(Kernel &k, int8_t phase) {
  if (!no_all_polarity_clauses(k, /*want_negated=*/phase > 0))
    return ApplyResult::Ok; // some clause defeats this constant
  for (Var v = 0; v < k.nvars; v++) {
    if (!lucky_active(k, v)) // kissat lucky.c:325
      continue;
    const Lit lit = make_lit(v, phase < 0);
    if (k.value(lit) != 0)
      continue;
    assume(k, lit);
    if (kernel_propagate(k)) { // defensive: abandon (kissat asserts !c)
      abandon_probe(k);
      return ApplyResult::Ok;
    }
  }
  return k.unassigned_active ? ApplyResult::Ok : ApplyResult::Sat;
}

// Sweep Kissat's external-variable import table in `forward` order while
// assuming phase `phase`.  Tumble makes internal identities follow first
// occurrence, but Lucky deliberately does not use that order.  On a conflict
// deeper than the first probe level, retry the flipped literal once; if both
// phases conflict, abandon the whole probe.
static ApplyResult sweep_probe(Kernel &k, int8_t phase, bool forward) {
  // Kissat walks its permanent external import table directly.  Tactix used
  // to copy the same table into a dense temporary vector for every direction,
  // adding four bytes per live variable at precisely the post-preprocessing
  // memory high-water mark.  Walk the existing map instead: a hole compaction
  // left reads as INVALID_VAR, and every identity it has not removed is
  // filtered below, so neither direction depends on compaction having run.
  const size_t count = k.external_to_internal.size();
  lucky_stage_trace(k, "sweep-entry", phase < 0, forward, count);
  for (size_t i = 0; i < count; i++) {
    if (!(i & ((size_t{1} << 20) - 1)))
      lucky_stage_trace(k, "sweep-progress", phase < 0, forward, i);
    const size_t external = forward ? i : count - 1 - i;
    const Var v = k.external_to_internal[external];
    // kissat lucky.c:88-95 skips an unimported or eliminated import entry and
    // an inactive identity before assuming anything. An identity outside the
    // active formula is not a probe opportunity: assuming one spends a probe
    // level, which pushes the next genuine failed literal below the first
    // probe level, where the sweep flips it instead of committing it at the
    // root.
    if (v == INVALID_VAR || !lucky_active(k, v))
      continue;
    Lit lit = make_lit(v, phase < 0);
    if (k.value(lit) != 0)
      continue;
    assume(k, lit);
    if (!kernel_propagate(k))
      continue;
    if (k.level > 1) { // retry the flipped assumption once
      k.conflict_valid = false;
      k.conflict_ref = INVALID_REF;
      kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
      assume(k, negate(lit));
      if (!kernel_propagate(k))
        continue;
      // Both phases failed above the first probe level. Neither polarity is a
      // root consequence; retain earlier committed units, abandon this sweep,
      // and let the next Lucky direction try from the clean root.
      abandon_probe(k);
      lucky_stage_trace(k, "sweep-abandoned", phase < 0, forward, i);
      return ApplyResult::Ok;
    }
    const ApplyResult committed = commit_failed_literal(k, lit);
    if (committed == ApplyResult::Unsat)
      return committed;
  }
  lucky_stage_trace(k, "sweep-exit", phase < 0, forward, count);
  return k.unassigned_active ? ApplyResult::Ok : ApplyResult::Sat;
}

ApplyResult lucky_apply_probe(Kernel &k, uint64_t probe) {
  assert(lucky_guard(k));
  // kissat_lucky runs with solver->probing set, so its speculative decisions
  // and implications must not overwrite search phases.
  const bool saved_suppression = k.phase_saving_suppressed;
  k.phase_saving_suppressed = true;
  struct RestorePhaseSaving {
    Kernel &k;
    bool saved;
    ~RestorePhaseSaving() { k.phase_saving_suppressed = saved; }
  } restore{k, saved_suppression};
  k.stats.lucky_probes++;
  switch (probe) {
  case LUCKY_ALL_TRUE:
    return constant_probe(k, 1);
  case LUCKY_ALL_FALSE:
    return constant_probe(k, -1);
  case LUCKY_FORWARD_FALSE:
    return sweep_probe(k, -1, true);
  case LUCKY_FORWARD_TRUE:
    return sweep_probe(k, 1, true);
  case LUCKY_BACKWARD_FALSE:
    return sweep_probe(k, -1, false);
  case LUCKY_BACKWARD_TRUE:
    return sweep_probe(k, 1, false);
  default:
    k.stats.lucky_probes--; // unknown payload: nothing ran
    return ApplyResult::NoOp;
  }
}

// canonical instance: kissat's cascade, first success wins
ApplyResult lucky_apply(Kernel &k) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  static constexpr uint64_t CASCADE[6] = {
      LUCKY_ALL_TRUE,       LUCKY_ALL_FALSE,    LUCKY_FORWARD_FALSE,
      LUCKY_FORWARD_TRUE,   LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE};
  for (const uint64_t probe : CASCADE) {
    const ApplyResult r = lucky_apply_probe(k, probe);
    if (r == ApplyResult::Sat || r == ApplyResult::Unsat)
      return r;
  }
  return ApplyResult::Ok;
}

static uint32_t lucky_instances(Kernel &, uint64_t *out, uint32_t cap) {
  const uint32_t n = cap < 6 ? cap : 6;
  for (uint32_t i = 0; i < n; i++)
    out[i] = i;
  return n;
}

static ApplyResult lucky_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return lucky_apply(k);
  return lucky_apply_probe(k, inst);
}

TACTIX_REGISTER_RULE(Lucky, RuleTier::Macro, lucky_guard, lucky_apply,
                     lucky_instances, lucky_apply_instance);

} // namespace tactix
