/* Public-API policy for symbreak's Chvatal-conjecture SAT subproblems.
 *
 * These formulas are a few hundred "semantic" set-membership variables tied
 * together by long clauses, plus thousands of auxiliary variables from the
 * CNF cardinality encodings (clauses of at most three literals).  The policy
 * runs its own CDCL loop through tactix.h and branches on the semantic layer:
 *
 *  - primary variables are those occurring in some clause of four or more
 *    literals (a structural rule, not an instance-specific list);
 *  - the first `cube_levels` decision levels branch on unassigned primary
 *    variables in a static order (most long-clause occurrences first), which
 *    makes the top of the search a semantic cube split inside one session;
 *  - deeper levels prefer the heuristic's best-ranked primary variable among
 *    the first `scan` candidates, otherwise its best candidate.
 *
 * Everything else (propagation, analysis, restarts, reduction, inprocessing)
 * is a registered public rule; the certificate is the session's DRAT stream.
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

enum mode { MODE_FOCUSED, MODE_STABLE, MODE_ALTERNATE };
enum phase { PHASE_NEGATIVE, PHASE_POSITIVE, PHASE_HEURISTIC };

struct options {
  int solve_default;
  unsigned cube_levels;
  int scan;
  enum mode mode;
  enum phase phase;
  uint64_t restart_period;
  uint64_t vivify_interval;
  uint64_t mode_interval;
  int order_by_index;
};

struct policy {
  tactix *solver;
  struct options opt;
  int propagate, analyze, restart, reduce, switchmode, vivify;
  int *primary_order;
  int primary_count;
  unsigned char *is_primary;
  int maximum_variable;
  int *scan_buffer;
  uint64_t rel_u, rel_v, rel_wait;
  int rel_trigger;
  uint64_t reduce_next, vivify_next, mode_next, mode_switches;
  uint64_t static_decisions, primary_decisions, other_decisions;
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

/* Reads DIMACS into the session and counts, per variable, its occurrences in
 * clauses of four or more literals. */
static int read_dimacs(tactix *solver, const char *path, int *maximum_variable,
                       uint64_t **long_occurrences) {
  FILE *input = fopen(path, "r");
  static char line[1 << 20];
  int open_clause = 0, saw_header = 0;
  int *clause = NULL;
  size_t clause_count = 0, clause_capacity = 0, occ_capacity = 0;
  uint64_t *occ = NULL;
  if (!input) {
    fprintf(stderr, "tactix-chvatal-policy: cannot open '%s': %s\n", path,
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
          strcmp(kind, "cnf") || variables < 0)
        goto malformed;
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
      if (end == cursor || errno || value < INT_MIN || value > INT_MAX)
        goto malformed;
      if (tactix_add_checked(solver, (int)value)) goto malformed;
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > *maximum_variable) *maximum_variable = variable;
        if (clause_count == clause_capacity) {
          clause_capacity = clause_capacity ? 2 * clause_capacity : 64;
          clause = (int *)realloc(clause, clause_capacity * sizeof *clause);
          if (!clause) goto malformed;
        }
        clause[clause_count++] = variable;
        open_clause = 1;
      } else {
        if ((size_t)*maximum_variable + 1 > occ_capacity) {
          const size_t next = 2 * ((size_t)*maximum_variable + 1);
          uint64_t *grown = (uint64_t *)realloc(occ, next * sizeof *grown);
          if (!grown) goto malformed;
          memset(grown + occ_capacity, 0,
                 (next - occ_capacity) * sizeof *grown);
          occ = grown;
          occ_capacity = next;
        }
        if (clause_count >= 4) {
          size_t i;
          for (i = 0; i < clause_count; ++i) ++occ[clause[i]];
        }
        clause_count = 0;
        open_clause = 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) goto malformed;
  fclose(input);
  free(clause);
  if ((size_t)*maximum_variable + 1 > occ_capacity) {
    uint64_t *grown = (uint64_t *)realloc(
        occ, ((size_t)*maximum_variable + 1) * sizeof *grown);
    if (!grown) return 0;
    memset(grown + occ_capacity, 0,
           ((size_t)*maximum_variable + 1 - occ_capacity) * sizeof *grown);
    occ = grown;
  }
  *long_occurrences = occ;
  return 1;

malformed:
  fprintf(stderr, "tactix-chvatal-policy: malformed or rejected DIMACS\n");
  fclose(input);
  free(clause);
  free(occ);
  return 0;
}

static const uint64_t *sort_occurrences;
static int compare_primary(const void *left, const void *right) {
  const int a = *(const int *)left, b = *(const int *)right;
  if (sort_occurrences[a] != sort_occurrences[b])
    return sort_occurrences[a] > sort_occurrences[b] ? -1 : 1;
  return a - b;
}

