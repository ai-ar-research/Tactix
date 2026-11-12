#ifndef TACTIX_CRUXMITER_PROGRAM_H
#define TACTIX_CRUXMITER_PROGRAM_H
#include "tactix.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* A public-rule program, shared by the paper CLI and the persistent frontend.
 * No kernel access. Stages/caps are the existing cruxmiter schedule; stopping
 * between actions preserves the next stage for explicit restoration/re-entry. */
enum { CRUXMITER_HANDOFF = 30 };
typedef struct cruxmiter_program {
  int propagate,congruence,substitute,backbone;
  unsigned stage,phase,round,count;
  uint64_t applications,round_applications,round_merges,round_substitutions;
  uint64_t requests,eligible,effective,rounds;
} cruxmiter_program;
int cruxmiter_begin(cruxmiter_program *);
/* hook returns nonzero to suspend before the next action; result is UNKNOWN.
 * Root preprocessing deliberately precedes assumption installation. The
 * configured search handoff installs A through the common query executor. */
int cruxmiter_run(cruxmiter_program *,tactix *,int (*hook)(void *),void *,FILE *log);
const char *cruxmiter_setting(size_t index,const char **value);
int cruxmiter_configure(tactix *);
#ifdef __cplusplus
}
#endif
#endif
