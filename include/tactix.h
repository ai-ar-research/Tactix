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
 *  - Restart: the keep-level. Reduce: the number of ranked victims to delete.
 *    ConflictAnalysis: the backjump level it may return to, 0..level-1.
 *  - Sweep: the seed it sweeps around, as an OPAQUE value obtained from
 *    tactix_rule_instances. It is an internal variable index, not a DIMACS
 *    literal and not constructible by the caller: enumerate, then apply one of
 *    the values returned, within the same solver state.
 *  - Propagate and SwitchMode have no instance surface at all, so a
 *    non-canonical payload to tactix_apply_instance on one of them is an
 *    invalid argument (-1), never a silent canonical application. Congruence,
 *    Factor and Walk do have the entry point but currently enumerate only the
 *    canonical sentinel. Both sets are pinned by tests/test_api_instances.c;
 *    joining or leaving either is a deliberate act, so update that test and
 *    this comment together.
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

/* X6 HyperBinaryRes: yield is the emitted resolvent count, effort the private
 * tick meter its EFFORT_SPECS row doses on. */
#define TACTIX_STAT_HBR_BINARIES 48 /* hyper binary resolvents emitted */
#define TACTIX_STAT_HBR_TICKS 49    /* HyperBinaryRes work */
#define TACTIX_STAT_SUBSTITUTE_TICKS 50 /* Substitute binary-watch scan work */

tactix *tactix_init(void);
void tactix_release(tactix *);

/* IPASIR-style ingest: literals in DIMACS convention, 0 terminates a
 * clause. */
void tactix_add(tactix *, int lit);

/* proof logging (N2): open before solving; TACTIX_PROOF_* format.
 * Returns 0, or -1 (unwritable path / unsupported format). */
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

/* branching surface (N1): candidates ranked by the active heuristic
 * (saved/target-phase literals, DIMACS convention); tactix_decide branches
 * on an arbitrary unassigned literal chosen by the client. */
int tactix_decide_candidates(tactix *, int *lits, int cap);
int tactix_decide(tactix *, int lit); /* 0 ok, -1 not applicable */

/* solver status: 0 unknown/in progress, 10 all-assigned (SAT),
 * 20 refuted (UNSAT) */
int tactix_status(tactix *);

/* read-only statistics: returns the counter for a TACTIX_STAT_*
 * id, UINT64_MAX for unknown ids. Allocation-free. */
uint64_t tactix_stat(tactix *, int stat_id);

#ifdef __cplusplus
}
#endif

#endif
