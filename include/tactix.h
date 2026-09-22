/* Tactix public C API — v1, frozen at the phase-2 review (plan §5.1).
 * Post-v1 changes are additive only.
 *
 * N1 guarantee: at any solver state, every rule whose guard holds can be
 * discovered (tactix_rules_applicable), its instances enumerated
 * (tactix_rule_instances), and any of them applied (tactix_apply /
 * tactix_apply_instance); branching in particular is fully client-controlled
 * via tactix_decide_candidates / tactix_decide. This surface exists in every
 * build and every mode.
 *
 * External policies are pull-based (O5): they drive this stepping surface
 * themselves and observe the solver through tactix_stat. There is no callback
 * registration in v1.
 *
 * Instances (O3): a rule instance is a uint64_t payload whose encoding the
 * rule documents.
 *  - Decide, Probe, Backbone, TransitiveReduce: the DIMACS literal,
 *    zero-extended, i.e. TACTIX_LIT_INSTANCE(elit) below. Zero-extension is
 *    required, not stylistic: a sign-extending cast maps -1 onto
 *    TACTIX_INSTANCE_CANONICAL, so branching on -1 would silently get the
 *    canonical pick — an N1 hole.
 *  - Restart: the keep-level. Reduce: v1 payloads are ranked victim counts;
 *    its additive semantic surface returns tagged opaque payloads that select
 *    one legal redundant clause independently of policy protection.
 *    ConflictAnalysis: the backjump level it may return to, 0..level-1.
 *  - Sweep: the seed it sweeps around, as an OPAQUE value obtained from
 *    tactix_rule_instances. It is an internal variable index, not a DIMACS
 *    literal and not constructible by the caller: enumerate, then apply one of
 *    the values returned, within the same solver state.
 *  - Propagate and SwitchMode have no instance surface at all, so a
 *    non-canonical payload to tactix_apply_instance on one of them is an
 *    invalid argument (-1), never a silent canonical application. Congruence
 *    and Walk currently enumerate only the canonical sentinel. Factor's scalar
 *    surface enumerates seed literals; online steps can carry a typed exact
 *    grid, while COMPLETE_PASS exhaustively discovers exact legal grids. These
 *    sets are pinned by tests/test_api_instances.c;
 *    joining or leaving one is deliberate, so update that test and this
 *    comment together.
 * Enumeration fills a caller buffer with the rule's own instances; a rule with
 * no surface reports exactly the canonical sentinel, and one with a surface
 * need not list the canonical instance at all — it is always reachable as
 * TACTIX_INSTANCE_CANONICAL. Values are snapshots, valid until the next
 * state-mutating call. Errors are -1 int returns (unknown rule, guard does not
 * hold, invalid argument). */
#ifndef TACTIX_H_INCLUDED
#define TACTIX_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tactix_solver tactix;

#define TACTIX_INSTANCE_CANONICAL UINT64_MAX

/* Literal-instance payload: the DIMACS literal's 32-bit pattern, zero-extended
   into [1, UINT32_MAX] — disjoint from TACTIX_INSTANCE_CANONICAL by
   construction, so every literal (notably -1) stays expressible (N1). */
#define TACTIX_LIT_INSTANCE(elit) ((uint64_t)(uint32_t)(int32_t)(elit))

/* proof formats (N2) */
#define TACTIX_PROOF_DRAT_BINARY 0
#define TACTIX_PROOF_DRAT_ASCII 1
#define TACTIX_PROOF_LRAT 2 /* native LRAT: antecedent hints (plan O4) */
#define TACTIX_PROOF_DSR_BINARY 3 /* canonical DSR stream */
#define TACTIX_PROOF_DSR_ASCII 4  /* diagnostic DSR spelling */

/* read-only statistics ids; the list grows additively */
#define TACTIX_STAT_CONFLICTS 0
#define TACTIX_STAT_DECISIONS 1
#define TACTIX_STAT_PROPAGATIONS 2
#define TACTIX_STAT_TICKS 3
#define TACTIX_STAT_LEARNED 4
#define TACTIX_STAT_RESTARTS 5
#define TACTIX_STAT_REDUCTIONS 6
#define TACTIX_STAT_SWITCHES 7
#define TACTIX_STAT_LEVEL 8
#define TACTIX_STAT_UNASSIGNED 9
#define TACTIX_STAT_LAST_LEARNED_GLUE 10
#define TACTIX_STAT_STABLE 11
#define TACTIX_STAT_REPHASED 12
#define TACTIX_STAT_SUBSUMPTIONS 13 /* SubsumeForward rounds */
/* live irredundant large clauses + binaries (kissat BINIRR_CLAUSES):
 * the size input of the default inprocessing interval scaling */
#define TACTIX_STAT_IRREDUNDANT_CLAUSES 14
#define TACTIX_STAT_ROOT_FIXED 15      /* root-level (fixed) assignments */
#define TACTIX_STAT_BINARY_CLAUSES 16  /* distinct binary clauses added */
#define TACTIX_STAT_STRENGTHENED 17    /* clauses strengthened */
#define TACTIX_STAT_VIVIFIED 18        /* clauses distilled */
#define TACTIX_STAT_PROBE_FAILED 19    /* failed literals -> units */
#define TACTIX_STAT_BACKBONE_FAILED 20 /* binary failed literals -> units */
#define TACTIX_STAT_TRANSITIVE_DELETED 21 /* redundant binaries deleted */
/* The plan §5.6 reward/cost counters, exposed so an external or learned policy
 * can meter the work its schedule buys. Every rule family the inprocessing
 * schedule doses must expose a (yield, effort) PAIR, and the effort id must be
 * the counter that family's `EFFORT_SPECS` meter reads, so yield/effort is a
 * real cost per unit of output — the axis that orders these rules' outcomes,
 * since the same rule can cost three orders of magnitude more per unit of yield
 * on one instance than on another. tools/docs/check-stat-ids.py enforces both
 * halves: a new dosed rule fails ctest until its pair is exposed here. */
