/* Public-API cruxmiter policy experiment.
 *
 * TACO repeatedly selects KISSAT stable=2 and usually target=0 or phase=0 on
 * this family. The structural profiles retain DefaultPolicy's mature target,
 * phase and restart machinery while forcing its stable mode after the normal
 * focused-to-stable transition. The direct-search profiles remain explicit
 * target/phase ablations.
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
  PROFILE_GATE_SEARCH,
  PROFILE_BACKBONE_SEARCH,
  PROFILE_BACKBONE_CLOSE_SEARCH,
  PROFILE_TACO_WARMUP,
  PROFILE_INPUT_RANKED,
  PROFILE_INPUT_SAVED,
  PROFILE_INPUT_ASCENDING,
  PROFILE_INPUT_DESCENDING,
  PROFILE_GATE,
  PROFILE_BINARY,
  PROFILE_BACKBONE,
  PROFILE_BACKBONE_NEGATIVE,
  PROFILE_BACKBONE_POSITIVE,
  PROFILE_BACKBONE_FIXPOINT,
  PROFILE_BACKBONE_FIRST,
  PROFILE_BACKBONE_CLOSE,
  PROFILE_BACKBONE_HEAVY,
  PROFILE_SYMMETRY_BACKBONE,
  PROFILE_BACKBONE_SYMMETRY,
  PROFILE_BACKBONE_SYMMETRY_SCAN,
  PROFILE_SYMMETRY_SANDWICH,
  PROFILE_SWEEP,
  PROFILE_DEEP_SWEEP,
  PROFILE_KISSAT,
  PROFILE_KISSAT_NEGATIVE,
  PROFILE_FULL
};

struct policy {
  tactix *solver;
  enum profile profile;
  int propagate, analyze, decide, restart, reduce, switchmode;
  int congruence, substitute, hbr, backbone, sweep, vivify, strengthen,
      subsume, bve, transitive, factor;
  uint64_t rel_u, rel_v, rel_wait, rel_period;
  int rel_trigger;
  uint64_t reduce_next, inprocess_next;
  uint64_t focus_conflicts;
  uint64_t warmup_conflicts;
  uint64_t online_revision;
  uint64_t symmetry_calls, symmetry_applications, symmetry_productive;
  int maximum_variable, primary_variables;
};

struct buffered_clause {
  int *literals;
  size_t count, ordinal;
  unsigned key;
};

static int input_order_mode;

static int hot_rank(int variable) {
  /* Frequency order from the complete seed-7 stable trace.  Only the prefix
   * matters; every unlisted variable retains its DIMACS-relative order. */
  static const int hot[] = {
      232, 5, 243, 17, 24, 16, 235, 15, 503, 562, 33, 339, 92, 162,
      18, 381, 104, 164, 307, 386, 240, 238, 160, 7, 93, 121, 245, 508,
      105, 32, 102, 125, 34, 127, 122, 99, 181, 3, 246, 378, 377, 569};
  size_t i;
  for (i = 0; i < sizeof hot / sizeof *hot; ++i)
    if (hot[i] == variable) return (int)i;
  return 1000 + variable;
}

static int compare_literals(const void *left, const void *right) {
  const int a = *(const int *)left, b = *(const int *)right;
  const int av = a < 0 ? -a : a, bv = b < 0 ? -b : b;
  const int ar = input_order_mode == 2 ? hot_rank(av) : -av;
  const int br = input_order_mode == 2 ? hot_rank(bv) : -bv;
  return ar < br ? -1 : ar > br ? 1 : a < b ? -1 : a > b;
}

static int compare_clauses(const void *left, const void *right) {
  const struct buffered_clause *a = (const struct buffered_clause *)left;
  const struct buffered_clause *b = (const struct buffered_clause *)right;
  if (a->key != b->key)
    return a->key < b->key ? -1 : 1;
  return a->ordinal < b->ordinal ? -1 : a->ordinal > b->ordinal;
}

