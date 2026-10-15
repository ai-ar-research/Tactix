/* Public-API cruxmiter fallback policy.
 *
 * An alternative to tactix-cruxmiter-policy: run the cruxmiter-specific
 * opening first, then fall back to the shipped DefaultPolicy in its default
 * configuration.  No DefaultPolicy setting is overridden, so whatever the
 * opening leaves behind (substituted gates, root units, learned clauses) is
 * handed to the unmodified default strategy.
 *
 * The opening is the selected crux prelude (bounded adaptive
 * Congruence -> Substitute closure and one startup Backbone dose).  The
 * `warm` profile then also runs a stable-mode public search phase for a
 * fixed number of conflicts before the handoff.  tactix_solve cannot be
 * bounded through tactix.h, so the bounded phase has to be a pull-driven
 * loop; the handoff itself is the only call into DefaultPolicy.
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

/* The shipped CLI's BVE gate families (Kissat 4.0.4's elimination gates);
 * the C session default is every family, including XOR. */
#define FALLBACK_BVE_GATES                                                     \
  (TACTIX_BVE_GATE_EQUIV | TACTIX_BVE_GATE_AND_OR | TACTIX_BVE_GATE_ITE |      \
   TACTIX_BVE_GATE_DEFINITION)

enum profile { PROFILE_PRELUDE, PROFILE_WARM };

struct policy {
  tactix *solver;
  enum profile profile;
  int propagate, analyze, decide, restart, reduce, switchmode;
  int congruence, substitute, backbone;
  uint64_t warm_conflicts;
  uint64_t rel_u, rel_v, rel_wait, rel_period;
  int rel_trigger;
  uint64_t reduce_next;
};

static int terminal(int result) { return result == 10 || result == 20; }

static int rule_id(const char *name) {
  int id;
  for (id = 0; id < tactix_rule_count(); ++id) {
    const char *candidate = tactix_rule_name(id);
    if (candidate && !strcmp(candidate, name)) return id;
  }
  return -1;
}

static int read_dimacs(tactix *solver, const char *path, int *maximum_variable) {
  FILE *input = fopen(path, "r");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-crux-fallback-policy: cannot open '%s': %s\n",
            path, strerror(errno));
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
        fprintf(stderr, "tactix-crux-fallback-policy: malformed DIMACS header\n");
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
        fprintf(stderr, "tactix-crux-fallback-policy: malformed DIMACS literal\n");
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
        fprintf(stderr, "tactix-crux-fallback-policy: rejected DIMACS input\n");
        fclose(input);
        return 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) {
    fprintf(stderr, "tactix-crux-fallback-policy: incomplete DIMACS input\n");
    fclose(input);
    return 0;
  }
  fclose(input);
  return 1;
}

static int propagate(struct policy *policy) {
  return tactix_apply(policy->solver, policy->propagate);
}

/* Bounded public-rule dose; every successful mutation is settled by Propagate
 * before the next application observes it. */
static int dose_rule(struct policy *policy, int rule, unsigned cap) {
  unsigned count;
  for (count = 0; count < cap; ++count) {
    int result = tactix_apply(policy->solver, rule);
    if (terminal(result) || result == 2 || result != 0) return result;
    result = propagate(policy);
    if (terminal(result) || result == 2) return result;
  }
  return 1;
}

/* The crux opening: two Congruence -> Substitute rounds, rounds three and
 * four only while the public merge/substitution counters still move, then
 * one bounded Backbone dose. */
static int crux_prelude(struct policy *policy) {
  unsigned round;
  int result = propagate(policy);
  if (terminal(result) || result == 2) return result;
  for (round = 0; round < 4; ++round) {
    const uint64_t merges =
        tactix_stat(policy->solver, TACTIX_STAT_CONGRUENCE_MERGES);
    const uint64_t substitutions =
        tactix_stat(policy->solver, TACTIX_STAT_SUBSTITUTE_VARS);
    result = dose_rule(policy, policy->congruence, 4);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24);
    if (terminal(result) || result == 2) return result;
    printf("c crux-fallback gate-closure round=%u merges=%" PRIu64
           " substitutions=%" PRIu64 "\n",
           round + 1,
           tactix_stat(policy->solver, TACTIX_STAT_CONGRUENCE_MERGES) - merges,
           tactix_stat(policy->solver, TACTIX_STAT_SUBSTITUTE_VARS) -
               substitutions);
    if (round + 1 >= 2 &&
        tactix_stat(policy->solver, TACTIX_STAT_CONGRUENCE_MERGES) == merges &&
        tactix_stat(policy->solver, TACTIX_STAT_SUBSTITUTE_VARS) ==
            substitutions)
      break;
  }
  return dose_rule(policy, policy->backbone, 512);
}

