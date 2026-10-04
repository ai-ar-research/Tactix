/* Arm-space coverage: every registry entry is exercised through its declared
 * public instance surface. Most use uint64 payloads; X5's variable-length
 * (clause,cube) tuple necessarily uses tactix_apply_condition. A rule without
 * a uint64 payload rejects an arbitrary payload (-1). */
#undef NDEBUG
#include "tactix.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_RULES 64
#define NINST 64

/* Entries with no uint64 payload surface: a numeric payload is invalid.
 * Condition instead has a structured public tuple surface below. */
static const char *const NO_SURFACE[] = {"Propagate", "SubstitutionRedundancy",
                                         "SwitchMode", "Condition", "ParityConsequence"};

/* Entries whose enumeration is exactly the canonical sentinel: an
 * `apply_instance` with nothing to choose between. Joining or leaving this set
 * is a deliberate act. Congruence is here by specification, not by omission:
 * V6's effect is the congruence closure, so a single merge is a prefix of one
 * application rather than an application (`N1-CONG-INSTANCES`, disposed in
 * docs/calculus.md §6 V6 and pinned by test_congruence.cpp). Factor left this
 * set when its seed-literal surface landed (`N1-FACTOR-INSTANCES`). */
static const char *const CANONICAL_ONLY[] = {"Congruence", "Propagate",
                                             "SubstitutionRedundancy",
                                             "SwitchMode", "Walk", "ParityConsequence"};

static int in_list(const char *const *list, size_t n, const char *name) {
  size_t i;
  for (i = 0; i < n; i++)
    if (!strcmp(list[i], name))
      return 1;
  return 0;
}

#define NO_SURFACE_P(nm) \
  in_list(NO_SURFACE, sizeof NO_SURFACE / sizeof NO_SURFACE[0], (nm))
#define CANONICAL_ONLY_P(nm) \
  in_list(CANONICAL_ONLY, sizeof CANONICAL_ONLY / sizeof CANONICAL_ONLY[0], (nm))

static int rule_id(const char *name) {
  int i;
  for (i = 0; i < tactix_rule_count(); i++)
    if (!strcmp(tactix_rule_name(i), name))
      return i;
  return -1;
}

static void add_clause(tactix *s, const int *lits) {
  for (; *lits; lits++)
    tactix_add(s, *lits);
  tactix_add(s, 0);
}

/* A level-0 formula with material for every inprocessing family, satisfiable
 * so no rule can refute it out from under the walk. */
static void fixture(tactix *s) {
  /* the same AND gate twice, so a congruence has something to merge */
  add_clause(s, (const int[]){-10, 1, 0});
  add_clause(s, (const int[]){-10, 2, 0});
  add_clause(s, (const int[]){10, -1, -2, 0});
  add_clause(s, (const int[]){-11, 1, 0});
  add_clause(s, (const int[]){-11, 2, 0});
  add_clause(s, (const int[]){11, -1, -2, 0});
  /* implication chain 5 -> 6 -> 7 with the transitive shortcut 5 -> 7 */
  add_clause(s, (const int[]){-5, 6, 0});
  add_clause(s, (const int[]){-6, 7, 0});
  add_clause(s, (const int[]){-5, 7, 0});
  /* 2x3 grid over factors {12,13} and remainders {14,15,16} */
  add_clause(s, (const int[]){12, 14, 0});
  add_clause(s, (const int[]){12, 15, 0});
  add_clause(s, (const int[]){12, 16, 0});
  add_clause(s, (const int[]){13, 14, 0});
  add_clause(s, (const int[]){13, 15, 0});
  add_clause(s, (const int[]){13, 16, 0});
  /* (17 18 19) subsumes; (21 22 23) and (21 22 -23) strengthen to (21 22) */
  add_clause(s, (const int[]){17, 18, 19, 20, 0});
  add_clause(s, (const int[]){17, 18, 19, 0});
  add_clause(s, (const int[]){21, 22, 23, 0});
  add_clause(s, (const int[]){21, 22, -23, 0});
  /* 24 occurs positively only: every clause holding it is blocked */
  add_clause(s, (const int[]){24, 25, 26, 0});
  add_clause(s, (const int[]){24, -25, 0});
}

/* A fresh solver where `id`'s guard holds; search-state entries need more
 * than a quiescent root. */
static FILE *structural_records;
static tactix *prepare(int id) {
  const char *name = tactix_rule_name(id);
  tactix *s = tactix_init();
  fixture(s);
  if (!strcmp(name, "ParityConsequence")) {
    structural_records = tmpfile(); assert(structural_records);
    assert(tactix_bind_structural_provenance(s, structural_records) == 0);
  }
  if (!strcmp(name, "Propagate")) {
    assert(tactix_decide(s, 5) == 0); /* a decision leaves the queue nonempty */
  } else if (!strcmp(name, "Restart")) {
    /* Exercise both noncanonical keep-levels; the root no-op is covered by
       the dedicated C API state-boundary test. */
    assert(tactix_decide(s, 5) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) != -1);
  } else if (!strcmp(name, "ConflictAnalysis")) {
    /* three decisions falsify (17 18 19) outright, so propagation reports the
       conflict analyze_guard needs */
    assert(tactix_decide(s, -17) == 0);
    assert(tactix_decide(s, -18) == 0);
    assert(tactix_decide(s, -19) == 0);
    assert(tactix_apply(s, rule_id("Propagate")) == 2 &&
           "the fixture must reach a conflict for Analyze's guard");
  }
  return s;
}

