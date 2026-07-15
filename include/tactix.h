/* Tactix public C API — v1, frozen at the phase-2 review (plan §5.1,
 * rev 20/21; O3 + O5 decided). Post-v1 changes are additive only.
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
 * rule documents — Decide: the DIMACS literal (signed, cast to int64);
 * Restart: the keep-level; Backjump: the target level; Reduce: the number
 * of ranked victims to delete. Rules without per-application choice
 * (Propagate, Analyze, SwitchMode) expose only the canonical instance.
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

/* proof formats (N2) */
#define TACTIX_PROOF_DRAT_BINARY 0
#define TACTIX_PROOF_DRAT_ASCII 1
#define TACTIX_PROOF_LRAT 2 /* native LRAT: antecedent hints (plan O4) */

/* read-only statistics ids (rev 21); the list grows additively */
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

tactix *tactix_init(void);
void tactix_release(tactix *);

/* IPASIR-style ingest: literals in DIMACS convention, 0 terminates a
 * clause. */
void tactix_add(tactix *, int lit);

/* proof logging (N2): open before solving; TACTIX_PROOF_* format.
 * Returns 0, or -1 (unwritable path / unsupported format). */
int tactix_proof(tactix *, const char *path, int format);

/* run the default policy to completion: 10 SAT / 20 UNSAT */
int tactix_solve(tactix *);

/* model access after SAT: >0 true, <0 false (DIMACS convention) */
int tactix_value(tactix *, int lit);

/* --- R3/N1: rule introspection and stepwise application ---------------- */

/* number of registered rule families (stable ids: 0..count-1, sorted by
 * name) */
int tactix_rule_count(void);
const char *tactix_rule_name(int rule_id);

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
 * returns as tactix_apply. Out-of-range payloads are clamped or no-ops per
 * the rule's documented encoding — never unsound. */
int tactix_apply_instance(tactix *, int rule_id, uint64_t instance);

/* branching surface (N1): candidates ranked by the active heuristic
 * (saved/target-phase literals, DIMACS convention); tactix_decide branches
 * on an arbitrary unassigned literal chosen by the client. */
int tactix_decide_candidates(tactix *, int *lits, int cap);
int tactix_decide(tactix *, int lit); /* 0 ok, -1 not applicable */

/* solver status: 0 unknown/in progress, 10 all-assigned (SAT),
 * 20 refuted (UNSAT) */
int tactix_status(tactix *);

/* read-only statistics (rev 21): returns the counter for a TACTIX_STAT_*
 * id, UINT64_MAX for unknown ids. Allocation-free. */
uint64_t tactix_stat(tactix *, int stat_id);

#ifdef __cplusplus
}
#endif

#endif
