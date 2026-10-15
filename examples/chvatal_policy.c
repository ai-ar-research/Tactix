/* Public-API policy for symbreak's Chvatal-conjecture SAT subproblems.
 *
 * The CNF encodes seven cardinality constraints with independent totalizers
 * that all count the same y-literals in the same order, so most auxiliary
 * counter variables exist in seven structurally identical copies.  Clause
 * learning about one copy never transfers to the others.
 *
 * The policy finds such "twins" generically and merges them before handing
 * the smaller formula to the shipped DefaultPolicy:
 *
 *  - two variables are twins when the clauses containing their negative
 *    literals are identical after removing that literal and mapping already
 *    merged variables to their representative; repeating the test bottom-up
 *    finds the twins of twins;
 *  - for a twin b of representative a, the policy adds (a | -b) with witness
 *    {a} and (b | -a) with witness {b} through the public
 *    SubstitutionRedundancy rule, which validates every instance itself and
 *    records the witness in the DSR certificate;
 *  - Substitute then collapses the resulting equivalences, and tactix_solve
 *    runs DefaultPolicy on the merged formula.
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
#include <time.h>

struct formula {
  int *literals;       /* all clause literals, clause after clause */
  size_t *starts;      /* clause i is literals[starts[i] .. starts[i+1]) */
  size_t clauses, literal_count;
  int variables;
};

static int rule_id(const char *name) {
  int id;
  for (id = 0; id < tactix_rule_count(); ++id) {
    const char *candidate = tactix_rule_name(id);
    if (candidate && !strcmp(candidate, name)) return id;
  }
  return -1;
}

static int push(void **array, size_t *capacity, size_t count, size_t size) {
  if (count < *capacity) return 1;
  {
    const size_t next = *capacity ? 2 * *capacity : 1024;
    void *grown = realloc(*array, next * size);
    if (!grown) return 0;
    *array = grown;
    *capacity = next;
  }
  return 1;
}

static int read_dimacs(tactix *solver, const char *path, struct formula *f) {
  FILE *input = fopen(path, "r");
  static char line[1 << 20];
  size_t literal_capacity = 0, start_capacity = 0;
  int open_clause = 0, saw_header = 0;
  if (!input) {
    fprintf(stderr, "tactix-chvatal-policy: cannot open '%s': %s\n", path,
            strerror(errno));
    return 0;
  }
  memset(f, 0, sizeof *f);
  if (!push((void **)&f->starts, &start_capacity, 0, sizeof(size_t)))
    goto fail;
  f->starts[0] = 0;
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
        goto fail;
      saw_header = 1;
      if (variables > f->variables) f->variables = variables;
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
        goto fail;
      if (tactix_add_checked(solver, (int)value)) goto fail;
      if (value) {
        const int variable = value < 0 ? -(int)value : (int)value;
        if (variable > f->variables) f->variables = variable;
        if (!push((void **)&f->literals, &literal_capacity, f->literal_count,
                  sizeof(int)))
          goto fail;
        f->literals[f->literal_count++] = (int)value;
        open_clause = 1;
      } else {
        if (!push((void **)&f->starts, &start_capacity, f->clauses + 1,
                  sizeof(size_t)))
          goto fail;
        f->starts[++f->clauses] = f->literal_count;
        open_clause = 0;
      }
      cursor = end;
    }
  }
  if (ferror(input) || !saw_header || open_clause) goto fail;
  fclose(input);
  return 1;
fail:
  fprintf(stderr, "tactix-chvatal-policy: malformed or rejected DIMACS\n");
  fclose(input);
  return 0;
}

