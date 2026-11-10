/* The Tactix default strategy as a client of the public C interface.
 *
 * tactix_default_solve runs the strategy the command-line solver runs
 * (DefaultPolicy) over tactix.h alone: each decision is computed here from
 * public observations, each action is a public request, and every setting is
 * read from the session's default-strategy configuration
 * (tactix_strategy_integer / tactix_strategy_real), so no strategy number is
 * restated here. On the same session it takes DefaultPolicy's actions in
 * DefaultPolicy's order: verdicts, models, certificates and trajectory
 * counters agree (tests/test_c_default_correspondence.cpp). */
#ifndef TACTIX_DEFAULT_H_INCLUDED
#define TACTIX_DEFAULT_H_INCLUDED

#include <signal.h>

#include "tactix.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run the strategy to completion on a session whose input is loaded:
 * 10 SAT, 20 UNSAT, 0 unknown (interrupted through `stop`, or a strategy
 * path this port does not cover yet, which tactix_default_unported names).
 * Sets the session's guidance publication to TACTIX_GUIDANCE_NATIVE. */
int tactix_default_solve(tactix *, volatile sig_atomic_t *stop);

/* The strategy path the last solve stopped at because this port does not
 * cover it yet, or NULL. */
const char *tactix_default_unported(void);

#ifdef __cplusplus
}
#endif

#endif