/* Reluctant doubling (Luby sequence times the period), as in stable mode. */
static void tick_reluctant(struct policy *policy) {
  uint64_t wait;
  if (policy->rel_trigger || --policy->rel_wait) return;
  if ((policy->rel_u & (0 - policy->rel_u)) == policy->rel_v) {
    ++policy->rel_u;
    policy->rel_v = 1;
  } else {
    policy->rel_v *= 2;
  }
  wait = policy->rel_v * policy->rel_period;
  if (wait > (UINT64_C(1) << 20)) {
    policy->rel_u = policy->rel_v = 1;
    wait = policy->rel_period;
  }
  policy->rel_wait = wait;
  policy->rel_trigger = 1;
}

/* Stable-mode public search until warm_conflicts.  Decisions take the
 * heuristic's own ranked literal (stable heap, saved/target phase).  Returns
 * 10/20 when the phase decides the formula, 1 at the handoff, 0 on error. */
static int warm_search(struct policy *policy) {
  if (tactix_stat(policy->solver, TACTIX_STAT_STABLE) == 0 &&
      tactix_apply(policy->solver, policy->switchmode) < 0)
    return 0;
  for (;;) {
    int result = propagate(policy);
    uint64_t conflicts;
    int candidate;
    if (terminal(result)) return result;
    if (result == 2) {
      result = tactix_apply(policy->solver, policy->analyze);
      if (terminal(result)) return result;
      if (result < 0) return 0;
      tick_reluctant(policy);
      continue;
    }
    if (!tactix_stat(policy->solver, TACTIX_STAT_UNASSIGNED)) return 10;
    conflicts = tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS);
    if (conflicts >= policy->warm_conflicts) return 1;
    if (conflicts >= policy->reduce_next) {
      const int reduced = tactix_apply(policy->solver, policy->reduce);
      if (terminal(reduced)) return reduced;
      policy->reduce_next = conflicts + 2000 +
          300 * tactix_stat(policy->solver, TACTIX_STAT_REDUCTIONS);
      if (reduced >= 0) continue;
    }
    if (policy->rel_trigger) {
      const int restarted = tactix_apply_instance(
          policy->solver, policy->restart, TACTIX_INSTANCE_CANONICAL);
      if (terminal(restarted)) return restarted;
      if (restarted >= 0) {
        policy->rel_trigger = 0;
        continue;
      }
    }
    if (tactix_decide_candidates(policy->solver, &candidate, 1) != 1)
      return tactix_status(policy->solver);
    if (tactix_decide(policy->solver, candidate)) return 0;
  }
}

static int solve(struct policy *policy) {
  int result;
  if (tactix_status(policy->solver) == 20) return 20;
  result = crux_prelude(policy);
  if (terminal(result)) return result;
  if (result == 2) {
    /* A root conflict: let ConflictAnalysis derive the refutation. */
    result = tactix_apply(policy->solver, policy->analyze);
    if (terminal(result)) return result;
  }
  if (policy->profile == PROFILE_WARM) {
    result = warm_search(policy);
    if (terminal(result)) return result;
    if (!result) return 0;
    result = tactix_support(policy->solver, TACTIX_SUPPORT_ROOT_PREPARATION, 0);
    if (terminal(result)) return result;
    if (result < 0) return 0;
  }
  printf("c crux-fallback handoff conflicts=%" PRIu64 " fixed=%" PRIu64 "\n",
         tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS),
         tactix_stat(policy->solver, TACTIX_STAT_ROOT_FIXED));
  fflush(stdout);
  return tactix_solve(policy->solver);
}

