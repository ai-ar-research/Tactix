// Sweep (V5): prove `x ≡ y` by a budgeted refutation over a local clause set
// around a seed, then add the two equivalence binaries with a checkable chain —
// a RUP fast path where one exists, a general core→chain path otherwise.
// Contract, proof delta and soundness: `docs/calculus.md` V5; design
// `docs/design/v5-sweep-design.md`; the deviations from kissat `sweep.c` and
// cadical `sweep.cpp` (each a weakening of what is detected, never of what may
// be concluded) are the sweep rows of `docs/scheduling-parity-ledger.md` §13.2.
//
// The invariant everything here serves: the environment is entailed by F, so
// `env ⊨ x ≡ y` gives `F ⊨ x ≡ y`. Every environment clause is a clause the
// kernel holds, cited by its own proof id, minus its root-false literals, whose
// justifying root-unit ids travel with it in `SweepEnv::strip_ids`.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep_defaults.hpp"
#include "engine/subsolver.hpp"

#include <vector>

namespace tactix {

// "How hard to sweep" is a policy decision (plan §5), so the limits are a
// caller-owned parameter rather than rule state. Defaults are kissat's option
// values scaled to one environment; `env_ticks` is in the sub-solver's own
// currency, not the kernel's.
struct SweepLimits {
  uint32_t max_depth = SweepReferenceDefaults::DEPTH;
  uint32_t max_depth_ceiling = SweepReferenceDefaults::MAX_DEPTH;
  uint32_t max_env_vars = SweepReferenceDefaults::VARS;
  uint32_t max_env_vars_ceiling = SweepReferenceDefaults::MAX_VARS;
  uint32_t max_env_clauses = SweepReferenceDefaults::CLAUSES;
  uint32_t max_env_clauses_ceiling = SweepReferenceDefaults::MAX_CLAUSES;
  uint32_t max_candidates = SweepReferenceDefaults::CANDIDATE_PAIRS;
  uint64_t env_ticks = SweepReferenceDefaults::ENVIRONMENT_TICKS;
  uint32_t max_seeds = SweepReferenceDefaults::SEED_CAP;
  uint64_t round_ticks = 0;       // whole-round tick ceiling; 0 = unbounded
  // Remove root-false literals from environment clauses (kissat sweep.c:295).
  // Ships on; `TACTIX_SWEEP_STRIP=0` is the off arm.
  bool strip_false = true;
  // Measure environment closure (`Stats::sweep_occ_*`) — how much of a
  // variable's real neighbourhood the environment holds. Off by default: an
  // extra pass per seed is a diagnostic's cost. `TACTIX_SWEEP_CLOSURE=1`.
  bool measure_closure = false;
  // All-environment-literal partition and model-flip prefilter.
  bool full_partition = true;
  // The shipped strategy runs after the root probe pipeline and follows
  // Kissat's `kitten_fixed` filter. Standalone rule applications retain the
  // stronger detector because they need not have had that preceding probe.
  bool skip_embedded_fixed = false;
  uint32_t flip_rounds = SweepReferenceDefaults::FLIP_ROUNDS;
  // Retaining a seed tail is scheduling, not formula state. Standalone rule
  // applications use the reference-on default; policies pose their own value.
  bool resume = true;
};

// Sweep owns both currencies.  The policy prices their sum when scheduling a
// later call, while the live semantic dose follows Kissat's Kitten-only limit.
// Keeping these meanings separate matters after a productive environment:
// Tactix has to rebuild a variable-width arena, but that representation bridge
// must not consume the remaining reference-ordered candidate tail.
inline uint64_t sweep_effort(const Kernel &k) {
  // The embedded solver is only part of Tactix's cost.  A dense call crosses
  // the variable-width representation bridge once after its ordered
  // environment sequence; a sessionless call does so after each productive
  // environment.  Keep that real host work in the rule-owned scheduler meter.
  return k.stats.sweep_ticks_sub + k.stats.sweep_ticks_repr;
}

inline uint64_t sweep_semantic_effort(const Kernel &k) {
  return k.stats.sweep_ticks_sub;
}

constexpr uint32_t SWEEP_NO_SUB_VAR = UINT32_MAX;

// The local clause set around one seed, encoded for the sub-solver, plus the
// two mappings emission needs: sub var → kernel var, and environment clause id
// → main-formula proof id.
struct SweepEnv {
  SubSolver solver;
  // Formula-mutation epoch after this environment was fully built. Inline
  // emission may rewrite F while the embedded solver still holds this local
  // snapshot; captured general proofs are usable only at this epoch.
  uint64_t formula_epoch = 0;
  std::vector<Var> vars;            // sub var -> kernel var
  std::vector<uint32_t> sub_of_var; // kernel var -> sub var (SWEEP_NO_SUB_VAR)
  std::vector<Var> touched;         // sub_of_var restore list (see reset)
  std::vector<uint32_t> depth;      // by sub var: BFS layer (fixed vars: UINT32_MAX)
  // main-formula proof id of environment clause i+1 (sub ids run from 1 in add
  // order). 0 means the proof has no id for it, so emission must decline such a
  // core rather than cite 0.
  std::vector<uint64_t> clause_ids;
  // Open-addressed set of the clause keys already encoded, so the dedup test is
  // O(1) rather than quadratic in the environment. UINT64_MAX is the empty slot:
  // no key can be all-ones, since KEY_LARGE and KEY_UNIT are never both set and
  // a binary key's top bits hold a literal, not a tag.
  std::vector<uint64_t> key_slots;
  uint64_t key_mask = 0;
  std::vector<Lit> clause_lits;     // flat kernel literals
  std::vector<uint32_t> clause_off; // start of clause i in clause_lits (+end)
  // Strip ledger: `strip_ids[strip_off[i] .. strip_off[i+1])` are the root-unit
  // ids justifying the literals removed from environment clause i. A chain
  // citing clause i must cite them, and the environment is gone by emission
  // time — hence carried, not re-derived.
  std::vector<uint64_t> strip_ids;
  std::vector<uint32_t> strip_off; // size nclauses()+1
  // per-clause scratch, kept here so the BFS pays no allocation per clause
  std::vector<Lit> kept_scratch;
  std::vector<uint64_t> strip_scratch;
  std::vector<uint32_t> deg_scratch; // by sub var: large-clause degree in env

