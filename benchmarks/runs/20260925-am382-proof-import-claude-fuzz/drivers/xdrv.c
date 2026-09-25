/* AM372 public-C-API X5 Condition driver (campaign-owned; public API only).
 * Adapted from the AM369 owner driver; X5 tuples only through tactix_apply_condition.
 *
 * usage: xdrv CNF PROGRAM PROOF FMT ATTACH SURFACE SEED
 *   ATTACH: -1 no proof; K >= 0 attaches after K input clauses (K >= m: after
 *   all input). SURFACE: api | online (rule actions through one-step online
 *   programs; X5 tuples always use tactix_apply_condition).
 * PROGRAM lines (one op each; rule NAMES, resolved through tactix_rule_name):
 *   C nc c1..cnc nw w1..wnw  explicit tuple -> "op I cond RC st S obs CODES|CODES"
 *   M kind nc c.. nw w..     malformed call: kind 0 clause=NULL, 1 witness=NULL,
 *                            2 clause_count=-1, 3 witness_count=-1
 *   A Name                   canonical action
 *   I Name k                 k-th (mod count) enumerated instance
 *   S Name k                 k-th semantic instance (api surface only)
 *   L Name elit              literal instance TACTIX_LIT_INSTANCE(elit)
 *   K Name n                 raw instance n (Restart keep-level)
 *   D lit                    decide lit (0: first candidate)
 *   R n                      n seed-random legal actions (any applicable rule)
 *   P                        online canonical Condition step (presentation only)
 *   O                        observe: "roots I obs V lit.." (every true literal) when valid
 *   V                        tactix_solve; model on SAT
 *   Q                        tactix_status; model on SAT
 *   X                        list applicable rule ids
 * Root observation: outside a terminal state tactix_value returns the current
 * partial value's sign, so a literal is true iff value(l)==l and value(-l)==l,
 * false iff both == -l, else unassigned. Observations are flagged invalid
 * (obs 0) once any SAT terminal was seen (model reads replace trail reads).
 * Every SAT (from any op) prints "model OP v ..." over 1..nvars. */
#include "tactix.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tactix *S;
static int nvars, online, sat_seen;
static uint64_t revision, rng_state;

static unsigned below(unsigned n) {
  rng_state = rng_state * 6364136223846793005ull + 1442695040888963407ull;
  return n ? (unsigned)((rng_state >> 33) % n) : 0;
}

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static void print_model(int op) {
  sat_seen = 1;
  printf("model %d v", op);
  for (int v = 1; v <= nvars; v++)
    printf(" %d", tactix_value(S, v) > 0 ? v : -v);
  printf(" 0\n");
}

static char obs_code(int lit) {
  if (!lit || lit == INT_MIN || abs(lit) > nvars)
    return 'x';
  const int a = tactix_value(S, lit), b = tactix_value(S, -lit);
  if (a == lit && b == lit)
    return 't';
  if (a == -lit && b == -lit)
    return 'f';
  return 'u';
}

static int obs_valid(void) { return !sat_seen && tactix_status(S) == 0; }

static void terminal(int op, int rc) {
  if (rc != 10 && rc != 20)
    return;
  const int st = tactix_status(S);
  printf("term %d rc %d status %d\n", op, rc, st);
  if (st == 10)
    print_model(op);
}

static int online_rc(int adv, const tactix_online_result *r) {
  if (adv < 0)
    return -1;
  switch (r->status) {
  case TACTIX_ONLINE_CONFLICT: return 2;
  case TACTIX_ONLINE_SAT: return 10;
  case TACTIX_ONLINE_UNSAT: return 20;
  case TACTIX_ONLINE_COMPLETED:
  case TACTIX_ONLINE_SCOPED_FIXPOINT: return r->made_rule_progress ? 0 : 1;
  case TACTIX_ONLINE_NO_CHANGE: return 1;
  default: return -1;
  }
}