static int init_policy(struct policy *policy, tactix *solver,
                       enum profile profile, uint64_t warm_conflicts) {
  memset(policy, 0, sizeof *policy);
  policy->solver = solver;
  policy->profile = profile;
  policy->warm_conflicts = warm_conflicts;
  policy->propagate = rule_id("Propagate");
  policy->analyze = rule_id("ConflictAnalysis");
  policy->decide = rule_id("Decide");
  policy->restart = rule_id("Restart");
  policy->reduce = rule_id("Reduce");
  policy->switchmode = rule_id("SwitchMode");
  policy->congruence = rule_id("Congruence");
  policy->substitute = rule_id("Substitute");
  policy->backbone = rule_id("Backbone");
  policy->rel_u = policy->rel_v = 1;
  policy->rel_period = policy->rel_wait = 1024;
  policy->reduce_next = 2000;
  return policy->propagate >= 0 && policy->analyze >= 0 &&
         policy->decide >= 0 && policy->restart >= 0 && policy->reduce >= 0 &&
         policy->switchmode >= 0 && policy->congruence >= 0 &&
         policy->substitute >= 0 && policy->backbone >= 0;
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

static void usage(const char *program) {
  fprintf(stderr,
          "usage: %s [--profile=prelude|warm] [--warm-conflicts=N] [--drat] "
          "CNF [PROOF]\n",
          program);
}

int main(int argc, char **argv) {
  enum profile profile = PROFILE_PRELUDE;
  uint64_t warm_conflicts = 10000;
  const char *cnf = NULL, *proof = NULL;
  int maximum_variable = 0, result, arg;
  FILE *provenance;
  tactix *solver;
  struct policy policy;
  for (arg = 1; arg < argc; ++arg) {
    if (!strcmp(argv[arg], "--profile=prelude")) {
      profile = PROFILE_PRELUDE;
    } else if (!strcmp(argv[arg], "--profile=warm")) {
      profile = PROFILE_WARM;
    } else if (!strncmp(argv[arg], "--warm-conflicts=", 17)) {
      char *end;
      errno = 0;
      warm_conflicts = strtoull(argv[arg] + 17, &end, 10);
      if (errno || *end || !warm_conflicts) {
        usage(argv[0]);
        return 1;
      }
    } else if (!strcmp(argv[arg], "--drat")) {
      /* Makes the certificate contract explicit to benchmark runners. */
    } else if (argv[arg][0] == '-' && argv[arg][1]) {
      usage(argv[0]);
      return 1;
    } else if (!cnf) {
      cnf = argv[arg];
    } else if (!proof) {
      proof = argv[arg];
    } else {
      usage(argv[0]);
      return 1;
    }
  }
  if (!cnf) {
    usage(argv[0]);
    return 1;
  }
  solver = tactix_init();
  /* Mirror the shipped CLI's session setup for the DefaultPolicy handoff:
   * an anonymous structural provenance stream and its BVE gate families. */
  provenance = tmpfile();
  if (!provenance || tactix_bind_structural_provenance(solver, provenance)) {
    fprintf(stderr, "tactix-crux-fallback-policy: cannot bind provenance\n");
    tactix_release(solver);
    return 1;
  }
  tactix_set_bve_gates(solver, FALLBACK_BVE_GATES);
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-crux-fallback-policy: cannot create proof '%s'\n",
            proof);
    tactix_release(solver);
    fclose(provenance);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&policy, solver, profile, warm_conflicts)) {
    tactix_release(solver);
    fclose(provenance);
    return 1;
  }
  printf("c crux-fallback-policy profile=%s warm-conflicts=%" PRIu64
         " fallback=default-policy\n",
         profile == PROFILE_WARM ? "warm" : "prelude",
         profile == PROFILE_WARM ? warm_conflicts : 0);
  result = solve(&policy);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c crux-rules congruence=%" PRIu64 " substitute=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES),
         tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, maximum_variable);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  fclose(provenance);
  return result == 10 || result == 20 ? result : 0;
}
