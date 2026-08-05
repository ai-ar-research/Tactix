// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"

#include <csignal>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

class Proof;
struct FactorMatchPolicy;
// Search-guidance state `H` (VMTF queue, EVSIDS heap, phases). Held OPAQUELY by
// Kernel (only a pointer below) so kernel.hpp never depends on its layout; the
// full struct + operations live in policies/heuristics.hpp (Track H · H4).
struct Heuristics;

// per-variable assignment metadata (kissat/src/assign.h)
struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
};

// VMTF queue links (kissat/src/queue.h)
struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  // Share of `propagations` spent inside simplification phases — POLICY-
  // written bookkeeping (accumulated at phase exit, mirroring the policy's
  // inprocess-ticks split), never read by kernel or guards. Splitting the
  // headline props/s into search vs inprocessing is TB5's first step: the
  // 1.39× throughput gap vs kissat is only comparable on the search share
  // (kissat keeps search_ticks and probing_ticks apart).
  uint64_t inprocess_propagations = 0;
  // --- wall-clock accounting (DIAGNOSTIC ONLY) ------------------------------
  // Same contract as inprocess_propagations above: written by the rule-dispatch
  // wrapper and the policy, NEVER read by the kernel, a guard, or any decision
  // — so no trajectory can depend on a clock and runs stay bit-reproducible.
  // Only the statically-dispatched search path (Propagate/Decide/Analyze) is
  // deliberately NOT timed: it runs per-conflict, so a clock call there would
  // be the very hot-path cost N3 cannot afford. Search time is therefore
  // DERIVED as solve_nanos − (everything measured here), which is exactly the
  // quantity worth reporting anyway (see `c timing` in main.cpp).
  static constexpr uint32_t MAX_TIMED_RULES = 64;
  uint64_t rule_nanos[MAX_TIMED_RULES] = {}; // per registry rule id
  uint64_t rule_calls[MAX_TIMED_RULES] = {};
  uint64_t inprocess_nanos = 0; // whole slot, incl. scheduling not in a rule
  uint64_t solve_nanos = 0;     // wall time of solve(), set by the frontend
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t reordered = 0;    // Reorder applications (S7, policy op)
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  // S6 Walk (policy op): applications, literal flips, and the rule's own
  // effort currency (occurrence-list entries scanned = kissat walk_steps).
  uint64_t walks = 0;
  uint64_t walk_flips = 0;
  uint64_t walk_steps = 0;
  uint64_t walk_improved = 0; // walks that lowered the unsatisfied count
  // unsatisfied-clause count the last walk reached (UINT64_MAX = never walked).
  // Read by the policy to decide whether to KEEP a walk's phases (rev 17: the
  // op owns the mechanism, the policy owns the accept/reject decision).
  uint64_t walk_minimum = UINT64_MAX;
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  // Clause-QUALITY instrumentation (2026-07-28 probe): the size/glue
  // distribution of what we learn, which the conflict-count gap vs kissat
  // (FmlaEquiv 1.53x) is a symptom of. Counters only — no trajectory effect.
  uint64_t learned_lits = 0;      // sum of FINAL learned clause sizes
  uint64_t learned_glue_sum = 0;  // sum of learned glues
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  // glue histogram of clauses USED in conflict analysis, per search mode
  // (kissat statistics.h used[2].glue): the input to Reduce's dynamic tier
  // quantiles. Always maintained; read only when the arm is on.
  static constexpr uint32_t MAX_GLUE_USED = 127;
  uint64_t used_glue[2][MAX_GLUE_USED + 1] = {};
  uint64_t retiered = 0;           // dynamic tier recomputations
  uint64_t reason_bumped = 0;      // extra vars marked by reason-side bumping
  uint64_t reason_bump_rounds = 0; // conflicts the reason-side pass ran on
  uint64_t reason_bump_aborted = 0; // rounds rolled back at the mark limit
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  // Decisions that landed on an E2 Factor EXTENSION variable. Diagnostic for
  // task #11: cadical's `factorunbump` (kissat's
  // adjust_scores_and_phases_of_fresh_varaibles) is worth 10x on jgiraldezlevy,
  // so "are we branching on extension vars?" must be answerable from a normal
  // run rather than inferred. Counts vars >= first_extension_var.
  uint64_t decisions_extension = 0;
  // Decisions landing on a variable V3 Substitute has already rewritten away.
  // Such a variable occurs in NO clause, so deciding it propagates nothing and
  // only deepens the trail — pure wasted depth. Measured on veer_axi 2026-08-01:
  // 80,124 decisions per conflict against kissat's 147, at 1.13 propagations
  // per decision.
  uint64_t decisions_substituted = 0;
  // Task #11 increment 3: WHICH CHANNEL re-promotes extension variables.
  // cadical branches on its ext vars 0.10% of decisions where we do 31.4%
  // (probe 1577499) while burying them at birth exactly as we do, so something
  // keeps lifting ours back out. The two candidates are separable at the bump
  // site: conflict-side (SEEN — the var was resolution-visited or sits in the
  // learned clause) vs reason-side only (REASON_BUMP without SEEN — the
  // bumpreasons extension, where cadical's rate is 100, kissat's 10, ours 20).
  // A high reason-side share is a MECHANISM gap with a targeted fix; a high
  // conflict-side share means our factorings are structurally more active and
  // the lever is WHICH factorings we choose.
  // §6 (task #10): how many rule applications ABORTED on the interrupt.
  // Reachability evidence — a latency claim is unfalsifiable without it
  // ([[ab-arm-identical-artefacts]]: prove the path was taken).
  mutable uint64_t rule_interrupted = 0;
  // Times kernel_terminating() ITSELF reported stop under the stress knob.
  // Counted at the source, so it is independent of whether any particular
  // abort site remembers to increment — the reachability instrument.
  mutable uint64_t stop_fuzz_fires = 0;
  uint64_t bumps = 0;                // bump events (both modes, both channels)
  uint64_t bumps_extension = 0;      // ...of which landed on an extension var
  uint64_t bumps_ext_reason_only = 0; // ...of those, reason-side ONLY
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary; V4)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds (C1)
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked (C1)
  // SCHED-B4: PARTNER VISITS — cadical's `stats.subchecks` (subsume.cpp:86),
  // bumped once per candidate partner actually compared, which is the unit its
  // budget is denominated in (`check_limit = stats.subchecks + delta`,
  // subsume.cpp:362/459). `*_checked` counts CLAUSES, i.e. 1 for a clause whose
  // occurrence lists hold 10,000 partners and 1 for a clause holding 2 — so it
  // under-prices exactly the expensive candidates, which is how C2 spent 97.8 s
  // of DivS_862_11 inside FOUR "rounds" without exceeding its budget.
  uint64_t forward_visits = 0;
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds (C2)
  uint64_t strengthen_checked = 0; // Strengthen candidate checks (C2)
  uint64_t strengthen_visits = 0;  // SCHED-B4, see forward_visits
  uint64_t strengthened = 0;       // clauses strengthened (C2 add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by C2's fused round
  // Rounds cut short by `Kernel::work_budget` (ledger D-STRENGTHEN-BUDGET).
  // REACHABILITY WITNESS, not decoration: a `break` that never executes makes
  // every green fuzz leg vacuous for this change, which is exactly how the
  // identical-artefacts trap works. Assert this is NONZERO in any run that
  // claims to have exercised the mid-round budget.
  uint64_t budget_stops_c1 = 0; // SubsumeForward rounds stopped mid-round
  uint64_t budget_stops_c2 = 0; // Strengthen rounds stopped mid-round
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified
                        // literal strips by C2's flush pre-pass
  uint64_t vivify_rounds = 0;  // Vivify rounds (C3)
  uint64_t vivify_checked = 0; // Vivify candidates probed (C3)
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
  uint64_t probe_rounds = 0;   // Probe rounds (V1)
  uint64_t probed = 0;         // probe assumptions made (V1)
  uint64_t probe_failed = 0;   // failed literals -> root units derived (V1)
  uint64_t backbone_rounds = 0;  // Backbone rounds (V2)
  uint64_t backbone_probed = 0;  // binary probe assumptions made (V2)
  uint64_t backbone_failed = 0;  // failed literals -> root units derived (V2)
  uint64_t transitive_rounds = 0;  // TransitiveReduce rounds (V4)
  uint64_t transitive_checked = 0; // candidate edges walked (V4)
  uint64_t bce_rounds = 0;         // X1 rounds that deleted something
  uint64_t bce_deleted = 0;        // blocked clauses deleted (witnessed)
  uint64_t probe_ticks = 0;        // V1's own propagation ticks (SCHED-B2)
  uint64_t backbone_ticks = 0;     // V2's own (SCHED-B2)
  uint64_t vivify_ticks = 0;       // C3's own (SCHED-B2)
  uint64_t instantiate_ticks = 0;  // X4's own (SCHED-B2)
  uint64_t bce_ticks = 0;          // occurrences + pairs scanned — X1's budget
                                   // currency for a policy (deviation 3)
  uint64_t instantiate_rounds = 0;  // X4 rounds that strengthened something
  uint64_t instantiate_checked = 0; // (clause, literal) pairs probed
  uint64_t instantiated = 0;        // literals removed by X4

  // V5 Sweep · S-c (DETECTION ONLY — nothing is derived from these yet, so
  // every counter here is a pure observation; see src/rules/sweep/sweep.hpp).
  // They are the increment's deliverable: "does sweeping find equivalences at
  // all, and how many?" (design §6.3), and the reachability witness for its
  // fuzz legs — a zero `sweep_environments` means the leg never entered the
  // detector ([[ab-arm-identical-artefacts]]).
  uint64_t sweep_rounds = 0;       // detection rounds run
  uint64_t sweep_environments = 0; // environments built AND handed to the solver
  uint64_t sweep_env_clauses = 0;  // clauses encoded into them
  uint64_t sweep_solves = 0;       // sub-solver solve() calls
  uint64_t sweep_candidates = 0;   // candidate-pair directions actually solved
  uint64_t sweep_equivalences = 0; // pairs PROVED equivalent (S-d/S-e emit these)
  uint64_t sweep_binaries = 0;     // S-d: equivalence binaries ADDED with a proof
  uint64_t sweep_not_rup = 0;      // S-d: equivalences declined (not RUP) -> S-e
  uint64_t sweep_known = 0;        // pairs the formula already states as binaries
  uint64_t sweep_env_unsat = 0;    // environments refuted outright (F is UNSAT)
  uint64_t sweep_budget = 0;       // solves abandoned on the tick budget
  uint64_t sweep_ticks = 0;        // V5's effort currency (scan + sub-solver)

  uint64_t ternary_rounds = 0;     // X2 rounds that added something
  uint64_t ternary_resolvents = 0; // ternary resolvents added (redundant)
  uint64_t ternary_binaries = 0;   // binary resolvents added (permanent)
  uint64_t ternary_ticks = 0;      // occurrence entries + pairs examined — X2's
                                   // budget currency for a policy (deviation 2)
  uint64_t transitive_ticks = 0;   // cache lines scanned by V4's walks — the
                                   // policy's budget currency (kissat
                                   // transitive_ticks; same 16-words/line
                                   // unit as stats.ticks)
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_failed = 0;  // sources abandoned to Probe as failed (V4)
  uint64_t substitute_rounds = 0;  // Substitute rounds (V3) — detection passes
  // Variables actually SUBSTITUTED away by V3, summed over applications. The
  // consumer side of `congruence_merges`: the 2026-07-30 diagnosis could not
  // tell whether V3 removes the 161,462 equivalences V6 finds on `veer_axi`.
  uint64_t substitute_vars = 0;
  // Discriminates "V3 never DETECTED an equivalence" from "V3 detected some and
  // the caller DROPPED them" — the open veer_axi question (V6 reports 179,110
  // merges, V3 substitutes 0). Non-trivial SCCs (size > 1) returned by
  // substitute_find_sccs, and the literals inside them, summed over rounds.
  // If both are 0 while congruence_merges is large, V6's equivalence binaries
  // are not in the binary implication graph by the time V3 walks it.
  uint64_t substitute_sccs = 0;
  uint64_t substitute_scc_lits = 0;
  // V3 rounds abandoned WHOLESALE by do_substitution's step-(A) hazard
  // pre-check (a binary pair with >=2 live copies cannot be deleted by name, so
  // the round is skipped rather than half-applied). The check is all-or-nothing
  // and its "rare" assumption is PER LITERAL, so the probability a round
  // survives falls as the marked set grows — exactly backwards, since the
  // instances with the most equivalences to exploit mark the most literals.
  uint64_t substitute_hazard_skips = 0;
  // Rounds that survived the hazard check with SOME classes poisoned, and the
  // literals those classes cost — the per-SCC skip's yield.
  uint64_t substitute_hazard_partial = 0;
  uint64_t substitute_hazard_lits = 0;
  // R-d reachability: deactivations actually performed, and how many of
  // those variables were later RE-INSERTED into a ranking structure (which
  // would silently undo the deactivation). Counted at the site itself, because
  // without a counter AT the site a reading of 0 cannot tell "never fired" from
  // "never measured" — task #10's abort paths passed 4 x 10,000 fuzz cases
  // while being unreachable on every case the harness generates.
  uint64_t deactivations = 0;
  uint64_t reactivations = 0;
  // Inprocessing PHASES run by the policy (each = a root descent + one dose per
  // enabled technique). Counted in the kernel so it survives into the stats
  // block: repeated during-search BVE's cost is search-TRAJECTORY damage, not
  // per-phase work (dosing with no admission at all costs +192.67 Tier C PAR-2
  // and three solves while BVE's own runtime stays ~0.1 %), so the open
  // question is whether that cost tracks the phase COUNT rather than the work
  // inside a phase — unanswerable without this counter.
  // Chronological backtracking (TB4). Both were absent for the first chrono
  // experiment, which is why a pathology that dropped conflicts outright was
  // invisible in a full Tier B run: `chronological` counts chrono targets taken,
  // `backjump_stale` counts pending clauses discarded because the asserting
  // literal was still assigned after the backtrack — the latter should be 0.
  // TB4b(A): conflicts resolved by REUSING the conflict as the driving clause
  // (kissat one_literal_on_conflict_level) — no clause learned, no proof line.
  uint64_t conflict_reused = 0;
  // Of those, the ones that needed the watch-repair primitive first — i.e. the
  // reuses the CRIPPLED version declined (TB4b). Reachability evidence for the
  // un-crippling: 0 here with conflict_reuse ON means the repair path never
  // executed and any A/B on it measured nothing ([[ab-arm-identical-artefacts]]).
  uint64_t conflict_watch_repaired = 0;
  uint64_t chronological = 0;
  uint64_t backjump_stale = 0;
  uint64_t inprocess_phases = 0;
  uint64_t congruence_rounds = 0;  // Congruence rounds (V6) — gate-closure passes
  // V6 merges dropped because no checkable LRAT chain could be built for them
  // (a bridging BIG path blocked by a root-assigned literal). Sound — the
  // inference is lost, never emitted unjustified. Expected 0; a nonzero count is
  // a real signal, not noise.
  uint64_t congruence_unjustified = 0;
  // Root units derived from degenerate gates (V6's unit channel, D-CONG-UNIT):
  // an XOR whose representative inputs all cancel forces its output.
  uint64_t congruence_units = 0;
  // Owed instrumentation, flagged by the 2026-07-30 Tier B timeout diagnosis:
  // `c congruence` printed neither gates nor merges, so on `veer_axi` whether
  // V3 Substitute actually consumes the 161,462 equivalences V6 finds was
  // UNANSWERABLE from a run. Gates EXTRACTED and equivalences MERGED, summed
  // over all applications.
  // Extraction split by gate kind, mirroring kissat's congruent_gates_ands /
  // _ites / _xors so the two are directly comparable. Motive (probe 1579464):
  // on T87 we extract **78,652,093 gates — 7.2 per clause** and convert 0.005 %
  // of them into merges, while kissat finds 3.3x MORE congruences in 12.5x less
  // time. Which extractor produces the 78 M is the whole question.
  mutable uint64_t congruence_gates_and = 0;
  mutable uint64_t congruence_gates_xor = 0;
  mutable uint64_t congruence_gates_ite = 0;
  uint64_t congruence_gates = 0;
  uint64_t congruence_merges = 0;
  uint64_t factorings = 0;
  // R5 diagnostic: multi-literal remainders OFFERED by the widened grid scope.
  // Zero here with factor_size >= 3 means the widened branch never fired at all,
  // which is a different bug from "fired but never won the delta contest".
  uint64_t factor_large_remainders = 0;         // Factor/BVA applications (E2) — grids replaced
  // Factor's unit of WORK and hence its budget currency (kissat
  // statistics.factor_ticks, cache-line granularity): 1 + one per 128 bytes
  // touched, charged per watch-list walk and per heap (re-)score. The
  // factoring count cannot express the seed-scan cost, which dominates.
  uint64_t factor_ticks = 0;
  uint64_t bve_rounds = 0;         // BVE canonical-apply calls (E1; a policy
                                   // that doses per instance never bumps this)
  uint64_t bve_eliminated = 0;     // variables eliminated by BVE resolution
  // resolvents attempted (kissat statistics.eliminate_resolutions): BVE's unit
  // of WORK, and so the currency an effort budget meters it in — one variable
  // costs occ(x)·occ(¬x) attempts, which the elimination count cannot express
  uint64_t bve_resolutions = 0;
};