#define TACTIX_STAT_FACTORINGS 22       /* Factor/BVA grids applied */
#define TACTIX_STAT_FACTOR_TICKS 23     /* Factor work, cache-line units */
#define TACTIX_STAT_BVE_ELIMINATED 24   /* variables eliminated */
#define TACTIX_STAT_BVE_RESOLUTIONS 25  /* BVE work: resolvent attempts */
#define TACTIX_STAT_BCE_DELETED 26      /* blocked clauses deleted */
#define TACTIX_STAT_BCE_TICKS 27        /* BCE work: occurrences + pairs */
#define TACTIX_STAT_TERNARY_RESOLVENTS 28 /* ternary resolvents added */
#define TACTIX_STAT_TERNARY_TICKS 29    /* HyperTernaryRes work */
#define TACTIX_STAT_INSTANTIATED 30     /* literals removed (X4) */
#define TACTIX_STAT_REORDERED 31        /* decision re-rankings (S7 policy op) */
#define TACTIX_STAT_VIVIFY_TICKS 32     /* Vivify work: propagation ticks */
#define TACTIX_STAT_PROBE_TICKS 33      /* Probe work: propagation ticks */
#define TACTIX_STAT_BACKBONE_TICKS 34   /* Backbone work: propagation ticks */
#define TACTIX_STAT_TRANSITIVE_TICKS 35 /* TransitiveReduce work: cache lines */
/* SubsumeForward: TACTIX_STAT_SUBSUMPTIONS above counts rounds, neither a yield
 * nor an effort, so its pair is clauses deleted / partner visits (cadical
 * `subchecks`). */
#define TACTIX_STAT_FORWARD_SUBSUMED 36 /* clauses deleted by SubsumeForward */
#define TACTIX_STAT_FORWARD_VISITS 37   /* SubsumeForward work: partner visits */
#define TACTIX_STAT_STRENGTHEN_VISITS 38 /* Strengthen work: partner visits */
#define TACTIX_STAT_INSTANTIATE_TICKS 39 /* Instantiate work: probe ticks */
#define TACTIX_STAT_SWEEP_EQUIVALENCES 40 /* pairs proved equivalent (V5) */
#define TACTIX_STAT_SWEEP_TICKS 41      /* Sweep work: scan + sub-solver ticks */
#define TACTIX_STAT_CONGRUENCE_MERGES 42 /* equivalences merged (gate closure) */
#define TACTIX_STAT_CONGRUENCE_TICKS 43 /* Congruence work, its own currency */
#define TACTIX_STAT_CCE_DELETED 44      /* clauses eliminated by CCE (X3) */
#define TACTIX_STAT_CCE_TICKS 45        /* CCE work: clause literals scanned */
#define TACTIX_STAT_SUBSTITUTE_VARS 46  /* variables substituted away */
#define TACTIX_STAT_SUBSTITUTE_SCC_LITS 47 /* Substitute work: literals in the
                                              non-trivial SCCs it walked */

/* X6 HyperBinaryRes exposes ordinary binary outputs and the unit-degenerate
 * semantic outputs separately. Its effort counter is the private tick meter
 * used by the EFFORT_SPECS row. */
#define TACTIX_STAT_HBR_BINARIES 48 /* binary resolvents emitted */
#define TACTIX_STAT_HBR_TICKS 49    /* HyperBinaryRes work */
#define TACTIX_STAT_SUBSTITUTE_TICKS 50 /* Substitute binary-watch scan work */
#define TACTIX_STAT_HBR_UNITS 51 /* semantic resolvents collapsed to units */

tactix *tactix_init(void);
void tactix_release(tactix *);

/* IPASIR-style ingest: literals in DIMACS convention, 0 terminates a
 * clause. */
void tactix_add(tactix *, int lit);

/* Proof logging (N2): attach exactly once, before the first operation that
 * reasons about the input (including status/enumeration/solve calls).
 * TACTIX_PROOF_* selects the format. Returns 0, or -1 for a late/duplicate
 * attachment, an unwritable path, or an unsupported format. */
int tactix_proof(tactix *, const char *path, int format);

/* BVE gate-type selection (additive): bitmask of the gate types BVE may
 * recognize; default (unset) is all. A performance knob only — every mask is
 * sound, a disabled type just falling back to the full resolvent cross product.
 * Settable at any state (N1). */
#define TACTIX_BVE_GATE_EQUIV 1u  /* equivalence x<->~y */
#define TACTIX_BVE_GATE_AND_OR 2u /* AND/OR x<->OR/AND(...) */
#define TACTIX_BVE_GATE_ITE 4u    /* if-then-else x<->(c?t:e) */
#define TACTIX_BVE_GATE_XOR 8u    /* parity x<->a1^...^am */
#define TACTIX_BVE_GATE_DEFINITION 16u /* general x<->phi, via the sub-solver */
void tactix_set_bve_gates(tactix *, unsigned mask);

/* Kernel configuration setters — the pose surface for external and learned
 * policies (§5.6). Every value is sound: config narrows or reorders which sound
 * applications happen, never their proof obligations. Settable at any state
 * (N1).
 *  - bve_bound: BVE growth slack (eliminate iff resolvents <= occs + bound)
 *  - factor_bound: Factor acceptance (accept a grid iff delta > bound)
 *  - factor_composite: nonzero = Factor composes over extension vars
 *  - work_budget: per-round budget for budget-aware rules, in each rule's own
 *      currency (Vivify: propagation ticks; Factor: factor ticks); 0 = off
 *  - vivify_scope: which clause band Vivify may touch. 0 = all, 1 = redundant
 *      within the live tier2, 2 = irredundant, 3/4/5 = the redundant tier1 /
 *      tier2 / tier3 bands. A value above 5 leaves the setting unchanged and is
 *      never substituted by another band, which would remove the capability
 *      rather than decline it (N1). */
void tactix_set_bve_bound(tactix *, unsigned bound);
void tactix_set_factor_bound(tactix *, unsigned bound);
void tactix_set_factor_composite(tactix *, int on);
void tactix_set_work_budget(tactix *, uint64_t budget);
void tactix_set_vivify_scope(tactix *, unsigned scope);
/* Complete Vivify pose. Returns -1 for an unknown scope or zero population
 * ratio and leaves the prior pose untouched. */
int tactix_set_vivify_parameters(tactix *, unsigned scope, int reuse,
                                 int flush, uint32_t irredundant_sort_ratio);

/* Canonical C1/C2 discovery costs, never explicit-action preconditions.
 * UINT32_MAX removes a cap. Explicit clause instances bypass these values. */
void tactix_set_subsume_limits(tactix *, uint32_t clause_size,
                              uint32_t large_occurrences,
                              uint32_t binary_occurrences);
void tactix_set_strengthen_limits(tactix *, uint32_t clause_size,
                                  uint32_t occurrences);
void tactix_set_analyze_minimize_depth(tactix *, uint32_t depth);
int tactix_set_congruence_limits(tactix *, uint32_t and_arity,
                                 uint64_t gate_count, uint32_t xor_arity);
