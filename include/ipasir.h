/* Experimental IPASIR frontend; full conformance qualification is pending.
 * Literals use signed DIMACS integers; zero terminates an input clause. */
#ifndef TACTIX_IPASIR_H_INCLUDED
#define TACTIX_IPASIR_H_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
const char *ipasir_signature(void);
void *ipasir_init(void);
void ipasir_release(void *solver);
void ipasir_add(void *solver, int literal);
void ipasir_assume(void *solver, int literal);
int ipasir_solve(void *solver);
int ipasir_val(void *solver, int literal);
int ipasir_failed(void *solver, int literal);
void ipasir_set_terminate(void *solver, void *state,
                          int (*terminate)(void *state));
/* Export F-licensed learned clauses in original DIMACS coordinates. Clause
 * buffers are zero-terminated and valid only during the callback. Callback
 * code must not re-enter the solver or a borrowed execution view. */
void ipasir_set_learn(void *solver, void *state, int max_length,
                     void (*learn)(void *state, int *clause));
#ifdef __cplusplus
}
#endif
#endif
