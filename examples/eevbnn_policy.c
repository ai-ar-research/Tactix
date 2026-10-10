/* Public-interface policy experiments for EEVBNN-generated CNFs.
 *
 * The encodings are dominated by Boolean-network gates and cardinality
 * auxiliaries.  The bnn profiles expose those layers before search; the
 * stable profiles keep EVSIDS active and make phase selection explicit.
 * This client uses only include/tactix.h.
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
#include <zlib.h>

enum profile {
  PROFILE_DEFAULT,
  PROFILE_STABLE_NEGATIVE,
  PROFILE_STABLE_POSITIVE,
  PROFILE_STABLE_SAVED,
  PROFILE_GATE_DEFAULT,
  PROFILE_BNN_DEFAULT,
  PROFILE_GATE_STABLE_NEGATIVE,
  PROFILE_BNN_STABLE_NEGATIVE,
  PROFILE_LUCKY_FORWARD_FALSE,
  PROFILE_LUCKY_FORWARD_TRUE,
  PROFILE_LUCKY_BACKWARD_FALSE,
  PROFILE_LUCKY_BACKWARD_TRUE,
  PROFILE_LUCKY_BNN_CASCADE,
  PROFILE_INPUT_STABLE_NEGATIVE,
  PROFILE_INPUT_STABLE_POSITIVE,
  PROFILE_INPUT_AUTO
};

struct policy {
  tactix *solver;
  enum profile profile;
  int propagate, analyze, restart, reduce, switchmode;
  int congruence, substitute, hyperbinary, vivify, strengthen, subsume, bve;
  int lucky;
  int input_extent, input_cursor, input_positive, admitted;
  uint64_t input_decisions, heap_decisions;
  uint64_t reluctant_u, reluctant_v, reluctant_wait;
  int reluctant_trigger;
  uint64_t reduce_next;
};

static const char *profile_name(enum profile profile) {
  switch (profile) {
  case PROFILE_DEFAULT: return "default";
  case PROFILE_STABLE_NEGATIVE: return "stable-negative";
  case PROFILE_STABLE_POSITIVE: return "stable-positive";
  case PROFILE_STABLE_SAVED: return "stable-saved";
  case PROFILE_GATE_DEFAULT: return "gate-default";
  case PROFILE_BNN_DEFAULT: return "bnn-default";
  case PROFILE_GATE_STABLE_NEGATIVE: return "gate-stable-negative";
  case PROFILE_BNN_STABLE_NEGATIVE: return "bnn-stable-negative";
  case PROFILE_LUCKY_FORWARD_FALSE: return "lucky-forward-false";
  case PROFILE_LUCKY_FORWARD_TRUE: return "lucky-forward-true";
  case PROFILE_LUCKY_BACKWARD_FALSE: return "lucky-backward-false";
  case PROFILE_LUCKY_BACKWARD_TRUE: return "lucky-backward-true";
  case PROFILE_LUCKY_BNN_CASCADE: return "lucky-bnn-cascade";
  case PROFILE_INPUT_STABLE_NEGATIVE: return "input-stable-negative";
  case PROFILE_INPUT_STABLE_POSITIVE: return "input-stable-positive";
  case PROFILE_INPUT_AUTO: return "input-auto";
  }
  return "invalid";
}

static int parse_profile(const char *text, enum profile *profile) {
  enum profile candidate;
  for (candidate = PROFILE_DEFAULT;
       candidate <= PROFILE_INPUT_AUTO; ++candidate)
    if (!strcmp(text, profile_name(candidate))) {
      *profile = candidate;
      return 1;
    }
  return 0;
}

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); ++i) {
    const char *candidate = tactix_rule_name(i);
    if (candidate && !strcmp(candidate, name)) return i;
  }
  fprintf(stderr, "tactix-eevbnn-policy: missing rule '%s'\n", name);
  return -1;
}

static int read_dimacs(tactix *solver, const char *path, int *maximum_variable,
                       int *input_extent, uint64_t *declared_clauses) {
  gzFile input = gzopen(path, "rb");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-eevbnn-policy: cannot open '%s'\n", path);
    return 0;
  }
  *maximum_variable = 0;
  *input_extent = 0;
  *declared_clauses = 0;
  while (gzgets(input, line, sizeof line)) {
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!*cursor) continue;
    if (*cursor == 'c') {
      int variable;
      char name[64];
      if (sscanf(cursor, "c vname %d %63s", &variable, name) == 2 &&
          !strncmp(name, "inp_", 4) && variable > *input_extent)
        *input_extent = variable;
      continue;
    }
    if (*cursor == 'p') {
      char kind[8];
      int variables;
      unsigned long long clauses;
      if (sscanf(cursor, "p %7s %d %llu", kind, &variables, &clauses) != 3 ||
          strcmp(kind, "cnf") || variables < 0) {
        fprintf(stderr, "tactix-eevbnn-policy: malformed DIMACS header\n");
        gzclose(input);
        return 0;
      }
      saw_header = 1;
      *declared_clauses = (uint64_t)clauses;
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
        fprintf(stderr, "tactix-eevbnn-policy: malformed DIMACS literal\n");
        gzclose(input);
        return 0;
      }
      if (tactix_add_checked(solver, (int)value)) {
        fprintf(stderr, "tactix-eevbnn-policy: rejected DIMACS literal\n");
        gzclose(input);
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
  if (!gzeof(input) || !saw_header || open_clause) {
    int error_number = Z_OK;
    const char *message = gzerror(input, &error_number);
    fprintf(stderr, "tactix-eevbnn-policy: incomplete DIMACS input%s%s\n",
            error_number == Z_OK ? "" : ": ",
            error_number == Z_OK ? "" : message);
    gzclose(input);
    return 0;
  }
  gzclose(input);
  return 1;
}

static int terminal(int result) { return result == 10 || result == 20; }

static int dose(struct policy *policy, int rule, unsigned cap) {
  unsigned i;
  for (i = 0; i < cap; ++i) {
    int result = tactix_apply(policy->solver, rule);
    if (terminal(result) || result == 2) return result;
    if (result < 0) break;
    result = tactix_apply(policy->solver, policy->propagate);
    if (terminal(result) || result == 2) return result;
  }
  return 1;
}

static int uses_gate_prelude(enum profile profile) {
  return profile == PROFILE_GATE_DEFAULT || profile == PROFILE_BNN_DEFAULT ||
         profile == PROFILE_GATE_STABLE_NEGATIVE ||
         profile == PROFILE_BNN_STABLE_NEGATIVE;
}

static int uses_bnn_prelude(enum profile profile) {
  return profile == PROFILE_BNN_DEFAULT ||
         profile == PROFILE_BNN_STABLE_NEGATIVE;
}

static int uses_manual_search(enum profile profile) {
  return profile == PROFILE_STABLE_NEGATIVE ||
         profile == PROFILE_STABLE_POSITIVE ||
         profile == PROFILE_STABLE_SAVED ||
         profile == PROFILE_GATE_STABLE_NEGATIVE ||
         profile == PROFILE_BNN_STABLE_NEGATIVE ||
         profile == PROFILE_INPUT_STABLE_NEGATIVE ||
         profile == PROFILE_INPUT_STABLE_POSITIVE ||
         profile == PROFILE_INPUT_AUTO;
}

static int uses_input_branching(enum profile profile) {
  return profile == PROFILE_INPUT_STABLE_NEGATIVE ||
         profile == PROFILE_INPUT_STABLE_POSITIVE ||
         profile == PROFILE_INPUT_AUTO;
}

static int lucky_probe(enum profile profile) {
  switch (profile) {
  case PROFILE_LUCKY_FORWARD_FALSE: return 2;
  case PROFILE_LUCKY_FORWARD_TRUE: return 3;
  case PROFILE_LUCKY_BACKWARD_FALSE: return 4;
  case PROFILE_LUCKY_BACKWARD_TRUE: return 5;
  default: return -1;
  }
}

static int prelude(struct policy *policy) {
  int result = tactix_apply(policy->solver, policy->propagate);
  int probe;
  if (terminal(result) || result == 2) return result;
  probe = lucky_probe(policy->profile);
  if (probe >= 0)
    return tactix_apply_instance(policy->solver, policy->lucky,
                                 (uint64_t)probe);
  if (policy->profile == PROFILE_LUCKY_BNN_CASCADE) {
    /* M22 gives the counter auxiliaries a stable topological order. */
    static const uint64_t order[] = {2, 4, 3, 5};
    unsigned i;
    for (i = 0; i < sizeof order / sizeof *order; ++i) {
      result = tactix_apply_instance(policy->solver, policy->lucky, order[i]);
      if (terminal(result) || result == 2) return result;
    }
    return result;
  }
  if (!uses_gate_prelude(policy->profile))
    return result;

  /* Preserve gates until equivalences have been published and collapsed. */
  result = dose(policy, policy->congruence, 2);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->substitute, 16);
  if (terminal(result) || result == 2 || !uses_bnn_prelude(policy->profile))
    return result;

  /* Mine binary implications from sequential/cardinality counters, collapse
   * newly exposed gates, then simplify the remaining clause layer. */
  result = dose(policy, policy->hyperbinary, 32);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->congruence, 2);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->substitute, 16);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->vivify, 8);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->strengthen, 4);
  if (terminal(result) || result == 2) return result;
  result = dose(policy, policy->subsume, 4);
  if (terminal(result) || result == 2) return result;
  return dose(policy, policy->bve, 128);
}