#define TACTIX_CONGRUENCE_CANONICAL_WATCH_ORDER 0
#define TACTIX_CONGRUENCE_PRESERVE_WATCH_ORDER 1
/* Controls entry ordering only; inference and required cleanup remain active.
 * Invalid values return -1 without changing configuration. */
int tactix_set_congruence_watch_order(tactix *, int order);
void tactix_set_instantiate_limits(tactix *, uint32_t minimum_clause_size,
                                   uint32_t occurrence_limit,
                                   uint32_t minimum_unassigned,
                                   int include_redundant);
void tactix_set_instantiate_material_threshold(tactix *, uint32_t threshold);
void tactix_set_backbone_round_limits(tactix *, uint32_t initial_rounds,
                                      uint32_t maximum_rounds);

/* Config readback (additive) — the pose surface's anti-vacuity half: a policy
 * that poses a value must be able to prove the solver holds it, or a silently
 * dropped setting yields an arm that measures nothing while reading as
 * configured. One id per setter above, same order; returns the stored value, or
 * UINT64_MAX for an unknown id. */
#define TACTIX_CFG_BVE_GATES 0
#define TACTIX_CFG_BVE_BOUND 1
#define TACTIX_CFG_FACTOR_BOUND 2
#define TACTIX_CFG_FACTOR_COMPOSITE 3
#define TACTIX_CFG_WORK_BUDGET 4
#define TACTIX_CFG_VIVIFY_SCOPE 5
#define TACTIX_CFG_SUBSUME_CLAUSE_SIZE 6
#define TACTIX_CFG_SUBSUME_LARGE_OCCURRENCES 7
#define TACTIX_CFG_SUBSUME_BINARY_OCCURRENCES 8
#define TACTIX_CFG_STRENGTHEN_CLAUSE_SIZE 9
#define TACTIX_CFG_STRENGTHEN_OCCURRENCES 10
#define TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH 11
#define TACTIX_CFG_CONGRUENCE_AND_ARITY 12
#define TACTIX_CFG_CONGRUENCE_GATE_COUNT 13
#define TACTIX_CFG_CONGRUENCE_XOR_ARITY 14
#define TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE 16
#define TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT 17
#define TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED 18
#define TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT 19
#define TACTIX_CFG_BACKBONE_INITIAL_ROUNDS 20
#define TACTIX_CFG_BACKBONE_MAXIMUM_ROUNDS 21
#define TACTIX_CFG_VIVIFY_REUSE 22
#define TACTIX_CFG_VIVIFY_FLUSH 23
#define TACTIX_CFG_VIVIFY_IRREDUNDANT_SORT_RATIO 24
#define TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD 25
#define TACTIX_CFG_CONGRUENCE_WATCH_ORDER 26
uint64_t tactix_config(tactix *, int cfg_id);

/* run the default policy to completion: 10 SAT / 20 UNSAT */
int tactix_solve(tactix *);

/* model access after SAT: >0 true, <0 false (DIMACS convention) */
int tactix_value(tactix *, int lit);

/* --- R3/N1: rule introspection and stepwise application ---------------- */

/* number of registered rule families (stable ids: 0..count-1, sorted by
 * name) */
int tactix_rule_count(void);
const char *tactix_rule_name(int rule_id);

/* Action kind (plan §4): a proof rule, which affects the proof state and so
 * carries a proof obligation, or an H-only policy operation. Both share one
 * name-sorted id space. Returns TACTIX_ACTION_*, or -1 if rule_id is out of
 * range. */
#define TACTIX_ACTION_PROOF_RULE 0
#define TACTIX_ACTION_POLICY_OP 1
int tactix_action_kind(int rule_id);

/* Fill `out` with the ids of rules whose guard (the soundness precondition)
 * currently holds; returns the count (<= cap). Any of these may be applied now,
 * in any order — this is theoretical applicability, N1. */
int tactix_rules_applicable(tactix *, int *out, int cap);

/* Fill `out` with up to `cap` instance payloads of a rule, ranked with the
 * canonical instance first (encodings in the header comment). Returns the
 * count, or -1 if the rule is unknown or its guard does not hold. Rules without
 * per-application choice yield exactly TACTIX_INSTANCE_CANONICAL. */
int tactix_rule_instances(tactix *, int rule_id, uint64_t *out, int cap);

/* apply a rule's canonical instance. Returns: 0 progress, 1 no-op,
 * 2 conflict found, 10 SAT concluded, 20 UNSAT concluded, -1 guard does
 * not hold. */
int tactix_apply(tactix *, int rule_id);

/* Apply a chosen instance; same returns as tactix_apply, plus -1 for a
 * non-canonical payload on a rule with no instance surface. Out-of-range
 * payloads on a rule that has one are clamped or no-ops per that rule's
 * documented encoding, never unsound. */
int tactix_apply_instance(tactix *, int rule_id, uint64_t instance);

/* Additive fine-grained action surface. Most rules delegate to the frozen v1
 * payloads. Rules whose v1 payload grouped multiple choices enumerate opaque
 * semantic payloads here; Instantiate, for example, exposes one payload per
 * (large-clause,literal) pair. Values are valid until the next mutation. */
int tactix_rule_semantic_instances(tactix *, int rule_id, uint64_t *out,
                                   int cap);
int tactix_apply_semantic_instance(tactix *, int rule_id, uint64_t instance);

/* --- additive online strategy composition -------------------------------
 * Configuration is copied and validated transactionally. All pointers only
 * need to remain valid for the configure call. One advance performs at most
 * one atomic registered rule application; the caller may reconfigure between
 * advances without restarting or discarding learned state. */
#define TACTIX_ONLINE_GRANULARITY_ONE 0
#define TACTIX_ONLINE_GRANULARITY_BATCH 1
#define TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS 2
#define TACTIX_ONLINE_GRANULARITY_CANONICAL 3
#define TACTIX_ONLINE_REPEAT_ONCE 0
#define TACTIX_ONLINE_REPEAT_COUNT 1
#define TACTIX_ONLINE_REPEAT_FIXPOINT 2
/* FIXPOINT whose round ceiling is one pass plus
 * clamp(ceil((2^23 / (literals + variables))^1.5), 1, limit) repetitions, read
 * from the live formula at every round; limit 0 = scale only. */
#define TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT 3
#define TACTIX_ONLINE_POST_NONE 0
#define TACTIX_ONLINE_POST_PROPAGATE 1
#define TACTIX_ONLINE_WORK_APPLICATIONS 0
#define TACTIX_ONLINE_WORK_BVE_TICKS 1
#define TACTIX_ONLINE_WORK_PROPAGATIONS 2
#define TACTIX_ONLINE_RECONFIGURE_RESET 0
#define TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS 1