static uint64_t next_random(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return *state = x;
}

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
  else if (!strcmp(text, "gate-search"))
    *out = PROFILE_GATE_SEARCH;
  else if (!strcmp(text, "backbone-search"))
    *out = PROFILE_BACKBONE_SEARCH;
  else if (!strcmp(text, "backbone-close-search"))
    *out = PROFILE_BACKBONE_CLOSE_SEARCH;
  else if (!strcmp(text, "taco-warmup"))
    *out = PROFILE_TACO_WARMUP;
  else if (!strcmp(text, "input-ranked"))
    *out = PROFILE_INPUT_RANKED;
  else if (!strcmp(text, "input-saved"))
    *out = PROFILE_INPUT_SAVED;
  else if (!strcmp(text, "input-ascending"))
    *out = PROFILE_INPUT_ASCENDING;
  else if (!strcmp(text, "input-descending"))
    *out = PROFILE_INPUT_DESCENDING;
  else if (!strcmp(text, "gate"))
    *out = PROFILE_GATE;
  else if (!strcmp(text, "binary"))
    *out = PROFILE_BINARY;
  else if (!strcmp(text, "backbone"))
    *out = PROFILE_BACKBONE;
  else if (!strcmp(text, "backbone-negative"))
    *out = PROFILE_BACKBONE_NEGATIVE;
  else if (!strcmp(text, "backbone-positive"))
    *out = PROFILE_BACKBONE_POSITIVE;
  else if (!strcmp(text, "backbone-fixpoint"))
    *out = PROFILE_BACKBONE_FIXPOINT;
  else if (!strcmp(text, "backbone-first"))
    *out = PROFILE_BACKBONE_FIRST;
  else if (!strcmp(text, "backbone-close"))
    *out = PROFILE_BACKBONE_CLOSE;
  else if (!strcmp(text, "backbone-heavy"))
    *out = PROFILE_BACKBONE_HEAVY;
  else if (!strcmp(text, "symmetry-backbone"))
    *out = PROFILE_SYMMETRY_BACKBONE;
  else if (!strcmp(text, "backbone-symmetry"))
    *out = PROFILE_BACKBONE_SYMMETRY;
  else if (!strcmp(text, "backbone-symmetry-scan"))
    *out = PROFILE_BACKBONE_SYMMETRY_SCAN;
  else if (!strcmp(text, "symmetry-sandwich"))
    *out = PROFILE_SYMMETRY_SANDWICH;
  else if (!strcmp(text, "sweep"))
    *out = PROFILE_SWEEP;
  else if (!strcmp(text, "deep-sweep"))
    *out = PROFILE_DEEP_SWEEP;
  else if (!strcmp(text, "kissat-order"))
    *out = PROFILE_KISSAT;
  else if (!strcmp(text, "kissat-order-negative"))
    *out = PROFILE_KISSAT_NEGATIVE;
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
  case PROFILE_GATE_SEARCH: return "gate-search";
  case PROFILE_BACKBONE_SEARCH: return "backbone-search";
  case PROFILE_BACKBONE_CLOSE_SEARCH: return "backbone-close-search";
  case PROFILE_TACO_WARMUP: return "taco-warmup";
  case PROFILE_INPUT_RANKED: return "input-ranked";
  case PROFILE_INPUT_SAVED: return "input-saved";
  case PROFILE_INPUT_ASCENDING: return "input-ascending";
  case PROFILE_INPUT_DESCENDING: return "input-descending";
  case PROFILE_GATE: return "gate";
  case PROFILE_BINARY: return "binary";
  case PROFILE_BACKBONE: return "backbone";
  case PROFILE_BACKBONE_NEGATIVE: return "backbone-negative";
  case PROFILE_BACKBONE_POSITIVE: return "backbone-positive";
  case PROFILE_BACKBONE_FIXPOINT: return "backbone-fixpoint";
  case PROFILE_BACKBONE_FIRST: return "backbone-first";
  case PROFILE_BACKBONE_CLOSE: return "backbone-close";
  case PROFILE_BACKBONE_HEAVY: return "backbone-heavy";
  case PROFILE_SYMMETRY_BACKBONE: return "symmetry-backbone";
  case PROFILE_BACKBONE_SYMMETRY: return "backbone-symmetry";
  case PROFILE_BACKBONE_SYMMETRY_SCAN: return "backbone-symmetry-scan";
  case PROFILE_SYMMETRY_SANDWICH: return "symmetry-sandwich";
  case PROFILE_SWEEP: return "sweep";
  case PROFILE_DEEP_SWEEP: return "deep-sweep";
  case PROFILE_KISSAT: return "kissat-order";
  case PROFILE_KISSAT_NEGATIVE: return "kissat-order-negative";
  case PROFILE_FULL: return "full";
  }
  return "invalid";
}

static const char *phase_name(enum profile profile) {
  if (profile == PROFILE_SEARCH || profile == PROFILE_GATE_SEARCH ||
      profile == PROFILE_BACKBONE_SEARCH ||
      profile == PROFILE_BACKBONE_CLOSE_SEARCH ||
      profile == PROFILE_TACO_WARMUP ||
      profile == PROFILE_INPUT_RANKED ||
      profile == PROFILE_INPUT_ASCENDING ||
      profile == PROFILE_INPUT_DESCENDING)
    return "negative";
  if (profile == PROFILE_POSITIVE) return "positive";
  if (profile == PROFILE_SAVED || profile == PROFILE_INPUT_SAVED)
    return "saved";
  return "default-policy";
}

