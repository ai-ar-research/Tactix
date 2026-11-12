// Sweep (V5): prove `x ≡ y` by a budgeted refutation over a local clause set
// around a seed, then add the two equivalence binaries with a checkable chain
// (RUP fast path, else a core→chain path). Contract: `docs/dev/calculus.md` V5;
// design `docs/design/v5-sweep-design.md`; deviations from kissat `sweep.c` and
// cadical `sweep.cpp`: parity-ledger §13.2. Invariant: the environment holds
// kernel clauses (minus root-false literals), so `env ⊨ x ≡ y` gives
// `F ⊨ x ≡ y`.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include "strategy/sweep_parameters.hpp"
#include "engine/subsolver.hpp"

#include <memory>
#include <deque>
#include <ranges>
#include <span>
#include <vector>

namespace tactix {

// Frozen source alias; the owned type lives with other strategy parameters.
using SweepLimits = strategy::SweepParameters;

// Policy-priced effort: embedded plus representation work. The live semantic
// dose follows Kissat's Kitten-only limit.
inline uint64_t sweep_effort(const Kernel &k) {
  // Includes the variable-width representation bridge (once per dense call, per
  // productive environment otherwise).
  return k.stats.sweep_ticks_sub + k.stats.sweep_ticks_repr;
}

inline uint64_t sweep_semantic_effort(const Kernel &k) {
  return k.stats.sweep_ticks_sub;
}

constexpr uint32_t SWEEP_NO_SUB_VAR = UINT32_MAX;

// Shared replay payloads; an ID hit also compares the payload. deque keeps
// older versions stable.
struct SweepProofSteps {
  std::deque<SubStep> steps;
  std::vector<const SubStep *> by_id;
  const SubStep *retain(SubStep &&);
};

// The local clause set of one seed, with sub var → kernel var and environment
// clause id → main proof id.
struct SweepEnv {
  SubSolver solver;
  std::shared_ptr<SweepProofSteps> proof_steps;
  // Formula epoch after the build; captured proofs are usable only at it.
  uint64_t formula_epoch = 0;
  std::vector<Var> vars;            // sub var -> kernel var
  std::vector<uint32_t> sub_of_var; // kernel var -> sub var (SWEEP_NO_SUB_VAR)
  std::vector<Var> touched;         // sub_of_var restore list (see reset)
  std::vector<uint32_t> depth;      // by sub var: BFS layer (fixed vars: UINT32_MAX)
  // Main proof id of environment clause i+1; 0 means none, so emission
  // declines.
  std::vector<uint64_t> clause_ids;
  // Open-addressed set of encoded keys; UINT64_MAX (no valid key) is empty.
  std::vector<uint64_t> key_slots;
  uint64_t key_mask = 0;
  std::vector<Lit> clause_lits;     // flat kernel literals
  std::vector<uint32_t> clause_off; // start of clause i in clause_lits (+end)
  // per-clause scratch, kept here so the BFS pays no allocation per clause
  std::vector<Lit> kept_scratch;
  std::vector<uint32_t> deg_scratch; // by sub var: large-clause degree in env

