// Sweep (design `docs/design/v5-sweep-design.md`): prove `x ≡ y` by a budgeted
// refutation over a LOCAL clause set around a seed, then add the two
// equivalence binaries with a checkable chain — a RUP fast path where one
// exists, and a general core→chain path otherwise.
//
// The rule is registered (sweep.cpp) and scheduled like every other
// Inprocess-tier rule; DefaultPolicy meters it in `Stats::sweep_ticks`. Note
// that a rule whose `apply` could only ever return NoOp must NOT be registered:
// it would put an inert move into N1's action space — the surface
// `RandomPolicy` enumerates and the phase-6 learner reasons over — and be
// scheduled by default for zero yield.
//
// THE SOUNDNESS ARGUMENT, in one line: the environment is ENTAILED BY F, so
// `env ⊨ x ≡ y` implies `F ⊨ x ≡ y`. Everything in `sweep_env_build` exists to
// keep that checkable — every environment clause is a clause the kernel (and
// the checker) currently holds, cited by its own proof id:
//   - large: irredundant, live, not root-satisfied; provenance = `Clause::id()`
//   - binary: live in the watch lists; provenance = `kernel_binary_id`
// with its root-FALSE literals removed. That is the one place the environment
// is not literally a subset of F, and it is justified rather than assumed:
// `C` minus a root-false `l` follows from `C` and the root unit for `¬l`,
// whose id travels with the clause in `SweepEnv::strip_ids` for emission to
// cite. Nothing else is strengthened, shrunk, or invented on the way in.
// Emission maps a core entry back to a main-formula clause by indexing
// `SweepEnv::clause_ids`.
//
// References (plan §5 "evidence, not templates"; MIT, transliterated in
// essence, no code copied):
//  - kissat 4.0.4 `sweep.c`: the shape adopted here is its
//    schedule → environment BFS (`sweep_binary`/`sweep_reference`, depth-
//    limited, clause/variable capped) → solve → refine-on-model →
//    assume-both-directions loop (`sweep_equivalence_candidates`), and its
//    representative map (`sweep_repr`) so one class is not re-proved from
//    every member.
//  - cadical `sweep.cpp`: same idea, newer and smaller.
//  - kitten's core is deliberately NOT our interface — our sub-solver gives
//    the stronger thing (an ordered LRAT derivation), see subsolver.hpp.
//
// Deviations from kissat, and why (the ledger entries; each is a WEAKENING of
// what we detect, never of what we may conclude):
// (1) ROOT-FALSE LITERALS ARE STRIPPED, as in both references (kissat
//     sweep.c:295). The one case that keeps a literal is an LRAT run whose
//     `unit_ids[v]` is 0, i.e. the proof holds no unit clause to cite;
//     `propagate_service`'s `emit_root_units` makes that vanishingly rare. A
//     format with no hints needs no id at all — the checker re-derives every
//     root unit by propagation. `TACTIX_SWEEP_STRIP=0` is the off arm.
// (2) IRREDUNDANT CLAUSES ONLY. Learned clauses are entailed, so using them
//     would be sound today, but `Reduce` may delete one between detection and
//     emission, and a chain citing a deleted id is a rejected proof. kissat
//     also connects irredundant only.
// (3) NO `kitten_flip_literal` PRE-FILTER (kissat sweep.c:1152). It refines the
//     partition without a solve by flipping one literal of the current model;
//     it is a pure cost optimization over the same partition, and it needs a
//     "can this literal be flipped in the current model" query the sub-solver
//     does not expose. Revisit if the solve count dominates.
// (4) NO BACKBONE HALF. kissat's sweeper extracts root units from the same
//     environments (`sweep_backbone_candidate`). Ours would need its own proof
//     delta (add-RUP unit) and Probe/Backbone already own that delta; the hook
//     exists (`Stats::sweep_env_unsat` counts environments refuted outright)
//     but nothing derives from it.
// (5) SEED-ANCHORED CANDIDATES, not kissat's full partition stack. We refine
//     "which literals still agree with the SEED in every model seen" rather
//     than a general partition over all environment literals, so one
//     environment proves equivalences to its seed only. Same soundness, less
//     yield per environment — this and (3) are the measured remainder of the
//     yield gap once the environment's cost is fixed (`SP-SWEEP-7`).
//
// (6) THE ROUND CEILING TRUNCATES THE ENVIRONMENT SCAN (`SweepBudget`). kissat
//     and cadical both bound the environment by ticks as well as by size;
//     cadical charges the occurrence walk to the same budget as sub-solving and
//     polls it inside the expansion loop. A truncated environment is smaller,
//     never wrong — the same weakening the size caps already make.
//
// EFFORT. Every scan and every sub-solver tick is charged to
// `Stats::sweep_ticks` — the currency `transitive_ticks`/`bce_ticks`/
// `ternary_ticks` use. The rule CHARGES, the policy BUDGETS (plan §5.2). The
// ceiling is enforced INSIDE `sweep_env_build`, not only between seeds: the
// size caps bound what an environment holds, not the scan that finds it, so
// without an in-scan poll one seed can outspend the whole round dose.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/sweep/subsolver.hpp"