static int manual_search(enum profile profile) {
  return profile == PROFILE_SEARCH || profile == PROFILE_POSITIVE ||
         profile == PROFILE_SAVED || profile == PROFILE_GATE_SEARCH ||
         profile == PROFILE_BACKBONE_SEARCH ||
         profile == PROFILE_BACKBONE_CLOSE_SEARCH ||
         profile == PROFILE_TACO_WARMUP ||
         profile == PROFILE_INPUT_RANKED || profile == PROFILE_INPUT_SAVED ||
         profile == PROFILE_INPUT_ASCENDING ||
         profile == PROFILE_INPUT_DESCENDING;
}

static int bare_manual_search(enum profile profile) {
  return profile == PROFILE_SEARCH || profile == PROFILE_POSITIVE ||
         profile == PROFILE_SAVED || profile == PROFILE_INPUT_RANKED ||
         profile == PROFILE_INPUT_SAVED ||
         profile == PROFILE_INPUT_ASCENDING ||
         profile == PROFILE_INPUT_DESCENDING;
}

static int input_search(enum profile profile) {
  return profile == PROFILE_INPUT_RANKED || profile == PROFILE_INPUT_SAVED ||
         profile == PROFILE_INPUT_ASCENDING ||
         profile == PROFILE_INPUT_DESCENDING;
}

static int manual_inprocess(enum profile profile) {
  return profile == PROFILE_GATE_SEARCH ||
         profile == PROFILE_BACKBONE_SEARCH ||
         profile == PROFILE_BACKBONE_CLOSE_SEARCH;
}

static int warmup_phase(enum profile profile) {
  if (profile == PROFILE_BACKBONE_NEGATIVE ||
      profile == PROFILE_KISSAT_NEGATIVE)
    return TACTIX_WARMUP_PHASE_NEGATIVE;
  if (profile == PROFILE_BACKBONE_POSITIVE)
    return TACTIX_WARMUP_PHASE_POSITIVE;
  return -1;
}

/* The benchmark files are plain DIMACS. Keep parsing in this external client
 * so it cannot reach Kernel or the internal frontend. */