// learned clause handed from Analyze to Backjump (τ changes are Backjump's)
struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
  uint64_t id = 0; // proof clause id (LRAT)
};

// Shared literal→large-clause occurrence index for a simplification PHASE
// (inprocessing-revamp increment 2, kissat dense.c analogue). Built once per
// phase (kernel_dense_connect), maintained incrementally — new irredundant
// large clauses push on add, deleted clauses are lazy (garbage bit, skipped on
// read), and kernel_collect_garbage rebuilds after it relocates Refs. Only
// irredundant large clauses (binaries are O(1)-enumerable from watches; kissat
// connects irredundant only). OPTIONAL acceleration: `Kernel::dense` is null in
// search and outside a phase, and every consumer (BVE/Subsume/Strengthen) keeps
// its round-local arena-scan path for that case (N1 — rules stay independently
// applicable via the public API).
struct OccIndex {
  std::vector<std::vector<Ref>> large; // by literal; may hold stale garbage Refs
};

// Undo log of the SEARCH BOOKKEEPING a probing propagation touches (kissat
// keeps probing propagation in its own engine, src/proprobe.c; ours is the same
// engine with this log switched on — see kernel_propagate_probing in
// kernel.cpp). Unit propagation only stays complete under the two-watched-
// literal scheme if a falsified watch is MOVED, so a probing propagation cannot
// simply refrain from moving it; instead it records every incidental update it
// makes — watch-list contents and per-clause literal permutation + `searched`
// scan hint — and puts them back once the speculative trail is unwound. The
// derived literals and the conflict are bit-identical to kernel_propagate's;
// only the bookkeeping is rolled back.
struct ProbingLog {
  bool recording = false;
  // kernel state the log is relative to: the restore only fires when the caller
  // has unwound the trail back to exactly this (see kernel_probing_end)
  uint32_t base_trail = 0, base_head = 0, base_level = 0;
  // watch-list snapshots, one per literal, taken at FIRST TOUCH (first scan
  // mutation or first delayed push, whichever comes first)
  struct ListSnap {
    Lit lit;
    uint32_t off, size; // offset into `words`, and the list's original length
  };
  std::vector<ListSnap> lists;
  std::vector<uint32_t> words; // flat backing store for the snapshots above
  std::vector<uint32_t> stamp; // by lit: epoch this literal was snapshotted in
  uint32_t epoch = 0;          // bumped per scope; 0 is "never snapshotted"
  // per-relocation clause records, replayed in REVERSE order
  struct ClauseSnap {
    Ref ref;
    uint32_t rpos;             // index the replacement literal came from (>= 2)
    Lit lit0, lit1;            // the watched pair before the relocation
    uint32_t searched;
  };
  std::vector<ClauseSnap> clauses;
};

