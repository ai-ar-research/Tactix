/* Public-API cruxmiter policy experiment.
 *
 * TACO repeatedly selects KISSAT stable=2 and usually target=0 or phase=0 on
 * this family. This client translates that signal into always-stable EVSIDS
 * search, reluctant restarts, no target capture, and explicit phase variants.
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

enum profile {
  PROFILE_SEARCH,
  PROFILE_POSITIVE,
  PROFILE_SAVED,
  PROFILE_GATE,
  PROFILE_BINARY,
  PROFILE_BACKBONE,
  PROFILE_BACKBONE_FIRST,
  PROFILE_BACKBONE_CLOSE,
  PROFILE_BACKBONE_HEAVY,
  PROFILE_SWEEP,
  PROFILE_FULL
};

struct policy {
  tactix *solver;
  enum profile profile;
  int propagate, analyze, restart, reduce, switchmode;
  int congruence, substitute, hbr, backbone, sweep, vivify, strengthen,
      subsume, bve, transitive;
  uint64_t rel_u, rel_v, rel_wait;
  int rel_trigger;
  uint64_t reduce_next, inprocess_next;
};

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); ++i) {
    const char *candidate = tactix_rule_name(i);
    if (candidate && !strcmp(candidate, name))
      return i;
  }
  fprintf(stderr, "tactix-cruxmiter-policy: missing rule '%s'\n", name);
  return -1;
}

static int parse_profile(const char *text, enum profile *out) {
  if (!strcmp(text, "search"))
    *out = PROFILE_SEARCH;
  else if (!strcmp(text, "positive"))
    *out = PROFILE_POSITIVE;
  else if (!strcmp(text, "saved"))
    *out = PROFILE_SAVED;
  else if (!strcmp(text, "gate"))
    *out = PROFILE_GATE;
  else if (!strcmp(text, "binary"))
    *out = PROFILE_BINARY;
  else if (!strcmp(text, "backbone"))
    *out = PROFILE_BACKBONE;
  else if (!strcmp(text, "backbone-first"))
    *out = PROFILE_BACKBONE_FIRST;
  else if (!strcmp(text, "backbone-close"))
    *out = PROFILE_BACKBONE_CLOSE;
  else if (!strcmp(text, "backbone-heavy"))
    *out = PROFILE_BACKBONE_HEAVY;
  else if (!strcmp(text, "sweep"))
    *out = PROFILE_SWEEP;
  else if (!strcmp(text, "full"))
    *out = PROFILE_FULL;
  else
    return 0;
  return 1;
}

static const char *profile_name(enum profile profile) {
  switch (profile) {
  case PROFILE_SEARCH: return "search";
  case PROFILE_POSITIVE: return "positive";
  case PROFILE_SAVED: return "saved";
  case PROFILE_GATE: return "gate";
  case PROFILE_BINARY: return "binary";
  case PROFILE_BACKBONE: return "backbone";
  case PROFILE_BACKBONE_FIRST: return "backbone-first";
  case PROFILE_BACKBONE_CLOSE: return "backbone-close";
  case PROFILE_BACKBONE_HEAVY: return "backbone-heavy";
  case PROFILE_SWEEP: return "sweep";
  case PROFILE_FULL: return "full";
  }
  return "invalid";
}

static const char *phase_name(enum profile profile) {
  if (profile == PROFILE_SEARCH) return "negative";
  if (profile == PROFILE_POSITIVE) return "positive";
  if (profile == PROFILE_SAVED) return "saved";
  return "default-policy";
}

static int manual_search(enum profile profile) {
  return profile == PROFILE_SEARCH || profile == PROFILE_POSITIVE ||
         profile == PROFILE_SAVED;
}

/* The benchmark files are plain DIMACS. Keep parsing in this external client
 * so it cannot reach Kernel or the internal frontend. */
