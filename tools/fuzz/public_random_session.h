/* Fuzz-client bridge, compiled from the same public strategy as the CLI. */
#ifndef TACTIX_PUBLIC_RANDOM_SESSION_H
#define TACTIX_PUBLIC_RANDOM_SESSION_H
#include "tactix_ipasir.h"
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tactix_random_session tactix_random_session;
tactix_random_session *tactix_random_session_new(uint64_t seed);
void tactix_random_session_delete(tactix_random_session *);
void tactix_random_session_variables(tactix_random_session *,uint32_t);
void tactix_random_session_seed(tactix_random_session *,uint64_t);
int tactix_random_session_run(void *,void *,struct tactix_solver *,unsigned);
/* JSON object: per-query action/form requests and effective applications. */
void tactix_random_session_coverage(tactix_random_session *,FILE *);
#ifdef __cplusplus
}
#endif
#endif