#include <vector>

namespace tactix {

// "How hard to sweep" is a POLICY decision (plan §5), so the limits are
// a caller-owned parameter rather than rule state or a Kernel field. Defaults
// are kissat's option values scaled to one environment (sweepdepth 2,
// sweepvars 256, sweepclauses 1024); `env_ticks` is in the sub-solver's own
// currency (watch entries inspected), not the kernel's.
struct SweepLimits {
  uint32_t max_depth = 2;         // BFS layers around the seed (kissat sweepdepth)
  uint32_t max_env_vars = 256;    // kissat sweepvars
  uint32_t max_env_clauses = 1024; // kissat sweepclauses
  uint32_t max_candidates = 32;   // candidate PAIRS solved per environment
  uint64_t env_ticks = 200000;    // sub-solver budget per environment (cumulative)
  uint32_t max_seeds = 0;         // seeds per round; 0 = every schedulable var
  uint64_t round_ticks = 0;       // whole-round tick ceiling; 0 = unbounded
  // Enforce `round_ticks` INSIDE environment construction, not only between
  // seeds (see SweepBudget). Ships on; `TACTIX_SWEEP_ENV_BUDGET=0` reverts to
  // the between-seeds-only check for an A/B arm.
  bool enforce_in_env = true;
  // Remove root-FALSE literals from environment clauses (kissat sweep.c:295).
  // Ships on; `TACTIX_SWEEP_STRIP=0` reverts to keeping them, which is what the
  // rule did through S-a…S-f and why it confirmed almost nothing.
  bool strip_false = true;
};

// THE ROUND CEILING IS ENFORCED INSIDE ENVIRONMENT CONSTRUCTION, not only
// between seeds. `max_env_vars`/`max_env_clauses` bound what an environment
// HOLDS, not what it costs to find: once the variable cap is reached the BFS
// keeps walking every remaining occurrence of every environment literal, and
// on a formula with long occurrence lists one seed can outspend the whole
// round dose (measured 41x). Declining to expand is always sound — the
// environment only ever shrinks, and it stays a subset of F — so the ceiling
// is a plain early exit, the discipline cadical uses (`kitten_ticks_limit_hit`
// polled inside the expansion loop, and occurrence walking charged to the same
// budget as sub-solving).
struct SweepBudget {
  uint64_t start = 0; // `Stats::sweep_ticks` when the round began
  uint64_t limit = 0; // round ceiling in that currency; 0 = unbounded
  bool over(const Kernel &k, uint64_t pending) const;
};

constexpr uint32_t SWEEP_NO_SUB_VAR = UINT32_MAX;

// The local clause set around one seed, encoded for the sub-solver, plus the
// two mappings the rest of V5 needs: sub var → kernel var, and environment
// clause id → main-formula proof id (the core lookup at emission).
struct SweepEnv {
  SubSolver solver;
  std::vector<Var> vars;            // sub var -> kernel var
  std::vector<uint32_t> sub_of_var; // kernel var -> sub var (SWEEP_NO_SUB_VAR)
  std::vector<Var> touched;         // sub_of_var restore list (see reset)
  std::vector<uint32_t> depth;      // by sub var: BFS layer (fixed vars: UINT32_MAX)
  // clause_ids[i] is the main-formula proof id of environment clause i+1
  // (the sub-solver hands out ids from 1 in add order). 0 = the kernel holds
  // the clause but the proof has no id for it, which only happens in modes
  // that do not track ids; emission must decline such a core, never cite 0.
  std::vector<uint64_t> clause_ids;
  // Open-addressed set of the clause keys already encoded: "have I encoded
  // this clause?" in O(1). A LINEAR scan per clause CONSIDERED makes
  // environment construction quadratic in the environment, and that scan — not
  // the sub-solver — is what consumed the round's effort ceiling. UINT64_MAX is
  // the empty slot: no key can be all-ones (KEY_LARGE and KEY_UNIT are never
  // both set, and a binary key's top 32 bits hold a literal, not a tag).
  std::vector<uint64_t> key_slots;
  uint64_t key_mask = 0;
  std::vector<Lit> clause_lits;     // flat kernel literals
  std::vector<uint32_t> clause_off; // start of clause i in clause_lits (+end)
  // THE STRIP LEDGER. Environment clause i is the main-formula clause
  // `clause_ids[i]` with its root-FALSE literals removed; `strip_ids[strip_off[i]
  // .. strip_off[i+1])` are the proof ids of the root units that justify each
  // removal. An LRAT chain citing clause i must cite those units too, which is
  // why they are carried here rather than re-derived at emission (by then the
  // environment is gone). Empty for every clause that had nothing to strip.
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

// One direction's refutation, CAPTURED AT DETECTION TIME. The sub-solver keeps
// only the last solve's derivation and an environment does not outlive its
// seed, so a refutation that is not copied out here is gone by emission time.
// `steps` stays in sub-solver space; `orig_ids` and `vars` are the translation
// tables (sub clause id -> main proof id, sub var -> kernel var) captured with
// it. Bounded by SWEEP_MAX_PROOF_STEPS so a pathological refutation cannot make
// detection allocate without limit.
constexpr uint32_t SWEEP_MAX_PROOF_STEPS = 4096;
struct SweepDirProof {
  // The binary THIS direction proves, in kernel literals. Recorded explicitly
  // because `SweepEquiv` normalises its pair to {min,max}: when the seed is the
  // larger literal, deriving the targets from that normalised pair silently
  // swaps the two directions and every translation then fails its target check.
  Lit t0 = INVALID_LIT, t1 = INVALID_LIT;
  std::vector<SubStep> steps;
  std::vector<uint64_t> orig_ids;
  // the strip ledger of the environment this refutation was found in, captured
  // with `orig_ids` for the same reason: an antecedent's chain is incomplete
  // without the units that justify its stripped literals
  std::vector<uint64_t> strip_ids;
  std::vector<uint32_t> strip_off;
  std::vector<Var> vars;
  bool ok = false;
};
struct SweepPairProof {
  SweepDirProof fwd, rev;
};

// A proved equivalence `a ≡ b` over KERNEL literals, canonical with a < b.
// Emission turns one of these into the two binaries (¬a ∨ b), (a ∨ ¬b) plus
// their chains.
struct SweepEquiv {
  Lit a = INVALID_LIT;
  Lit b = INVALID_LIT;
};

// Round-local scratch, reusable across rounds so a sweeping policy pays no
// allocation per environment (the per-call linear-scratch hazard: `sub_of_var`
// and `repr` are O(nvars) and must be initialized ONCE, then restored by
// touched-list).
struct SweepRound {
  SweepEnv env;
  std::vector<std::vector<Ref>> occ_owned; // built only when k.dense is unusable
  const std::vector<std::vector<Ref>> *occ = nullptr; // by literal
  std::vector<Lit> repr;                   // union-find over literals
  std::vector<SweepEquiv> found;
  std::vector<SweepPairProof> proofs; // parallel to `found`
  std::vector<Var> seeds;
  // candidate literals still agreeing with the seed in every model seen
  std::vector<Lit> candidates;
  std::vector<SubLit> sub_scratch, assumptions;
  // The round's effort ceiling, so `sweep_env_build` can stop mid-scan. Left
  // at its default (unbounded) by every caller that builds one environment by
  // hand — tests, the API, the probe.
  SweepBudget budget;
};

// Guard: quiescent root, consistent. A pure precondition — the environment
// cites level-0 unit ids and reads the watch lists, and both mean what they
// say only at a drained root (the same argument every inprocessing rule makes).
bool sweep_guard(const Kernel &);

// Canonical seed order: active, unassigned variables with at least one
// occurrence, ascending. This is the ACTION SURFACE `instances()` exposes
// (payload = the seed literal), so it deliberately ranks rather than filters —
// no "unlikely to pay off" omissions (rule-dev §instances).
void sweep_seeds(const Kernel &, std::vector<Var> &out);

// Start a round: prepare the occurrence index over irredundant large clauses
// (`Kernel::dense` when a simplification phase has connected it — identical
// contents — else a round-local one built with a single arena pass) and reset
// the representative map. Must precede any `sweep_detect_seed`.
void sweep_round_begin(Kernel &, SweepRound &);

// Build the environment around `seed`. False = nothing usable (no clauses, or
// the seed is fixed/retired). Charges its scan to `Stats::sweep_ticks`.
bool sweep_env_build(Kernel &, Var seed, const SweepLimits &, SweepRound &);

// One seed: build the environment, refute both directions of each surviving
// candidate pair, append what is proved to `round.found`. Returns the number of
// equivalences found. DETECTION ONLY: changes nothing in `F/R/τ/ρ`.
uint32_t sweep_detect_seed(Kernel &, Var seed, const SweepLimits &,
                           SweepRound &);

// A full round over `sweep_seeds` order. Returns the number of equivalences
// found (also in `round.found`).
uint32_t sweep_detect_round(Kernel &, const SweepLimits &, SweepRound &);

// The canonical full apply: detect, then emit each equivalence that is RUP
// over the main formula as two binaries (declining the rest). Exposed for
// tests as well as for the registry entry.
ApplyResult sweep_apply(Kernel &);

// Read the SweepLimits overrides from the environment (TACTIX_SWEEP_DEPTH /
// _VARS / _CLAUSES / _CANDIDATES / _TICKS / _SEEDS / _ROUND_TICKS). Lives here
// rather than in the policy so the probe hook and the tests configure the
// detector the same way.
SweepLimits sweep_limits_from_env();

} // namespace tactix