  uint32_t nclauses() const {
    return static_cast<uint32_t>(clause_ids.size());
  }
};

// One direction's refutation, captured at detection time: the sub-solver keeps
// only the last solve's derivation and an environment does not outlive its
// seed. `steps` stays in sub-solver space, with `orig_ids` and `vars` the
// translation tables captured alongside; SWEEP_MAX_PROOF_STEPS bounds what a
// pathological refutation may allocate.
constexpr uint32_t SWEEP_MAX_PROOF_STEPS = 4096;
struct SweepDirProof {
  // The binary this direction proves. Recorded explicitly because `SweepEquiv`
  // normalises its pair to {min,max}, so deriving the targets from that pair
  // silently swaps the two directions when the seed is the larger literal.
  Lit t0 = INVALID_LIT, t1 = INVALID_LIT;
  std::vector<SubStep> steps;
  std::vector<uint64_t> orig_ids;
  // the environment's strip ledger, captured with `orig_ids`: an antecedent's
  // chain is incomplete without the units justifying its stripped literals
  std::vector<uint64_t> strip_ids;
  std::vector<uint32_t> strip_off;
  std::vector<Var> vars;
  uint64_t formula_epoch = 0;
  bool ok = false;
};
struct SweepPairProof {
  SweepDirProof fwd, rev;
};

struct SweepUnitProof {
  Lit unit = INVALID_LIT;
  SweepDirProof proof;
};

// A proved equivalence `a ≡ b` over kernel literals, canonical with a < b.
struct SweepEquiv {
  Lit a = INVALID_LIT;
  Lit b = INVALID_LIT;
};

// Round-local scratch, reusable across rounds: `sub_of_var` and `repr` are
// O(nvars) and must be initialized once, then restored by touched-list.
struct SweepRound {
  SweepEnv env;
  SweepSubstitutionSession substitution;
  std::vector<std::vector<Ref>> occ_owned; // built only when k.dense is unusable
  const std::vector<std::vector<Ref>> *occ = nullptr; // by literal
  // `Kernel::arena_generation` when `occ_owned` was built. A collection inside
  // the round invalidates every Ref in it, and unlike `Kernel::dense` nothing
  // else rebuilds it.
  uint64_t occ_generation = 0;
  std::vector<Lit> repr;                   // union-find over literals
  std::vector<SweepEquiv> found;
  std::vector<SweepPairProof> proofs; // parallel to `found`
  std::vector<SweepUnitProof> units;
  // Emission cursors let the apply path commit each newly proved consequence
  // immediately while the public detection path remains pure.  `emission_batches`
  // is monotone within a round so a caller can observe a commit even after the
  // pending vectors have been drained.
  size_t units_emitted = 0;
  size_t equivalences_emitted = 0;
  uint64_t emission_batches = 0;
  std::vector<Var> seeds;
  // candidate literals still agreeing with the seed in every model seen
  std::vector<Lit> candidates;
  std::vector<Lit> backbone;
  std::vector<std::vector<Lit>> partition;
  std::vector<SubLit> sub_scratch, assumptions;
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  // Diagnostic stopped-state wall-clock attribution. Production omits both
  // these fields and every timing branch.
  uint64_t time_kitten = 0, time_dense = 0, time_occ = 0, time_ledger = 0,
           time_repr = 0, time_schedule = 0, time_process = 0,
           time_post_seed = 0, time_collect = 0;
#endif
};

// Guard: quiescent root, consistent. A pure precondition — the environment
// cites level-0 unit ids and reads the watch lists, and both mean what they say
// only at a drained root.
RuleEligibility sweep_guard(const Kernel &);
// The external identity the resumable schedule will seed next (tombstones
// skipped, the reschedule front before the tail), INVALID_VAR when empty.
// Read-only; the round loop consumes through its own cursor.
Var sweep_next_scheduled(const Kernel &);

// Canonical seed order: active, unassigned variables, ascending occurrence
// degree. This is the action surface `instances()` exposes, so it ranks and
// never filters — no "unlikely to pay off" omissions (N1, rule-dev §instances).
void sweep_seeds(const Kernel &, std::vector<Var> &out);

// Start a round: prepare the occurrence index over irredundant large clauses
// (`Kernel::dense`, else a round-local one) and reset the representative map.
// Must precede any `sweep_detect_seed`.
void sweep_round_begin(Kernel &, SweepRound &);

// Build the environment around `seed`. False = nothing usable (no clauses, or
// the seed is fixed/retired). Charges its scan to `Stats::sweep_ticks`.
bool sweep_env_build(Kernel &, Var seed, const SweepLimits &, SweepRound &);

// One seed: build the environment, refute both directions of each surviving
// candidate pair, append what is proved to `round.found`. Returns the number of
// equivalences found. Detection only: changes nothing in `F/R/τ/ρ`.
uint32_t sweep_detect_seed(Kernel &, Var seed, const SweepLimits &,
                           SweepRound &);

// A full round over `sweep_seeds` order. Returns the number of equivalences
// found (also in `round.found`).
uint32_t sweep_detect_round(Kernel &, const SweepLimits &, SweepRound &);

// One seed end to end: detect around it and emit what is proved. The
// per-instance half of the N1 surface; `INSTANCE_CANONICAL` runs the round.
ApplyResult sweep_try(Kernel &, Var seed);

// The canonical full apply: detect, then emit each justifiable equivalence as
// two binaries, declining the rest.
ApplyResult sweep_apply(Kernel &);

// DefaultPolicy uses its centralized typed profile through this entry point.
// Public/registry applications use `sweep_apply` and the standalone reference
// defaults above; both retain the same proof-rule semantics.
ApplyResult sweep_apply_with_limits(Kernel &, const SweepLimits &);

// Tactix commits Sweep equivalences per environment, while Kissat's following
// Substitute deactivates the complete union in one internal-variable scan.
// Re-pose the still-unconsumed event suffix at that transaction boundary.
void sweep_order_retirement_suffix(Kernel &, size_t retired_begin,
                                   size_t substituted_begin);

// Read the SweepLimits overrides from the environment. Lives here rather than
// in the policy so the probe hook and the tests configure the detector alike.
SweepLimits sweep_limits_from_env(SweepLimits base = SweepLimits{});

} // namespace tactix