// Retirement cause for a variable rewritten out of the formula. See
// Kernel::var_status. cadical carries the same concept as Flags::status
// {ACTIVE, FIXED, ELIMINATED, SUBSTITUTED, PURE}; we keep only what a rule of
// ours actually produces, and route it through advice rather than the decision
// loop (design §3).
enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };

struct Kernel {
  uint32_t nvars = 0;
  // Variables at index >= first_extension_var are NON-ORIGINAL "extension"
  // variables introduced mid-solve (E2 Factor / BVA). They are assigned by search
  // like any other variable but are PROJECTED OUT of the reported model — the
  // competition `v` line ranges over original variables only (a model restricted
  // to the originals satisfies the original formula; see docs/design/
  // e2-factor-design.md §3). UINT32_MAX = none added yet, so all vars are original.
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  // off-fixpoint-decide taint (D-LEVEL-1). A Decide taken while the trail is
  // NOT at a propagation fixpoint (propagate_head < |τ|) lets a not-yet-run
  // watch scan observe an "ahead" assignment and treat a clause as satisfied
  // by it; if a later backtrack unassigns that ahead literal, the stale scan
  // is never redone and the clause can be left unit with no watch firing
  // (escaped unit). The cure is to re-propagate the WHOLE trail after any
  // trail-shrink that follows such a decision, so every stale scan is redone
  // against the post-shrink assignment: kernel_backtrack resets propagate_head
  // to 0 (not the level boundary) whenever this flag is set. DefaultPolicy
  // always decides at a fixpoint, so it NEVER sets this — its backtracks keep
  // the boundary reset and it stays bit-exact. Set by decide_apply; once set it
  // stays set for the rest of the solve (re-propagating a short N1/test trail
  // from 0 is cheap, and clearing it soundly is subtle — a fixpoint reached
  // while an ahead literal is still true does NOT make the trail safe).
  bool trail_tainted = false;
  // trail height at the start of each level; frames[0] = 0 exists from
  // construction — the `frames.size() == level + 1` invariant must hold
  // even for a kernel that never saw a variable (empty formula; corpus
  // regression 001-empty-formula-frames)
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  // R-c2: of those, how many are NOT retired. Lets the retirement skip answer
  // "is any non-retired candidate left?" in O(1) instead of walking the queue.
  // Without it the skip is quadratic in the TAIL of every descent: once all
  // non-retired variables are assigned, each remaining decision walked ~195 k
  // links before falling back (measured — veer_axi 204 s -> 600 s TIMEOUT).
  // Kept exact by the same three sites that move `unassigned`, plus retirement.
  // A drift here is a PERFORMANCE bug only, never a soundness one: too low
  // falls back early (decides a retired variable while real ones remain), too
  // high scans and finds nothing, then falls through to the same answer.
  uint32_t unassigned_active = 0;

  Arena arena;
  WatchLists watches;
  // shared large-clause occurrence index, non-null only during a simplification
  // phase (increment 2). Owned by the caller (the policy); the kernel maintains
  // it through add/GC. Null everywhere else — see OccIndex.
  OccIndex *dense = nullptr;
  // delayed watch relocations from an in-flight propagation scan
  // (lit, blocking, ref) triples; see watch.hpp for why they must wait
  std::vector<uint32_t> delayed;
  // probing-propagation bookkeeping log; inert (recording == false) outside a
  // kernel_probing_begin/end scope, so search never pays for it
  ProbingLog probing;

  // Search-guidance state `H` (VMTF queue, EVSIDS score heap, saved/target/best
  // phases): held OPAQUELY and NON-OWNINGLY (Track H · H7) — the full `struct
  // Heuristics` lives in policies/heuristics.hpp, and only code that includes it
  // may read `h->…`. The kernel NEITHER allocates NOR owns it: the session (or a
  // policy) that outlives a single `solve()` owns the storage and ATTACHES it
  // (`k.h = &heuristics`; see tactix_solver / main). OPTIONAL by design (plan rev
  // 60): a solver with `h == nullptr` is sound, just unguided — that is the
  // DEFAULT here (a bare `Kernel` is unguided until a heuristic is attached).
  // Grown by heuristics_reserve_vars alongside kernel_reserve_vars when present.
  Heuristics *h = nullptr;

  // --- reconstruction stack (rho, plan §4) ----------------------------
  // Witness-labelled clause segments for rebuilding a SAT model through
  // eliminated/substituted variables (Kissat's extension stack; the
  // Jarvisalo-Heule-Biere reverse reconstruction). Flat, length-prefixed:
  // each segment is [ rest_lits..., witness_lit, len ] with len = 1 +
  // #rest and witness_lit the blocking literal. kernel_extend_model walks
  // it in REVERSE and, for any segment whose clause is unsatisfied under
  // the current assignment, sets its witness true. Empty until a
  // variable-eliminating rule (V3 Substitute, E1 BVE, ...) pushes a
  // witness — a no-op for unit-only inprocessing (V1 Probe / V2 Backbone).
  std::vector<Lit> reconstruction;
  bool model_extended = false; // kernel_extend_model idempotency guard

  // conflict handoff: propagate -> analyze
  bool conflict_valid = false;
  Ref conflict_ref = INVALID_REF; // INVALID means binary conflict
  Lit conflict_bin[2] = {INVALID_LIT, INVALID_LIT};

  // learned-clause handoff: analyze -> backjump
  PendingLearned pending;

  // scratch (analyze) — persistent so the per-conflict hot path never
  // allocates (kissat keeps these as solver-level stacks for the same
  // reason: clause, analyzed, shrinkable in internal.h)
  std::vector<uint8_t> seen;   // by var
  std::vector<Var> analyzed;   // vars marked seen
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  // |{v ∈ analyzed : seen[v] & (SEEN|REASON_BUMP)}| — the BUMP set size,
  // which is what kissat's `analyzed` stack holds (its removable/poisoned
  // marks live on separate stacks). Ours is a flag-cleanup list that also
  // carries minimization scratch and, in LRAT mode, recorded antecedents,
  // so `analyzed.size()` is neither the bump-set size nor mode-independent.
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  // --- proof clause ids (native LRAT; CaDiCaL's id scheme) ------------
  // every clause — including input tautologies/duplicates that are never
  // stored — consumes one id in input/derivation order. Large clauses
  // carry theirs in the header; units live in unit_ids (by var; 0 =
  // none); binaries, which never materialize, in binary_ids keyed by the
  // canonical literal pair. THE INVARIANT (rev 33): while a pair is live in
  // the watch lists, its id here names a clause the checker still holds.
  // Before binaries were deletable this was free (ids only ever accumulated,
  // so "first id wins" was as good as last). kernel_delete_binary breaks that,
  // so two rules now hold it up: (a) kernel_add_binary_clause installs the
  // LAST id, because a pair re-derived after V4 deleted it must not keep
  // citing the id the `del` retired; (b) a pair with two live copies is NOT
  // deletable — one id cannot name two clauses (the primitive refuses).
  // Lookups of a DEAD pair never happen: every kernel_binary_id caller
  // resolves a pair it just read out of a watch list or a live binary reason.
  // The map itself is size-monotone — deletion keeps the key, since
  // `binary_ids.size()` is the "new binaries appeared" inprocessing trigger
  // (default_policy.cpp) and feeds kernel_irredundant_clauses; both want
  // monotone growth, not a live count. SubsumeForward's soundness rests on the
  // V4 equivalence argument, not on permanence — see subsume_forward.hpp.
  uint64_t next_clause_id = 0;
  std::vector<uint64_t> unit_ids; // by var
  // (A flat open-addressing replacement was built and A/B-measured
  // 2026-07-25 — trajectory-identical but uniformly ~1-3 % SLOWER on
  // crypto1/s055/mp1-LRAT, probe 1562428 — the map is not a bottleneck;
  // review §c item 3 closed as a recorded negative.)
  std::unordered_map<uint64_t, uint64_t> binary_ids;
  // antecedent scratch (analyze, LRAT mode only): (trail position of the
  // literal the antecedent propagated, antecedent id). Emitted sorted by
  // trail position ascending with the conflict id last — that order is
  // checker-propagatable by construction (each hint's other literals are
  // clause negations, roots, or earlier-propagated literals)
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below

  Proof *proof = nullptr;
  Stats stats;
  // NOTE (plan §5 rev 17): scheduling state (restart EMAs, reduce
  // intervals, ...) lives in the Policy object, not here; kernel H-state
  // is confined to instance-ranking heuristics (VMTF, phases).