/* Native rule-work allowance, separate from the composition budget below.
 * INHERIT preserves the solver's current rule profile. UNLIMITED clears it.
 * BOUNDED accepts zero and is polled at each rule's documented safe points;
 * an indivisible rule transition may therefore finish past the allowance. */
#define TACTIX_ONLINE_RULE_BUDGET_INHERIT 0
#define TACTIX_ONLINE_RULE_BUDGET_UNLIMITED 1
#define TACTIX_ONLINE_RULE_BUDGET_BOUNDED 2

/* Tri-state overrides for shared canonical rule settings. Zero-initialized
 * callers inherit the active strategy profile. */
#define TACTIX_ONLINE_SETTING_INHERIT 0
#define TACTIX_ONLINE_SETTING_DISABLE 1
#define TACTIX_ONLINE_SETTING_ENABLE 2

#define TACTIX_ONLINE_PARAMETERS_INHERIT 0
#define TACTIX_ONLINE_PARAMETERS_BVE 1
#define TACTIX_ONLINE_PARAMETERS_SUBSUME 2
#define TACTIX_ONLINE_PARAMETERS_STRENGTHEN 3
#define TACTIX_ONLINE_PARAMETERS_CONGRUENCE 4
#define TACTIX_ONLINE_PARAMETERS_INSTANTIATE 5
#define TACTIX_ONLINE_PARAMETERS_BACKBONE 6
#define TACTIX_ONLINE_PARAMETERS_SWEEP 7
#define TACTIX_ONLINE_PARAMETERS_VIVIFY 8
#define TACTIX_ONLINE_PARAMETERS_ANALYZE 9
#define TACTIX_ONLINE_PARAMETERS_BCE 10
#define TACTIX_ONLINE_PARAMETERS_TERNARY 11
#define TACTIX_ONLINE_PARAMETERS_FACTOR 12
#define TACTIX_ONLINE_PARAMETERS_TRANSITIVE 13
#define TACTIX_ONLINE_PARAMETERS_SUBSTITUTE 14
#define TACTIX_ONLINE_PARAMETERS_REDUCE 15
#define TACTIX_ONLINE_PARAMETERS_WALK 16
#define TACTIX_ONLINE_PARAMETERS_REORDER 17
#define TACTIX_ONLINE_PARAMETERS_PARITY_MODEL 18
#define TACTIX_ONLINE_PARAMETERS_ONEHOT_MODEL 19
#define TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY 20
#define TACTIX_ONLINE_PARAMETERS_AFFINE_MODEL 21
#define TACTIX_ONLINE_PARAMETERS_BALANCED_SIGNATURE_MODEL 22
#define TACTIX_ONLINE_PARAMETERS_FERMAT_MODEL 23
#define TACTIX_ONLINE_PARAMETERS_LOOKUP_CIRCUIT_MODEL 24
#define TACTIX_ONLINE_PARAMETERS_SIMON_MODEL 25
#define TACTIX_ONLINE_PARAMETERS_STCONN_PROOF 26
#define TACTIX_ONLINE_PARAMETERS_SIGNED_CARDINALITY_PROOF 27
#define TACTIX_ONLINE_PARAMETERS_EXACT_COVER_MODEL 28
#define TACTIX_ONLINE_PARAMETERS_INDEPENDENT_SET_MODEL 29
#define TACTIX_ONLINE_BVE_TRAVERSAL_ELIMINATE 0
#define TACTIX_ONLINE_BVE_TRAVERSAL_FAST_ELIMINATE 1
#define TACTIX_ONLINE_AFFINE_BOUNDARY 0
#define TACTIX_ONLINE_AFFINE_ALL_PARITY 1
#define TACTIX_ONLINE_AFFINE_EXPLICIT 2

#define TACTIX_ONLINE_BVE_CANDIDATES_ALL 0
#define TACTIX_ONLINE_BVE_CANDIDATES_PURE_ONLY 1
/* The complete pure-literal closure, enumerated layer by layer from one
 * occurrence image: the sequence a repeated PURE_ONLY pass would select. */
#define TACTIX_ONLINE_BVE_CANDIDATES_PURE_CLOSURE 2

#define TACTIX_ONLINE_SR_EXPLICIT_ONLY 0
#define TACTIX_ONLINE_SR_ORBITOPAL 1
#define TACTIX_ONLINE_SR_GENERAL_COMPLETE 2
#define TACTIX_ONLINE_SR_GRAPH_SYMMETRY 3

#define TACTIX_ONLINE_ORBITOPAL_ROWS 0
#define TACTIX_ONLINE_ORBITOPAL_COLUMNS 1
#define TACTIX_ONLINE_ORBITOPAL_BOTH 2

#define TACTIX_ONLINE_ORBITOPAL_ORDER_CANONICAL 0
#define TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_MAXIMUM_CLIQUE 1
#define TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_GREEDY_CLIQUE 2
#define TACTIX_ONLINE_ORBITOPAL_ORDER_COOCCURRENCE_BOUNDED_GREEDY_CLIQUE 3
#define TACTIX_ONLINE_ORBITOPAL_COMPLETION_MATRIX_ONLY 0
#define TACTIX_ONLINE_ORBITOPAL_COMPLETION_INDIVIDUALIZED_GRAPH 1
#define TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_THEN_UNIQUE_LITERAL 0
#define TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_THEN_REFINEMENT 1
#define TACTIX_ONLINE_ORBITOPAL_DISCOVERY_REFINEMENT_ONLY 2
#define TACTIX_ONLINE_ORBITOPAL_DISCOVERY_UNIQUE_LITERAL_ONLY 3
#define TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_DETECTED_GENERATORS 0
#define TACTIX_ONLINE_ORBITOPAL_ROW_ORBITS_REFINEMENT_CELLS 1

#define TACTIX_ONLINE_SYMMETRY_STABILIZER_ONLY 0
#define TACTIX_ONLINE_SYMMETRY_GENERATORS_ONLY 1
#define TACTIX_ONLINE_SYMMETRY_STABILIZER_THEN_GENERATORS 2

#define TACTIX_ONLINE_SYMMETRY_ORDER_CANONICAL 0
#define TACTIX_ONLINE_SYMMETRY_ORDER_REVERSE 1
#define TACTIX_ONLINE_SYMMETRY_ORDER_SEEDED 2

#define TACTIX_ONLINE_SYMMETRY_CLAUSES_UNIQUE_LITERALS 0
#define TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL 1

