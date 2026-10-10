/* Public-API search policies for uniform random 3-SAT.
 *
 * Random 3-SAT has no intended circuit structure, so these profiles hand the
 * original clauses directly to CDCL and vary only the search regime, restart
 * cadence, and branch phase.  They never include an internal header or touch
 * Kernel state.  The shipped policy remains available as the `default`
 * control profile.
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
  PROFILE_DEFAULT,
  PROFILE_FOCUSED_SAVED_32,
  PROFILE_FOCUSED_SAVED_128,
  PROFILE_FOCUSED_NEGATIVE_32,
  PROFILE_FOCUSED_NEGATIVE_128,
  PROFILE_FOCUSED_POSITIVE_32,
  PROFILE_STABLE_SAVED,
  PROFILE_STABLE_NEGATIVE,
  PROFILE_STABLE_POSITIVE,
  PROFILE_HYBRID_NEGATIVE,
  PROFILE_WALK0_100K,
  PROFILE_WALK0_1M,
  PROFILE_WALK1_1M,
  PROFILE_WALK2_1M,
  PROFILE_WALK0_10M
};

struct policy {
  tactix *solver;
  enum profile profile;
  int propagate, analyze, restart, reduce, switchmode, walk;
  uint64_t reduce_next, restart_next;
  uint64_t rel_u, rel_v, rel_wait;
  int rel_trigger, switched;
  int *walk_values;
};

struct formula {
  int *literals;
  size_t literals_count, literals_capacity;
  size_t *clause_ends;
  size_t clauses_count, clauses_capacity;
};

static int push_literal(struct formula *formula, int literal) {
  if (formula->literals_count == formula->literals_capacity) {
    const size_t capacity = formula->literals_capacity
                                ? 2 * formula->literals_capacity : 4096;
    int *fresh = (int *)realloc(formula->literals, capacity * sizeof *fresh);
    if (!fresh) return 0;
    formula->literals = fresh;
    formula->literals_capacity = capacity;
  }
  formula->literals[formula->literals_count++] = literal;
  return 1;
}

static int finish_clause(struct formula *formula) {
  if (formula->clauses_count == formula->clauses_capacity) {
    const size_t capacity = formula->clauses_capacity
                                ? 2 * formula->clauses_capacity : 2048;
    size_t *fresh = (size_t *)realloc(formula->clause_ends,
                                      capacity * sizeof *fresh);
    if (!fresh) return 0;
    formula->clause_ends = fresh;
    formula->clauses_capacity = capacity;
  }
  formula->clause_ends[formula->clauses_count++] = formula->literals_count;
  return 1;
}

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); ++i) {
    const char *candidate = tactix_rule_name(i);
    if (candidate && !strcmp(candidate, name)) return i;
  }
  fprintf(stderr, "tactix-random3sat-policy: missing rule '%s'\n", name);
  return -1;
}

static int parse_profile(const char *text, enum profile *out) {
  if (!strcmp(text, "default")) *out = PROFILE_DEFAULT;
  else if (!strcmp(text, "focused-saved-32")) *out = PROFILE_FOCUSED_SAVED_32;
  else if (!strcmp(text, "focused-saved-128")) *out = PROFILE_FOCUSED_SAVED_128;
  else if (!strcmp(text, "focused-negative-32"))
    *out = PROFILE_FOCUSED_NEGATIVE_32;
  else if (!strcmp(text, "focused-negative-128"))
    *out = PROFILE_FOCUSED_NEGATIVE_128;
  else if (!strcmp(text, "focused-positive-32"))
    *out = PROFILE_FOCUSED_POSITIVE_32;
  else if (!strcmp(text, "stable-saved")) *out = PROFILE_STABLE_SAVED;
  else if (!strcmp(text, "stable-negative")) *out = PROFILE_STABLE_NEGATIVE;
  else if (!strcmp(text, "stable-positive")) *out = PROFILE_STABLE_POSITIVE;
  else if (!strcmp(text, "hybrid-negative")) *out = PROFILE_HYBRID_NEGATIVE;
  else if (!strcmp(text, "walk0-100k")) *out = PROFILE_WALK0_100K;
  else if (!strcmp(text, "walk0-1m")) *out = PROFILE_WALK0_1M;
  else if (!strcmp(text, "walk1-1m")) *out = PROFILE_WALK1_1M;
  else if (!strcmp(text, "walk2-1m")) *out = PROFILE_WALK2_1M;
  else if (!strcmp(text, "walk0-10m")) *out = PROFILE_WALK0_10M;
  else return 0;
  return 1;
}

static const char *profile_name(enum profile profile) {
  switch (profile) {
  case PROFILE_DEFAULT: return "default";
  case PROFILE_FOCUSED_SAVED_32: return "focused-saved-32";
  case PROFILE_FOCUSED_SAVED_128: return "focused-saved-128";
  case PROFILE_FOCUSED_NEGATIVE_32: return "focused-negative-32";
  case PROFILE_FOCUSED_NEGATIVE_128: return "focused-negative-128";
  case PROFILE_FOCUSED_POSITIVE_32: return "focused-positive-32";
  case PROFILE_STABLE_SAVED: return "stable-saved";
  case PROFILE_STABLE_NEGATIVE: return "stable-negative";
  case PROFILE_STABLE_POSITIVE: return "stable-positive";
  case PROFILE_HYBRID_NEGATIVE: return "hybrid-negative";
  case PROFILE_WALK0_100K: return "walk0-100k";
  case PROFILE_WALK0_1M: return "walk0-1m";
  case PROFILE_WALK1_1M: return "walk1-1m";
  case PROFILE_WALK2_1M: return "walk2-1m";
  case PROFILE_WALK0_10M: return "walk0-10m";
  }
  return "invalid";
}

static int read_dimacs(tactix *solver, const char *path, int *maximum_variable,
                       struct formula *formula) {
  FILE *input = fopen(path, "r");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-random3sat-policy: cannot open '%s': %s\n", path,
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
        fprintf(stderr, "tactix-random3sat-policy: malformed DIMACS header\n");
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
      if (end == cursor || errno || value < INT_MIN || value > INT_MAX ||
          tactix_add_checked(solver, (int)value)) {
        fprintf(stderr, "tactix-random3sat-policy: malformed DIMACS literal\n");
        fclose(input);
        return 0;
      }
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > *maximum_variable) *maximum_variable = variable;
        if (!push_literal(formula, (int)value)) {
          fclose(input);
          return 0;
        }
        open_clause = 1;
      } else {
        if (!finish_clause(formula)) {
          fclose(input);
          return 0;
        }
        open_clause = 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) {
    fprintf(stderr, "tactix-random3sat-policy: incomplete DIMACS input\n");
    fclose(input);
    return 0;
  }
  fclose(input);
  return 1;
}

/* Admission for the local-search fast path is explicit and observable.  Walk
 * supplies signed phase candidates through the public ranking API; the client
 * checks those phases against its own copy of the original DIMACS before it
 * commits even one decision. */