static void tick_reluctant(struct policy *policy) {
  if (policy->reluctant_trigger || --policy->reluctant_wait) return;
  if ((policy->reluctant_u & (0 - policy->reluctant_u)) ==
      policy->reluctant_v) {
    ++policy->reluctant_u;
    policy->reluctant_v = 1;
  } else {
    policy->reluctant_v *= 2;
  }
  policy->reluctant_wait = policy->reluctant_v * UINT64_C(1024);
  if (policy->reluctant_wait > (UINT64_C(1) << 20)) {
    policy->reluctant_u = policy->reluctant_v = 1;
    policy->reluctant_wait = 1024;
  }
  policy->reluctant_trigger = 1;
}

static int solve(struct policy *policy) {
  int result = prelude(policy);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  if (!uses_manual_search(policy->profile) ||
      (policy->profile == PROFILE_INPUT_AUTO && !policy->admitted))
    return tactix_solve(policy->solver);

  /* SwitchMode at the root enters stable EVSIDS.  It is deliberately never
   * applied again, matching a stable-only search. */
  if (tactix_stat(policy->solver, TACTIX_STAT_STABLE) == 0 &&
      tactix_apply(policy->solver, policy->switchmode) < 0)
    return 0;

  for (;;) {
    result = tactix_apply(policy->solver, policy->propagate);
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
      if (policy->reluctant_trigger) {
        const int restarted = tactix_apply_instance(
            policy->solver, policy->restart, TACTIX_INSTANCE_CANONICAL);
        if (terminal(restarted)) return restarted;
        if (restarted >= 0) {
          policy->reluctant_trigger = 0;
          policy->input_cursor = 1;
          continue;
        }
      }
    }
    {
      int candidate;
      if (uses_input_branching(policy->profile)) {
        int input_decided = 0;
        while (policy->input_cursor <= policy->input_extent) {
          candidate = policy->input_cursor++;
          if (policy->profile == PROFILE_INPUT_STABLE_NEGATIVE ||
              (policy->profile == PROFILE_INPUT_AUTO &&
               !policy->input_positive))
            candidate = -candidate;
          if (!tactix_decide(policy->solver, candidate)) {
            ++policy->input_decisions;
            input_decided = 1;
            break;
          }
        }
        if (input_decided) continue;
      }
      if (tactix_decide_candidates(policy->solver, &candidate, 1) != 1)
        return tactix_status(policy->solver);
      if (policy->profile == PROFILE_STABLE_POSITIVE)
        candidate = candidate < 0 ? -candidate : candidate;
      else if (policy->profile != PROFILE_STABLE_SAVED)
        candidate = candidate < 0 ? candidate : -candidate;
      if (tactix_decide(policy->solver, candidate)) return 0;
      ++policy->heap_decisions;
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
  policy->hyperbinary = rule_id("HyperBinaryRes");
  policy->vivify = rule_id("Vivify");
  policy->strengthen = rule_id("Strengthen");
  policy->subsume = rule_id("SubsumeForward");
  policy->bve = rule_id("BVE");
  policy->lucky = rule_id("Lucky");
  policy->reluctant_u = policy->reluctant_v = 1;
  policy->reluctant_wait = 1024;
  policy->reduce_next = 2000;
  policy->input_cursor = 1;
  return policy->propagate >= 0 && policy->analyze >= 0 &&
         policy->restart >= 0 && policy->reduce >= 0 &&
         policy->switchmode >= 0 && policy->congruence >= 0 &&
         policy->substitute >= 0 && policy->hyperbinary >= 0 &&
         policy->vivify >= 0 && policy->strengthen >= 0 &&
         policy->subsume >= 0 && policy->bve >= 0 && policy->lucky >= 0;
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
  fprintf(stderr, "usage: %s [--profile=default|stable-negative|stable-positive|stable-saved|gate-default|bnn-default|gate-stable-negative|bnn-stable-negative|lucky-forward-false|lucky-forward-true|lucky-backward-false|lucky-backward-true|lucky-bnn-cascade|input-stable-negative|input-stable-positive|input-auto] [--drat] CNF[.gz] [PROOF]\n", program);
}

int main(int argc, char **argv) {
  enum profile profile = PROFILE_INPUT_AUTO;
  const char *cnf = NULL, *proof = NULL;
  int maximum_variable = 0, input_extent = 0, result, arg;
  uint64_t declared_clauses = 0;
  tactix *solver;
  struct policy policy;
  for (arg = 1; arg < argc; ++arg) {
    if (!strncmp(argv[arg], "--profile=", 10)) {
      if (!parse_profile(argv[arg] + 10, &profile)) {
        fprintf(stderr, "unknown EEVBNN profile '%s'\n", argv[arg] + 10);
        usage(argv[0]);
        return 1;
      }
    } else if (!strcmp(argv[arg], "--drat")) {
      /* Declares the proof format to tools/bench/run.py. */
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
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-eevbnn-policy: cannot create proof '%s'\n", proof);
    tactix_release(solver);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable, &input_extent,
                   &declared_clauses) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&policy, solver, profile)) {
    tactix_release(solver);
    return 1;
  }
  policy.input_extent = input_extent;
  policy.admitted = input_extent >= 2400 && input_extent <= 2800 &&
                    maximum_variable >= 1000000 &&
                    maximum_variable <= 1200000 &&
                    declared_clauses * 10 >= (uint64_t)maximum_variable * 28 &&
                    declared_clauses * 10 <= (uint64_t)maximum_variable * 32;
  policy.input_positive = maximum_variable < 1100000;
  printf("c eevbnn-policy profile=%s interface=public\n", profile_name(profile));
  printf("c eevbnn-admission input-vname-extent=%d variables=%d clauses=%" PRIu64
         " admitted=%s phase=%s\n", input_extent, maximum_variable,
         declared_clauses, policy.admitted ? "yes" : "no",
         policy.input_positive ? "positive" : "negative");
  result = solve(&policy);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  printf("c eevbnn-telemetry input-decisions=%" PRIu64
         " heap-decisions=%" PRIu64 " total-decisions=%" PRIu64
         " conflicts=%" PRIu64 " propagations=%" PRIu64
         " restarts=%" PRIu64 " reductions=%" PRIu64
         " stable=%" PRIu64 " root-fixed=%" PRIu64 "\n",
         policy.input_decisions, policy.heap_decisions,
         tactix_stat(solver, TACTIX_STAT_DECISIONS),
         tactix_stat(solver, TACTIX_STAT_CONFLICTS),
         tactix_stat(solver, TACTIX_STAT_PROPAGATIONS),
         tactix_stat(solver, TACTIX_STAT_RESTARTS),
         tactix_stat(solver, TACTIX_STAT_REDUCTIONS),
         tactix_stat(solver, TACTIX_STAT_STABLE),
         tactix_stat(solver, TACTIX_STAT_ROOT_FIXED));
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