  // --- E1 BVE per-variable RETRY FILTER (kissat flags->eliminate) ------------
  // by var: 1 = "this variable's elimination neighbourhood has changed since
  // BVE last attempted it — retry", 0 = "BVE already attempted it against
  // exactly this neighbourhood and got nothing". Purely a WORK filter, never a
  // soundness one: it removes only re-attempts whose outcome is already known,
  // so no rule application that was possible becomes impossible (N1 — see
  // bve.hpp for the completeness argument).
  //
  // Set (kernel-side, so there is one home per mutation kind):
  //   - every variable of an IRREDUNDANT clause that is ADDED
  //     (kernel_add_large_clause / kernel_add_binary_clause) or DELETED
  //     (kernel_mark_garbage / kernel_delete_binary*). Redundant (learned)
  //     clauses are deliberately excluded — BVE never resolves them (they are
  //     delete-only collateral), and marking on learned-clause traffic would
  //     mark every variable on every conflict and defeat the filter.
  //   - every variable of a clause a newly ROOT-FIXED literal occurs in
  //     (kernel_bve_retry_sync_fixed) — fixing y makes the clauses y satisfies
  //     delete-only and strips ¬y from the rest, which changes what BVE
  //     computes for the OTHER variables in them.
  //   - every freshly reserved variable (kernel_reserve_vars initialises to 1),
  //     so the very first schedule considers everything.
  // Cleared by the rule when it ATTEMPTS a variable (bve_eliminate), matching
  // kissat eliminate.c's `FLAGS (idx)->eliminate = false` at attempt time.
  std::vector<uint8_t> bve_retry;
  // trail index up to which root-fixed literals have been folded into
  // `bve_retry`; see kernel_bve_retry_sync_fixed.
  uint32_t bve_retry_fixed_head = 0;
  // SCHED-B3 RESUMPTION CURSOR — solver STATE, not config. `instances()` can
  // only offer `cap` sources per call, and it used to offer the FIRST `cap`
  // every round, so a budget-truncated round re-derived the same list and
  // started from the front again: the tail was never reached. kissat has no
  // index either — it keeps a per-variable `flags[idx].transitive` pending bit
  // and clears it as each source is processed, so whatever the budget did not
  // reach stays scheduled for the next round (`transitivekeep`, DEFAULT 1,
  // options.h:157). This is the same idea in one word. N1 is untouched:
  // `apply_instance` still accepts ANY source literal directly, so rotating the
  // enumeration order removes no capability — `instances()` is a convenience.
  uint32_t transitive_cursor = 0;
  // by var: the MINIMUM growth slack at which x is currently eliminable (its
  // #resolvents − occ⁺ − occ⁻), as last computed by bve_eliminate; UINT16_MAX =
  // never (an oversized resolvent, or growth beyond the tracking ceiling). Only
  // meaningful while `bve_retry[x] == 0` (attempted, neighbourhood unchanged) —
  // a set retry bit means "recompute". A finer-grained WORK filter than the
  // retry bit for the DefaultPolicy BVE ramp: rejected at a low slack, x can be
  // skipped at every slack below what it actually needs instead of re-resolved
  // at each — same completeness/N1 story as `bve_retry` (bve.hpp). Written only
  // when `bve_track_min_bound` is set (the ramp); every other path leaves it
  // untouched and byte-identical. Sized with `bve_retry`.
  std::vector<uint16_t> bve_min_bound;
  // Opt-in: when set, bve_eliminate resolves a rejected variable up to the ramp
  // ceiling (not just the current slack) so it can record the EXACT `bve_min_bound`
  // the policy needs to skip precisely. Off by default → the abort is at the
  // current slack, exactly as before this filter existed. Policy-transient
  // (DefaultPolicy sets it around its ramp), never persisted.
  bool bve_track_min_bound = false;

