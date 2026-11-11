/* tactix-default: the default strategy over tactix.h as a command-line
 * solver, `tactix-default [--auto-proof|--dsr|--pbp] [--reference] <cnf> [proof]`
 * (proof flags as for `tactix`, the last one winning); --reference
 * runs the library's built-in strategy (tactix_solve) on the same session
 * instead, for A/B comparison. Session setup follows the production
 * `tactix <cnf> [proof]` (src/main.cpp): Kissat's BVE gates, no retained
 * structural provenance, and its deferred startup lifetime. */
#include "tactix_default.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t interrupted = 0;
static void on_signal(int signal_number) {
  (void)signal_number;
  interrupted = 1;
}

static int usage(void) {
  fprintf(stderr, "usage: tactix-default [--auto-proof|--dsr|--pbp] [--reference] <cnf> [proof]\n");
  printf("s UNKNOWN\n");
  return 1;
}

static void print_model(tactix *s) {
  const int variables = (int)tactix_observe(s, TACTIX_OBSERVE_INPUT_VARIABLES, 0);
  printf("v");
  for (int v = 1, column = 1; v <= variables; ++v) {
    const int literal = tactix_value(s, v) > 0 ? v : -v;
    column += printf(" %d", literal);
    if (column > 72 && v < variables) {
      printf("\nv");
      column = 1;
    }
  }
  printf(" 0\n");
}

int main(int argc, char **argv) {
  const char *cnf = NULL, *proof = NULL;
  int format = TACTIX_PROOF_AUTO, reference = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--auto-proof"))
      format = TACTIX_PROOF_AUTO;
    else if (!strcmp(argv[i], "--dsr"))
      format = TACTIX_PROOF_DSR_BINARY;
    else if (!strcmp(argv[i], "--pbp"))
      format = TACTIX_PROOF_PBP;
    else if (!strcmp(argv[i], "--reference"))
      reference = 1;
    else if (argv[i][0] == '-' && argv[i][1])
      return usage();
    else if (!cnf)
      cnf = argv[i];
    else if (!proof)
      proof = argv[i];
    else
      return usage();
  }
  if (!cnf) return usage();
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  tactix *s = tactix_init();
  tactix_defer_startup_state(s);
  tactix_set_bve_gates(s, TACTIX_BVE_GATES_DEFAULT);
  if (tactix_discard_structural_provenance(s) || (proof && tactix_proof(s, proof, format))) {
    fprintf(stderr, "tactix-default: cannot write proof to '%s'\n", proof);
    printf("s UNKNOWN\n");
    return 1;
  }
  FILE *input = fopen(cnf, "r");
  if (!input || tactix_read_dimacs(s, input)) {
    fprintf(stderr, "tactix-default: cannot read '%s'\n", cnf);
    printf("s UNKNOWN\n");
    return 1;
  }
  fclose(input);

  if (reference) tactix_set_interrupt(s, &interrupted);
  const int verdict = reference ? tactix_solve(s) : tactix_default_solve(s, &interrupted);
  printf("c conflicts %llu\nc decisions %llu\nc propagations %llu\n",
         (unsigned long long)tactix_stat(s, TACTIX_STAT_CONFLICTS),
         (unsigned long long)tactix_stat(s, TACTIX_STAT_DECISIONS),
         (unsigned long long)tactix_stat(s, TACTIX_STAT_PROPAGATIONS));
  if (!reference && tactix_default_unported())
    printf("c unported %s\n", tactix_default_unported());
  /* tactix_status reports the verdict and completes its certificate. */
  if (verdict == 10 && tactix_status(s) == 10) {
    printf("s SATISFIABLE\n");
    print_model(s);
  } else if (verdict == 20 && tactix_status(s) == 20) {
    if (proof) printf("c proof-route %s\n", tactix_proof_route(s));
    printf("s UNSATISFIABLE\n");
  } else {
    printf("s UNKNOWN\n");
  }
  fflush(stdout);
  tactix_release(s);
  return verdict == 10 ? 10 : verdict == 20 ? 20 : 0;
}