#define TACTIX_ONLINE_SYMMETRY_PAIR_CANONICAL 0
#define TACTIX_ONLINE_SYMMETRY_PAIR_REVERSE 1
#define TACTIX_ONLINE_SYMMETRY_PAIR_SEEDED 2

#define TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_CLAUSE_VERTICES 1
#define TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_DIRECT_EDGES 2
#define TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_AUTOMATIC 3

#define TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_INPUT_ORDER 1
#define TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_SHORTEST 2
#define TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_AMO_THEN_SHORTEST 3

#define TACTIX_ONLINE_SYMMETRY_CHAIN_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_CHAIN_CLAUSAL_FIXING 1
#define TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT 2
#define TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT_RESIFT 3

#define TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_PUBLISH 1
#define TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_FORCING 2
#define TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_REQUIRE_COMPOUND_PROGRESS 3

#define TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_WHOLE_MATRIX 1
#define TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_AXIS_ANCHORS 2
#define TACTIX_ONLINE_SYMMETRY_BASE_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_BASE_CLAUSE_ORDER 1
#define TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO 2
#define TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_COMMON_CLAUSE 3
#define TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_ORDERED_REMAINDER 4
#define TACTIX_ONLINE_SYMMETRY_BASE_FIRST_AMO_THEN_FORWARD_REMAINDER 5
#define TACTIX_ONLINE_SYMMETRY_BASE_SHORTEST_CLAUSE 6
#define TACTIX_ONLINE_SYMMETRY_BASE_FIRST_SHORTEST_THEN_FORWARD_REMAINDER 7
/* Base point of a FIX_AND_CUT cut level once no clause is fixable. */
#define TACTIX_ONLINE_SYMMETRY_CUT_POINT_INHERIT 0
#define TACTIX_ONLINE_SYMMETRY_CUT_POINT_FIRST_OPEN 1
#define TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST 2

typedef struct {
  uint32_t minimize_depth_limit;
  uint32_t chronological_levels, bump_reason_limit, bump_reason_depth;
  uint32_t eager_subsume;
  int conflict_reuse;
  /* These names preserve the historical behavior of zero-initialized steps. */
  int disable_jump_reasons;
  int enable_jump_reasons_bigbig;
  int disable_shrink_glue_promotion;
  int exclude_extension_reason_bumps;
} tactix_online_analyze_parameters;