  // --- rule configuration (set by a policy or the public API at any state) ---
  // BVE syntactic gate detection: bitmask of enabled gate types (see bve.hpp
  // BveGate). Default = all bits on. This is a PERFORMANCE knob, never a
  // soundness one — every setting is sound because a disabled gate type only
  // makes BVE fall back to the full resolvent cross product (E1c, N1: gate type
  // is *how* a chosen variable's resolvents are computed, not *whether* it can
  // be eliminated). The first entry of what will become a rule-config struct.
  uint32_t bve_gate_mask = ~0u;
  // E1 BVE growth slack (kissat `bounds.eliminate.additional_clauses`, option
  // eliminatebound): eliminate iff #non-tautological resolvents ≤ occ⁺+occ⁻+this.
  // CONFIGURATION, like bve_gate_mask — not scheduling state: any policy may set
  // it at any time and any value is sound (a larger slack only admits more
  // eliminations, each individually sound). The default is the MAXIMUM
  // (BVE_MAX_BOUND in rules/bve/bve.hpp — keep the two in step), i.e. a caller
  // that ignores this field gets the old fixed-slack behaviour. A policy wanting
  // kissat's fine-grained dose ramps it 0,1,2,4,8,16, re-scheduling every
  // variable at each level (eliminate.c set_next_elimination_bound).
  uint32_t bve_bound = 16;
  // X1 BCE candidate-occurrence ceiling (cadical option `blockocclim`, whose own
  // default is 1e2): decline a blocking literal occurring more than this many
  // times. CONFIGURATION like bve_bound — any policy may set it at any time and
  // any value is sound, because it only narrows WHICH candidates the rule offers
  // (a within-rule scope choice, the same category as BCE's irredundant-only
  // filter — implementation-notes §1's X1 row), never whether a deletion is
  // justified.
  //
  // **Ships 0 = UNCAPPED, and porting cadical's 100 is MEASURED-DEAD** (probes
  // 1573405-07, 2026-07-29). It is a real lever in both directions on the same
  // axis, which is why it stays as a §5.6 arm rather than a constant:
  //   j3037_9_rggt_b  993 s -> 354 s  (2.8x FASTER capped; 7.60M -> 2.67M conflicts)
  //   mod4block       13.2 s -> TIMEOUT at 100 AND at 1000 (BCE's deletions there
  //                   drop 6,343 -> 535: the clauses mod4block needs removed are
  //                   exactly the ones a dense-neighbourhood cap forbids)
  //   clique          identical (1,530 deleted either way)
  // cadical can afford the cap because it does not depend on BCE at all (it ships
  // `block` OFF); we do, on mod4block. Set via TACTIX_BCE_OCC_LIMIT for an A/B.
  uint32_t bce_occ_limit = 0;
  // X2 HyperTernaryRes governors, ported from cadical (`ternary.cpp`), which
  // bounds ternary resolution FOUR ways where we had NONE. CONFIGURATION like
  // bve_bound — the POLICY sets them, any value is sound (they only narrow which
  // resolvents the rule offers), 0 = unlimited.
  //
  // Measured motivation (2026-07-29): X2 has no bound of its own, and BCE was
  // silently serving as one — deleting blocked clauses shrinks X2's candidate
  // pool. With BCE disabled on tierB/mod4block, X2 ran from 6,048 resolvents in
  // 17 rounds to **32,155 in 83 rounds** and the solve died. cadical allows 2
  // rounds. Giving X2 its own governor is what lets the trio be scheduled
  // instead of having one rule police another (user decision 2026-07-30: do not
  // disable X1/X2/X4 — fix the scheduling).
  //
  // `ternary_occ_limit` = cadical `ternaryocclim` (default 100): skip a pivot
  // whose EITHER polarity has more ternary occurrences than this
  // (`ternary.cpp:247`). `ternary_max_add_percent` = cadical `ternarymaxadd`
  // (default 1000, i.e. 1000 %): stop a round once it has added more clauses
  // than `total_clauses * pct / 100` (`ternary.cpp:387`).
  // E1 BVE dose ADMISSION by measured COST-PER-YIELD (resolutions per eliminated
  // variable), replacing the binary-density proxy for during-search doses.
  // CONFIGURATION like bve_bound: policy-owned, any value sound (it only
  // declines doses), 0 = no limit.
  //
  // Why cost and not density (measured 2026-07-30, windows `20260730-1118*`):
  // removing the density gate is +192.67 Tier C PAR-2 and THREE lost solves
  // (T105/T87/asconhash), so the gate is load-bearing — but on Tier B it is a
  // ONE-INSTANCE defense: `crypto1` IMPROVED without it (7.0 -> 3.3 s), while
  // it costs `sted5` a solve (600 TO -> 2.4 s), klieber 118.4 -> 6.5 and b22_1
  // 40.9 -> 10.5. Density is a proxy for "elimination is safe here" that is
  // false on every sparse instance in the tier. The DIRECT signal is cost per
  // yield. What SUGGESTED it: under forced doses `mod4block` spends 3.53 M
  // resolutions on 34 eliminations (~104,000 EACH), an outlier no productive
  // instance shows — a counter reading, and taken on an instance whose own wall
  // time ranges 4.36 s to timeout under a stock kissat, so it motivates the
  // metric and cannot validate it.
  // V4 TransitiveReduce: exit the reachability walk as soon as `dst` is reached
  // (kissat transitive.c:130). Ships FALSE — deviation 3 in
  // transitive_reduce.hpp runs each walk to exhaustion so the failed-literal
  // detection sees the whole reachable set, so this changes INFERENCE (fewer
  // derived units), not just cost. Sound either way (dropping an inference never
  // makes a deletion unjustified); measured separately from the meter arm
  // because only the meter is a pure unit fix. CONFIGURATION, policy-owned.
  // E2 Factor clause-size scope (kissat `factorsize`, default 5). 2 = the
  // binary-only variant we shipped through 2026-07-30; >= 3 lets the grid range
  // over clauses up to this size, i.e. remainders become sub-clauses rather than
  // single literals. CONFIGURATION, policy-owned, any value sound (it only
  // widens which factorings the rule offers; the add-RAT argument is
  // indifferent to |r| — docs/design/e2-factorsize-design.md §4).
  // Motivation: `kissat --factorsize=2`, i.e. kissat restricted to OUR variant,
  // loses or_randxor entirely (TO@300 vs 12.27 s) and costs fclqcolor 12.6x.
  // DEFAULT 2 -> 5 on 2026-07-31, one window, throttle 48: **Tier C 337.50 ->
  // 327.99 PAR-2 (-9.51)**, Tier B 104.79 -> 105.30 (+0.50, inside noise), no
  // solve-count change on either tier. The gain is real rather than variance —
  // af-synthesis -190.9 s against a measured identical-config range of 2.5 s,
  // constraints_25 -51.6 (range 15.0), j3037 -15.6 (range 2.5) — and the R5
  // design's pre-registered KILL instances are all clean (x9 +0.8,
  // mp1-blockpuzzle -0.0, FmlaEquivChain -0.6, mod4block +0.4).
  // NOTE this is the SCOPE half only. It admits no new instances, so it cannot
  // cause the damage the union-admission arm does (factor_scope_percent below).
  // D-STRENGTHEN-BUDGET: let C1/C2 stop mid-round on `work_budget`.
  // **DEFAULT OFF — MEASURED NEGATIVE 2026-08-03.** Same-window A/B
  // (`20260803-231532-B` / `20260803-231548-C`, hatch below): Tier B 72.22 vs
  // 70.51 and Tier C **366.58 vs 353.26**, 26/28 and 29/30 in both arms — the
  // interruptible default costs ~1.7 and ~13.3 PAR-2 for NO solve gain, and
  // makes both T3 halves worse (1.177x vs 1.134x cadical; 1.323x vs 1.278x
  // kissat on the subset). It is also inert on its own sentinel: `DivS_862_11`
  // 102.4 s on vs 103.2 s off, where C2 owns 89.7 % of the solve.
  // Mechanism, so the negative is understood rather than merely recorded: at
  // 100 per mille the budget binds about once per solve, and one stopped round
  // just defers its candidates to the next round — the work is rescheduled, not
  // saved. The DOSE is a real lever where interruptibility is not (forcing
  // 1 per mille takes DivS to 11.5 s), which is why the mechanism stays here as
  // a §5.6 dosing arm rather than being deleted.
  // `TACTIX_MIDROUND_BUDGET=1` turns it on.
  bool midround_budget = false;
  uint32_t factor_size = 5;
  // E2 Factor SCOPE-FRACTION admission (R5 joint admission, 2026-07-30). Admit
  // Factor when at least this PERCENT of the live irredundant clauses are within
  // its clause-size scope (<= factor_size). 0 = use the legacy binaries-per-var
  // density gate instead.
  //
  // Why a fraction and not a density. The shipped gate is binaries/var >= 6,
  // which its own comment calls an overfit magic number, and the obvious
  // scope-aware version (small-clauses/var) does NOT separate: mod4block scores
  // 20.18 against or_randxor's 6.49, so any threshold admitting rxor admits
  // mod4block too. The FRACTION does separate, cleanly and with a reason —
  // "most of the formula is inside Factor's scope" vs "Factor is nibbling at the
  // edge of a formula it cannot see":
  //     ON  rxor 96 %, fclq 99 %, sted5 96 %, mp1 100 %, battleship 92 %, x9 98 %
  //     OFF mod4block 8 %, crypto1 24 %
  // It is also automatically scope-aware: raising factor_size moves exactly the
  // instances that gain material across the threshold (rxor 0 % at size 2, 96 %
  // at size 5), which is what makes the size widening and the admission ONE
  // decision rather than two interacting ones.
  uint32_t factor_scope_percent = 0;
  bool transitive_early_exit = false;
  uint32_t bve_cost_limit = 0;
  uint32_t ternary_occ_limit = 100;
  uint32_t ternary_max_add_percent = 1000;
  // Factor grid-selection strategy (Track S · S2a): consulted by factor_apply
  // for its scoring/accept scalars when set; null = the built-in watch-size
  // path, byte-identical to pre-S2a. CONFIGURATION like bve_bound — any policy
  // may set it at any time and any value is sound (selection only chooses
  // WHICH positive-gain grid reaches the byte-identical apply_factoring, or
  // whether one does). Non-owning; DefaultPolicy scopes it to the inprocess
  // phase (set at entry, cleared at exit) so it never dangles.
  FactorMatchPolicy *factor_match = nullptr;
  // Factor acceptance bound: accept a grid iff delta > this. Default 0 ≡
  // the FACTOR_MIN_GAIN = 1 floor. CONFIGURATION like bve_bound — any value
  // is sound (it only narrows WHICH positive-gain grids reach the
  // byte-identical apply_factoring); an attached FactorMatchPolicy keeps
  // precedence. N1 config surface for external/learned policies; no shipped
  // policy writes it (the kissat eliminate-level coupling was measured dead
  // — gate report §2 — and removed).
  uint32_t factor_bound = 0;
  // Demote Factor's fresh extension variables to the BOTTOM of the decision
  // queue after an application (kissat factor.c
  // adjust_scores_and_phases_of_fresh_varaibles). Repairs a FOCUSED-mode
  // defect: heuristics_reserve_vars appends new vars at `queue_last`, which is
  // where VMTF's decision search starts, so every variable Factor adds becomes
  // a most-preferred branching candidate. Default false = today's behaviour
  // until the tier pair says otherwise (`TACTIX_FACTOR_DEMOTE`).
  bool factor_demote_fresh = false;
  // E2 Factor CANDIDATE REDUCTION rounds (kissat `factorcandrounds`, cadical
  // `--factorcandrounds`, both default 2). A large clause is grid material only
  // if every literal in it occurs in >= 2 candidate clauses; the prune iterates
  // to fixpoint, bounded by this. 0 = the unfiltered pre-2026-07-31 candidate
  // set (bit-identical to before). CONFIGURATION, policy-owned, any value sound
  // -- it only narrows which grids the rule OFFERS, never whether one is legal.
  uint32_t factor_cand_rounds = 0;
  // DIAGNOSTIC ONLY (task #11, `TACTIX_NO_EXT_DECISIONS`) — never ship this on.
  // Skips E2 Factor's extension variables when branching (falling back only
  // when nothing else is unassigned). It makes `Decide` itself unable to RETURN
  // those variables, i.e. it removes the capability — which N1 forbids for a
  // default (plan §1.1 N1 scope note, rev 76). Contrast RETIREMENT and
  // `no_ext_reason_bump`, which only change what the POLICY prefers and are
  // therefore compliant. It exists to test whether branching on extension vars
  // is what makes broad Factor admission damage our search.
  bool no_ext_decisions = false;
  // Task #11 (`TACTIX_NO_EXT_REASON_BUMP`, arm — default off). Withhold E2
  // Factor's extension variables from the REASON-SIDE bump channel only; the
  // conflict-side channel (resolution-visited / in the learned clause) is
  // untouched, so an extension var still rises whenever it earns it.
  //
  // UNLIKE `no_ext_decisions` above this is N1-CLEAN and shippable: reason-side
  // bumping is `H`-only advice with no proof obligation, and this changes only
  // the ORDER candidates are offered in, never which are legal (rule-dev: rank
  // freely, never omit). Every variable remains decidable at every point.
  //
  // Measured motivation (probe 1577502, broad admission): on ContextModel
  // extension vars take 2.97x as many bumps PER VARIABLE as original vars and
  // **71.68 % of those bumps are reason-side ONLY** (46 % on jgiraldez, 36 % on
  // sted5). A BVA extension variable IS by construction the hub of the clauses
  // it replaced, so it appears in a great many reasons — reason-side bumping
  // promotes it for its structural position rather than for evidence, and the
  // promotion self-reinforces (promoted -> decided more -> in more reasons).
  // This is the channel that plausibly separates us from cadical's 0.10 %
  // ext-decision share on the same instance (probe 1577499) while preserving
  // its 8.81 % on sted5 — i.e. DISCRIMINATION, not suppression.
  bool no_ext_reason_bump = false;
  // A/B knob for the 2026-08-01 V3 hazard fix (`TACTIX_SUBST_HAZARD_ROUND=1`):
  // restore the OLD per-ROUND skip, so both arms come from one binary and are
  // comparable in ONE window — a delta read across windows is not readable
  // (this same solver, at documentation-only-different commits, scored Tier C
  // 309.05 and 296.82 in two idle windows on one day).
  // Default false = the per-SCC skip. Diagnostic only; the per-SCC form is the
  // fix, and the per-round form is what wasted all 179,110 of veer_axi's
  // equivalences by losing 5 of 5 rounds to 284 poisoned literals in 406,256.
  bool subst_hazard_round = false;
  // VARIABLE RETIREMENT (design `docs/design/variable-retirement-design.md`,
  // plan §11 O17 / §1.1 N1 scope note, rev 76). A variable a rule has rewritten
  // out of every clause occurs in NO clause, so deciding it propagates nothing
  // and only deepens the trail — measured at 84.03 % of `veer_axi`'s decisions.
  //
  // ADVICE, NOT A RESTRICTION. `Decide::instances()` still enumerates these and
  // any policy may decide them (N1 binds the API, not DefaultPolicy's taste);
  // DefaultPolicy merely declines while a non-retired candidate exists. That is
  // the `bve_retry` shape, and the line that separates this from
  // `no_ext_decisions` (which removes the capability).
  //
  // The CAUSE is kept, not just a bit: the two causes have different owners and
  // different invariants, and `c retired` reports them apart when diagnosing.
  // Empty vector == every variable Active.
  std::vector<VarStatus> var_status;
  // Demote substituted variables to the bottom of the decision order after each
  // V3 round (`TACTIX_SUBST_DEMOTE=1`). Ranking advice only.
  bool subst_demote = false;
  // R-d, SHIPPED ON 2026-08-02. DefaultPolicy retires variables a rule has
  // rewritten out of the formula: `heuristics_deactivate_var` unlinks them from
  // the VMTF queue and the score heap (kissat `flags.c` deactivate_variable),
  // `kernel_unassign` keeps them out, and the SAT test counts ACTIVE variables.
  // Worth **Tier B 109.60 → 75.41 PAR-2 and +2 SOLVES** (`veer_axi`, `sted5`).
  // `TACTIX_RETIRE=0` reverts. N1 intact: `decide_candidates` enumerates from
  // `k.nvars`, never from `H`, so the API still offers every variable.
  bool retire_enabled = true;
  // §6 COMPLIANCE (task #10). External interrupt, published to the RULES.
  //
  // `stop` was a `Policy::solve` parameter only, so the longest-running rule
  // applications could not see it: V6 Congruence spent 131,327 ms over 3
  // applications on T105/T87 and left the solver SIGTERM-deaf for ~1,100 s.
  // A competition harness escalates to SIGKILL, and then the PROOF IS LOST —
  // so this is an N2/§6 defect, not a latency nuisance. Polling at the phase
  // round loop and slot boundaries (`0a1e0aa`) bounds latency to ONE
  // application, which is exactly the block that is too long.
  //
  // NOT policy knowledge (the kernel stays policy-free, §5.3): this is
  // external-environment state, the same category as `proof`. The policy
  // publishes its own `stop` here on entry and clears it on exit.
  //
  // SOUNDNESS: aborting is always legal because a rule that stops early has
  // simply done LESS. Everything it did is already proof-logged, and abort
  // points are chosen between complete units of work, never mid-mutation — the
  // same contract budget-cut doses already satisfy (Factor's tick limit). It
  // can therefore never change a verdict, only how much simplification happens.
  volatile sig_atomic_t *stop = nullptr;
  // Stress knob for the abort paths above (0 = off). `mutable` because the
  // extraction half of V6 takes a `const Kernel &` — diagnostic state only,
  // never read by a guard, so it cannot affect which rules are APPLICABLE (N1).
  // NB: the poll sites bypass their 256-iteration cadence when this is set.
  // Without that, small formulas never reach a first poll at all — measured:
  // `battleship` (65,573 conflicts, 2 congruence rounds) aborts ZERO times at
  // every setting, so a fuzz leg over tiny CNFs would pass vacuously.
  uint32_t stop_fuzz = 0;
  mutable uint64_t stop_fuzz_state = 0x9E3779B97F4A7C15ull;
  // S4 composite grids (S2b, kissat resize_factoring): widen Factor's
  // factor/remainder universe to ALL live literals — extension vars included,
  // growing as the pass applies. Default false (measured: composition
  // over-applies in a single preprocessing blast, S2b §7; it belongs to the
  // arrangement's small interleaved doses). CONFIGURATION like bve_bound —
  // any value sound, termination stays delta-monotone (S2b §4).
  // (The TACTIX_FACTOR_COMPOSITE env override was removed in the 2026-07-25
  // negative-arm cleanup — this field is the one switch.)
  bool factor_composite = false;
  // ADVICE (N1): may the policy's scheduling of Factor decline this occasion?
  //
  // DEFAULT true = NO RESTRICTION, and that default is the N1 guarantee. Until
  // 2026-08-04 the binary-density selectivity heuristic lived inside
  // `factor_apply` as a NoOp, so `factor_guard` reported APPLICABLE,
  // `factor_instances` offered its canonical instance, and applying it did
  // NOTHING on any binary-sparse formula — no caller of the public API (an LLM
  // policy, an RL policy, RandomPolicy) could apply a legal factoring at all.
  // The rule's own comment claimed the opposite. That is a capability removed,
  // which rev 76's carve-out does not cover: the carve-out permits the built-in
  // POLICY to decline, not the RULE to refuse everyone.
  //
  // So the predicate moved to DefaultPolicy (where BVE's identical density gate
  // already correctly lives) and reaches the rule through this field — the same
  // advice shape as `bve_retry`. An API caller that never writes it sees the
  // complete surface. Sound at any value: declining an application is always
  // sound, and forcing one is what the rule would have done anyway.
  bool factor_admit = true;
  // SCHED-B2 — PRIVATE PER-RULE TICK METERING.
  //
  // `stats.ticks` is the GLOBAL propagation counter, and four slot entries
  // (Probe, Backbone, Vivify, Instantiate) budget against it. Both references
  // keep PRIVATE per-technique counters instead, and the difference is not
  // cosmetic: with one shared meter an earlier slot entry spends the budget a
  // later one was handed, and nothing distinguishes "this rule was STARVED"
  // from "this rule ran and found nothing". Measured on `sted5` before this
  // change: Instantiate and Vivify were budget-declined on 22 occasions of 33.
  //
  // When non-null, every kernel tick charge is added HERE as well as to
  // `stats.ticks`. Pure accounting: the kernel never reads it, no guard
  // consults it, and no `instances()`/`apply` branches on it, so a null sink
  // (every caller except DefaultPolicy) is byte-identical to before. The policy
  // scopes it around one apply, the same discipline as `work_budget`.
  //
  // Placement argument: the alternative is each rule charging its own counter,
  // which would put per-technique accounting inside rules that delegate
  // propagation to the kernel, and would not survive a rule calling
  // `kernel_propagate_probing`. The sink keeps the accounting where the work
  // happens and the INTERPRETATION where the schedule lives.
  uint64_t *tick_sink = nullptr;
  // Per-round work budget, read by any budget-aware canonical apply in its
  // OWN currency (kissat SET_EFFORT_LIMIT): Vivify meters propagation ticks
  // (stats.ticks), Factor its cache-line factor_ticks (S4 dosing — kissat
  // factor.c:442,914 quits mid-round on the same counter). A round stops
  // visiting further candidates once it has spent this much; 0 = unbounded
  // (the default — byte-identical legacy behaviour). CONFIGURATION like
  // bve_bound, not a guard: stopping early only makes the round's effect a
  // prefix/subset of the unbounded round's, and every prefix is sound (each
  // rewrite is individually justified). Added for the S4 arrangement, whose
  // during-search phases must dose their techniques — an unbounded round
  // measured as the dominant diffuse Tier C loss (attribution 2026-07-24).
  uint64_t work_budget = 0;
  // C3 Vivify round scope (kissat's redundant-tier vs irredundant split):
  // 0 = all candidates (default, legacy), 1 = redundant (learned) clauses
  // only, 2 = irredundant only. CONFIGURATION like work_budget — a
  // scoped round's effect is a subset of the full round's, each rewrite
  // individually justified. Lets the policy mirror kissat's structure where
  // the redundant tiers ALWAYS run within budget and only the irredundant
  // pass is delay-gated (vivifyirr) — delaying the whole round measured as
  // a Tier B loss (mod4block TO, 2026-07-24 arr4).
  uint8_t vivify_scope = 0;
  // TB4b(A): reuse the conflict as its own driving clause when exactly one of
  // its literals sits at the conflict level (kissat
  // one_literal_on_conflict_level) instead of learning a fresh clause.
  // CONFIGURATION like work_budget/vivify_scope — the frontend owns the on/off
  // (TACTIX_CONFLICT_REUSE=1 enables), the rule owns the mechanism. Both
  // settings are sound; they differ only in trajectory and in how many clauses
  // are learned.
  // DEFAULT OFF. The 2026-07-28 tier verdict against it (Tier B 8.32 vs 7.62,
  // Tier C 330.17/310.93 vs 312.17/307.07) is RETIRED: it was measured on a
  // BROKEN arm, not merely a crippled one — DefaultPolicy ran its whole
  // learn/backjump tail on a stale `pending` record after every reuse, undoing
  // the reuse's own assignment and tripping TB4's must-stay-0 `backjump_stale`
  // (implementation-notes.md §12). Both that defect and the watch-repair
  // crippling are fixed; see PROGRESS TB4b for the re-measurement.
  bool conflict_reuse = false;
  // DIAGNOSTIC ONLY (TACTIX_CONFLICT_REUSE_NOREPAIR=1, needs conflict_reuse
  // too): restore the CRIPPLED behaviour — decline the shortcut instead of
  // repairing the watches. It exists to attribute the arm's tier verdict
  // between "the crippling" and "the mechanism", which is otherwise
  // unanswerable: an ON-vs-OFF pair cannot tell them apart. Not a shipped
  // setting and not a policy surface; both values are sound.
  bool conflict_reuse_no_repair = false;
  // Reason-side bumping (kissat option bumpreasonslimit, cadical
  // bumpreasonlimit — both 10): after the learned clause is final, also mark
  // for bumping the literals of the REASONS of the clause's own literals, up
  // to `this × |bump set|` marks; 0 = off (the default, byte-identical to not
  // having the mechanism). CONFIGURATION like bve_bound: it changes only
  // which variables `H` bumps, so every value is sound and proof-neutral —
  // the clause database, the trail and the certificate are untouched. The
  // GATE (decision-rate EMA + delay ladder) is scheduling and lives in the
  // policy (rev 17), which scopes this field around ConflictAnalysis.
  uint32_t bump_reason_limit = 0;
  // Reduce retention tiers (kissat options tier1/tier2, defaults 2/6): glue
  // bands that survive a reduction while recently used. CONFIGURATION like
  // bve_bound — retention is policy, and every value is sound (Reduce only
  // ever deletes redundant clauses). A policy that wants kissat 4.0's
  // DYNAMIC tiers calls reduce_retier(), which overwrites these with
  // quantiles of Stats::used_glue.
  uint32_t tier1 = 2, tier2 = 6;

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);
// heuristics_reserve_vars (the `H`-growth half) is declared in
// policies/heuristics.hpp; kernel_reserve_vars calls it (Track H · H4).

