// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

class Proof;

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
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary; V4)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds (C1)
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked (C1)
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds (C2)
  uint64_t strengthen_checked = 0; // Strengthen candidate checks (C2)
  uint64_t strengthened = 0;       // clauses strengthened (C2 add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by C2's fused round
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
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_failed = 0;  // sources abandoned to Probe as failed (V4)
  uint64_t substitute_rounds = 0;  // Substitute rounds (V3) — detection passes
  uint64_t congruence_rounds = 0;  // Congruence rounds (V6) — gate-closure passes
  // V6 merges dropped because no checkable LRAT chain could be built for them
  // (a bridging BIG path blocked by a root-assigned literal). Sound — the
  // inference is lost, never emitted unjustified. Expected 0; a nonzero count is
  // a real signal, not noise.
  uint64_t congruence_unjustified = 0;
  // Root units derived from degenerate gates (V6's unit channel, D-CONG-UNIT):
  // an XOR whose representative inputs all cancel forces its output.
  uint64_t congruence_units = 0;
  uint64_t factorings = 0;         // Factor/BVA applications (E2) — grids replaced
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
  std::vector<int8_t> saved;      // by var: saved phase (+1/-1)

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

  // VMTF (focused-mode decision heuristic)
  std::vector<Link> links; // by var
  uint32_t queue_first = UINT32_MAX, queue_last = UINT32_MAX;
  uint32_t queue_stamp = 0;
  uint32_t search_idx = UINT32_MAX, search_stamp = 0;

  // EVSIDS score heap (stable-mode decision heuristic; kissat heap.h /
  // inlineheap.h): pos-indexed binary max-heap over vars. pos UINT32_MAX
  // means "not in heap" (lazily re-inserted on unassign while stable).
  struct {
    std::vector<uint32_t> stack; // heap array of vars, max at stack[0]
    std::vector<uint32_t> pos;   // by var: index into stack, or UINT32_MAX
    std::vector<double> score;   // by var
  } scores;
  double scinc = 1.0; // EVSIDS bump increment (grows by 1/(1-decay))

  // active instance-ranking mode for Decide (§5.1 H-state): false = VMTF
  // queue (focused), true = score heap (stable). Flipped only by the S4
  // SwitchMode rule; WHEN to flip is policy state (rev 17).
  bool stable = false;

  // target phases (kissat phases.c): assignment snapshot at the deepest
  // consistent trail seen while stable; preferred over saved phase there
  std::vector<int8_t> target; // by var: +1/-1, 0 = never saved
  uint32_t target_assigned = 0;
  // best phases (kissat phases.c, S3 Rephase): like target, but the
  // watermark only resets when Rephase consumes it (type Best)
  std::vector<int8_t> best; // by var: +1/-1, 0 = never saved
  uint32_t best_assigned = 0;

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

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);

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
// assignment_level. Bit-exact under DefaultPolicy: at a propagation fixpoint
// the true level always equals the current level, so s055 is unchanged.
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

// --- VMTF ------------------------------------------------------------------
void kernel_queue_init(Kernel &);
void kernel_queue_bump(Kernel &, Var v); // move to front (most recent)
Var kernel_queue_next_unassigned(Kernel &); // last-enqueued unassigned var
// reset the queue search cache to the queue tail (entering focused mode)
void kernel_reset_queue_search(Kernel &);

// --- EVSIDS score heap (stable mode) ----------------------------------------
void kernel_heap_push(Kernel &, Var v);    // insert (must not be contained)
void kernel_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void kernel_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
void kernel_update_scores(Kernel &);       // push all missing vars (enter stable)
Var kernel_heap_next_unassigned(Kernel &); // max-score unassigned (lazy pop)
inline bool kernel_heap_contains(const Kernel &k, Var v) {
  return k.scores.pos[v] != UINT32_MAX;
}
// snapshot assigned values into target/best phases when the trail is the
// deepest consistent one seen past each watermark (kissat backtrack.c
// kissat_update_target_and_best_phases). Stable mode only; Rephase (S3)
// consumes best and resets the watermarks.
void kernel_update_target_phases(Kernel &);

// number of root-fixed assignments (the level-0 trail prefix)
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
// allocates and consecutive calls can share/dedup one buffer.
void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries);
// Seed the walk from the recorded conflict (binary in conflict_bin, else the
// large clause at conflict_ref): walk every conflicting literal's reason and
// return the conflicting clause's id (the RUP chain's closing hint). The
// probe-conflict case of every probing rule.
uint64_t kernel_conflict_reason_walk(
    Kernel &, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries);

// --- decision ranking dispatch (§5.1) ---------------------------------------
inline Var kernel_next_decision_var(Kernel &k) {
  return k.stable ? kernel_heap_next_unassigned(k)
                  : kernel_queue_next_unassigned(k);
}
// phase priority (kissat decide.c kissat_decide_phase): target (stable
// only) -> saved; both default positive
inline Lit kernel_decide_lit(const Kernel &k, Var v) {
  int8_t phase = k.stable ? k.target[v] : 0;
  if (phase == 0)
    phase = k.saved[v];
  return make_lit(v, phase < 0);
}

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