static int read_dimacs(tactix *solver, const char *path, int *maximum_variable) {
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
      if (tactix_add_checked(solver, (int)value)) {
        fprintf(stderr, "tactix-cruxmiter-policy: rejected DIMACS literal\n");
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

/* Apply a bounded public-rule dose.  Every successful mutation is settled by
 * Propagate before the next application observes it.  A dose deliberately
 * hands control on after its cap: the development runs showed that driving
 * HBR or BVE to a local fixpoint spends far more than the whole default solve
 * on this family. */
static int dose_rule(struct policy *policy, int rule, unsigned cap,
                     uint64_t *applications) {
  unsigned count;
  if (rule < 0) return 1;
  for (count = 0; count < cap; ++count) {
    const int result = tactix_apply(policy->solver, rule);
    if (terminal(result) || result == 2) return result;
    if (result != 0) return result;
    ++*applications;
    {
      const int settled = propagate(policy);
      if (terminal(settled) || settled == 2) return settled;
    }
  }
  return 1;
}

static int root_pipeline(struct policy *policy) {
  uint64_t applications = 0;
  unsigned round;
  int result = propagate(policy);
  if (terminal(result) || result == 2 || manual_search(policy->profile))
    return result;

  if (policy->profile == PROFILE_BACKBONE_FIRST)
    return dose_rule(policy, policy->backbone, 512, &applications);

  /* Preserve circuit signatures: gate congruence publishes equivalences and
   * Substitute then collapses the whole CNF.  The doses mirror the useful
   * counts observed in DefaultPolicy's size-20 trace. */
  for (round = 0; round < 2; ++round) {
    const uint64_t before = applications;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, &applications);
    if (terminal(result) || result == 2) return result;
    if (applications == before) break;
  }
  if (policy->profile == PROFILE_GATE) return 1;

  if (policy->profile == PROFILE_BACKBONE) {
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    return 1;
  }

  if (policy->profile == PROFILE_BACKBONE_HEAVY) {
    result = dose_rule(policy, policy->backbone, 1024, &applications);
    if (terminal(result) || result == 2) return result;
    return 1;
  }

  if (policy->profile == PROFILE_BACKBONE_CLOSE) {
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    return dose_rule(policy, policy->substitute, 24, &applications);
  }

  if (policy->profile == PROFILE_SWEEP) {
    result = dose_rule(policy, policy->sweep, 4, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    return dose_rule(policy, policy->substitute, 24, &applications);
  }

  /* Implications exposed by the first collapse can reveal another gate layer.
   * Clean only after mining that layer. */
  for (round = 0; round < 2; ++round) {
    const uint64_t before = applications;
    result = dose_rule(policy, policy->hbr, 64, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, &applications);
    if (terminal(result) || result == 2) return result;
    if (applications == before) break;
  }
  if (policy->profile == PROFILE_BINARY) return 1;

  /* General clause work follows structural closure. BVE is deliberately late:
   * eliminating gate outputs first would erase the signatures above. */
  result = dose_rule(policy, policy->vivify, 16, &applications);
  if (terminal(result) || result == 2) return result;
  result = dose_rule(policy, policy->strengthen, 8, &applications);
  if (terminal(result) || result == 2) return result;
  result = dose_rule(policy, policy->subsume, 8, &applications);
  if (terminal(result) || result == 2) return result;
  result = dose_rule(policy, policy->bve, 300, &applications);
  if (terminal(result) || result == 2) return result;
  result = dose_rule(policy, policy->transitive, 64, &applications);
  if (terminal(result) || result == 2) return result;
  result = dose_rule(policy, policy->congruence, 4, &applications);
  if (terminal(result) || result == 2) return result;
  return dose_rule(policy, policy->substitute, 24, &applications);
}

static void tick_reluctant(struct policy *policy) {
  uint64_t wait;
  if (policy->rel_trigger || --policy->rel_wait) return;
  if ((policy->rel_u & (0 - policy->rel_u)) == policy->rel_v) {
    ++policy->rel_u;
    policy->rel_v = 1;
  } else {
    policy->rel_v *= 2;
  }
  wait = policy->rel_v * UINT64_C(1024);
  if (wait > (UINT64_C(1) << 20)) {
    policy->rel_u = policy->rel_v = 1;
    wait = 1024;
  }
  policy->rel_wait = wait;
  policy->rel_trigger = 1;
}

static int solve(struct policy *policy) {
  if (tactix_status(policy->solver) == 20) return 20;
  int result = root_pipeline(policy);
  if (terminal(result)) return result;
  if (result == 2) return 20;

  /* The three structural profiles are ordered public-rule preludes followed
   * by the shipped search policy.  This isolates the question under test—the
   * proof-rule order—while retaining the mature restart/rephase machinery.
   * PROFILE_SEARCH remains the direct stable=2,target=0,phase=0 translation. */
  if (!manual_search(policy->profile))
    return tactix_solve(policy->solver);

  /* Enter stable mode once, never switch back, and never capture a target
   * phase: the public-policy equivalent of stable=2,target=0. */
  if (tactix_stat(policy->solver, TACTIX_STAT_STABLE) == 0 &&
      tactix_apply(policy->solver, policy->switchmode) < 0)
    return 0;

  for (;;) {
    result = propagate(policy);
    if (terminal(result)) return result;
    if (result == 2) {
      result = tactix_apply(policy->solver, policy->analyze);
      if (terminal(result)) return result;
      if (result < 0) return 0;
      tick_reluctant(policy);
      continue;
    }
    if (!tactix_stat(policy->solver, TACTIX_STAT_UNASSIGNED)) return 10;

    {
      const uint64_t conflicts =
          tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS);
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
      if (!manual_search(policy->profile) &&
          conflicts >= policy->inprocess_next) {
        if (tactix_stat(policy->solver, TACTIX_STAT_LEVEL)) {
          const int restarted =
              tactix_apply_instance(policy->solver, policy->restart, 0);
          if (terminal(restarted)) return restarted;
        }
        result = root_pipeline(policy);
        if (terminal(result)) return result;
        if (result == 2) return 20;
        policy->inprocess_next = conflicts + 10000;
        continue;
      }
    }

    /* The stable heap supplies variable order. Negating the returned literal
     * fixes the polarity to false, translating KISSAT phase=0. */
    {
      int candidate;
      if (tactix_decide_candidates(policy->solver, &candidate, 1) != 1)
        return tactix_status(policy->solver);
      if (policy->profile == PROFILE_SEARCH)
        candidate = candidate < 0 ? candidate : -candidate;
      else if (policy->profile == PROFILE_POSITIVE)
        candidate = candidate < 0 ? -candidate : candidate;
      if (tactix_decide(policy->solver, candidate)) return 0;
    }
  }
}

static int init_policy(struct policy *policy, tactix *solver,
                       enum profile profile) {
  memset(policy, 0, sizeof *policy);
  policy->solver = solver;
  policy->profile = profile;
  policy->propagate = rule_id("Propagate");
  policy->analyze = rule_id("ConflictAnalysis");
  policy->restart = rule_id("Restart");
  policy->reduce = rule_id("Reduce");
  policy->switchmode = rule_id("SwitchMode");
  policy->congruence = rule_id("Congruence");
  policy->substitute = rule_id("Substitute");
  policy->hbr = rule_id("HyperBinaryRes");
  policy->backbone = rule_id("Backbone");
  policy->sweep = rule_id("Sweep");
  policy->vivify = rule_id("Vivify");
  policy->strengthen = rule_id("Strengthen");
  policy->subsume = rule_id("SubsumeForward");
  policy->bve = rule_id("BVE");
  policy->transitive = rule_id("TransitiveReduce");
  policy->rel_u = policy->rel_v = 1;
  policy->rel_wait = 1024;
  policy->reduce_next = 2000;
  policy->inprocess_next = 10000;
  return policy->propagate >= 0 && policy->analyze >= 0 &&
         policy->restart >= 0 && policy->reduce >= 0 &&
         policy->switchmode >= 0 && policy->congruence >= 0 &&
         policy->substitute >= 0 && policy->hbr >= 0 &&
         policy->backbone >= 0 && policy->sweep >= 0 &&
         policy->vivify >= 0 && policy->strengthen >= 0 &&
         policy->subsume >= 0 && policy->bve >= 0 &&
         policy->transitive >= 0;
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
  enum profile profile = PROFILE_FULL;
  const char *cnf = NULL, *proof = NULL;
  int maximum_variable = 0, result;
  tactix *solver;
  struct policy policy;
  int arg;
  for (arg = 1; arg < argc; ++arg) {
    if (!strncmp(argv[arg], "--profile=", 10)) {
      if (!parse_profile(argv[arg] + 10, &profile)) {
        fprintf(stderr, "unknown cruxmiter profile '%s'\n", argv[arg] + 10);
        return 1;
      }
    } else if (!strcmp(argv[arg], "--drat")) {
      /* Makes the certificate contract explicit to benchmark runners. */
    } else if (!cnf) {
      cnf = argv[arg];
    } else if (!proof) {
      proof = argv[arg];
    } else {
        fprintf(stderr, "usage: %s [--profile=search|positive|saved|gate|binary|backbone|backbone-first|backbone-close|backbone-heavy|sweep|full] "
                      "[--drat] CNF [PROOF]\n", argv[0]);
      return 1;
    }
  }
  if (!cnf) {
    fprintf(stderr, "usage: %s [--profile=search|positive|saved|gate|binary|backbone|backbone-first|backbone-close|backbone-heavy|sweep|full] "
                    "[--drat] CNF [PROOF]\n", argv[0]);
    return 1;
  }
  solver = tactix_init();
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-cruxmiter-policy: cannot create proof '%s'\n", proof);
    tactix_release(solver);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable)) {
    tactix_release(solver);
    return 1;
  }
  if (tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&policy, solver, profile)) {
    tactix_release(solver);
    return 1;
  }
  printf("c cruxmiter-policy profile=%s "
         "search=evsids/reluctant phase=%s target=off\n",
         profile_name(profile), phase_name(profile));
  result = solve(&policy);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c crux-rules congruence=%" PRIu64 " substitute=%" PRIu64
         " hbr=%" PRIu64 " vivify=%" PRIu64 " bve=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES),
         tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS),
         tactix_stat(solver, TACTIX_STAT_HBR_BINARIES),
         tactix_stat(solver, TACTIX_STAT_VIVIFIED),
         tactix_stat(solver, TACTIX_STAT_BVE_ELIMINATED));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, maximum_variable);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  return result == 10 || result == 20 ? result : 0;
}