static uint64_t mix(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

static int compare_int(const void *left, const void *right) {
  const int a = *(const int *)left, b = *(const int *)right;
  return (a > b) - (a < b);
}

struct keyed {
  uint64_t key;
  int variable;
};

static int compare_keyed(const void *left, const void *right) {
  const struct keyed *a = (const struct keyed *)left;
  const struct keyed *b = (const struct keyed *)right;
  if (a->key != b->key) return a->key < b->key ? -1 : 1;
  return a->variable - b->variable;
}

struct pair {
  int representative, twin;
};

/* Bottom-up twin rounds.  Returns the merge pairs in a valid application
 * order (every pair's children are merged in an earlier round). */
static struct pair *find_twins(const struct formula *f, size_t *pair_count,
                               unsigned *rounds_out, unsigned **round_out) {
  const int n = f->variables;
  int *rep = (int *)malloc(((size_t)n + 1) * sizeof(int));
  uint64_t *signature = (uint64_t *)calloc((size_t)n + 1, sizeof(uint64_t));
  uint64_t *negatives = (uint64_t *)calloc((size_t)n + 1, sizeof(uint64_t));
  struct keyed *keyed = (struct keyed *)malloc(((size_t)n + 1) * sizeof *keyed);
  int *scratch = NULL;
  size_t scratch_capacity = 0, pair_capacity = 0, i;
  struct pair *pairs = NULL;
  unsigned *pair_round = NULL;
  size_t round_capacity = 0;
  unsigned rounds = 0;
  int v;
  *pair_count = 0;
  if (!rep || !signature || !negatives || !keyed) goto done;
  for (v = 0; v <= n; ++v) rep[v] = v;
  for (;;) {
    size_t merged = 0, count = 0;
    memset(signature, 0, ((size_t)n + 1) * sizeof(uint64_t));
    memset(negatives, 0, ((size_t)n + 1) * sizeof(uint64_t));
    for (i = 0; i < f->clauses; ++i) {
      const size_t begin = f->starts[i], end = f->starts[i + 1];
      const size_t size = end - begin;
      size_t j, k;
      if (size > scratch_capacity) {
        scratch_capacity = 2 * size;
        scratch = (int *)realloc(scratch, scratch_capacity * sizeof(int));
        if (!scratch) goto done;
      }
      for (j = begin; j < end; ++j) {
        const int literal = f->literals[j];
        const int variable = literal < 0 ? -literal : literal;
        if (literal > 0) continue;
        /* The clause without -variable, with representatives mapped in. */
        {
          size_t m = 0;
          uint64_t h = UINT64_C(1469598103934665603);
          for (k = begin; k < end; ++k) {
            const int other = f->literals[k];
            const int ov = other < 0 ? -other : other;
            if (k == j) continue;
            scratch[m++] = other < 0 ? -rep[ov] : rep[ov];
          }
          qsort(scratch, m, sizeof(int), compare_int);
          for (k = 0; k < m; ++k)
            if (!k || scratch[k] != scratch[k - 1])
              h = mix(h ^ (uint64_t)(uint32_t)scratch[k]);
          signature[variable] += mix(h);
          ++negatives[variable];
        }
      }
    }
    for (v = 1; v <= n; ++v)
      if (rep[v] == v && negatives[v]) {
        keyed[count].key = mix(signature[v] ^ mix(negatives[v]));
        keyed[count].variable = v;
        ++count;
      }
    qsort(keyed, count, sizeof *keyed, compare_keyed);
    for (i = 0; i < count;) {
      size_t j = i + 1;
      while (j < count && keyed[j].key == keyed[i].key) ++j;
      for (size_t k = i + 1; k < j; ++k) {
        if (!push((void **)&pairs, &pair_capacity, *pair_count,
                  sizeof *pairs))
          goto done;
        if (!push((void **)&pair_round, &round_capacity, *pair_count,
                  sizeof *pair_round))
          goto done;
        pairs[*pair_count].representative = keyed[i].variable;
        pairs[*pair_count].twin = keyed[k].variable;
        pair_round[*pair_count] = rounds;
        ++*pair_count;
        ++merged;
      }
      i = j;
    }
    /* Map after the round so every pair of this round sees only earlier
     * merges, exactly the order in which they are applied. */
    for (i = *pair_count - merged; i < *pair_count; ++i)
      rep[pairs[i].twin] = pairs[i].representative;
    if (!merged) break;
    ++rounds;
  }
done:
  *rounds_out = rounds;
  *round_out = pair_round;
  free(rep);
  free(signature);
  free(negatives);
  free(keyed);
  free(scratch);
  return pairs;
}

/* Adds both directions of every twin equivalence with SubstitutionRedundancy
 * and returns the number of accepted instances (-1 on an API error). */
static long long merge_twins(tactix *solver, const struct pair *pairs,
                             size_t pair_count, const unsigned *pair_round) {
  const size_t count = 2 * pair_count;
  tactix_online_sr_instance *instances;
  int *storage;
  tactix_online_step_v2 step;
  tactix_online_program_v2 program;
  tactix_online_result result;
  long long accepted = 0;
  size_t i;
  if (!count) return 0;
  instances = (tactix_online_sr_instance *)calloc(count, sizeof *instances);
  storage = (int *)malloc(count * 3 * sizeof(int));
  if (!instances || !storage) return -1;
  /* Within a round, every (a | -b) on the representative's pivot precedes
   * every (b | -a): a reverse clause contains -a and would otherwise become a
   * resolution partner of a later forward instance on pivot a. */
  {
    size_t out = 0, begin = 0;
    while (begin < pair_count) {
      size_t end = begin, j;
      while (end < pair_count && pair_round[end] == pair_round[begin]) ++end;
      for (int direction = 0; direction < 2; ++direction)
        for (j = begin; j < end; ++j) {
          const int a = pairs[j].representative, b = pairs[j].twin;
          int *c = storage + 3 * out;
          c[0] = direction ? b : a;
          c[1] = direction ? -a : -b;
          c[2] = c[0];
          instances[out].clause = c;
          instances[out].clause_count = 2;
          instances[out].witness = c + 2;
          instances[out].witness_count = 1;
          ++out;
        }
      begin = end;
    }
  }
  memset(&step, 0, sizeof step);
  step.base.step_id = 1;
  step.base.rule_id = rule_id("SubstitutionRedundancy");
  step.base.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
  step.base.parameters.value.substitution_redundancy.maximum_validation_ticks =
      UINT64_C(1) << 20;
  step.base.granularity = TACTIX_ONLINE_GRANULARITY_BATCH;
  step.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.base.repeat_limit = 1;
  step.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.base.budget_unlimited = 1;
  step.base.post_application = TACTIX_ONLINE_POST_NONE;
  step.sr_instances = instances;
  if (getenv("CHVATAL_OFFSET")) {
    const int offset = atoi(getenv("CHVATAL_OFFSET"));
    step.sr_instances = instances + offset;
    fprintf(stderr, "instance clause %d %d witness %d\n",
            instances[offset].clause[0], instances[offset].clause[1],
            instances[offset].witness[0]);
  }
  step.sr_instance_count = (int)count;
  if (getenv("CHVATAL_LIMIT"))
    step.sr_instance_count = atoi(getenv("CHVATAL_LIMIT"));
  memset(&program, 0, sizeof program);
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;
  if (step.base.rule_id < 0 ||
      tactix_online_configure_v2(solver, &program, 1,
                                 TACTIX_ONLINE_RECONFIGURE_RESET)) {
    free(instances);
    free(storage);
    return -1;
  }
  {
    uint64_t failures = 0, advances = 0;
    while (!tactix_online_complete(solver)) {
      const int code = tactix_online_advance(solver, &result);
      ++advances;
      if (code) {
        if (getenv("CHVATAL_DEBUG"))
          fprintf(stderr, "advance %" PRIu64 " code=%d status=%d instance=%"
                  PRIu64 " rule_result=%d\n", advances, code, result.status,
                  result.instance, result.rule_result);
        if (++failures > 16) break;
        continue;
      }
      if (getenv("CHVATAL_TRACE") && advances < 20)
        fprintf(stderr, "advance %" PRIu64 " status=%d instance=%" PRIu64
                " progress=%d apps=%" PRIu64 " rule_result=%d sel=%d\n",
                advances, result.status, result.instance,
                result.made_rule_progress, result.applications,
                result.rule_result, result.selection_complete);
      if (result.made_rule_progress) ++accepted;
      else if (getenv("CHVATAL_REJECTS") && result.instance < (uint64_t)count)
        fprintf(stderr, "reject %" PRIu64 " status=%d rr=%d clause %d %d\n",
                result.instance, result.status, result.rule_result,
                instances[result.instance].clause[0],
                instances[result.instance].clause[1]);
      if (result.status == TACTIX_ONLINE_INTERRUPTED) break;
    }
    if (getenv("CHVATAL_DEBUG"))
      fprintf(stderr, "advances=%" PRIu64 " failures=%" PRIu64 "\n",
              advances, failures);
  }
  free(instances);
  free(storage);
  return accepted;
}

static int terminal(int result) { return result == 10 || result == 20; }

/* Applies a rule until it stops changing the formula, propagating after
 * every application. */
static int saturate(tactix *solver, int rule, int propagate, unsigned cap) {
  unsigned count;
  for (count = 0; count < cap; ++count) {
    int result = tactix_apply(solver, rule);
    if (terminal(result)) return result;
    if (result != 0) return 1;
    result = tactix_apply(solver, propagate);
    if (terminal(result) || result == 2) return result;
  }
  return 1;
}

/* Collapses merged equivalences and drops the copies' now-duplicate
 * definitions so the next validation round sees a smaller formula. */
static int collapse(tactix *solver, int propagate) {
  int result = saturate(solver, rule_id("Substitute"), propagate, 64);
  if (terminal(result) || result == 2) return result;
  if (!getenv("CHVATAL_NO_SUBSUME")) {
    const int subsumed = tactix_apply(solver, rule_id("SubsumeForward"));
    if (terminal(subsumed)) return subsumed;
  }
  (void)tactix_support(solver, TACTIX_SUPPORT_SPARSE_COLLECT, 0);
  result = tactix_apply(solver, propagate);
  if (terminal(result) || result == 2) return result;
  (void)tactix_support(solver, TACTIX_SUPPORT_COLLECT_GARBAGE, 0);
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
  const char *cnf = NULL, *proof = NULL;
  int twins = 1, format = TACTIX_PROOF_DSR_BINARY, arg, result = 0;
  int per_round_off = getenv("CHVATAL_ONE_COLLAPSE") != NULL;
  int witnessed = 0;
  struct formula f;
  tactix *solver;
  for (arg = 1; arg < argc; ++arg) {
    const char *a = argv[arg];
    if (!strcmp(a, "--default")) twins = 0;
    else if (!strcmp(a, "--twins")) twins = 1;
    else if (!strcmp(a, "--dsr")) format = TACTIX_PROOF_DSR_BINARY;
    else if (!strcmp(a, "--drat")) format = TACTIX_PROOF_DRAT_BINARY;
    else if (a[0] == '-' && a[1]) {
      fprintf(stderr, "tactix-chvatal-policy: unknown option '%s'\n", a);
      return 1;
    } else if (!cnf) cnf = a;
    else if (!proof) proof = a;
    else return 1;
  }
  if (!cnf) {
    fprintf(stderr,
            "usage: tactix-chvatal-policy [--twins|--default] [--dsr|--drat] "
            "CNF [PROOF]\n");
    return 1;
  }
  solver = tactix_init();
  if (proof && tactix_proof(solver, proof, format)) {
    fprintf(stderr, "tactix-chvatal-policy: cannot create proof '%s'\n", proof);
    return 1;
  }
  if (!read_dimacs(solver, cnf, &f) ||
      tactix_set_guidance_publication(solver, TACTIX_GUIDANCE_NATIVE)) {
    tactix_release(solver);
    return 1;
  }
  if (tactix_status(solver) == 20) result = 20;
  if (!result && twins) {
    size_t pair_count;
    unsigned rounds, *pair_round = NULL;
    struct pair *pairs = find_twins(&f, &pair_count, &rounds, &pair_round);
    const int propagate = rule_id("Propagate");
    long long accepted;
    result = tactix_apply(solver, propagate);
    if (result == 2) result = tactix_apply(solver, rule_id("ConflictAnalysis"));
    if (!terminal(result)) {
      size_t begin = 0;
      accepted = 0;
      while (begin < pair_count && !terminal(result)) {
        size_t end = begin;
        long long round_accepted;
        while (end < pair_count && pair_round[end] == pair_round[begin]) ++end;
        round_accepted = merge_twins(solver, pairs + begin, end - begin,
                                     pair_round + begin);
        if (round_accepted < 0) break;
        accepted += round_accepted;
        result = tactix_apply(solver, propagate);
        if (result == 2)
          result = tactix_apply(solver, rule_id("ConflictAnalysis"));
        if (!terminal(result) && !per_round_off)
          result = collapse(solver, propagate);
        if (result == 2)
          result = tactix_apply(solver, rule_id("ConflictAnalysis"));
        if (getenv("CHVATAL_DEBUG"))
          fprintf(stderr, "round %u pairs=%zu accepted=%lld irredundant=%"
                  PRIu64 " substituted=%" PRIu64 " clock=%.1f\n",
                  pair_round[begin], end - begin, round_accepted,
                  tactix_stat(solver, TACTIX_STAT_IRREDUNDANT_CLAUSES),
                  tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS),
                  (double)clock() / CLOCKS_PER_SEC);
        begin = end;
      }
      witnessed = accepted > 0;
      printf("c chvatal-twins pairs=%zu rounds=%u accepted=%lld\n",
             pair_count, rounds, accepted);
      fflush(stdout);
      if (!terminal(result))
        result = collapse(solver, propagate);
      if (result == 2)
        result = tactix_apply(solver, rule_id("ConflictAnalysis"));
      printf("c chvatal-twins substituted=%" PRIu64 "\n",
             tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS));
      fflush(stdout);
    }
    free(pairs);
    free(pair_round);
    if (!terminal(result)) result = 0;
  }
  if (getenv("CHVATAL_MERGE_ONLY")) return 0;
  if (!result) result = tactix_solve(solver);
  /* The merge's SubstitutionRedundancy steps carry witnesses, so the DSR
   * certificate takes the witnessed (dsr-trim -> Trestle) route. */
  if (result == 20 && proof && format == TACTIX_PROOF_DSR_BINARY && witnessed)
    puts("c proof-route dsr-trestle");
  printf("c conflicts %" PRIu64 "\n",
         tactix_stat(solver, TACTIX_STAT_CONFLICTS));
  if (result == 10) {
    puts("s SATISFIABLE");
    print_model(solver, f.variables);
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(solver);
  free(f.literals);
  free(f.starts);
  return result == 10 || result == 20 ? result : 0;
}
