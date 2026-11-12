#ifndef TACTIX_IPASIR_EXTENSIONS_INCLUDED
#define TACTIX_IPASIR_EXTENSIONS_INCLUDED
#include "ipasir.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Prefix for the current result's hash-bound .json/.cnf/.proof/.model/.core
 * artifacts. Files live until release, or longer when retained by an owned
 * result handle below. A successful
 * add/assume or another solve invalidates this borrowed result view. */
/* Licensed smaller cores also bind -core.cnf and -core.proof. */
const char *tactix_ipasir_bundle(void *solver);
/* Owned immutable terminal result. Acquisition is valid only while the session
 * has a published SAT/UNSAT result and is idle; otherwise it returns NULL.
 * The handle and its certificate files survive subsequent queries, input changes
 * and session release. Release every acquired handle; getters borrow from it.
 * Original assumptions preserve order/duplicates; core literals are a sound
 * signed subset. NULL/out-of-range/wrong-result getters return zero or NULL. */
typedef struct tactix_ipasir_result tactix_ipasir_result;
tactix_ipasir_result *tactix_ipasir_result_acquire(void *solver);
void tactix_ipasir_result_release(tactix_ipasir_result *);
int tactix_ipasir_result_status(const tactix_ipasir_result *);
size_t tactix_ipasir_result_variables(const tactix_ipasir_result *);
int tactix_ipasir_result_val(const tactix_ipasir_result *,int literal);
int tactix_ipasir_result_failed(const tactix_ipasir_result *,int literal);
size_t tactix_ipasir_result_assumptions(const tactix_ipasir_result *);
int tactix_ipasir_result_assumption(const tactix_ipasir_result *,size_t index);
size_t tactix_ipasir_result_core_size(const tactix_ipasir_result *);
int tactix_ipasir_result_core_literal(const tactix_ipasir_result *,size_t index);
const char *tactix_ipasir_result_bundle(const tactix_ipasir_result *);
/* SHA-256 of the exact permanent input, excluding A/helpers: the ASCII domain
 * "tactix-permanent-input-i32le-v1\n" followed by each signed input word as four
 * little-endian bytes, including zero clause terminators. */
const char *tactix_ipasir_result_input_sha256(const tactix_ipasir_result *);
/* Optional deletion minimization using this session and its selected driver.
 * Reject foreign/stale/non-UNSAT sources, pending input/assumptions, busy calls
 * and context literals outside source A. max_probes=UINT64_MAX is unlimited;
 * zero performs no probes. No permanent input or caller result view changes.
 * Every probe consumes its own A, has its own identity and (when terminal)
 * owned bundle. UNKNOWN/budget exhaustion preserves the last certified cube.
 * minimal means subset-minimal relative to fixed context, never min weight or
 * cardinality. Empty core proves F+context UNSAT, not necessarily F UNSAT.
 * Returned reduction and every result acquired from it have independent lives.
 * NULL means invalid request. Out-of-range getters return zero/NULL. */
typedef struct tactix_ipasir_reduction tactix_ipasir_reduction;
tactix_ipasir_reduction *tactix_ipasir_minimize_core(void *,const tactix_ipasir_result *,
    const int *context,size_t context_size,uint64_t max_probes);
void tactix_ipasir_reduction_release(tactix_ipasir_reduction *);
int tactix_ipasir_reduction_minimal(const tactix_ipasir_reduction *);
size_t tactix_ipasir_reduction_core_size(const tactix_ipasir_reduction *);
int tactix_ipasir_reduction_core_literal(const tactix_ipasir_reduction *,size_t index);
size_t tactix_ipasir_reduction_context_size(const tactix_ipasir_reduction *);
int tactix_ipasir_reduction_context_literal(const tactix_ipasir_reduction *,size_t index);
tactix_ipasir_result *tactix_ipasir_reduction_source(const tactix_ipasir_reduction *);
tactix_ipasir_result *tactix_ipasir_reduction_certificate(const tactix_ipasir_reduction *);
size_t tactix_ipasir_reduction_probes(const tactix_ipasir_reduction *);
int tactix_ipasir_reduction_probe_status(const tactix_ipasir_reduction *,size_t index);
size_t tactix_ipasir_reduction_probe_assumptions(const tactix_ipasir_reduction *,size_t index);
int tactix_ipasir_reduction_probe_assumption(const tactix_ipasir_reduction *,size_t index,size_t assumption);
tactix_ipasir_result *tactix_ipasir_reduction_probe_result(const tactix_ipasir_reduction *,size_t index);
size_t tactix_ipasir_reduction_witnesses(const tactix_ipasir_reduction *);
int tactix_ipasir_reduction_witness_literal(const tactix_ipasir_reduction *,size_t index);
size_t tactix_ipasir_reduction_witness_probe(const tactix_ipasir_reduction *,size_t index);
/* Optional F-licensed extraction budget, zero => full cube. Not minimization.
 * Setters preserve the published result and apply to subsequent queries. */
