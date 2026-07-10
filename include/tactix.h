/* Tactix public C API — phase-1 draft (plan §5.1, open decision O3:
 * iterator lifetimes and error reporting are drafted minimally here and
 * freeze at the phase-2 API v1 review).
 *
 * N1 guarantee: at any solver state, every rule whose guard holds can be
 * discovered (tactix_rules_applicable) and applied (tactix_apply);
 * branching in particular is fully client-controlled via
 * tactix_decide_candidates / tactix_decide. This surface exists in every
 * build and every mode.
 */
#ifndef TACTIX_H_INCLUDED
#define TACTIX_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tactix_solver tactix;

tactix *tactix_init(void);
void tactix_release(tactix *);

/* IPASIR-style ingest: literals in DIMACS convention, 0 terminates a
 * clause. */
void tactix_add(tactix *, int lit);

/* proof logging (N2): open before solving; binary DRAT if binary != 0 */
int tactix_proof(tactix *, const char *path, int binary);

/* run the default policy to completion: 10 SAT / 20 UNSAT */
int tactix_solve(tactix *);

/* model access after SAT: >0 true, <0 false (DIMACS convention) */
int tactix_value(tactix *, int lit);

/* --- R3/N1: rule introspection and stepwise application ---------------- */

/* number of registered rule families (stable ids: 0..count-1, sorted by
 * name) */
int tactix_rule_count(void);
const char *tactix_rule_name(int rule_id);

/* fill `out` with the ids of rules whose guard currently holds; returns
 * the count (<= cap) */
int tactix_rules_applicable(tactix *, int *out, int cap);

/* apply a rule's canonical instance. Returns: 0 progress, 1 no-op,
 * 10 SAT concluded, 20 UNSAT concluded, 2 conflict found, -1 guard does
 * not hold. */
int tactix_apply(tactix *, int rule_id);

/* branching surface (N1): candidates ranked by the active heuristic
 * (saved-phase literals, DIMACS convention); tactix_decide branches on an
 * arbitrary unassigned literal chosen by the client. */
int tactix_decide_candidates(tactix *, int *lits, int cap);
int tactix_decide(tactix *, int lit); /* 0 ok, -1 not applicable */

/* solver status: 0 unknown/in progress, 10 all-assigned (SAT),
 * 20 refuted (UNSAT) */
int tactix_status(tactix *);

#ifdef __cplusplus
}
#endif

#endif