typedef struct {
  uint32_t gate_mask, growth_bound, occurrence_limit, clause_size_limit;
  uint32_t xor_arity_limit, resolvent_size_limit;
  uint32_t backward_occurrence_limit, definition_cores;
  uint32_t definition_core_budget_multiplier;
  uint64_t definition_budget;
  int backward;
  /* Appended generated-pass scope. Explicit selections remain unrestricted. */
  int candidate_scope;
  /* Appended absolute non-tautological resolvent cap. Zero preserves the
     historical unlimited behavior for zero-initialized public steps. */
  uint32_t resolvent_count_limit;
  /* Appended physical traversal profile. Zero retains ordinary Eliminate. */
  int traversal;
} tactix_online_bve_parameters;
typedef struct {
  uint32_t clause_size_limit, large_occurrence_limit;
  uint32_t binary_occurrence_limit;
} tactix_online_subsume_parameters;
typedef struct {
  uint32_t clause_size_limit, occurrence_limit;
} tactix_online_strengthen_parameters;
typedef struct {
  uint32_t and_arity_limit, xor_arity_limit;
  uint64_t gate_count_limit;
  int binary_watch_order; /* TACTIX_CONGRUENCE_*_WATCH_ORDER; zero is canonical. */
} tactix_online_congruence_parameters;
typedef struct {
  uint32_t minimum_clause_size, occurrence_limit, minimum_unassigned;
  uint32_t new_material_threshold;
  int include_redundant;
} tactix_online_instantiate_parameters;
typedef struct {
  uint32_t initial_rounds, maximum_rounds;
} tactix_online_backbone_parameters;
typedef struct {
  uint32_t max_depth, max_depth_ceiling;
  uint32_t max_env_vars, max_env_vars_ceiling;
  uint32_t max_env_clauses, max_env_clauses_ceiling;
  uint32_t max_candidates, max_proof_steps, max_seeds, flip_rounds;
  uint64_t env_ticks, round_ticks;
  int strip_false, measure_closure, full_partition, skip_embedded_fixed, resume;
  int force_general_proof;
} tactix_online_sweep_parameters;
typedef struct {
  uint32_t scope, irredundant_sort_ratio;
  int reuse, flush;
} tactix_online_vivify_parameters;
typedef struct {
  uint32_t occurrence_limit;
} tactix_online_bce_parameters;
typedef struct {
  uint32_t occurrence_limit, maximum_add_percent;
} tactix_online_ternary_parameters;
typedef struct {
  uint32_t maximum_clause_size, growth_bound, candidate_rounds;
  int composite, demote_fresh;
} tactix_online_factor_parameters;
typedef struct {
  int early_exit, live_meter, checked_budget_meter;
} tactix_online_transitive_parameters;
typedef struct {
  int demote_substituted;
} tactix_online_substitute_parameters;
typedef struct {
  uint32_t tier1, tier2;
  /* A zero denominator selects the reference 3/4 fraction. With a nonzero
   * denominator, numerator may be zero and must not exceed denominator.
   * Switch modes use TACTIX_ONLINE_SETTING_INHERIT/DISABLE/ENABLE; INHERIT
   * selects the reference value for these appended source-compatible fields. */
  uint32_t victim_numerator, victim_denominator;
  int tier_protection_mode, compact_collection_mode;
  int reduction_bookkeeping_mode;
} tactix_online_reduce_parameters;
typedef struct {
  uint32_t average_clause_size_milli, break_base_milli;
} tactix_online_walk_score_point;
typedef struct {
  uint64_t step_budget, random_seed;
  /* Appended scoring fields: zero selects the reference default so existing
   * zero-initialized source callers retain their behavior. Explicit break
   * bases use positive per-mille fixed point. A nonzero point
   * count requires a copied, strictly size-ascending curve. */
  uint32_t even_break_base_milli;
  const tactix_online_walk_score_point *score_curve;
  int score_curve_count;
} tactix_online_walk_parameters;
typedef struct {
  uint32_t maximum_clause_size;
  /* Appended fields use per-mille fixed point; zero selects the reference
   * default for source compatibility. */
  uint32_t clause_decay_milli, binary_weight_milli;
  uint32_t balanced_polarity_milli;
} tactix_online_reorder_parameters;
typedef struct {
  /* Parameters for Lucky semantic instance 6 (structural parity model). */
  uint32_t maximum_variables, maximum_groups, maximum_constraints;
  uint32_t minimum_group_clauses, minimum_equations;
  uint32_t maximum_free_variables;
  uint64_t candidate_budget;
  /* Exact even-incidence XOR refutation and its streaming proof bounds.
   * Zero keeps the solver defaults for backward-compatible callers. */
  uint32_t refutation_maximum_variables, refutation_maximum_groups;
  uint32_t refutation_maximum_constraints, refutation_maximum_arity;
  uint32_t refutation_probe_constraints, refutation_minimum_probe_repeats;
  uint64_t refutation_maximum_proof_additions;
} tactix_online_parity_model_parameters;
typedef struct {
  /* Parameters for Lucky semantic instance 7 (one-hot GF(3) model). */
  uint32_t maximum_variables, maximum_domains, maximum_constraints;
  uint32_t maximum_equation_arity, minimum_domains, minimum_equations;
  uint32_t maximum_free_domains;
  uint64_t candidate_budget;
  /* Bipartite exact-one mod-3 refutation and its certificate bounds. Zero
   * keeps the solver defaults for backward-compatible callers. */
  uint32_t refutation_maximum_variables, refutation_maximum_groups;
  uint32_t refutation_maximum_constraints, refutation_maximum_arity;
  uint64_t refutation_maximum_proof_additions;
} tactix_online_onehot_model_parameters;
typedef struct {
  /* Parameters for Lucky semantic instance 10.  Recognition is structural;
   * these fields control its general balanced-signature local search and
   * private model-completion allowance. */
  uint32_t maximum_balls, maximum_width;
  uint64_t restarts, steps_per_restart;
  uint32_t samples_per_step;
  uint64_t seed;
  uint32_t completion_rounds;
} tactix_online_balanced_signature_model_parameters;
typedef struct {
  /* Parameters for Lucky instance 11.  The comparator window and Pollard-Rho
   * work controls follow the owning arithmetic-circuit strategy. */
  uint32_t maximum_bits, comparator_tail_clauses;
  uint64_t rho_constants, rho_iterations;
} tactix_online_fermat_model_parameters;
typedef struct {
  /* Parameters for Lucky instance 13. Recognition recovers feed-forward
   * lookup tables; these bounds dose SIMD primary-support enumeration. */
  uint32_t maximum_fanin, maximum_fanout, maximum_primary_inputs;
  uint32_t maximum_support;
  uint64_t maximum_vector_gate_evaluations;
} tactix_online_lookup_circuit_model_parameters;
typedef struct {
  /* Parameters for Lucky instance 14. Recognition recovers the Simon round
   * circuit; these fields bound the admitted rounds and exhaustive key work. */
  uint32_t maximum_rounds;
  uint64_t maximum_candidates;
} tactix_online_simon_model_parameters;
typedef struct {
  /* Parameters for Lucky instance 15. The bounds dose exact structural
   * recognition and its bounded-width crossing-sequence RUP refutation. */
  uint32_t maximum_width, maximum_columns;
  uint64_t maximum_rup_clauses;
} tactix_online_stconn_proof_parameters;
typedef struct {
  /* Parameters for Lucky instance 16. These bounds govern exact complete
   * signed-ternary recognition and its native BDD cutting-planes proof. */
  uint32_t maximum_variables, maximum_groups, maximum_constraints;
  uint32_t maximum_group_arity;
  uint64_t maximum_bdd_nodes, maximum_proof_additions;
} tactix_online_signed_cardinality_proof_parameters;
typedef struct {
  /* Parameters for Lucky instance 17: signed at-most-one block exact cover.
   * block_arity is the block and covering-clause size; the node bounds dose
   * the deterministic search and each randomized restart. */
  uint32_t block_arity;
  uint64_t maximum_nodes, restarts, restart_nodes, seed;
} tactix_online_exact_cover_model_parameters;
typedef struct {
  /* Parameters for Lucky instance 18: independent set with a layered
   * exact-count encoding. Zero steps or rounds select the vertex count. */
  uint32_t maximum_vertices;
  uint64_t maximum_steps, completion_rounds, seed;
} tactix_online_independent_set_model_parameters;
typedef struct {
  /* UINT64_MAX selects half of the current coordinate count. */
  uint64_t left_size, left_weight, right_weight, total_weight;
} tactix_online_affine_scope;
typedef struct {
  /* Lucky instance 8. Every scalar is explicit; zero limits mean zero work or
   * an empty domain, never an implicit default. Arrays are copied on configure.
   * Projection literals use stable external DIMACS identities. Duplicate or
   * complementary coordinates of the same variable are invalid. A currently
   * unavailable retired coordinate declines this attempt without mutation.
   * Explicit empty projection and empty scopes are both valid requests. */
  uint64_t maximum_variables, maximum_constraints, maximum_arity;
  uint64_t maximum_projection, minimum_equations, maximum_entries;
  uint64_t import_bytes, linear_bytes, table_bytes, maximum_work;
  uint64_t candidate_budget, permutations, seed;
  int projection_mode, automatic_negated, shuffle_first;
  int free_phase, residual_phase;
  const int *projection;
  int projection_count;
  const tactix_online_affine_scope *scopes;
  int scope_count;
} tactix_online_affine_model_parameters;
typedef struct {
  /* Zero is a valid disabling limit. UINT64_MAX is unbounded. */
  uint64_t maximum_validation_ticks;
  /* Deprecated compatibility selectors. New callers set producer below. */
  int orbitopal_producer;
  uint32_t minimum_orbitopal_rows, minimum_orbitopal_columns;
  uint64_t maximum_orbitopal_matrices, maximum_orbitopal_instances;
  uint64_t maximum_detection_ticks;
  /* Mutually exclusive with orbitopal_producer. The general producer walks
   * the complete finite candidate/witness/substitution domain lazily. */
  int general_producer;
  uint64_t maximum_general_candidates_per_advance;
  int producer;
  int orbitopal_orientation;
  uint64_t maximum_graph_vertices, maximum_graph_edges;
  uint64_t maximum_symmetry_generators;
  uint64_t symmetry_backend_search_budget;
  uint64_t symmetry_backend_support_budget;
  uint64_t symmetry_backend_component_size_limit;
  uint64_t maximum_symmetry_fixing_instances;
  uint64_t maximum_symmetry_generator_instances_per_pass;
  uint64_t maximum_symmetry_group_ticks;
  uint32_t symmetry_seed;
  /* Zero selects the C++ default error bound. */
  uint32_t symmetry_error_bound;
  int symmetry_fixing_mode;
  int symmetry_generator_order;
  int symmetry_clause_order;
  int symmetry_literal_order;
  int symmetry_clause_scope;
  int symmetry_pair_orientation;
  int symmetry_direct_generator_involutions_mode;
  /* TACTIX_ONLINE_SETTING_*; INHERIT uses the typed C++ defaults. */
  int symmetry_decompose_mode;
  int symmetry_prefer_depth_first_mode;
  int symmetry_strong_certification_mode;
  /* Structured-action ordering. Bounded greedy retains a valid incumbent on
   * local clique-work exhaustion (zero work retains canonical order). Global
   * work, vertex and interruption limits still stop the current matrix. */
  int orbitopal_column_order;
  uint64_t maximum_orbitopal_clique_vertices;
  uint64_t maximum_orbitopal_clique_ticks;
  /* Zero selects the C++ default (currently five columns). */
  uint32_t minimum_orbitopal_clique_columns;
  int orbitopal_completion;
  /* Compact induced-column group states used by product-action completion.
   * Zero disables that fallback; UINT64_MAX is unbounded. */
  uint64_t maximum_orbitopal_product_states;
  /* TACTIX_ONLINE_SETTING_*; complete unique-literal clauses with pairwise
   * blocked AMO binaries before matrix detection. */
  int orbitopal_unique_literal_amo_mode;
  /* TACTIX_ONLINE_SETTING_*; allow individualized matrix completion to use
   * the configured generic formula generators after direct refinement fails. */
  int orbitopal_generator_fallback_mode;
  /* TACTIX_ONLINE_SETTING_*; INHERIT preserves the typed C++ default,
   * DISABLE examines no refinement roots, and ENABLE uses the following
   * maximum (UINT64_MAX is unbounded). */
  int orbitopal_refinement_mode;
  uint64_t maximum_orbitopal_refinement_orbits;
  /* TACTIX_ONLINE_SETTING_*; stop row/product refinement after the owning
   * Satsuma work allowance produces no complete useful matrix. Productive
   * matrices reset the allowance. */
  int orbitopal_progress_feedback_mode;
  /* TACTIX_ONLINE_SETTING_*; restrict refinement roots by the probed base.
   * A zero factor selects the typed C++ default (currently eight). */
  int orbitopal_base_probe_filter_mode;
  uint32_t orbitopal_base_probe_factor;
  /* Select which complete discovery phase runs first. A productive phase
   * yields to the caller; a scoped-fixpoint repeat resumes with the next
   * formula state. */
  int orbitopal_discovery_order;
  /* Appended exact graph representation. INHERIT preserves the C++ default;
   * automatic mode selects direct binary edges when 1000*binary clauses is at
   * least density*variables. A zero density selects the typed default. */
  int symmetry_binary_graph_mode;
  uint32_t symmetry_binary_graph_density_permille;
  /* Appended selection policy for stabilizer-chain base clauses. INHERIT
   * preserves the typed C++ default. */
  int symmetry_clause_selection;
  /* Appended Schreier schedule. Zero preserves source compatibility and the
   * typed defaults; DefaultPolicy explicitly selects fix-and-cut with
   * common-clause/AMO base ranking. */
  int symmetry_chain_mode;
  /* TACTIX_ONLINE_SYMMETRY_BASE_*; active with FIX_AND_CUT, otherwise dormant. */
  int symmetry_base_selection;
  /* TACTIX_ONLINE_SETTING_*; INHERIT leaves discovery unrestricted. ENABLE
   * fixes assigned root literals pointwise during graph discovery only.
   * Explicit SR application is unaffected. */
  int symmetry_pointwise_root_stabilizer_mode;
  /* Candidate source for individualized row discovery. Refinement cells need
   * no preliminary generic search and still require exact action validation. */
  int orbitopal_row_orbit_source;
  /* Optional FixAndCut planning threshold between complete levels. Zero
   * disables it. The maximum_symmetry_group_ticks hard limit still applies
   * to both planning and finalization. */
  uint64_t symmetry_chain_planning_tick_limit;
  /* TACTIX_ONLINE_SETTING_*; factor disjoint generator supports before group
   * planning. Independent of chain mode; INHERIT retains the disabled default. */
  int symmetry_factor_support_components_mode;
  /* TACTIX_ONLINE_SETTING_*; preserve first detector occurrence before
   * canonical/reverse/seeded traversal. INHERIT keeps canonical sorting. */
  int symmetry_preserve_detector_order_mode;
  /* Independent per-call structured basis and remaining-subgroup discovery.
     INHERIT keeps the default (disabled); matrix settings above are shared. */
  int symmetry_use_structured_generators_mode;
  int symmetry_stabilize_structured_variables_mode;
  /* TACTIX_ONLINE_SYMMETRY_STRUCTURED_SCOPE_*; INHERIT keeps WholeMatrix.
   * Axis anchors are first-column row anchors and first-row column anchors
   * for each complete discovered action; explicit SR legality is unchanged. */
  int symmetry_structured_scope;
  /* Above this variable count, use exact-generator negation/binary fixing
   * instead of constructing a Schreier chain. Zero keeps the typed default
   * (UINT64_MAX, disabled). */
  uint64_t symmetry_schreier_domain_limit;
  /* A complete kind-2-only FixAndCut batch is legal but only changes the
   * search fundamental domain. INHERIT/PUBLISH retains full Satsuma behavior;
   * REQUIRE_FORCING publishes the atomic batch only when it also contains a
   * representative or negation-orbit unit. REQUIRE_COMPOUND_PROGRESS also
   * withholds a singleton forcing batch which exposes no second certified
   * consequence in the same pass. */
  int symmetry_nonforcing_cut_admission;
  /* Appended. TACTIX_ONLINE_SYMMETRY_CUT_POINT_*; active with FIX_AND_CUT.
   * LARGEST_ORBIT_LAST takes the largest stabilizer orbit, the highest
   * variable among equals, then a strictly better common-clause score. */
  int symmetry_cut_point;
  /* Appended. TACTIX_ONLINE_SETTING_*; ENABLE makes a FIX_AND_CUT chain fix
   * every variable whose stabilizer orbit holds its own complement before any
   * fix or cut level. INHERIT retains the disabled default. */
  int symmetry_negation_fixing_first_mode;
  /* Appended. TACTIX_ONLINE_SETTING_*; ENABLE lets a chain fix a clause only
   * through its first stored literal, so a clause whose first literal is
   * already false is left to a cut level. INHERIT retains the disabled
   * default. */
  int symmetry_first_literal_fix_anchor_mode;
} tactix_online_sr_parameters;