// Allocate `count` fresh NON-ORIGINAL (extension) variables at a root fixpoint and
// return the index of the first one. Grows all per-variable state via
// kernel_reserve_vars (additive) and records the extension boundary so the model
// printer projects them out. Used by E2 Factor (BVA). Caller must be at level 0.
Var kernel_new_extension_vars(Kernel &, uint32_t count);

// Number of variables to emit in the model `v` line — original variables only
// (extension variables introduced by Factor are projected out).
inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.first_extension_var == UINT32_MAX ? k.nvars : k.first_extension_var;
}

// --- reconstruction (rho) --------------------------------------------------
// Push one witness-labelled clause segment {witness} u {rest} onto rho.
// `witness` is the blocking literal (set true at extend if the segment's
// clause is unsatisfied). Internal state only — emits NO proof line
// (push-witness is kernel-side; the DRAT/LRAT justification is the rule's
// add-RUP/delete of the actual clauses).
void kernel_push_witness(Kernel &, Lit witness, const Lit *rest, uint32_t nrest);
// Rebuild a full model through rho: replay the stack in reverse, forcing
// each unsatisfied segment's witness true. Idempotent (model_extended).
// Call once on a SAT result before reading k.values as the model.
void kernel_extend_model(Kernel &);

// --- shared proof idioms (review 2026-07-25 item 7) -------------------------
// {a} + {¬a} ⊢ ∅: emit the empty clause (LRAT chain {id_a, id_b}; DRAT bare)
// and set `inconsistent`. The pair-refutation emission lives in exactly one
// place (the kernel_reason_walk precedent) — copy drift here is a soundness
// risk, which is why four rules share this instead of inlining it.
void kernel_emit_empty_from_pair(Kernel &, uint64_t id_a, uint64_t id_b);