/* Per-rule coverage, accumulated across the fresh solvers below. */
static int hit_canonical[MAX_RULES];
static int hit_payload[MAX_RULES];
static int hit_structured[MAX_RULES];
static int enum_empty[MAX_RULES];
static int progressed;

/* Apply the entry, preferring a payload its own enumeration offered. */
static void drive(tactix *s, int id) {
  uint64_t insts[NINST];
  const char *name = tactix_rule_name(id);
  const int n = tactix_rule_instances(s, id, insts, NINST);
  assert(n >= 0 && "prepare() must reach a state where the guard holds");

  const int bare = (n == 1 && insts[0] == TACTIX_INSTANCE_CANONICAL);
  if (!strcmp(name, "Condition")) {
    const int clause[] = {24, 25, 26};
    const int witness[] = {24};
    assert(bare && "structured X5 has no uint64 tuple encoding");
    assert(tactix_apply_instance(s, id, TACTIX_INSTANCE_CANONICAL) == 1 &&
           "the X5 canonical placeholder must not guess a cube");
    assert(tactix_apply_condition(s, clause, 3, witness, 1) == 0 &&
           "the explicit X5 tuple must reach an effect");
    hit_structured[id] = 1;
    progressed++;
    return;
  }
  assert(bare == CANONICAL_ONLY_P(name) &&
         "the canonical-only set moved; see CANONICAL_ONLY");
  if (n == 0)
    enum_empty[id] = 1; /* an instance surface with nothing to offer here */

  /* the first payload that is not the sentinel: enumeration is ranked with the
     canonical instance first, so element 0 may legitimately be it */
  int pick = -1, i;
  for (i = 0; i < n && pick < 0; i++)
    if (insts[i] != TACTIX_INSTANCE_CANONICAL)
      pick = i;

  if (pick >= 0) {
    const int r = tactix_apply_instance(s, id, insts[pick]);
    assert(r != -1 && "the guard held and the payload came from instances()");
    progressed += (r == 0);
    hit_payload[id] = 1;
  } else {
    assert(bare || n == 0);
    const int r = tactix_apply_instance(s, id, TACTIX_INSTANCE_CANONICAL);
    assert(r != -1);
    progressed += (r == 0);
    hit_canonical[id] = 1;
  }
}

/* The payload contract, on its own solver: 7 is a legal keep-level for
 * Restart, a victim count for Reduce, a variable for BVE, and nonsense to a
 * closure. */
static void alias_contract(tactix *s, int id) {
  const char *name = tactix_rule_name(id);
  const uint64_t before = tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                          tactix_stat(s, TACTIX_STAT_PROPAGATIONS) +
                          tactix_stat(s, TACTIX_STAT_DECISIONS);
  const int r = tactix_apply_instance(s, id, 7);
  if (NO_SURFACE_P(name)) {
    assert(r == -1 && "PAYLOAD ALIASING: a payload on an entry with no "
                      "instance surface ran the canonical round instead of "
                      "failing");
    assert(before == tactix_stat(s, TACTIX_STAT_CONFLICTS) +
                         tactix_stat(s, TACTIX_STAT_PROPAGATIONS) +
                         tactix_stat(s, TACTIX_STAT_DECISIONS) &&
           "the rejected call still did work");
  } else {
    assert(r != -1 &&
           "an entry WITH an instance surface must interpret, clamp or no-op "
           "any payload; -1 is reserved for a failed guard / unknown rule");
  }
}

int main(void) {
  const int n = tactix_rule_count();
  int id, reached = 0, payload = 0, structured = 0;
  assert(n <= MAX_RULES);

  for (id = 0; id < n; id++) {
    /* a fresh solver per entry: a shared one lets an earlier rule's effect
       make a later guard false, so the run reads green over less */
    tactix *s = prepare(id);
    drive(s, id);
    tactix_release(s);
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
    tactix *t = prepare(id);
    alias_contract(t, id);
    tactix_release(t);
    if (structural_records) { fclose(structural_records); structural_records = NULL; }
    reached += (hit_canonical[id] || hit_payload[id] || hit_structured[id]);
    payload += hit_payload[id];
    structured += hit_structured[id];
  }

  /* Per entry: the chosen payload, structured tuple, or sentinel path ran. */
  for (id = 0; id < n; id++)
    printf("  %-18s %s%s\n", tactix_rule_name(id),
           hit_structured[id] ? "STRUCT "
                              : hit_payload[id] ? "PAYLOAD" : "CANON  ",
           enum_empty[id] ? "  (enumeration empty here)" : "");

  assert(reached == n &&
         "every registry entry must be reachable through its public action surface");
  assert(progressed > 0 &&
         "not one application made progress, so this run pinned the return "
         "convention and nothing else");
  printf("test_api_instances: %d/%d entries reached via public actions "
         "(%d through a chosen uint64 payload, %d structured), %d made progress\n",
         reached, n, payload, structured, progressed);
  return 0;
}