/* One exact Factor remainder column and grid. Literals use permanent DIMACS
 * identities. The grid is copied during configure; caller storage may be
 * released immediately afterward. Matching large-clause occurrences are
 * chosen deterministically when the action executes. */
typedef struct {
  const int *literals;
  int literal_count;
} tactix_online_factor_remainder;

typedef struct {
  const int *factors;
  int factor_count;
  const tactix_online_factor_remainder *remainders;
  int remainder_count;
} tactix_online_factor_grid;

typedef struct {
  int kind;
  union {
    tactix_online_bve_parameters bve;
    tactix_online_subsume_parameters subsume;
    tactix_online_strengthen_parameters strengthen;
    tactix_online_congruence_parameters congruence;
    tactix_online_instantiate_parameters instantiate;
    tactix_online_backbone_parameters backbone;
    tactix_online_sweep_parameters sweep;
    tactix_online_vivify_parameters vivify;
    tactix_online_analyze_parameters analyze;
    tactix_online_bce_parameters bce;
    tactix_online_ternary_parameters ternary;
    tactix_online_factor_parameters factor;
    tactix_online_transitive_parameters transitive;
    tactix_online_substitute_parameters substitute;
    tactix_online_reduce_parameters reduce;
    tactix_online_walk_parameters walk;
    tactix_online_reorder_parameters reorder;
    tactix_online_parity_model_parameters parity_model;
    tactix_online_onehot_model_parameters onehot_model;
    tactix_online_balanced_signature_model_parameters balanced_signature_model;
    tactix_online_fermat_model_parameters fermat_model;
    tactix_online_lookup_circuit_model_parameters lookup_circuit_model;
    tactix_online_simon_model_parameters simon_model;
    tactix_online_stconn_proof_parameters stconn_proof;
    tactix_online_signed_cardinality_proof_parameters signed_cardinality_proof;
    tactix_online_exact_cover_model_parameters exact_cover_model;
    tactix_online_independent_set_model_parameters independent_set_model;
    tactix_online_affine_model_parameters affine_model;
    tactix_online_sr_parameters substitution_redundancy;
  } value;
} tactix_online_parameters;