// Batched root units for one simplification round (V3's record_unit; corpus
// 007 is the regression that mandates the opposite-sign refutation): deduped
// per variable; {u} and {¬u} recorded in one round refute via
// kernel_emit_empty_from_pair. The caller has already emitted the unit's
// proof clause under `id`; Recorded installs it as the variable's unit id.
struct PendingUnits {
  std::vector<Lit> units;  // first-recorded order (assert after the round)
  std::vector<Lit> by_var; // INVALID_LIT = none recorded yet
  enum class Outcome { Recorded, Duplicate, Refuted };
  void init(uint32_t nvars) {
    units.clear();
    by_var.assign(nvars, INVALID_LIT);
  }
  Outcome record(Kernel &, Lit u, uint64_t id);
};

// Clause-replacement tail (review 2026-07-25 item 8): after the caller has
// emitted the shrunken clause's proof ADD under `id`, delete the original
// (proof `del` + garbage mark) and materialize `lits` by size. Unit handling
// stays with the CALLER (rules differ: Vivify asserts + ends its round,
// Substitute batches via PendingUnits); the empty clause refutes here. Large
// clauses carry (redundant, glue) — glue is a redundant-clause concept, so
// callers pass 0 for irredundant (the divergence the review aligned).
enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref); // nullable

// --- BVE retry filter (see Kernel::bve_retry) -------------------------------
inline void kernel_mark_bve_retry(Kernel &k, Var v) { k.bve_retry[v] = 1; }
// Fold every ROOT-FIXED literal not yet accounted for into `bve_retry`: fixing
// y changes the effective content of every clause y occurs in (the ones y
// satisfies become delete-only, the rest lose the falsified literal), so every
// OTHER variable in those clauses may now be eliminable where it was not.
// Incremental and idempotent — `bve_retry_fixed_head` is the trail index up to
// which this has been done, and the root prefix of the trail never shrinks
// (kernel_backtrack compacts kept level-0 literals down but never touches the
// prefix below its boundary), so the watermark is monotone. Costs O(the new
// fixed literals' occurrences) with `dense` attached, one arena scan without.
void kernel_bve_retry_sync_fixed(Kernel &);

// --- clauses ---------------------------------------------------------------
// size >= 3; watches lits[0] and lits[1]. `id` is the proof clause id
// (0 in id-free unit tests; solver paths pass ingest input ids or
// ++next_clause_id for derived clauses)
Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id = 0);
// `id` is the pair's proof clause id; a re-add overwrites a previous one (see
// the binary_ids invariant above)
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);
// Delete binary (a v b): drop both mirror watch entries, retire a live ROOT
// reason to UNIT_REASON (the binary analog of corpus 003 — a root literal this
// binary propagated stays assigned as a root unit, already justified by its
// unit id), and emit the proof `delete`. binary_ids keeps the key, so the map
// stays size-monotone (see the binary_ids invariant above). Returns false and
// does nothing if the binary is
//   - absent (idempotent), or
//   - DUPLICATED — two live copies of a pair share one binary_ids id, and the
//     `del` would retire it out from under the survivor (an LRAT-only hazard:
//     DRAT deletes by literals, so it stays consistent), or
//   - a live reason for a NON-root literal (never at a quiescent root; keeps
//     direct/off-root callers safe).
// Redundancy of (a v b) is the CALLER's proof obligation, not checked here.
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
// DRAT-only: remove EVERY copy of pair (a,b) (E2 Factor grid removal; see impl).
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);

// --- watch repair (TB4b) ---------------------------------------------------
// The kernel could ADD a large-clause watch (WatchLists::push_large) but never
// REMOVE one without deleting the clause; every rule that needed to re-point a
// watch had to allocate a fresh clause instead (otfs_strengthen, Strengthen,
// Vivify). These two primitives close that gap. They mutate WHICH literals a
// live clause watches and nothing else: membership, size, id, redundancy and
// arena position are untouched, so the proof stream, the dense occurrence
// index, every recorded Ref and every LRAT chain stay valid. Sound for the
// same reason the propagation engine's own watch relocation is: two-watched-
// literal completeness constrains only which literals are watched, and the
// caller owns re-establishing it (see kernel_promote_watched_lit's contract).
//
// PRECONDITIONS (both):
//   - `ref` is a live (non-garbage) large clause currently watched by the
//     literal named — i.e. that literal is lits[0] or lits[1];
//   - NO propagation scan is in flight (`delayed` empty): a scan holds raw
//     pointers into the very slice these compact, and a push can relocate any
//     list (watch.hpp);
//   - NOT inside a kernel_probing_begin/end scope — the probing rollback log
//     records only the scan's own edits, so an unlogged watch move would
//     survive kernel_probing_end.
//
// Remove the large-clause watch of `ref` from watches[l]: the exact inverse of
// WatchLists::push_large (kissat watch.h kissat_unwatch_blocking). In-place
// compaction of l's slice — no growth, so no other list moves.
void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Move clause `ref`'s literal at position `from` (>= 2) into WATCHED slot
// `slot` (0 or 1); the displaced literal takes position `from` and the watch
// follows the promoted literal (kissat analyze.c's unwatch / swap / watch
// triple). The caller is responsible for the two-watched-literal invariant it
// leaves behind: a false watched literal at level d must be paired with a
// watch that is TRUE at level <= d, or an unassigning backtrack can strand the
// clause unit with no watch to fire on (regression corpus 002/005).
void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

// Mark a large clause deleted. THE single home for large-clause deletion: every
// rule that retires a clause goes through here, so the bookkeeping that must
// accompany a deletion (today: the BVE retry filter) has exactly one place to
// live instead of ~10 open-coded `flags |= GARBAGE` sites. Deletion stays LAZY
// (every reader skips garbage; kernel_collect_garbage reclaims later), and the
// PROOF `delete` remains the caller's — a rule owns its proof delta (§5.3), and
// several callers must emit it before the clause's literals are overwritten.
// Idempotent: a second mark of the same clause does nothing. Header-inline
// because Reduce retires clauses by the hundred thousand.
inline void kernel_mark_garbage(Kernel &k, Clause *c) {
  if (c->garbage())
    return; // idempotent
  c->flags |= Clause::GARBAGE;
  // BVE retry filter: losing an irredundant clause shrinks the elimination
  // neighbourhood of every variable in it, so each becomes retry-worthy
  // (kissat clause.c mark_clause_as_garbage -> kissat_mark_removed_literals).
  // Redundant clauses are skipped: BVE never resolves them, and Reduce's
  // learned-clause traffic would otherwise mark every variable every round.
  if (!c->redundant())
    for (const Lit l : *c)
      kernel_mark_bve_retry(k, var_of(l));
}

inline uint64_t kernel_binary_key(Lit a, Lit b) {
  const Lit lo = a < b ? a : b, hi = a < b ? b : a;
  return (static_cast<uint64_t>(lo) << 32) | hi;
}
inline uint64_t kernel_binary_id(const Kernel &k, Lit a, Lit b) {
  const auto it = k.binary_ids.find(kernel_binary_key(a, b));
  return it == k.binary_ids.end() ? 0 : it->second;
}

// --- assignment / trail ----------------------------------------------------
// Header-inline (kissat fastassign.h pattern): propagation executes an
// assignment per propagated literal, and a cross-TU call there costs
// measurably at 10^6+ props/sec — the store sequence must inline into the
// watch-scan loop.
// Has a rule rewritten v out of the formula? (Empty status vector == none.)
inline bool kernel_var_retired(const Kernel &k, Var v) {
  return v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
}

// SCHED-B2: the ONE place a tick is charged. Adds to the global counter (the
// search/mode machinery reads it) and, when the policy has scoped a sink around
// an apply, to that rule's private counter as well. Routing every site through
// here is what stops the two drifting: a new charge site cannot accidentally be
// invisible to per-rule metering, because there is nowhere else to charge.
inline void kernel_charge_ticks(Kernel &k, uint64_t n) {
  k.stats.ticks += n;
  if (k.tick_sink)
    *k.tick_sink += n;
}

