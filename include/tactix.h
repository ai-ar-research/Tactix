/* Tactix public C API — v1, frozen at the phase-2 review (plan §5.1).
 * Post-v1 changes are additive ONLY.
 *
 * N1 guarantee: at any solver state, every rule whose guard holds can be
 * discovered (tactix_rules_applicable), its instances enumerated
 * (tactix_rule_instances), and any of them applied (tactix_apply /
 * tactix_apply_instance); branching in particular is fully
 * client-controlled via tactix_decide_candidates / tactix_decide. This
 * surface exists in every build and every mode.
 *
 * External policies are pull-based (O5): they drive this stepping surface
 * themselves and observe the solver through tactix_stat. There is no
 * callback registration in v1.
 *
 * Instances (O3): a rule instance is a uint64_t payload whose encoding the
 * rule documents — Decide/Probe/Backbone/TransitiveReduce: the DIMACS
 * literal, ZERO-extended, i.e. TACTIX_LIT_INSTANCE(elit) below. Zero, never
 * sign, extension: a plain "cast to int64" maps -1 onto
 * TACTIX_INSTANCE_CANONICAL, so asking to branch on -1 silently gets the
 * canonical pick, which is an N1 hole;
 * Restart: the keep-level; Reduce: the number of ranked victims to delete;
 * ConflictAnalysis: the backjump level it may return to, 0..level-1.
 * The entries without per-application choice are exactly Propagate,
 * SwitchMode and Sweep: they expose only the canonical instance, and a
 * NON-canonical payload to tactix_apply_instance on one of those is an
 * invalid argument (-1), never a silent canonical application.
 * TACTIX_INSTANCE_CANONICAL always denotes the canonical pick. Enumeration
 * fills a caller buffer, ranked with the canonical instance first; values
 * are snapshots, valid until the next state-mutating call. Errors are -1
 * int returns (unknown rule, guard does not hold, invalid argument). */
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
/* The plan §5.6 reward/cost counters: the currencies dosing is denominated
 * in, exposed so an external/learned policy can meter the work its schedule
 * buys (the pull-based reward channel).
 *
 * Each rule is exposed as a (yield, effort) PAIR, deliberately.
 * Cost-per-unit-output — not yield alone — is the axis that orders these
 * rules' outcomes: the same rule can cost three orders of magnitude more per
 * unit of yield on one instance than on another, and a learner reading only
 * the yield cannot tell those apart. */
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
/* The PAIRS completed. Every rule family the inprocessing schedule doses has a
 * yield id and an effort id, and the effort id is the counter that family's
 * effort ledger is actually denominated in (its `EFFORT_SPECS` meter), so
 * yield/effort is a real cost-per-unit-output and not two unrelated numbers.
 * tools/docs/check-stat-ids.py enforces both halves mechanically: a new dosed
 * rule fails ctest until its pair is exposed here. */
#define TACTIX_STAT_VIVIFY_TICKS 32     /* Vivify work: propagation ticks */
#define TACTIX_STAT_PROBE_TICKS 33      /* Probe work: propagation ticks */
#define TACTIX_STAT_BACKBONE_TICKS 34   /* Backbone work: propagation ticks */
#define TACTIX_STAT_TRANSITIVE_TICKS 35 /* TransitiveReduce work: cache lines */
/* C1 SubsumeForward: TACTIX_STAT_SUBSUMPTIONS above counts ROUNDS, which is
 * neither a yield nor an effort. The pair is clauses deleted / partner visits
 * (cadical `subchecks`, the unit its budget is denominated in). */
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
/* Substitute is the one family with no effort ledger row at all (an
 * un-titratable closure — the slot gates its REPETITION, not its size), so its
 * effort id is a scan-volume proxy rather than a meter the policy doses on. */
#define TACTIX_STAT_SUBSTITUTE_VARS 46  /* variables substituted away */
#define TACTIX_STAT_SUBSTITUTE_SCC_LITS 47 /* Substitute work: literals in the
                                              non-trivial SCCs it walked */

/* X6 HyperBinaryRes (R9). Yield is the emitted resolvent count, effort is the
 * private tick meter its EFFORT_SPECS row doses on. */
#define TACTIX_STAT_HBR_BINARIES 48 /* hyper binary resolvents emitted */
#define TACTIX_STAT_HBR_TICKS 49    /* HyperBinaryRes work */

tactix *tactix_init(void);
void tactix_release(tactix *);

/* IPASIR-style ingest: literals in DIMACS convention, 0 terminates a
 * clause. */
void tactix_add(tactix *, int lit);