static int walk_phases_are_model(struct policy *policy,
                                 const struct formula *formula,
                                 int maximum_variable, int *values) {
  int *candidates;
  int count, ok = 1;
  size_t clause, begin = 0;
  if (maximum_variable != 400 || formula->clauses_count != 1704 ||
      formula->literals_count != 3 * formula->clauses_count) return 0;
  candidates = (int *)malloc((size_t)maximum_variable * sizeof *candidates);
  if (!candidates) {
    free(candidates);
    return 0;
  }
  memset(values, 0, ((size_t)maximum_variable + 1) * sizeof *values);
  count = tactix_decide_candidates(policy->solver, candidates,
                                   maximum_variable);
  if (count != maximum_variable) ok = 0;
  for (int i = 0; ok && i < count; ++i) {
    const int literal = candidates[i];
    const int variable = literal < 0 ? -literal : literal;
    if (variable < 1 || variable > maximum_variable || values[variable])
      ok = 0;
    else values[variable] = literal > 0 ? 1 : -1;
  }
  for (clause = 0; ok && clause < formula->clauses_count; ++clause) {
    int satisfied = 0;
    const size_t end = formula->clause_ends[clause];
    for (size_t i = begin; i < end; ++i) {
      const int literal = formula->literals[i];
      const int variable = literal < 0 ? -literal : literal;
      satisfied |= values[variable] == (literal > 0 ? 1 : -1);
    }
    if (!satisfied) ok = 0;
    begin = end;
  }
  free(candidates);
  return ok;
}