static int act(int id, int has_inst, uint64_t inst, int semantic, int *ostatus) {
  *ostatus = -100;
  if (id < 0)
    return -1;
  if (!online) {
    if (!has_inst)
      return tactix_apply(S, id);
    return semantic ? tactix_apply_semantic_instance(S, id, inst)
                    : tactix_apply_instance(S, id, inst);
  }
  tactix_online_step step;
  memset(&step, 0, sizeof step);
  step.step_id = ++revision;
  step.rule_id = id;
  if (has_inst && !semantic) {
    step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
    step.instances = &inst;
    step.instance_count = 1;
  } else {
    step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  }
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  if (!strcmp(tactix_rule_name(id), "Propagate")) {
    step.work_unit = TACTIX_ONLINE_WORK_PROPAGATIONS;
    step.budget_unlimited = 1;
  } else if (!strcmp(tactix_rule_name(id), "BVE")) {
    step.work_unit = TACTIX_ONLINE_WORK_BVE_TICKS;
    step.budget_unlimited = 1;
  } else {
    step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    step.budget = 1;
  }
  tactix_online_program program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  const int cfg = tactix_online_configure(S, &program, revision,
                                          TACTIX_ONLINE_RECONFIGURE_RESET);
  if (cfg) {
    *ostatus = -200 - (cfg < 0 ? -cfg : cfg);
    return -1;
  }
  tactix_online_result r;
  memset(&r, 0, sizeof r);
  const int adv = tactix_online_advance(S, &r);
  *ostatus = adv < 0 ? -300 : r.status;
  return online_rc(adv, &r);
}