static int build_primary(struct policy *policy, const uint64_t *occ) {
  int variable;
  policy->is_primary = (unsigned char *)calloc(
      (size_t)policy->maximum_variable + 1, 1);
  policy->primary_order = (int *)malloc(
      ((size_t)policy->maximum_variable + 1) * sizeof(int));
  policy->scan_buffer = (int *)malloc(
      ((size_t)policy->maximum_variable + 1) * sizeof(int));
  if (!policy->is_primary || !policy->primary_order || !policy->scan_buffer)
    return 0;
  for (variable = 1; variable <= policy->maximum_variable; ++variable)
    if (occ[variable]) {
      policy->is_primary[variable] = 1;
      policy->primary_order[policy->primary_count++] = variable;
    }
  if (!policy->opt.order_by_index) {
    sort_occurrences = occ;
    qsort(policy->primary_order, (size_t)policy->primary_count, sizeof(int),
          compare_primary);
  }
  return 1;
}

static int propagate(struct policy *policy) {
  return tactix_apply(policy->solver, policy->propagate);
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
  wait = policy->rel_v * policy->opt.restart_period;
  if (wait > (UINT64_C(1) << 20)) {
    policy->rel_u = policy->rel_v = 1;
    wait = policy->opt.restart_period;
  }
  policy->rel_wait = wait;
  policy->rel_trigger = 1;
}

static int phase_literal(struct policy *policy, int variable) {
  return policy->opt.phase == PHASE_POSITIVE ? variable : -variable;
}

/* Decides once.  Returns 1 after a decision, 0 when no unassigned variable
 * remains, -1 on an API error. */
static int decide(struct policy *policy) {
  const uint64_t level = tactix_stat(policy->solver, TACTIX_STAT_LEVEL);
  int i, count;
  if (level < policy->opt.cube_levels) {
    /* tactix_decide refuses an assigned literal without changing state, so
     * the first accepted primary in the static order is the cube split. */
    for (i = 0; i < policy->primary_count; ++i)
      if (!tactix_decide(policy->solver,
                         phase_literal(policy, policy->primary_order[i]))) {
        ++policy->static_decisions;
        return 1;
      }
  }
  count = tactix_decide_candidates(policy->solver, policy->scan_buffer,
                                   policy->opt.scan);
  if (count <= 0) return 0;
  for (i = 0; i < count; ++i) {
    const int literal = policy->scan_buffer[i];
    if (policy->is_primary[literal < 0 ? -literal : literal]) {
      if (tactix_decide(policy->solver, literal)) return -1;
      ++policy->primary_decisions;
      return 1;
    }
  }
  if (tactix_decide(policy->solver, policy->scan_buffer[0])) return -1;
  ++policy->other_decisions;
  return 1;
}

static int root_vivify(struct policy *policy) {
  int result;
  if (tactix_stat(policy->solver, TACTIX_STAT_LEVEL)) {
    result = tactix_apply_instance(policy->solver, policy->restart, 0);
    if (terminal(result)) return result;
  }
  result = tactix_apply(policy->solver, policy->vivify);
  if (terminal(result)) return result;
  result = propagate(policy);
  if (terminal(result)) return result;
  if (result == 2) {
    result = tactix_apply(policy->solver, policy->analyze);
    if (terminal(result)) return result;
  }
  return 1;
}

static int search(struct policy *policy) {
  int result;
  if (policy->opt.mode == MODE_STABLE &&
      tactix_apply(policy->solver, policy->switchmode) < 0)
    return 0;
  for (;;) {
    uint64_t conflicts;
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
    conflicts = tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS);
    if (conflicts >= policy->reduce_next) {
      const int reduced = tactix_apply(policy->solver, policy->reduce);
      if (terminal(reduced)) return reduced;
      policy->reduce_next = conflicts + 2000 +
          300 * tactix_stat(policy->solver, TACTIX_STAT_REDUCTIONS);
      if (reduced >= 0) continue;
    }
    if (policy->opt.mode == MODE_ALTERNATE && conflicts >= policy->mode_next) {
      const int switched = tactix_apply(policy->solver, policy->switchmode);
      if (terminal(switched)) return switched;
      ++policy->mode_switches;
      policy->mode_next = conflicts +
          policy->opt.mode_interval * (policy->mode_switches + 1);
      if (switched >= 0) continue;
    }
    if (policy->opt.vivify_interval && conflicts >= policy->vivify_next) {
      result = root_vivify(policy);
      if (terminal(result)) return result;
      policy->vivify_next = conflicts + policy->opt.vivify_interval;
      policy->rel_trigger = 0;
      continue;
    }
    if (policy->rel_trigger) {
      const int restarted = tactix_apply_instance(
          policy->solver, policy->restart, TACTIX_INSTANCE_CANONICAL);
      if (terminal(restarted)) return restarted;
      policy->rel_trigger = 0;
      if (restarted >= 0) continue;
    }
    result = decide(policy);
    if (result < 0) return 0;
    if (!result) return tactix_status(policy->solver);
  }
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