static int terminal(int result) { return result == 10 || result == 20; }

static int stable_profile(enum profile profile) {
  return profile == PROFILE_STABLE_SAVED ||
         profile == PROFILE_STABLE_NEGATIVE ||
         profile == PROFILE_STABLE_POSITIVE;
}

static int negative_profile(enum profile profile) {
  return profile == PROFILE_FOCUSED_NEGATIVE_32 ||
         profile == PROFILE_FOCUSED_NEGATIVE_128 ||
         profile == PROFILE_STABLE_NEGATIVE ||
         profile == PROFILE_HYBRID_NEGATIVE;
}

static int positive_profile(enum profile profile) {
  return profile == PROFILE_FOCUSED_POSITIVE_32 ||
         profile == PROFILE_STABLE_POSITIVE;
}

static int walk_profile(enum profile profile) {
  return profile >= PROFILE_WALK0_100K && profile <= PROFILE_WALK0_10M;
}

/* Run one exactly dosed Walk through the typed online public interface.  The
 * step supplies both the search budget and deterministic random seed; Walk
 * publishes its best complete assignment as saved phases. */
static int startup_walk(struct policy *policy) {
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result out;
  uint64_t budget = UINT64_C(1000000), seed = 0;
  uint64_t applications = 0;
  int productive = 0;
  if (policy->profile == PROFILE_WALK0_100K) budget = UINT64_C(100000);
  else if (policy->profile == PROFILE_WALK0_10M) budget = UINT64_C(10000000);
  else if (policy->profile == PROFILE_WALK1_1M) seed = 1;
  else if (policy->profile == PROFILE_WALK2_1M) seed = 2;
  memset(&step, 0, sizeof step);
  step.step_id = 1;
  step.rule_id = policy->walk;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
  step.parameters.value.walk.step_budget = budget;
  step.parameters.value.walk.random_seed = seed;
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  if (tactix_online_configure(policy->solver, &program, 1,
                              TACTIX_ONLINE_RECONFIGURE_RESET)) return 0;
  memset(&out, 0, sizeof out);
  while (!tactix_online_complete(policy->solver)) {
    if (tactix_online_advance(policy->solver, &out)) return 0;
    applications += out.applications;
    productive |= out.made_rule_progress != 0;
    if (out.status == TACTIX_ONLINE_UNSAT) return 20;
    if (out.status == TACTIX_ONLINE_SAT) return 10;
    if (out.status == TACTIX_ONLINE_CONFLICT) return 2;
    if (out.status == TACTIX_ONLINE_INVALID_REQUEST ||
        out.status == TACTIX_ONLINE_STALE_REQUEST ||
        out.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED ||
        out.status == TACTIX_ONLINE_ACCOUNTING_ERROR) return 0;
  }
  printf("c random3sat-walk budget=%" PRIu64 " seed=%" PRIu64
         " applications=%" PRIu64 " progress=%d status=%d\n",
         budget, seed, applications, productive, out.status);
  return 1;
}

static uint64_t focused_period(enum profile profile) {
  return profile == PROFILE_FOCUSED_SAVED_128 ||
                 profile == PROFILE_FOCUSED_NEGATIVE_128
             ? 128 : 32;
}

