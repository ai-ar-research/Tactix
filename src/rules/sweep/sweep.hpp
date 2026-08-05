// V5 Sweep · increment S-c: CANDIDATE SELECTION + ENVIRONMENT CONSTRUCTION.
// (design `docs/design/v5-sweep-design.md` §6.3 — "NoOp on every proof path;
// measurable: does it find equivalences at all, and how many?")
//
// This is the DETECTION half of Sweep. It answers one question and changes
// nothing: which literal pairs `x ≡ y` does a budgeted refutation over a LOCAL
// clause set prove? The deriving half (emit the two binaries with a checkable
// chain) is S-d (the RUP fast path) and S-e (the general core→chain path).
//
// WHY THERE IS NO `TACTIX_REGISTER_RULE` LINE HERE YET. A registered rule whose
// `apply` can only ever return `NoOp` would (a) put an inert move into N1's
// action space — the surface `RandomPolicy` enumerates and the phase-6 learner
// reasons over — and (b) be ON by default in the S4 arrangement (every
// Inprocess-tier entry is enabled), i.e. an unmeasured cost for zero yield.
// It would also be the exact shape of the open `Factor` N1 breach: guard true,
// apply NoOp. The registry line, the `docs/calculus.md` entry and the default-on
// decision all land together with the first PROOF-CARRYING application (S-d).
// Until then the detector is reachable through the policy's TACTIX_SWEEP_PROBE
// hook (default_policy.cpp), which is what makes the counters below — and the
// fuzz legs over them — non-vacuous.
//
// THE SOUNDNESS ARGUMENT, in one line: the environment is a SUBSET OF F, so
// `env ⊨ x ≡ y` implies `F ⊨ x ≡ y`. Everything in `sweep_env_build` exists to
// keep that literally true — every environment clause is a clause the kernel
// (and the checker) currently holds, cited by its own proof id:
//   - large: irredundant, live, not root-satisfied; provenance = `Clause::id()`
//   - binary: live in the watch lists; provenance = `kernel_binary_id`
//   - unit:  a root-fixed variable whose `unit_ids[v] != 0`, i.e. the unit
//            clause is really IN the proof (a variable fixed by root
//            propagation has no unit clause of its own — citing one would build
//            a chain naming a clause the checker never saw)
// Nothing is strengthened, shrunk, or invented on the way in. S-e maps a core
// entry back to a main-formula clause by indexing `SweepEnv::clause_ids`.
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
//  - kitten's core is deliberately NOT our interface — S-b already gives the
//    stronger thing (an ordered LRAT derivation), see subsolver.hpp.
//
// Deviations from kissat, and why (the ledger entries; each is a WEAKENING of
// what we detect, never of what we may conclude):
// (1) ROOT-FALSE LITERALS ARE KEPT, NOT STRIPPED (kissat sweep.c:295 drops
//     them). Stripping produces a clause that is entailed but is not a member
//     of F, so its core entry would have no id to cite; kissat can afford it
//     because kitten's core is re-justified on the way out. We instead add the
//     variable's UNIT clause when the proof holds one, which gives the
//     sub-solver the same information with an id attached. When it does not
//     (root propagation, `unit_ids[v] == 0`), the literal simply stays free:
//     a weaker environment, never a wrong one. Closing it needs the unit's
//     reason cone (`kernel_reason_walk`) threaded into the chain — S-e's call.
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
//     delta (add-RUP unit) and V1 Probe/V2 Backbone already own that delta; the
//     hook exists (`SweepStats::env_unsat` counts environments refuted outright)
//     but deriving from it is S-d work.
// (5) SEED-ANCHORED CANDIDATES, not kissat's full partition stack. We refine
//     "which literals still agree with the SEED in every model seen" rather
//     than a general partition over all environment literals, so one
//     environment proves equivalences to its seed only. Same soundness, less
//     yield per environment; the general partition is a later dosing question.
//
// EFFORT. Every scan and every sub-solver tick is charged to
// `Stats::sweep_ticks` — the currency `transitive_ticks`/`bce_ticks`/
// `ternary_ticks` use — so a policy can dose Sweep the moment S-d gives it
// something to schedule (rule charges, policy budgets; plan §5.2 rev 17).
#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/sweep/subsolver.hpp"