/* proof logging (N2): open before solving; TACTIX_PROOF_* format.
 * Returns 0, or -1 (unwritable path / unsupported format). */
int tactix_proof(tactix *, const char *path, int format);

/* BVE gate-type selection (E1d, additive): bitmask of the gate types bounded
 * variable elimination may recognize. A PERFORMANCE knob — every
 * mask is sound because a disabled type only makes BVE fall back to the full
 * resolvent cross product; it never changes a verdict. Settable at any state
 * (N1). Default (unset) = all gate types on. */
#define TACTIX_BVE_GATE_EQUIV 1u  /* equivalence x<->~y */
#define TACTIX_BVE_GATE_AND_OR 2u /* AND/OR x<->OR/AND(...) */
#define TACTIX_BVE_GATE_ITE 4u    /* if-then-else x<->(c?t:e) */
#define TACTIX_BVE_GATE_XOR 8u    /* parity x<->a1^...^am */
#define TACTIX_BVE_GATE_DEFINITION 16u /* general x<->phi, via the sub-solver */
void tactix_set_bve_gates(tactix *, unsigned mask);

/* Kernel CONFIGURATION setters — the pose surface for external and
 * learned policies (§5.6). Every value is sound: config narrows/reorders
 * WHICH sound applications happen, never their proof obligations (the
 * bve_gates argument above, generalized). Settable at any state (N1).
 *  - bve_bound: BVE growth slack (eliminate iff resolvents <= occs + bound)
 *  - factor_bound: Factor acceptance (accept a grid iff delta > bound)
 *  - factor_composite: nonzero = Factor composes over extension vars
 *  - work_budget: per-round budget for budget-aware rules, own currency
 *                 (Vivify: propagation ticks; Factor: factor ticks); 0 = off
 *  - vivify_scope: which clause band Vivify may touch. 0 = ALL bands,
 *      1 = redundant within the live tier2, 2 = irredundant, 3/4/5 =
 *      the redundant tier1 / tier2 / tier3 bands separately. A value
 *      above 5 leaves the current setting unchanged (it is never
 *      substituted by a different band: that would remove the capability
 *      instead of declining it, N1). */
void tactix_set_bve_bound(tactix *, unsigned bound);
void tactix_set_factor_bound(tactix *, unsigned bound);
void tactix_set_factor_composite(tactix *, int on);
void tactix_set_work_budget(tactix *, uint64_t budget);
void tactix_set_vivify_scope(tactix *, unsigned scope);

/* CONFIG READBACK (additive) — the pose surface's anti-vacuity half. A policy
 * that poses a value must be able to prove the solver HOLDS it: a setting that
 * was silently dropped, or substituted by a different one, produces an arm
 * that measures nothing while reading as configured. One id per setter above,
 * same order. Returns the stored value, or UINT64_MAX for an unknown id. */
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

/* action kind (plan §4): is registry entry rule_id a PROOF
 * RULE — a non-trivial effect on the proof state, so it carries a proof
 * obligation — or an H-only POLICY OPERATION (a sub-policy: Rephase, SwitchMode,
 * ...)? Both share this one name-sorted id space. Returns a TACTIX_ACTION_*
 * value, or -1 if rule_id is out of range. */
#define TACTIX_ACTION_PROOF_RULE 0
#define TACTIX_ACTION_POLICY_OP 1
int tactix_action_kind(int rule_id);

/* fill `out` with the ids of rules whose guard (SOUNDNESS precondition)
 * currently holds; returns the count (<= cap). Any of these may be applied
 * now, in any order — this is theoretical applicability, N1. */
int tactix_rules_applicable(tactix *, int *out, int cap);

/* fill `out` with up to `cap` instance payloads of a rule, ranked with the
 * canonical instance first (see the header comment for encodings).
 * Returns the count, or -1 if the rule is unknown or its guard does not
 * hold. Rules without per-application choice yield exactly
 * TACTIX_INSTANCE_CANONICAL. */
int tactix_rule_instances(tactix *, int rule_id, uint64_t *out, int cap);

/* apply a rule's canonical instance. Returns: 0 progress, 1 no-op,
 * 2 conflict found, 10 SAT concluded, 20 UNSAT concluded, -1 guard does
 * not hold. */
int tactix_apply(tactix *, int rule_id);

/* apply a chosen instance (TACTIX_INSTANCE_CANONICAL = canonical); same
 * returns as tactix_apply, plus -1 for a non-canonical payload on a rule with
 * no instance surface. Out-of-range payloads within a rule that HAS one are
 * clamped or no-ops per that rule's documented encoding — never unsound. */
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