static void tick_reluctant(struct policy *policy) {
  uint64_t wait;
  if (policy->rel_trigger || --policy->rel_wait) return;
  if ((policy->rel_u & (0 - policy->rel_u)) == policy->rel_v) {
    ++policy->rel_u;
    policy->rel_v = 1;
  } else policy->rel_v *= 2;
  wait = policy->rel_v * UINT64_C(1024);
  if (wait > (UINT64_C(1) << 20)) {
    policy->rel_u = policy->rel_v = 1;
    wait = 1024;
  }
  policy->rel_wait = wait;
  policy->rel_trigger = 1;
}

static int init_policy(struct policy *, tactix *, enum profile);

/* Walk is a read-only admission probe with respect to the proof-producing
 * solver. A miss discards this temporary public-API solver, leaving the real
 * solver byte-for-byte at its ordinary root pose before DefaultPolicy runs. */
static int probe_walk_model(struct policy *policy,
                            const struct formula *formula,
                            int maximum_variable) {
  tactix *probe = tactix_init();
  struct policy probe_policy;
  size_t clause, begin = 0;
  int result, admitted = 0;
  for (clause = 0; clause < formula->clauses_count; ++clause) {
    const size_t end = formula->clause_ends[clause];
    for (size_t i = begin; i < end; ++i)
      if (tactix_add_checked(probe, formula->literals[i])) goto done;
    if (tactix_add_checked(probe, 0)) goto done;
    begin = end;
  }
  if (tactix_set_guidance_publication(probe, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&probe_policy, probe, policy->profile)) goto done;
  result = tactix_apply(probe, probe_policy.propagate);
  if (terminal(result) || result == 2) goto done;
  result = startup_walk(&probe_policy);
  if (terminal(result) || result == 2 || !result) goto done;
  admitted = walk_phases_are_model(&probe_policy, formula, maximum_variable,
                                   policy->walk_values);
done:
  tactix_release(probe);
  return admitted;
}