typedef struct {
  uint64_t step_id;
  int rule_id;
  tactix_online_parameters parameters;
  int granularity;
  const uint64_t *instances;
  int instance_count;
  const tactix_online_factor_grid *factor_grids;
  int factor_grid_count;
  int repeat_mode;
  uint64_t repeat_limit;
  /* rule_budget must be zero for INHERIT and UNLIMITED. */
  int rule_budget_mode;
  uint64_t rule_budget;
  int touch_filter_mode;
  int likely_kept_redundant_mode;
  int work_unit;
  uint64_t budget;
  int budget_unlimited;
  /* TACTIX_ONLINE_POST_*; appended for source-compatible zero initialization.
   * The selected follow-up executes after each semantic application inside
   * the same state-version transaction and does not itself count as repeat
   * progress. */
  int post_application;
} tactix_online_step;

typedef struct {
  const tactix_online_step *steps;
  int step_count;
  int repeat_mode;
  uint64_t repeat_limit;
} tactix_online_program;

/* Additive typed v2 payload for general substitution redundancy. The base v1
 * step is copied first; SR instances are then copied and validated against the
 * same immutable configuration snapshot. */
typedef struct {
  int source_literal, target_literal;
} tactix_online_sr_mapping;
typedef struct {
  const int *clause;
  int clause_count;
  const int *witness;
  int witness_count;
  const tactix_online_sr_mapping *substitution;
  int substitution_count;
} tactix_online_sr_instance;
typedef struct {
  tactix_online_step base;
  const tactix_online_sr_instance *sr_instances;
  int sr_instance_count;
} tactix_online_step_v2;
typedef struct {
  const tactix_online_step_v2 *steps;
  int step_count;
  int repeat_mode;
  uint64_t repeat_limit;
} tactix_online_program_v2;

#define TACTIX_ONLINE_COMPLETED 0
#define TACTIX_ONLINE_SCOPED_FIXPOINT 1
#define TACTIX_ONLINE_NO_CHANGE 2
#define TACTIX_ONLINE_INAPPLICABLE 3
#define TACTIX_ONLINE_BUDGET_EXHAUSTED 4
#define TACTIX_ONLINE_INTERRUPTED 5
#define TACTIX_ONLINE_CONFLICT 6
#define TACTIX_ONLINE_SAT 7
#define TACTIX_ONLINE_UNSAT 8
#define TACTIX_ONLINE_INVALID_REQUEST 9
#define TACTIX_ONLINE_STALE_REQUEST 10
#define TACTIX_ONLINE_RESOURCE_EXHAUSTED 11
#define TACTIX_ONLINE_ACCOUNTING_ERROR 12

typedef struct {
  int status, rule_result, budget_status, work_unit;
  uint64_t work, applications, step_id, instance;
  uint32_t rule_id;
  int made_rule_progress, selection_complete;
  uint64_t before_arena_generation, before_touch_epoch;
  uint64_t after_arena_generation, after_touch_epoch;
  uint64_t before_strategy_epoch, after_strategy_epoch;
  uint64_t configuration_revision;
} tactix_online_result;

int tactix_online_configure(tactix *, const tactix_online_program *,
                            uint64_t revision, int reconfigure_mode);
int tactix_online_configure_v2(tactix *, const tactix_online_program_v2 *,
                               uint64_t revision, int reconfigure_mode);
int tactix_online_advance(tactix *, tactix_online_result *);
uint64_t tactix_online_step_consumed(tactix *, uint64_t step_id);
int tactix_online_complete(tactix *);

/* branching surface (N1): candidates ranked by the active heuristic
 * (saved/target-phase literals, DIMACS convention); tactix_decide branches
 * on an arbitrary unassigned literal chosen by the client. */
int tactix_decide_candidates(tactix *, int *lits, int cap);
int tactix_decide(tactix *, int lit); /* 0 ok, -1 not applicable */

/* solver status: 0 unknown/in progress, 10 all-assigned (SAT),
 * 20 refuted (UNSAT). SAT prepares the reconstructed original-formula model
 * for tactix_value without changing the resumable rule/search state. */
int tactix_status(tactix *);

/* read-only statistics: returns the counter for a TACTIX_STAT_*
 * id, UINT64_MAX for unknown ids. Allocation-free. */
uint64_t tactix_stat(tactix *, int stat_id);

#ifdef __cplusplus
}
#endif

#endif
