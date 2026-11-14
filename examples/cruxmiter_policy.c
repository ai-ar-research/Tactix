/* Public-API strategy used by the paper's cruxmiter experiment: at most four
 * alternating congruence/substitution rounds, Backbone, then the built-in
 * default strategy with the artifact's exact settings. */
#include "example_support.h"
#include "strategy/presets/cruxmiter.h"

#include <inttypes.h>
#include <stdint.h>

int main(int argc, char **argv) {
  const char *cnf, *proof = NULL;
  int maximum_variable = 0, result;
  tactix *solver;
  cruxmiter_program program;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", argv[0]);
    return 1;
  }
  cnf = argv[1];
  if (argc == 3) proof = argv[2];
  solver = tactix_init();
  if (!solver || !cruxmiter_configure(solver)) {
    tactix_release(solver);
    return 1;
  }
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DSR_BINARY)) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot create proof '%s'\n", proof);
    tactix_release(solver);
    return 1;
  }
  if (!example_read_dimacs(solver, cnf, "tactix-cruxmiter-policy", NULL, 1 << 16,
                           &maximum_variable, NULL, NULL) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !cruxmiter_begin(&program)) {
    tactix_release(solver);
    return 1;
  }
  puts("c cruxmiter-policy paper-strategy");
  result = cruxmiter_run(&program, solver, NULL, NULL, stdout);
  if (result == CRUXMITER_HANDOFF) result = tactix_solve(solver);
  printf("c conflicts %" PRIu64 "\n", tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c crux-rules congruence=%" PRIu64 " substitute=%" PRIu64
         " backbone=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES),
         tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS),
         tactix_stat(solver, TACTIX_STAT_BACKBONE_FAILED));
  example_print_result(solver, result, maximum_variable);
  tactix_release(solver);
  return result == 10 || result == 20 ? result : 0;
}