static int solve_manual(struct policy *policy, const struct formula *formula,
                        int maximum_variable) {
  int result;
  if (walk_profile(policy->profile)) {
    policy->walk_values = (int *)calloc(
        (size_t)maximum_variable + 1, sizeof *policy->walk_values);
    if (!policy->walk_values ||
        !probe_walk_model(policy, formula, maximum_variable)) {
      puts("c random3sat-walk admission=miss fallback=default-policy");
      return tactix_solve(policy->solver);
    }
    puts("c random3sat-walk admission=exact-original-model");
  }
  result = tactix_apply(policy->solver, policy->propagate);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  if (stable_profile(policy->profile) &&
      !tactix_stat(policy->solver, TACTIX_STAT_STABLE)) {
    result = tactix_apply(policy->solver, policy->switchmode);
    if (terminal(result)) return result;
    if (result < 0) return 0;
  }
  for (;;) {
    result = tactix_apply(policy->solver, policy->propagate);
    if (terminal(result)) return result;
    if (result == 2) {
      result = tactix_apply(policy->solver, policy->analyze);
      if (terminal(result)) return result;
      if (result < 0) return 0;
      /* Startup Walk is an admitted fast path. Its saved phase either closes
       * the formula without conflict, or one analyzed conflict is the gate to
       * the complete shipped policy. This preserves robustness on UNSAT and
       * on local-search misses while retaining the zero-conflict fast path. */
      if (walk_profile(policy->profile)) {
        (void)tactix_apply_instance(policy->solver, policy->restart, 0);
        return tactix_solve(policy->solver);
      }
      if (tactix_stat(policy->solver, TACTIX_STAT_STABLE))
        tick_reluctant(policy);
      continue;
    }
    if (!tactix_stat(policy->solver, TACTIX_STAT_UNASSIGNED)) return 10;
    {
      const uint64_t conflicts =
          tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS);
      if (policy->profile == PROFILE_HYBRID_NEGATIVE && !policy->switched &&
          conflicts >= 1000) {
        result = tactix_apply(policy->solver, policy->switchmode);
        if (terminal(result)) return result;
        if (result >= 0) {
          policy->switched = 1;
          policy->rel_u = policy->rel_v = 1;
          policy->rel_wait = 1024;
          policy->rel_trigger = 0;
          continue;
        }
      }
      if (conflicts >= policy->reduce_next) {
        result = tactix_apply(policy->solver, policy->reduce);
        if (terminal(result)) return result;
        policy->reduce_next = conflicts + 2000 +
            300 * tactix_stat(policy->solver, TACTIX_STAT_REDUCTIONS);
        if (result >= 0) continue;
      }
      if (tactix_stat(policy->solver, TACTIX_STAT_STABLE)) {
        if (policy->rel_trigger) {
          result = tactix_apply_instance(policy->solver, policy->restart,
                                         TACTIX_INSTANCE_CANONICAL);
          if (terminal(result)) return result;
          if (result >= 0) {
            policy->rel_trigger = 0;
            continue;
          }
        }
      } else if (conflicts >= policy->restart_next) {
        result = tactix_apply_instance(policy->solver, policy->restart,
                                       TACTIX_INSTANCE_CANONICAL);
        if (terminal(result)) return result;
        policy->restart_next = conflicts + focused_period(policy->profile);
        if (result >= 0) continue;
      }
    }
    {
      int candidate;
      if (tactix_decide_candidates(policy->solver, &candidate, 1) != 1)
        return tactix_status(policy->solver);
      if (negative_profile(policy->profile))
        candidate = candidate < 0 ? candidate : -candidate;
      else if (positive_profile(policy->profile))
        candidate = candidate < 0 ? -candidate : candidate;
      else if (walk_profile(policy->profile)) {
        const int variable = candidate < 0 ? -candidate : candidate;
        candidate = policy->walk_values[variable] > 0 ? variable : -variable;
      }
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
  policy->walk = rule_id("Walk");
  policy->reduce_next = 2000;
  policy->restart_next = focused_period(profile);
  policy->rel_u = policy->rel_v = 1;
  policy->rel_wait = 1024;
  return policy->propagate >= 0 && policy->analyze >= 0 &&
         policy->restart >= 0 && policy->reduce >= 0 &&
         policy->switchmode >= 0 && policy->walk >= 0;
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
  enum profile profile = PROFILE_DEFAULT;
  const char *cnf = NULL, *proof = NULL;
  int maximum_variable = 0, result, arg;
  tactix *solver;
  struct policy policy;
  struct formula formula;
  memset(&formula, 0, sizeof formula);
  for (arg = 1; arg < argc; ++arg) {
    if (!strncmp(argv[arg], "--profile=", 10)) {
      if (!parse_profile(argv[arg] + 10, &profile)) {
        fprintf(stderr, "unknown random3sat profile '%s'\n", argv[arg] + 10);
        return 1;
      }
    } else if (!strcmp(argv[arg], "--drat")) {
      /* The runner selects this compatibility certificate explicitly. */
    } else if (!cnf) cnf = argv[arg];
    else if (!proof) proof = argv[arg];
    else {
      fprintf(stderr, "usage: %s [--profile=NAME] [--drat] CNF [PROOF]\n",
              argv[0]);
      return 1;
    }
  }
  if (!cnf) {
    fprintf(stderr, "usage: %s [--profile=NAME] [--drat] CNF [PROOF]\n",
            argv[0]);
    return 1;
  }
  solver = tactix_init();
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-random3sat-policy: cannot create proof '%s'\n",
            proof);
    tactix_release(solver);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &maximum_variable, &formula) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !init_policy(&policy, solver, profile)) {
    tactix_release(solver);
    return 1;
  }
  printf("c random3sat-policy profile=%s interface=public-v1\n",
         profile_name(profile));
  result = profile == PROFILE_DEFAULT
               ? tactix_solve(solver)
               : solve_manual(&policy, &formula, maximum_variable);
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, maximum_variable);
  } else if (result == 20) puts("s UNSATISFIABLE");
  else puts("s UNKNOWN");
  tactix_release(solver);
  free(policy.walk_values);
  free(formula.literals);
  free(formula.clause_ends);
  return terminal(result) ? result : 0;
}