static int read_lits(FILE *f, int **out, int *count) {
  if (fscanf(f, "%d", count) != 1 || *count < 0 || *count > 1000000)
    return 0;
  *out = malloc(sizeof(int) * (size_t)(*count ? *count : 1));
  for (int i = 0; i < *count; i++)
    if (fscanf(f, "%d", &(*out)[i]) != 1)
      return 0;
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 8)
    return 2;
  const char *cnf = argv[1], *prog = argv[2], *proof = argv[3];
  const int fmt = atoi(argv[4]), attach = atoi(argv[5]);
  online = !strcmp(argv[6], "online");
  rng_state = strtoull(argv[7], NULL, 10) * 2654435761ull + 17;
  FILE *in = fopen(cnf, "r");
  if (!in)
    return 2;
  S = tactix_init();
  setvbuf(stdout, NULL, _IOLBF, 0);
  char line[1 << 16];
  int clauses = 0, attached = 0, lit;
  if (attach == 0) {
    printf("attach %d\n", tactix_proof(S, proof, fmt));
    attached = 1;
  }
  while (fscanf(in, " %c", line) == 1) {
    if (line[0] == 'c' || line[0] == 'p') {
      if (line[0] == 'p') {
        int m;
        if (fscanf(in, " cnf %d %d", &nvars, &m) != 2)
          return 2;
      } else if (!fgets(line, sizeof line, in))
        break;
      continue;
    }
    ungetc(line[0], in);
    if (fscanf(in, "%d", &lit) != 1)
      break;
    tactix_add(S, lit);
    if (!lit && ++clauses == attach && attach > 0) {
      printf("attach %d\n", tactix_proof(S, proof, fmt));
      attached = 1;
    }
  }
  fclose(in);
  if (attach > 0 && !attached) {
    printf("attach %d\n", tactix_proof(S, proof, fmt));
    attached = 1;
  }
  FILE *p = fopen(prog, "r");
  if (!p)
    return 2;
  const int condition = rule_id("Condition");
  char op[16], name[64];
  for (int i = 0; fscanf(p, "%15s", op) == 1; i++) {
    if (!strcmp(op, "C") || !strcmp(op, "M")) {
      int kind = -1, *c = NULL, *w = NULL, nc = 0, nw = 0;
      if (op[0] == 'M' && fscanf(p, "%d", &kind) != 1)
        return 2;
      if (!read_lits(p, &c, &nc) || !read_lits(p, &w, &nw))
        return 2;
      const int st = tactix_status(S), valid = obs_valid();
      char codes[2 * 4096 + 4];
      int n = 0;
      for (int j = 0; j < nc && n < 4096; j++)
        codes[n++] = valid ? obs_code(c[j]) : '?';
      codes[n++] = '|';
      for (int j = 0; j < nw && n < 2 * 4096; j++)
        codes[n++] = valid ? obs_code(w[j]) : '?';
      codes[n] = 0;
      const int *cp = kind == 0 ? NULL : c, *wp = kind == 1 ? NULL : w;
      const int cc = kind == 2 ? -1 : nc, wc = kind == 3 ? -1 : nw;
      const int rc = tactix_apply_condition(S, cp, cc, wp, wc);
      printf("op %d %s %d st %d obs %d %s\n", i, op[0] == 'C' ? "cond" : "malformed", rc, st, valid,
             codes);
      free(c);
      free(w);
    } else if (!strcmp(op, "A") || !strcmp(op, "I") || !strcmp(op, "S") ||
               !strcmp(op, "L") || !strcmp(op, "K")) {
      long long k = 0;
      if (fscanf(p, "%63s", name) != 1 || (op[0] != 'A' && fscanf(p, "%lld", &k) != 1))
        return 2;
      const int rid = rule_id(name);
      if (rid < 0)
        return 3;
      int os, rc;
      if (op[0] == 'A') {
        rc = act(rid, 0, 0, 0, &os);
        printf("op %d apply %s rc %d os %d\n", i, name, rc, os);
      } else if (op[0] == 'L' || op[0] == 'K') {
        const uint64_t inst = op[0] == 'L' ? TACTIX_LIT_INSTANCE((int)k) : (uint64_t)k;
        rc = act(rid, 1, inst, 0, &os);
        printf("op %d lit %s %lld rc %d os %d\n", i, name, k, rc, os);
      } else {
        const int semantic = op[0] == 'S' && !online;
        uint64_t inst[4096];
        const int m = semantic ? tactix_rule_semantic_instances(S, rid, inst, 4096)
                               : tactix_rule_instances(S, rid, inst, 4096);
        if (m <= 0) {
          printf("op %d inst %s none %d\n", i, name, m);
          continue;
        }
        const uint64_t chosen = inst[(unsigned long long)k % (unsigned)m];
        rc = act(rid, chosen != TACTIX_INSTANCE_CANONICAL, chosen, semantic, &os);
        printf("op %d inst %s %llu/%d rc %d os %d\n", i, name, (unsigned long long)chosen, m, rc, os);
      }
      terminal(i, rc);
    } else if (!strcmp(op, "D")) {
      int l;
      if (fscanf(p, "%d", &l) != 1)
        return 2;
      if (!l) {
        int cand[8];
        l = tactix_decide_candidates(S, cand, 8) > 0 ? cand[0] : 1;
      }
      int rc, os = -100;
      if (online) {
        rc = act(rule_id("Decide"), 1, TACTIX_LIT_INSTANCE(l), 0, &os);
        rc = rc == 0 ? 0 : -1;
      } else
        rc = tactix_decide(S, l);
      printf("op %d decide %d rc %d os %d\n", i, l, rc, os);
    } else if (!strcmp(op, "R")) {
      int n;
      if (fscanf(p, "%d", &n) != 1)
        return 2;
      for (int j = 0; j < n; j++) {
        int ids[64];
        const int m = tactix_rules_applicable(S, ids, 64);
        if (m <= 0) {
          printf("op %d rand %d none\n", i, j);
          break;
        }
        const int id = ids[below((unsigned)m)];
        int os, rc;
        if (!strcmp(tactix_rule_name(id), "Decide")) {
          int cand[64];
          const int mc = tactix_decide_candidates(S, cand, 64);
          if (mc <= 0) {
            printf("op %d rand %d decide none\n", i, j);
            continue;
          }
          int l = cand[below((unsigned)mc)];
          if (below(2))
            l = -l;
          if (online) {
            rc = act(id, 1, TACTIX_LIT_INSTANCE(l), 0, &os);
            rc = rc == 0 ? 0 : -1;
          } else
            rc = tactix_decide(S, l), os = -100;
          printf("op %d rand %d decide %d rc %d os %d\n", i, j, l, rc, os);
          continue;
        }
        uint64_t inst[64];
        const int semantic = !online && below(2);
        const int mi = semantic ? tactix_rule_semantic_instances(S, id, inst, 64)
                                : tactix_rule_instances(S, id, inst, 64);
        if (mi > 0 && below(2)) {
          const uint64_t chosen = inst[below((unsigned)mi)];
          rc = act(id, chosen != TACTIX_INSTANCE_CANONICAL, chosen, semantic, &os);
          printf("op %d rand %d %s %s %llu/%d rc %d os %d\n", i, j, tactix_rule_name(id),
                 semantic ? "sem" : "inst", (unsigned long long)chosen, mi, rc, os);
        } else {
          rc = act(id, 0, 0, 0, &os);
          printf("op %d rand %d %s canon rc %d os %d\n", i, j, tactix_rule_name(id), rc, os);
        }
        terminal(i, rc);
        if (rc == 10 || rc == 20)
          break;
      }
    } else if (!strcmp(op, "P")) {
      int os;
      const int save = online;
      online = 1;
      const int rc = act(condition, 0, 0, 0, &os);
      online = save;
      printf("op %d present rc %d os %d\n", i, rc, os);
      terminal(i, rc);
    } else if (!strcmp(op, "O")) {
      const int valid = obs_valid();
      printf("roots %d obs %d", i, valid);
      if (valid)
        for (int v = 1; v <= nvars; v++) {
          const char code = obs_code(v);
          if (code == 't')
            printf(" %d", v);
          else if (code == 'f')
            printf(" %d", -v);
        }
      printf("\n");
    } else if (!strcmp(op, "V")) {
      const int rc = tactix_solve(S);
      printf("op %d solve %d\n", i, rc);
      terminal(i, rc);
    } else if (!strcmp(op, "Q")) {
      const int st = tactix_status(S);
      printf("op %d status %d\n", i, st);
      if (st == 10)
        print_model(i);
    } else if (!strcmp(op, "X")) {
      int ids[64];
      const int m = tactix_rules_applicable(S, ids, 64);
      int has = 0;
      for (int j = 0; j < m; j++)
        has |= ids[j] == condition;
      printf("op %d applicable n %d condition %d\n", i, m, has);
    } else {
      return 2;
    }
  }
  fclose(p);
  const int st = tactix_status(S);
  printf("final %d\n", st);
  if (st == 10) {
    print_model(-1);
    /* repeated model read: a second read must be identical */
    print_model(-2);
  }
  printf("stats conflicts %llu decisions %llu irredundant %llu fixed %llu bce %llu elim %llu\n",
         (unsigned long long)tactix_stat(S, TACTIX_STAT_CONFLICTS),
         (unsigned long long)tactix_stat(S, TACTIX_STAT_DECISIONS),
         (unsigned long long)tactix_stat(S, TACTIX_STAT_IRREDUNDANT_CLAUSES),
         (unsigned long long)tactix_stat(S, TACTIX_STAT_ROOT_FIXED),
         (unsigned long long)tactix_stat(S, TACTIX_STAT_BCE_DELETED),
         (unsigned long long)tactix_stat(S, TACTIX_STAT_BVE_ELIMINATED));
  tactix_release(S);
  printf("released\n");
  return 0;
}