inline void kernel_assign(Kernel &k, Lit lit, uint32_t level,
                          uint32_t reason, bool binary) {
  assert(k.values[lit] == 0);
  const Var v = var_of(lit);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary};
  k.trail.push_back(lit);
  assert(k.unassigned > 0);
  k.unassigned--;
  if (!kernel_var_retired(k, v)) {
    assert(k.unassigned_active > 0);
    k.unassigned_active--;
  }
}
inline void kernel_assign_decision(Kernel &k, Lit lit) { // level++ inside
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}
inline void kernel_assign_unit(Kernel &k, Lit lit) { // root level
  kernel_assign(k, lit, 0, UNIT_REASON, false);
}
// True-level assignment (rev 38, D-LEVEL-2): a propagation-forced literal is
// stamped at its TRUE level — the max decision level among its reason clause's
// other (falsified) literals — not the current decision level. This is what
// lets a chronological backtrack keep a still-implied literal without stranding
// it (calculus D-LEVEL). Transliterated from kissat inlineassign.h
// assignment_level. This was inert under DefaultPolicy while chrono was off —
// at a propagation fixpoint on a level-sorted trail the true level always
// equals the current level — but chrono is ON by default since 2026-07-22, so
// the true level now genuinely differs and this computation is load-bearing.
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  kernel_assign(k, lit, k.assigned[var_of(other_false)].level, other_false,
                true);
}
inline void kernel_assign_ref(Kernel &k, Lit lit, Ref reason) {
  const Clause *c = clause_at(k.arena, reason);
  const Var v = var_of(lit);
  uint32_t lvl = 0;
  for (const Lit m : *c)
    if (var_of(m) != v) {
      const uint32_t ml = k.assigned[var_of(m)].level;
      if (ml > lvl)
        lvl = ml;
    }
  kernel_assign(k, lit, lvl, reason, false);
}
// update_phases=false is the probing variant (kissat backtrack.c
// kissat_backtrack_without_updating_phases): probe assignments must not
// pollute saved/target/best phases
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool update_phases = true);

// core propagation engine (kissat proplit.h; moved out of the M1 rule as
// a reviewed §5.3 kernel-primitive addition, 2026-07-14): propagates the
// trail to fixpoint or first conflict, recording the conflict in the
// handoff fields; returns true iff a conflict was found. Updates
// propagations/ticks, NOT conflicts — the M1 Propagate rule owns the
// ApplyResult/proof semantics and the conflict count; probing rules
// (S5 Lucky, phase-3 V-families) reuse the engine directly.
// `ignore`: optional clause ref excluded from propagation (CaDiCaL's
// ignore pointer — for rules probing a candidate's own literals; see
// propagate_literal in kernel.cpp). INVALID_REF = normal propagation.
bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// binary-only propagation to fixpoint or first conflict (kissat backbone.c):
// follows ONLY the binary-implication graph — no large-clause propagation,
// no watch mutation. V2 Backbone's engine; cheaper than kernel_propagate and
// derives a subset of its units. Same conflict-handoff semantics.
bool kernel_propagate_binary(Kernel &);

// --- probing propagation (kissat proprobe.c) --------------------------------
// FULL (large + binary) propagation for the probing rules that leaves search's
// bookkeeping alone: same engine, same implied literals, same conflict as
// kernel_propagate, but the watch-list updates and per-clause `searched` hints
// it makes along the way are recorded and rolled back at the end of the scope.
// Motivation (measured 2026-07-21): a Vivify round that derives NOTHING still
// relocated watches and rewrote `searched`, and that alone cost battleship-14-26
// 12x conflicts (1.95M -> 24.0M) — the formula was bit-identical afterwards.
//
// Usage — one scope per speculative episode:
//     kernel_probing_begin(k);                   // at a quiescent point
//     ... assume(); kernel_propagate_probing(k, ignore); ...
//     kernel_backtrack(k, base_level, false);    // unwind the assumptions
//     kernel_probing_end(k);                     // restore, THEN mutate the formula
//
// CONTRACT (all three are checked, not assumed):
//  * the restore only fires when trail size / propagate_head / level are back
//    at the scope's entry values. Any other state (e.g. a probing-derived
//    literal that turned out to be root-implied and was KEPT by the backtrack)
//    means the speculation left a permanent mark, so the log is DISCARDED and
//    the updates stay — i.e. exactly today's behaviour, always sound.
//  * no clause may be added, deleted, or relocated (GC) inside a scope: the log
//    holds Refs and watch-list contents. Both rules call end() before rewriting.
//  * kernel_propagate_probing outside a scope is plain kernel_propagate.
void kernel_probing_begin(Kernel &);
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);

// (VMTF / EVSIDS / phase-snapshot `H` operations moved to
// policies/heuristics.hpp — Track H · H4.)

// number of root-fixed assignments (the level-0 trail prefix)
// §6: has an external interrupt arrived? Rules poll this at their own safe
// abort points (see Kernel::stop). Cheap enough for a coarse cadence inside a
// long rule loop; never call it per-literal.
inline bool kernel_terminating(const Kernel &k) {
  // STRESS MODE (`TACTIX_STOP_FUZZ=n`, diagnostic): report "terminating" on a
  // deterministic 1-in-n cadence so rules abort at MANY different interior
  // points, while the POLICY's own `stop` (a separate parameter it never reads
  // through here) stays clear — so the solve still runs to a real verdict and
  // the fuzz oracles can check it. This is what makes "aborting a rule
  // mid-application is sound" a TESTED claim rather than an argued one: the
  // abort points are new control flow, and the risk they carry is leaving
  // kernel state or the proof stream inconsistent.
  if (k.stop_fuzz) {
    // LCG, deterministic in the seed — same input + same knob ⇒ same trace.
    k.stop_fuzz_state = k.stop_fuzz_state * 6364136223846793005ull + 1442695040888963407ull;
    if ((k.stop_fuzz_state >> 33) % k.stop_fuzz == 0) {
      k.stats.stop_fuzz_fires++;
      return true;
    }
  }
  return k.stop && *k.stop;
}

// RAII publisher for Kernel::stop — a policy's solve() publishes its interrupt
// for the duration of the call and always withdraws it, so an early return can
// never leave a dangling pointer in the kernel. Restores the previous value
// rather than nulling, so nested/reentrant solves compose.
struct KernelStopPublisher {
  Kernel &k;
  volatile sig_atomic_t *saved;
  KernelStopPublisher(Kernel &kk, volatile sig_atomic_t *s)
      : k(kk), saved(kk.stop) {
    k.stop = s;
  }
  ~KernelStopPublisher() { k.stop = saved; }
  KernelStopPublisher(const KernelStopPublisher &) = delete;
  KernelStopPublisher &operator=(const KernelStopPublisher &) = delete;
};

inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// assemble k.lrat_chain from k.lrat_entries: antecedent ids sorted by
// trail position ascending — the order that unit-propagates for the
// checker by construction — with `closing_id` (the conflicting/falsified
// clause, or the unit's reason) last. Shared by every chain-emitting
// rule so the ordering invariant is defined exactly once.
void kernel_lrat_chain(Kernel &, uint64_t closing_id);

// LRAT reason walk for probing rules (C3 Vivify, V1 Probe, V2 Backbone).
// Pure kernel-state read: collect (trail position, antecedent id) for every
// reason reachable from `start` down to root units and probe assumptions
// (assumptions are DECISION_REASON and skipped — they are the negation of the
// emitted clause). Ascending trail position unit-propagates for the checker;
// the caller closes the chain with the conflicting/final hint. `seen` is a
// caller-owned scratch (a var-indexed byte vector), so the walk never
// allocates and consecutive calls can share/dedup one buffer. When `touched`
// is non-null every newly-marked var is recorded there, so a PERSISTENT
// `seen` can be restored all-zero in O(marked) instead of a fresh
// O(nvars) zero-fill per walk (C4-perf: that memset was 97 % of ALL cycles
// on the hwmcc native-LRAT path — one per replayed ladder lemma; probe
// 1563224).
void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries,
                        std::vector<Var> *touched = nullptr);
// Seed the walk from the recorded conflict (binary in conflict_bin, else the
// large clause at conflict_ref): walk every conflicting literal's reason and
// return the conflicting clause's id (the RUP chain's closing hint). The
// probe-conflict case of every probing rule.
uint64_t kernel_conflict_reason_walk(
    Kernel &, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<Var> *touched = nullptr);

// (decision-ranking dispatch — heuristics_next_decision_var /
// heuristics_decide_lit — moved to policies/heuristics.hpp, Track H · H4.)

// --- garbage collection ------------------------------------------------
// compacts the arena (dropping GARBAGE clauses), fixes trail reasons via
// forwarding pointers, and rebuilds large watches (binaries kept in place)
void kernel_collect_garbage(Kernel &);

// Dense simplification phase (increment 2). connect builds `occ.large` from the
// current irredundant large clauses (one arena scan) and attaches it as
// `k.dense`; disconnect detaches it (the caller owns the storage). While
// attached, kernel_add_large_clause pushes new irredundant large clauses and
// kernel_collect_garbage rebuilds the index after relocating Refs. Precondition:
// level 0 (a quiescent root), like kissat's dense mode.
void kernel_dense_connect(Kernel &, OccIndex &occ);
void kernel_dense_disconnect(Kernel &);

// irredundant clause count: live irredundant large clauses + all binaries
// ever added (binaries have an irredundant lifecycle in this kernel — never
// reduced, and deleted only by V4 TransitiveReduce, whose deletions are
// equivalence-preserving; since binary_ids keeps a deleted binary's id, the
// binary term over-approximates the live count by the deleted ones — the
// log-scale consumers below tolerate it, and V4 is not in the default
// schedule). The kissat BINIRR_CLAUSES
// equivalent: feeds inprocessing interval scaling (kimits.c
// kissat_scale_delta) and TACTIX_STAT_IRREDUNDANT_CLAUSES. O(arena) walk —
// a per-round/statistics query, never a hot-path one.
uint64_t kernel_irredundant_clauses(const Kernel &);

// --- internal invariants (plan §8.2 oracle 4; debug builds) --------------
// aborts on: values/trail/unassigned incoherence, frame/level corruption,
// watch-list damage, or a missed conflict (fully propagated state with a
// falsified clause). No-op in NDEBUG builds.
void kernel_check_invariants(const Kernel &);

} // namespace tactix
