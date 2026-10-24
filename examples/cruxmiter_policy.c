/* Public-API strategy used by the paper's cruxmiter experiment.
 *
 * The opening alternates congruence detection and substitution for at most
 * four rounds, applies Backbone, then hands the reduced formula to the
 * built-in default strategy with the exact settings used by the artifact.
 */
#include "tactix.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct policy {
  tactix *solver;
  int propagate, congruence, substitute, backbone;
};

static int rule_id(const char *name) {
  int id;
  for (id = 0; id < tactix_rule_count(); ++id) {
    const char *candidate = tactix_rule_name(id);
    if (candidate && !strcmp(candidate, name)) return id;
  }
  fprintf(stderr, "tactix-cruxmiter-policy: missing rule '%s'\n", name);
  return -1;
}

static int read_dimacs(tactix *solver, const char *path,
                       int *maximum_variable) {
  FILE *input = fopen(path, "r");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot open '%s': %s\n", path,
            strerror(errno));
    return 0;
  }
  *maximum_variable = 0;
  while (fgets(line, sizeof line, input)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor || *cursor == 'c') continue;
    if (*cursor == 'p') {
      char kind[8];
      int variables;
      unsigned long long clauses;
      if (sscanf(cursor, "p %7s %d %llu", kind, &variables, &clauses) != 3 ||
          strcmp(kind, "cnf") || variables < 0) {
        fprintf(stderr, "tactix-cruxmiter-policy: malformed DIMACS header\n");
        fclose(input);
        return 0;
      }
      saw_header = 1;
      if (variables > *maximum_variable) *maximum_variable = variables;
      continue;
    }
    while (*cursor) {
      char *end;
      long value;
      while (isspace((unsigned char)*cursor)) ++cursor;
      if (!*cursor || *cursor == 'c') break;
      errno = 0;
      value = strtol(cursor, &end, 10);
      if (end == cursor || errno || value < INT_MIN || value > INT_MAX) {
        fprintf(stderr, "tactix-cruxmiter-policy: malformed DIMACS literal\n");
        fclose(input);
        return 0;
      }
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > *maximum_variable) *maximum_variable = variable;
        open_clause = 1;
      } else {
        open_clause = 0;
      }
      if (tactix_add_checked(solver, (int)value)) {
        fprintf(stderr, "tactix-cruxmiter-policy: rejected DIMACS input\n");
        fclose(input);
        return 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) {
    fprintf(stderr, "tactix-cruxmiter-policy: incomplete DIMACS input\n");
    fclose(input);
    return 0;
  }
  fclose(input);
  return 1;
}

static int terminal(int result) { return result == 10 || result == 20; }

static int propagate(struct policy *policy) {
  return tactix_apply(policy->solver, policy->propagate);
}

static int dose_rule(struct policy *policy, int rule, unsigned cap,
                     uint64_t *applications) {
  unsigned count;
  for (count = 0; count < cap; ++count) {
    int result = tactix_apply(policy->solver, rule);
    if (terminal(result) || result == 2) return result;
    if (result != 0) return result;
    ++*applications;
    result = propagate(policy);
    if (terminal(result) || result == 2) return result;
  }
  return 1;
}

static int gate_closure(struct policy *policy, uint64_t *applications) {
  unsigned round;
  for (round = 0; round < 4; ++round) {
    const uint64_t applications_before = *applications;
    const uint64_t merges_before =
        tactix_stat(policy->solver, TACTIX_STAT_CONGRUENCE_MERGES);
    const uint64_t substitutions_before =
        tactix_stat(policy->solver, TACTIX_STAT_SUBSTITUTE_VARS);
    uint64_t merges_after, substitutions_after;
    int result = dose_rule(policy, policy->congruence, 4, applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, applications);
    if (terminal(result) || result == 2) return result;
    merges_after =
        tactix_stat(policy->solver, TACTIX_STAT_CONGRUENCE_MERGES);
    substitutions_after =
        tactix_stat(policy->solver, TACTIX_STAT_SUBSTITUTE_VARS);
    printf("c crux-gate-closure round=%u applications=%" PRIu64
           " merges=%" PRIu64 " substitutions=%" PRIu64 "\n",
           round + 1, *applications - applications_before,
           merges_after - merges_before,
           substitutions_after - substitutions_before);
    fflush(stdout);
    if (round + 1 >= 2 && merges_after == merges_before &&
        substitutions_after == substitutions_before)
      break;
  }
  return 1;
}

static int solve(struct policy *policy) {
  uint64_t applications = 0;
  int result;
  if (tactix_status(policy->solver) == 20) return 20;
  result = propagate(policy);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  result = gate_closure(policy, &applications);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  result = dose_rule(policy, policy->backbone, 512, &applications);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  return tactix_solve(policy->solver);
}

static int init_policy(struct policy *policy, tactix *solver) {
  memset(policy, 0, sizeof *policy);
  policy->solver = solver;
  policy->propagate = rule_id("Propagate");
  policy->congruence = rule_id("Congruence");
  policy->substitute = rule_id("Substitute");
  policy->backbone = rule_id("Backbone");
  return policy->propagate >= 0 && policy->congruence >= 0 &&
         policy->substitute >= 0 && policy->backbone >= 0;
}

static int configure_default_strategy(tactix *solver) {
  static const char *const settings[][2] = {
      {"TACTIX_FORCE_MODE", "stable"},
      {"TACTIX_REORDER", "0"},
      {"TACTIX_NO_BVE", "1"},
      {"TACTIX_CONFLICT_REUSE", "0"},
      {"TACTIX_SLOT_PERMILLE", "Vivify:200"},
      {"TACTIX_NO_FACTOR", "1"},
      {"TACTIX_NO_SWEEP", "1"},
      {"TACTIX_SLOT_DISABLE", "Backbone"},
  };
  size_t i;
  for (i = 0; i < sizeof settings / sizeof *settings; ++i)
    if (tactix_set_default_option(solver, settings[i][0], settings[i][1])) {
      fprintf(stderr, "tactix-cruxmiter-policy: rejected setting %s=%s\n",
              settings[i][0], settings[i][1]);
      return 0;
    }
  return 1;
}

static void print_model(tactix *solver, int maximum_variable) {
  int variable;
  fputs("v", stdout);
  for (variable = 1; variable <= maximum_variable; ++variable) {
    const int value = tactix_value(solver, variable);
    printf(" %d", value > 0 ? variable : -variable);
    if (variable % 20 == 0 && variable != maximum_variable) fputs("\nv", stdout);
  }
  puts(" 0");
}

int main(int argc, char **argv) {
  const char *cnf, *proof = NULL;
  int maximum_variable = 0, result;
  tactix *solver;
  struct policy policy;
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", argv[0]);
    return 1;
  }
  cnf = argv[1];
  if (argc == 3) proof = argv[2];
  solver = tactix_init();
  if (!solver || !configure_default_strategy(solver)) {
    tactix_release(solver);
    return 1;
  }
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot create proof '%s'\n", proof);
    tactix_release(solver);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&policy, solver)) {
    tactix_release(solver);
    return 1;
  }
  puts("c cruxmiter-policy paper-strategy");
  result = solve(&policy);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c crux-rules congruence=%" PRIu64 " substitute=%" PRIu64
         " backbone=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES),
         tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS),
         tactix_stat(solver, TACTIX_STAT_BACKBONE_FAILED));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, maximum_variable);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  return terminal(result) ? result : 0;
}