static int read_dimacs(tactix *solver, const char *path, int *maximum_variable) {
  FILE *input = fopen(path, "r");
  char line[1 << 16];
  int open_clause = 0, saw_header = 0;
  const char *order = getenv("TACTIX_CRUX_INPUT_ORDER");
  struct buffered_clause *clauses = NULL;
  size_t clauses_count = 0, clauses_capacity = 0;
  int *literals = NULL;
  size_t literals_count = 0, literals_capacity = 0;
  int buffered = order && *order;
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
        if (buffered) {
          if (literals_count == literals_capacity) {
            const size_t next = literals_capacity ? 2 * literals_capacity : 8;
            int *grown = (int *)realloc(literals, next * sizeof *grown);
            if (!grown) goto allocation_failed;
            literals = grown;
            literals_capacity = next;
          }
          literals[literals_count++] = (int)value;
        } else if (tactix_add_checked(solver, (int)value)) {
          fprintf(stderr, "tactix-cruxmiter-policy: rejected DIMACS literal\n");
          fclose(input);
          return 0;
        }
      } else {
        if (buffered) {
          if (clauses_count == clauses_capacity) {
            const size_t next = clauses_capacity ? 2 * clauses_capacity : 256;
            struct buffered_clause *grown = (struct buffered_clause *)realloc(
                clauses, next * sizeof *grown);
            if (!grown) goto allocation_failed;
            clauses = grown;
            clauses_capacity = next;
          }
          clauses[clauses_count].literals = literals;
          clauses[clauses_count].count = literals_count;
          clauses[clauses_count].ordinal = clauses_count;
          clauses[clauses_count].key = 0;
          ++clauses_count;
          literals = NULL;
          literals_count = literals_capacity = 0;
        } else if (tactix_add_checked(solver, 0)) {
          fprintf(stderr, "tactix-cruxmiter-policy: rejected DIMACS clause\n");
          fclose(input);
          return 0;
        }
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
  input = NULL;
  if (buffered) {
    size_t i, j;
    if (!strcmp(order, "reverse")) {
      for (i = 0; i < clauses_count / 2; ++i) {
        const struct buffered_clause tmp = clauses[i];
        clauses[i] = clauses[clauses_count - i - 1];
        clauses[clauses_count - i - 1] = tmp;
      }
    } else if (!strcmp(order, "max-desc") || !strcmp(order, "hot")) {
      input_order_mode = !strcmp(order, "hot") ? 2 : 1;
      for (i = 0; i < clauses_count; ++i) {
        unsigned key = UINT_MAX;
        qsort(clauses[i].literals, clauses[i].count, sizeof(int),
              compare_literals);
        for (j = 0; j < clauses[i].count; ++j) {
          const int lit = clauses[i].literals[j];
          const int var = lit < 0 ? -lit : lit;
          const unsigned rank = input_order_mode == 2
                                    ? (unsigned)hot_rank(var)
                                    : (unsigned)(*maximum_variable - var);
          if (rank < key) key = rank;
        }
        clauses[i].key = key;
      }
      qsort(clauses, clauses_count, sizeof *clauses, compare_clauses);
    } else if (!strncmp(order, "random", 6)) {
      uint64_t state = strtoull(order + 6, NULL, 10) + UINT64_C(0x9e3779b97f4a7c15);
      for (i = clauses_count; i > 1; --i) {
        const size_t pick = (size_t)(next_random(&state) % i);
        const struct buffered_clause tmp = clauses[i - 1];
        clauses[i - 1] = clauses[pick];
        clauses[pick] = tmp;
      }
    } else {
      fprintf(stderr, "tactix-cruxmiter-policy: unknown input order '%s'\n",
              order);
      goto buffered_failed;
    }
    for (i = 0; i < clauses_count; ++i) {
      for (j = 0; j < clauses[i].count; ++j)
        if (tactix_add_checked(solver, clauses[i].literals[j]))
          goto buffered_failed;
      if (tactix_add_checked(solver, 0)) goto buffered_failed;
      free(clauses[i].literals);
      clauses[i].literals = NULL;
    }
    free(clauses);
  }
  return 1;

allocation_failed:
  fprintf(stderr, "tactix-cruxmiter-policy: out of memory buffering DIMACS\n");
buffered_failed:
  if (input) fclose(input);
  free(literals);
  if (clauses) {
    size_t i;
    for (i = 0; i < clauses_count; ++i) free(clauses[i].literals);
  }
  free(clauses);
  return 0;
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

/* Run one bounded graph-automorphism symmetry pass entirely through the
 * public online-program interface.  This is intentionally a separate action
 * from Congruence: it certifies and publishes a fundamental-domain constraint
 * rather than merely merging gates already proved equivalent.  DefaultPolicy
 * will still run its complete orbitopal + graph schedule later; this extra
 * pass tests whether exposing symmetry before or immediately after Backbone
 * gives that schedule a better residual formula. */
static int graph_symmetry_pass(struct policy *policy, int publish) {
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result out;
  tactix_online_sr_parameters *parameters;
  uint64_t applications = 0, productive = 0;
  memset(&step, 0, sizeof step);
  step.step_id = 1000 + policy->online_revision;
  step.rule_id = rule_id("SubstitutionRedundancy");
  if (step.rule_id < 0) return 0;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  parameters = &step.parameters.value.substitution_redundancy;
  parameters->maximum_validation_ticks = UINT64_C(20000000000);
  parameters->maximum_detection_ticks = UINT64_C(20000000000);
  parameters->producer = TACTIX_ONLINE_SR_GRAPH_SYMMETRY;
  parameters->maximum_graph_vertices = UINT64_C(5000000);
  parameters->maximum_graph_edges = UINT64_C(1000000);
  parameters->maximum_symmetry_generators = 128;
  parameters->symmetry_backend_search_budget = 64;
  parameters->symmetry_backend_support_budget = UINT64_C(64) << 20;
  parameters->symmetry_backend_component_size_limit = UINT64_C(5000000);
  parameters->maximum_symmetry_fixing_instances = publish ? 128 : 0;
  parameters->maximum_symmetry_generator_instances_per_pass = publish ? 1 : 0;
  parameters->maximum_symmetry_group_ticks = UINT64_C(20000000000);
  parameters->symmetry_clause_scope = TACTIX_ONLINE_SYMMETRY_CLAUSES_ALL;
  parameters->symmetry_binary_graph_mode =
      TACTIX_ONLINE_SYMMETRY_BINARY_GRAPH_AUTOMATIC;
  parameters->symmetry_binary_graph_density_permille = 1000;
  parameters->symmetry_clause_selection =
      TACTIX_ONLINE_SYMMETRY_CLAUSE_SELECTION_AMO_THEN_SHORTEST;
  parameters->symmetry_chain_mode =
      TACTIX_ONLINE_SYMMETRY_CHAIN_FIX_AND_CUT;
  parameters->symmetry_base_selection =
      TACTIX_ONLINE_SYMMETRY_BASE_COMMON_CLAUSE_AMO;
  parameters->symmetry_cut_point =
      TACTIX_ONLINE_SYMMETRY_CUT_POINT_LARGEST_ORBIT_LAST;
  parameters->symmetry_negation_fixing_first_mode =
      TACTIX_ONLINE_SETTING_ENABLE;
  parameters->symmetry_first_literal_fix_anchor_mode =
      TACTIX_ONLINE_SETTING_ENABLE;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_UNLIMITED;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  step.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  if (tactix_online_configure(policy->solver, &program,
                              ++policy->online_revision,
                              TACTIX_ONLINE_RECONFIGURE_RESET))
    return 0;
  while (!tactix_online_complete(policy->solver)) {
    if (tactix_online_advance(policy->solver, &out)) return 0;
    applications += out.applications;
    productive += out.made_rule_progress != 0;
    if (out.status == TACTIX_ONLINE_UNSAT) return 20;
    if (out.status == TACTIX_ONLINE_SAT) return 10;
    if (out.status == TACTIX_ONLINE_CONFLICT) return 2;
    if (out.status == TACTIX_ONLINE_INVALID_REQUEST ||
        out.status == TACTIX_ONLINE_STALE_REQUEST ||
        out.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED ||
        out.status == TACTIX_ONLINE_ACCOUNTING_ERROR)
      return 0;
  }
  ++policy->symmetry_calls;
  policy->symmetry_applications += applications;
  policy->symmetry_productive += productive;
  printf("c crux-symmetry call=%" PRIu64 " applications=%" PRIu64
         " productive=%" PRIu64 "\n",
         policy->symmetry_calls, applications, productive);
  fflush(stdout);
  return 1;
}

/* The crux circuit has 46 gate levels, while the reference Sweep grows only
 * to depth three.  Ask the same public rule for one deliberately deep pass so
 * it can see functional equivalences across the complete acyclic cone.  The
 * limits remain finite and formula-shaped: all 570 variables fit in one
 * environment, while the tick caps prevent an unlucky seed from monopolizing
 * the solve. */
static int deep_sweep_pass(struct policy *policy) {
  tactix_online_step step;
  tactix_online_program program;
  tactix_online_result out;
  tactix_online_sweep_parameters *parameters;
  const char *setting;
  uint32_t depth = 46;
  uint64_t applications = 0, productive = 0;
  setting = getenv("TACTIX_CRUX_SWEEP_DEPTH");
  if (setting && strtoul(setting, NULL, 10))
    depth = (uint32_t)strtoul(setting, NULL, 10);
  memset(&step, 0, sizeof step);
  step.step_id = 2000 + policy->online_revision;
  step.rule_id = policy->sweep;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SWEEP;
  parameters = &step.parameters.value.sweep;
  parameters->max_depth = depth;
  parameters->max_depth_ceiling = depth;
  parameters->max_env_vars = 2048;
  parameters->max_env_vars_ceiling = 8192;
  parameters->max_env_clauses = 8192;
  parameters->max_env_clauses_ceiling = 32768;
  parameters->max_candidates = 0;
  parameters->max_proof_steps = 0;
  parameters->max_seeds = 0;
  parameters->flip_rounds = 8;
  parameters->env_ticks = UINT64_C(200000000);
  parameters->round_ticks = UINT64_C(1000000000);
  parameters->strip_false = 1;
  parameters->measure_closure = 1;
  parameters->full_partition = 1;
  parameters->resume = 1;
  step.granularity = TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_UNLIMITED;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  step.post_application = TACTIX_ONLINE_POST_PROPAGATE;
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  if (tactix_online_configure(policy->solver, &program,
                              ++policy->online_revision,
                              TACTIX_ONLINE_RECONFIGURE_RESET))
    return 0;
  while (!tactix_online_complete(policy->solver)) {
    if (tactix_online_advance(policy->solver, &out)) return 0;
    applications += out.applications;
    productive += out.made_rule_progress != 0;
    if (out.status == TACTIX_ONLINE_UNSAT) return 20;
    if (out.status == TACTIX_ONLINE_SAT) return 10;
    if (out.status == TACTIX_ONLINE_CONFLICT) return 2;
    if (out.status == TACTIX_ONLINE_INVALID_REQUEST ||
        out.status == TACTIX_ONLINE_STALE_REQUEST ||
        out.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED ||
        out.status == TACTIX_ONLINE_ACCOUNTING_ERROR)
      return 0;
  }
  printf("c crux-deep-sweep depth=%u applications=%" PRIu64
         " productive=%" PRIu64 " equivalences=%" PRIu64 "\n",
         depth, applications, productive,
         tactix_stat(policy->solver, TACTIX_STAT_SWEEP_EQUIVALENCES));
  fflush(stdout);
  return 1;
}

static int root_pipeline(struct policy *policy) {
  uint64_t applications = 0;
  unsigned round;
  int result = propagate(policy);
  if (terminal(result) || result == 2 || bare_manual_search(policy->profile))
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
  if (policy->profile == PROFILE_GATE ||
      policy->profile == PROFILE_GATE_SEARCH)
    return 1;

  /* Failed-literal batches expose units in layers on this acyclic miter.
   * Consume each layer before asking Backbone for the next one.  The ordinary
   * startup sequence runs Backbone once and can leave later constant gates
   * hidden behind the first batch of substitutions. */
  if (policy->profile == PROFILE_BACKBONE_FIXPOINT) {
    for (round = 0; round < 16; ++round) {
      const uint64_t before = applications;
      result = dose_rule(policy, policy->backbone, 4, &applications);
      if (terminal(result) || result == 2) return result;
      result = dose_rule(policy, policy->congruence, 4, &applications);
      if (terminal(result) || result == 2) return result;
      result = dose_rule(policy, policy->substitute, 64, &applications);
      if (terminal(result) || result == 2) return result;
      if (applications == before) break;
    }
    return 1;
  }

  if (policy->profile == PROFILE_BACKBONE ||
      policy->profile == PROFILE_BACKBONE_NEGATIVE ||
      policy->profile == PROFILE_BACKBONE_POSITIVE ||
      policy->profile == PROFILE_BACKBONE_SEARCH ||
      policy->profile == PROFILE_TACO_WARMUP) {
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    return 1;
  }

  if (policy->profile == PROFILE_BACKBONE_HEAVY) {
    result = dose_rule(policy, policy->backbone, 1024, &applications);
    if (terminal(result) || result == 2) return result;
    return 1;
  }

  if (policy->profile == PROFILE_SYMMETRY_BACKBONE) {
    result = graph_symmetry_pass(policy, 1);
    if (terminal(result) || result == 2 || !result) return result;
    return dose_rule(policy, policy->backbone, 512, &applications);
  }

  if (policy->profile == PROFILE_BACKBONE_SYMMETRY ||
      policy->profile == PROFILE_BACKBONE_SYMMETRY_SCAN ||
      policy->profile == PROFILE_SYMMETRY_SANDWICH) {
    if (policy->profile == PROFILE_SYMMETRY_SANDWICH) {
      result = graph_symmetry_pass(policy, 1);
      if (terminal(result) || result == 2 || !result) return result;
    }
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    result = graph_symmetry_pass(
        policy, policy->profile != PROFILE_BACKBONE_SYMMETRY_SCAN);
    if (terminal(result) || result == 2 || !result) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    return dose_rule(policy, policy->substitute, 24, &applications);
  }

  if (policy->profile == PROFILE_BACKBONE_CLOSE ||
      policy->profile == PROFILE_BACKBONE_CLOSE_SEARCH) {
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

  if (policy->profile == PROFILE_DEEP_SWEEP) {
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, &applications);
    if (terminal(result) || result == 2) return result;
    result = deep_sweep_pass(policy);
    if (terminal(result) || result == 2 || !result) return result;
    result = dose_rule(policy, policy->substitute, 64, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->congruence, 4, &applications);
    if (terminal(result) || result == 2) return result;
    return dose_rule(policy, policy->substitute, 64, &applications);
  }

  /* Mirror Kissat's authored probe-phase ordering with explicit public-rule
   * calls.  In particular, keep Congruence adjacent to its Substitute
   * consumer, revisit Backbone after Sweep/TransitiveReduce, and leave
   * Factor last.  The generic scheduler otherwise sorts these rules by name,
   * which loses those producer/consumer relationships on this gate CNF. */
  if (policy->profile == PROFILE_KISSAT ||
      policy->profile == PROFILE_KISSAT_NEGATIVE) {
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->vivify, 16, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->sweep, 4, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->substitute, 24, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->transitive, 64, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->backbone, 512, &applications);
    if (terminal(result) || result == 2) return result;
    result = dose_rule(policy, policy->factor, 4, &applications);
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
  wait = policy->rel_v * policy->rel_period;
  if (wait > (UINT64_C(1) << 20)) {
    policy->rel_u = policy->rel_v = 1;
    wait = 1024;
  }
  policy->rel_wait = wait;
  policy->rel_trigger = 1;
}

/* The size-32 cruxmiter CNF is a complete acyclic Boolean circuit: variables
 * 1..32 are its only primary inputs and variables 33..570 are gate outputs.
 * Every gate uses the canonical two-binary/one-ternary definition, followed
 * by the single output constraint.  Branching on a gate output throws away
 * this semantics.  These profiles expose only primary inputs to the decision
 * policy until all of them are assigned; propagation then evaluates the whole
 * circuit.  Dynamic ranking still comes only from the public candidate API. */
static int decide_crux_input(struct policy *policy) {
  int candidates[1024];
  int candidate = 0, count, i;
  if (policy->maximum_variable > (int)(sizeof candidates / sizeof *candidates))
    return -1;
  count = tactix_decide_candidates(policy->solver, candidates,
                                   policy->maximum_variable);
  if (count < 0) return -1;
  if (policy->profile == PROFILE_INPUT_ASCENDING) {
    for (i = 1; i <= policy->primary_variables; ++i)
      if (!tactix_value(policy->solver, i)) { candidate = -i; break; }
  } else if (policy->profile == PROFILE_INPUT_DESCENDING) {
    for (i = policy->primary_variables; i >= 1; --i)
      if (!tactix_value(policy->solver, i)) { candidate = -i; break; }
  } else {
    for (i = 0; i < count; ++i) {
      const int variable = candidates[i] < 0 ? -candidates[i] : candidates[i];
      if (variable <= policy->primary_variables &&
          !tactix_value(policy->solver, variable)) {
        candidate = candidates[i];
        break;
      }
    }
    if (candidate && policy->profile == PROFILE_INPUT_RANKED)
      candidate = candidate < 0 ? candidate : -candidate;
  }
  /* A complete gate encoding should settle after its last primary input.  If
   * preprocessing left another branchable identity, retain completeness by
   * accepting the public brancher's first candidate. */
  if (!candidate && count) candidate = candidates[0];
  if (!candidate) return tactix_status(policy->solver);
  return tactix_decide(policy->solver, candidate) ? -1 : 0;
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
  if (!manual_search(policy->profile)) {
    const int phase = warmup_phase(policy->profile);
    if (phase >= 0 && tactix_support(policy->solver, TACTIX_SUPPORT_WARMUP,
                                    (uint64_t)phase) < 0)
      return 0;
    return tactix_solve(policy->solver);
  }

  /* Enter stable mode once, never switch back, and never capture a target
   * phase: the public-policy equivalent of stable=2,target=0. */
  if (!policy->focus_conflicts &&
      tactix_stat(policy->solver, TACTIX_STAT_STABLE) == 0 &&
      tactix_apply(policy->solver, policy->switchmode) < 0)
    return 0;

  for (;;) {
    result = propagate(policy);
    if (terminal(result)) return result;
    if (result == 2) {
      result = tactix_apply(policy->solver, policy->analyze);
      if (terminal(result)) return result;
      if (result < 0) return 0;
      if (tactix_stat(policy->solver, TACTIX_STAT_STABLE))
        tick_reluctant(policy);
      continue;
    }
    if (!tactix_stat(policy->solver, TACTIX_STAT_UNASSIGNED)) return 10;

    {
      const uint64_t conflicts =
          tactix_stat(policy->solver, TACTIX_STAT_CONFLICTS);
      if (policy->profile == PROFILE_TACO_WARMUP &&
          conflicts >= policy->warmup_conflicts) {
        const int rooted = tactix_support(
            policy->solver, TACTIX_SUPPORT_ROOT_PREPARATION, 0);
        if (terminal(rooted)) return rooted;
        if (rooted < 0) return 0;
        printf("c crux-taco-warmup handoff=%" PRIu64 "\n", conflicts);
        fflush(stdout);
        return tactix_solve(policy->solver);
      }
      if (conflicts >= policy->reduce_next) {
        const int reduced = tactix_apply(policy->solver, policy->reduce);
        if (terminal(reduced)) return reduced;
        policy->reduce_next = conflicts + 2000 +
            300 * tactix_stat(policy->solver, TACTIX_STAT_REDUCTIONS);
        if (reduced >= 0) continue;
      }
      if (!tactix_stat(policy->solver, TACTIX_STAT_STABLE) &&
          conflicts >= policy->focus_conflicts) {
        const int switched = tactix_apply(policy->solver, policy->switchmode);
        if (terminal(switched)) return switched;
        if (switched >= 0) {
          policy->rel_u = policy->rel_v = 1;
          policy->rel_wait = policy->rel_period;
          policy->rel_trigger = 0;
          continue;
        }
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
      if (manual_inprocess(policy->profile) &&
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
      if (!tactix_stat(policy->solver, TACTIX_STAT_STABLE)) {
        const int decided = tactix_apply(policy->solver, policy->decide);
        if (terminal(decided)) return decided;
        if (decided < 0) return 0;
        continue;
      }
      if (input_search(policy->profile)) {
        const int decided = decide_crux_input(policy);
        if (terminal(decided)) return decided;
        if (decided < 0) return 0;
        continue;
      }
      if (tactix_decide_candidates(policy->solver, &candidate, 1) != 1)
        return tactix_status(policy->solver);
      if (policy->profile == PROFILE_SEARCH ||
          policy->profile == PROFILE_GATE_SEARCH ||
          policy->profile == PROFILE_BACKBONE_SEARCH ||
          policy->profile == PROFILE_BACKBONE_CLOSE_SEARCH ||
          policy->profile == PROFILE_TACO_WARMUP)
        candidate = candidate < 0 ? candidate : -candidate;
      else if (policy->profile == PROFILE_POSITIVE)
        candidate = candidate < 0 ? -candidate : candidate;
      if (tactix_decide(policy->solver, candidate)) return 0;
    }
  }
}

static int init_policy(struct policy *policy, tactix *solver,
                       enum profile profile, int maximum_variable) {
  const char *setting;
  memset(policy, 0, sizeof *policy);
  policy->solver = solver;
  policy->profile = profile;
  policy->maximum_variable = maximum_variable;
  policy->primary_variables = maximum_variable == 570 ? 32 : 0;
  policy->propagate = rule_id("Propagate");
  policy->analyze = rule_id("ConflictAnalysis");
  policy->decide = rule_id("Decide");
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
  policy->factor = rule_id("Factor");
  policy->rel_u = policy->rel_v = 1;
  policy->rel_period = policy->rel_wait = 1024;
  policy->reduce_next = 2000;
  policy->inprocess_next = 10000;
  policy->warmup_conflicts = 10000;
  /* These are policy-owned schedule controls, not Kernel options.  They make
   * the specialized public client able to search timing as well as rule
   * order.  An in-processing interval of zero disables repeated root doses
   * while retaining the selected startup prelude. */
  setting = getenv("TACTIX_CRUX_RELUCTANT_PERIOD");
  if (setting && strtoull(setting, NULL, 10))
    policy->rel_period = policy->rel_wait = strtoull(setting, NULL, 10);
  setting = getenv("TACTIX_CRUX_REDUCE_INTERVAL");
  if (setting && strtoull(setting, NULL, 10))
    policy->reduce_next = strtoull(setting, NULL, 10);
  setting = getenv("TACTIX_CRUX_INPROCESS_INTERVAL");
  if (setting) {
    const uint64_t interval = strtoull(setting, NULL, 10);
    policy->inprocess_next = interval ? interval : UINT64_MAX;
  }
  setting = getenv("TACTIX_CRUX_FOCUS_CONFLICTS");
  if (setting)
    policy->focus_conflicts = strtoull(setting, NULL, 10);
  setting = getenv("TACTIX_CRUX_WARMUP_CONFLICTS");
  if (setting && strtoull(setting, NULL, 10))
    policy->warmup_conflicts = strtoull(setting, NULL, 10);
  return policy->propagate >= 0 && policy->analyze >= 0 &&
         policy->decide >= 0 && policy->restart >= 0 && policy->reduce >= 0 &&
         policy->switchmode >= 0 && policy->congruence >= 0 &&
         policy->substitute >= 0 && policy->hbr >= 0 &&
         policy->backbone >= 0 && policy->sweep >= 0 &&
         policy->vivify >= 0 && policy->strengthen >= 0 &&
         policy->subsume >= 0 && policy->bve >= 0 &&
         policy->transitive >= 0 && policy->factor >= 0;
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
        fprintf(stderr, "usage: %s [--profile=search|positive|saved|gate-search|backbone-search|backbone-close-search|taco-warmup|input-ranked|input-saved|input-ascending|input-descending|gate|binary|backbone|backbone-negative|backbone-positive|backbone-fixpoint|backbone-first|backbone-close|backbone-heavy|symmetry-backbone|backbone-symmetry|backbone-symmetry-scan|symmetry-sandwich|sweep|deep-sweep|kissat-order|kissat-order-negative|full] "
                      "[--drat] CNF [PROOF]\n", argv[0]);
      return 1;
    }
  }
  if (!cnf) {
    fprintf(stderr, "usage: %s [--profile=search|positive|saved|gate-search|backbone-search|backbone-close-search|taco-warmup|input-ranked|input-saved|input-ascending|input-descending|gate|binary|backbone|backbone-negative|backbone-positive|backbone-fixpoint|backbone-first|backbone-close|backbone-heavy|symmetry-backbone|backbone-symmetry|backbone-symmetry-scan|symmetry-sandwich|sweep|deep-sweep|kissat-order|kissat-order-negative|full] "
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
      !init_policy(&policy, solver, profile, maximum_variable) ||
      (input_search(profile) && !policy.primary_variables)) {
    tactix_release(solver);
    return 1;
  }
  printf("c cruxmiter-policy profile=%s search=%s phase=%s target=%s\n",
         profile_name(profile),
         manual_search(profile) ? "evsids/reluctant" : "default-policy",
         phase_name(profile), manual_search(profile) ? "off" : "default-policy");
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
  printf("c crux-symmetry-total calls=%" PRIu64 " applications=%" PRIu64
         " productive=%" PRIu64 "\n",
         policy.symmetry_calls, policy.symmetry_applications,
         policy.symmetry_productive);
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