#include <vector>

namespace tactix {

// "How hard to sweep" is a POLICY decision (plan §5 rev 17), so the limits are
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
};

constexpr uint32_t SWEEP_NO_SUB_VAR = UINT32_MAX;

// The local clause set around one seed, encoded for the sub-solver, plus the
// two mappings the rest of V5 needs: sub var → kernel var, and environment
// clause id → main-formula proof id (S-e's core lookup).
struct SweepEnv {
  SubSolver solver;
  std::vector<Var> vars;            // sub var -> kernel var
  std::vector<uint32_t> sub_of_var; // kernel var -> sub var (SWEEP_NO_SUB_VAR)
  std::vector<Var> touched;         // sub_of_var restore list (see reset)
  std::vector<uint32_t> depth;      // by sub var: BFS layer (fixed vars: UINT32_MAX)
  // clause_ids[i] is the main-formula proof id of environment clause i+1
  // (the sub-solver hands out ids from 1 in add order). 0 = the kernel holds
  // the clause but the proof has no id for it, which only happens in modes
  // that do not track ids; S-e must decline such a core rather than cite 0.
  std::vector<uint64_t> clause_ids;
  std::vector<uint64_t> keys;       // dedup keys, parallel to clause_ids
  std::vector<Lit> clause_lits;     // flat kernel literals
  std::vector<uint32_t> clause_off; // start of clause i in clause_lits (+end)

  uint32_t nclauses() const {
    return static_cast<uint32_t>(clause_ids.size());
  }
};

// A proved equivalence `a ≡ b` over KERNEL literals, canonical with a < b.
// S-d/S-e turn one of these into the two binaries (¬a ∨ b), (a ∨ ¬b) plus their
// chains; S-c only counts them.
// S-e: one direction's refutation, CAPTURED AT DETECTION TIME. The sub-solver
// keeps only the last solve's derivation and an environment does not outlive its
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
  std::vector<Var> vars;
  bool ok = false;
};
struct SweepPairProof {
  SweepDirProof fwd, rev;
};

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
  std::vector<SweepPairProof> proofs; // S-e, parallel to `found`
  std::vector<Var> seeds;
  // candidate literals still agreeing with the seed in every model seen
  std::vector<Lit> candidates;
  std::vector<SubLit> sub_scratch, assumptions;
};

// The (future) guard: quiescent root, consistent. A pure precondition — the
// environment cites level-0 unit ids and reads the watch lists, and both mean
// what they say only at a drained root (the C1/C2/C3/V1/V2/V3/V4 argument).
bool sweep_guard(const Kernel &);

// Canonical seed order: active, unassigned variables with at least one
// occurrence, ascending. This is the ACTION SURFACE S-d will expose as
// `instances()` (payload = the seed literal), so it deliberately ranks rather
// than filters — no "unlikely to pay off" omissions (rule-dev §instances).
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
// equivalences found. Changes NOTHING in `F/R/τ/ρ` — S-c is detection only.
uint32_t sweep_detect_seed(Kernel &, Var seed, const SweepLimits &,
                           SweepRound &);

// A full round over `sweep_seeds` order. Returns the number of equivalences
// found (also in `round.found`).
uint32_t sweep_detect_round(Kernel &, const SweepLimits &, SweepRound &);

// S-d: the canonical full apply -- detect, then emit each equivalence that is
// RUP over the main formula as two binaries (declining the rest). Exposed for
// tests as well as the registry entry.
ApplyResult sweep_apply(Kernel &);

// Read the SweepLimits overrides from the environment (TACTIX_SWEEP_DEPTH /
// _VARS / _CLAUSES / _CANDIDATES / _TICKS / _SEEDS / _ROUND_TICKS). Lives here
// rather than in the policy so the probe hook and the tests configure the
// detector the same way.
SweepLimits sweep_limits_from_env();

} // namespace tactix