static int parse_u64(const char *text, uint64_t *out) {
  char *end;
  errno = 0;
  *out = strtoull(text, &end, 10);
  return !errno && *text && !*end;
}

int main(int argc, char **argv) {
  struct policy policy;
  const char *cnf = NULL, *proof = NULL;
  uint64_t *occ = NULL, value;
  int arg, result;
  tactix *solver;
  memset(&policy, 0, sizeof policy);
  policy.opt.cube_levels = 0;
  policy.opt.scan = 64;
  policy.opt.mode = MODE_ALTERNATE;
  policy.opt.phase = PHASE_NEGATIVE;
  policy.opt.restart_period = 512;
  policy.opt.mode_interval = 1000;
  for (arg = 1; arg < argc; ++arg) {
    const char *a = argv[arg];
    if (!strcmp(a, "--default")) policy.opt.solve_default = 1;
    else if (!strncmp(a, "--cube-levels=", 14) && parse_u64(a + 14, &value))
      policy.opt.cube_levels = (unsigned)value;
    else if (!strncmp(a, "--scan=", 7) && parse_u64(a + 7, &value) && value)
      policy.opt.scan = (int)value;
    else if (!strcmp(a, "--mode=focused")) policy.opt.mode = MODE_FOCUSED;
    else if (!strcmp(a, "--mode=stable")) policy.opt.mode = MODE_STABLE;
    else if (!strcmp(a, "--mode=alternate")) policy.opt.mode = MODE_ALTERNATE;
    else if (!strcmp(a, "--phase=negative")) policy.opt.phase = PHASE_NEGATIVE;
    else if (!strcmp(a, "--phase=positive")) policy.opt.phase = PHASE_POSITIVE;
    else if (!strncmp(a, "--restart=", 10) && parse_u64(a + 10, &value) && value)
      policy.opt.restart_period = value;
    else if (!strncmp(a, "--vivify=", 9) && parse_u64(a + 9, &value))
      policy.opt.vivify_interval = value;
    else if (!strncmp(a, "--mode-interval=", 16) && parse_u64(a + 16, &value) &&
             value)
      policy.opt.mode_interval = value;
    else if (!strcmp(a, "--order=index")) policy.opt.order_by_index = 1;
    else if (!strcmp(a, "--drat")) continue;
    else if (a[0] == '-' && a[1]) {
      fprintf(stderr, "tactix-chvatal-policy: unknown option '%s'\n", a);
      return 1;
    } else if (!cnf) cnf = a;
    else if (!proof) proof = a;
    else return 1;
  }
  if (!cnf) {
    fprintf(stderr, "usage: tactix-chvatal-policy [options] CNF [PROOF]\n");
    return 1;
  }
  solver = tactix_init();
  if (proof && tactix_proof(solver, proof, TACTIX_PROOF_DRAT_BINARY)) {
    fprintf(stderr, "tactix-chvatal-policy: cannot create proof '%s'\n", proof);
    return 1;
  }
  policy.solver = solver;
  if (!read_dimacs(solver, cnf, &policy.maximum_variable, &occ) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE) ||
      !build_primary(&policy, occ)) {
    tactix_release(solver);
    return 1;
  }
  free(occ);
  policy.propagate = rule_id("Propagate");
  policy.analyze = rule_id("ConflictAnalysis");
  policy.restart = rule_id("Restart");
  policy.reduce = rule_id("Reduce");
  policy.switchmode = rule_id("SwitchMode");
  policy.vivify = rule_id("Vivify");
  if (policy.propagate < 0 || policy.analyze < 0 || policy.restart < 0 ||
      policy.reduce < 0 || policy.switchmode < 0 || policy.vivify < 0)
    return 1;
  policy.rel_u = policy.rel_v = 1;
  policy.rel_wait = policy.opt.restart_period;
  policy.reduce_next = 2000;
  policy.mode_next = policy.opt.mode_interval;
  policy.vivify_next = policy.opt.vivify_interval;
  printf("c chvatal-policy primary=%d cube-levels=%u scan=%d default=%d\n",
         policy.primary_count, policy.opt.cube_levels, policy.opt.scan,
         policy.opt.solve_default);
  fflush(stdout);
  if (tactix_status(solver) == 20) result = 20;
  else if (policy.opt.solve_default) result = tactix_solve(solver);
  else result = search(&policy);
  printf("c conflicts %" PRIu64 " decisions static=%" PRIu64 " primary=%" PRIu64
         " other=%" PRIu64 " mode-switches=%" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS), policy.static_decisions,
         policy.primary_decisions, policy.other_decisions,
         policy.mode_switches);
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, policy.maximum_variable);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  return result == 10 || result == 20 ? result : 0;
}