  uint32_t nclauses() const {
    return static_cast<uint32_t>(clause_ids.size());
  }
};

// One direction's refutation captured at detection, in sub-solver space with
// its translation tables. SweepLimits controls proof-resource admission.
struct SweepDirProof {
  // The binary this direction proves; `SweepEquiv` normalises to {min,max}.
  Lit t0 = INVALID_LIT, t1 = INVALID_LIT;
  std::vector<SubStep> steps;
  // Shared immutable derivation for units at different prefix lengths.
  std::shared_ptr<const SweepProofSteps> program;
  std::shared_ptr<const std::vector<const SubStep *>> program_steps;
  size_t shared_step_count = 0;
  auto steps_view() const {
    const size_t count = program_steps ? shared_step_count : steps.size();
    return std::views::iota(size_t{0}, count) |
        std::views::transform([this](size_t i) -> const SubStep & {
          return program_steps ? *(*program_steps)[i] : steps[i];
        });
  }
  // Saved at the query boundary; original eligibility does not change while
  // a later core commits units.
  std::vector<SubDerivation::CoreClause> core_clauses;
  std::vector<uint64_t> orig_ids;
  std::vector<Var> vars;
  uint64_t formula_epoch = 0;
  bool ok = false;
};
// False means optional capture allocation failed; older captures remain valid.
bool sweep_share_steps(SubDerivation &, std::shared_ptr<SweepProofSteps> &,
                       SweepDirProof &) noexcept;

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
  FlatLists<Ref> occ_owned; // built only when k.dense is unusable
  const OccIndex *connected_occ = nullptr;
  template <class Visit> decltype(auto) with_large(Lit lit, Visit visit) const {
    if (connected_occ)
      return connected_occ->with_large(lit, visit);
    return visit((*occ)[lit]);
  }
  size_t large_size(Lit lit) const {
    return connected_occ ? connected_occ->large_size(lit) : (*occ)[lit].size();
  }
  FlatLists<Ref> *occ = nullptr; // by literal
  // `Kernel::arena_generation` when `occ_owned` was built.
  uint64_t occ_generation = 0;
  std::vector<Lit> repr;                   // union-find over literals
  std::vector<SweepEquiv> found;
  std::vector<SweepPairProof> proofs; // parallel to `found`
  std::vector<SweepUnitProof> units;
  // Emission cursors; `emission_batches` is monotone within a round.
  size_t units_emitted = 0;
  size_t equivalences_emitted = 0;
  uint64_t emission_batches = 0;
  std::vector<Var> seeds;
  // candidate literals still agreeing with the seed in every model seen
  std::vector<Lit> candidates;
  std::vector<Lit> backbone;
  std::vector<std::vector<Lit>> partition;
  std::vector<SubLit> sub_scratch, assumptions;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  // The seed being swept and its next embedded-query ordinal.
  Var trace_seed = INVALID_VAR;
  uint32_t trace_query_ordinal = 0;
#endif
};

// Guard: quiescent root, consistent.
RuleEligibility sweep_guard(const Kernel &);

// Canonical seed order: active unassigned variables by ascending degree; ranks,
// never filters (N1).
void sweep_seeds(const Kernel &, std::vector<Var> &out);

// Start a round: occurrence index (`Kernel::dense` or round-local) and a reset
// representative map. Precedes `sweep_detect_seed`.
void sweep_round_begin(Kernel &, SweepRound &);

// Build the environment around `seed`. False = nothing usable (no clauses, or
// the seed is fixed/retired). Charges its scan to `Stats::sweep_ticks`.
bool sweep_env_build(Kernel &, Var seed, const SweepLimits &, SweepRound &);

// One seed: build the environment, refute both directions of each candidate,
// append proofs to `round.found`. Returns the count; changes nothing in
// F/R/τ/ρ.
uint32_t sweep_detect_seed(Kernel &, Var seed, const SweepLimits &,
                           SweepRound &);

// A full round over `sweep_seeds` order. Returns the number of equivalences
// found (also in `round.found`).
uint32_t sweep_detect_round(Kernel &, const SweepLimits &, SweepRound &);

// One seed end to end: detect around it and emit what is proved. The
// per-instance half of the N1 surface; `INSTANCE_CANONICAL` runs the round.
ApplyResult sweep_try(Kernel &, Var seed);
ApplyResult sweep_try_with_limits(Kernel &, Var seed, const SweepLimits &);

// The canonical full apply: detect, then emit each justifiable equivalence as
// two binaries, declining the rest.
ApplyResult sweep_apply(Kernel &);

// The full apply under a typed profile, used exactly: strategy settings are
// never read or amended inside the rule.
ApplyResult sweep_apply_with_limits(Kernel &, const SweepLimits &);

// Re-pose the unconsumed retirement suffix in Kissat's Substitute order.
void sweep_order_retirement_suffix(Kernel &, size_t retired_begin,
                                   size_t substituted_begin);

} // namespace tactix