int tactix_ipasir_set_core_budget(void *solver,uint64_t ticks);
/* Optional F-only licensing work budget per query for learned exports.
 * Default UINT64_MAX; zero disables export work. Does not alter search/results. */
int tactix_ipasir_set_learn_budget(void *solver,uint64_t ticks);

/* Additive programmable-session interface, version 1. The execution view uses
 * the existing tactix.h rule/parameter/program functions on this same query.
 * It is borrowed: do not release it or retain it after finish/repair/release.
 * Input mutation and simultaneous execution drivers are rejected while active. */
/* Abort an active manual query with finish(...,0) before releasing the session.
 * release calls during execution/callbacks are ignored. */
struct tactix_solver;
#define TACTIX_IPASIR_SESSION_VERSION 1u
#define TACTIX_IPASIR_ENTER_QUERY 0u
#define TACTIX_IPASIR_ENTER_RESTORED 1u
typedef int (*tactix_ipasir_strategy)(void *state,void *session,
    struct tactix_solver *execution,unsigned entry_reason);
/* Return 0/10/20 for UNKNOWN/SAT/UNSAT. A negative return aborts the query
 * without re-entering the strategy for pending restoration. */
int tactix_ipasir_set_strategy(void *,void *state,tactix_ipasir_strategy);
int tactix_ipasir_set_default_option(void *,const char *name,const char *value);
/* Native programs are compiled once with the library. NULL/"default" selects
 * DefaultPolicy; "cruxmiter", "chvatal", "dpll", "cdcl-no-restarts" and "scripted-default" select shared
 * programs from the corresponding existing examples.
 * Unknown names fail without changing the selected driver or result view.
 * Preset options overlay caller options for that query only; selecting another
 * driver preserves the caller's original option set. Manual begin still gives
 * control to the caller. Name/counters are borrowed/read only and require idle.
 * Counters accumulate since selection. root_eligible counts nonnegative public
 * rule results; root_effective counts successful (zero) applications, including
 * completed progress, rather than counting new clauses or theorems. */
int tactix_ipasir_set_strategy_name(void *,const char *name);
const char *tactix_ipasir_strategy_name(void *);
typedef struct tactix_ipasir_strategy_counters {
  unsigned version;
  size_t size;
  uint64_t query_entries,restored_entries;
  uint64_t root_requests,root_eligible,root_effective,root_rounds,default_handoffs;
} tactix_ipasir_strategy_counters;
int tactix_ipasir_get_strategy_counters(void *,tactix_ipasir_strategy_counters *);

#define TACTIX_QUERY_READY 0
#define TACTIX_QUERY_ASSUME 1
#define TACTIX_QUERY_PROPAGATE 2
#define TACTIX_QUERY_ANALYZE 3
#define TACTIX_QUERY_BACKTRACK 4
#define TACTIX_QUERY_RESTORE 5
#define TACTIX_QUERY_BUSY 6
#define TACTIX_QUERY_INTERRUPTED 7
#define TACTIX_QUERY_INVALID 8
#define TACTIX_QUERY_SAT 10
#define TACTIX_QUERY_UNSAT 20
typedef struct tactix_ipasir_query_state {
  unsigned version;
  size_t size;
  int action;
  size_t assumption_index;
  int literal;
  uint64_t generation;
} tactix_ipasir_query_state;

/* begin/repair: 1 => ready with a view, 0 => query ended UNKNOWN, -1 => invalid
 * call. finish returns 0/10/20, or -1 for an unproved/invalid proposal, leaving
 * the active query available for more work. Standard solve uses the installed
 * strategy (NULL selects DefaultPolicy); restoration re-enters that strategy
 * with ENTER_RESTORED. Each strategy owns its persistent continuation state. */
int tactix_ipasir_begin(void *,struct tactix_solver **execution);
int tactix_ipasir_next(void *,tactix_ipasir_query_state *);
/* Read-only count of rejected model checks for the current execution epoch.
 * A public rule can construct a model of F which misses A: its return code
 * remains nonterminal, while this counter increases. A strategy can observe
 * that event and explicitly Restart before continuing its root schedule.
 * Internal checks can count the same candidate more than once; use changes
 * to detect rejection, not as a count of distinct models.
 * Repeated reads do not reconstruct/check another model or mutate search.
 * Repair may replace the epoch and reset the count. UINT64_MAX means no active
 * borrowed execution view (including before begin and after finish). */
uint64_t tactix_ipasir_rejected_models(void *);
/* A boundary observation does not reserve the next action: termination can
 * arrive in between. assume_next returns 0 on success, ACTION_INTERRUPTED when
 * stopped, or -1 for an invalid call/boundary. A strategy returns UNKNOWN on
 * interruption; a manual client completes with finish(...,0). */
#define TACTIX_IPASIR_ACTION_INTERRUPTED (-2)
int tactix_ipasir_assume_next(void *);
int tactix_ipasir_repair(void *,struct tactix_solver **execution);
int tactix_ipasir_finish(void *,int result);
#ifdef __cplusplus
}
#endif
#endif
