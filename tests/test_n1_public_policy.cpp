// Public C API regressions: policy disabling/protection cannot hide legal acts.
#undef NDEBUG
#define tactix tactix_handle
#include "tactix.h"
#undef tactix
#include "policies/default_inprocess.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(tactix_rule_name(i), name))
      return i;
  assert(false);
  return -1;
}

static void clause(tactix_handle *s, std::initializer_list<int> lits) {
  for (int lit : lits)
    tactix_add(s, lit);
  tactix_add(s, 0);
}

static void sweep(bool disabled, bool explicit_seed) {
  if (disabled)
    setenv("TACTIX_NO_SWEEP", "1", 1);
  else
    unsetenv("TACTIX_NO_SWEEP");
  tactix_handle *s = tactix_init();
  clause(s, {1, 2});
  clause(s, {1, -2});
  const int id = rule("Sweep");
  uint64_t choices[8];
  const int count = tactix_rule_instances(s, id, choices, 8);
  assert(count == 2);
  const int result = explicit_seed ? tactix_apply_instance(s, id, choices[0])
                                   : tactix_apply(s, id);
  assert(result == 0 && tactix_stat(s, TACTIX_STAT_SWEEP_TICKS) > 0);
  assert(tactix_stat(s, TACTIX_STAT_ROOT_FIXED) >= 1);
  tactix_release(s);
  unsetenv("TACTIX_NO_SWEEP");
}

static void reduce(bool locked) {
  tactix_handle *s = tactix_init();
  clause(s, {-1, -3, 4});
  clause(s, {-2, -3, 5});
  clause(s, {-4, -5});
  for (int lit : {1, 2}) {
    assert(tactix_decide(s, lit) == 0);
    assert(tactix_apply(s, rule("Propagate")) == 0);
  }
  assert(tactix_decide(s, 3) == 0);
  assert(tactix_apply(s, rule("Propagate")) == 2);
  assert(tactix_apply(s, rule("ConflictAnalysis")) == 0);
  assert(tactix_stat(s, TACTIX_STAT_LEARNED) == 1);
  if (!locked)
    assert(tactix_apply_instance(s, rule("Restart"), 0) == 0);
  uint64_t choices[16];
  const int count =
      tactix_rule_semantic_instances(s, rule("Reduce"), choices, 16);
  constexpr uint64_t clause_tag = uint64_t{1} << 63;
  int named = 0;
  for (int i = 0; i < count; ++i)
    if ((choices[i] & clause_tag) &&
        choices[i] != TACTIX_INSTANCE_CANONICAL) {
      ++named;
      if (!locked)
        assert(tactix_apply_semantic_instance(s, rule("Reduce"), choices[i]) ==
               0);
    }
  assert(named == (locked ? 0 : 1));
  // Malformed tagged snapshots must not dereference arbitrary arena offsets.
  assert(tactix_apply_semantic_instance(
             s, rule("Reduce"), clause_tag | (uint64_t{1} << 40)) == 1);
  tactix_release(s);
}

int main(int argc, char **argv) {
  if (argc == 1 || !std::strcmp(argv[1], "sweep")) {
    for (bool disabled : {false, true}) {
      for (bool explicit_seed : {false, true})
        sweep(disabled, explicit_seed);
      tactix::Kernel k;
      tactix::DefaultStrategyConfig config;
      config.inprocess.sweep_enabled = !disabled;
      tactix::DefaultInprocessScheduler scheduler(&config.inprocess);
      scheduler.build(k);
      bool found = false;
      for (const auto &entry : scheduler.entries)
        if (!std::strcmp(tactix_rule_name(entry.rule), "Sweep")) {
          found = true;
          assert(entry.enabled == !disabled);
        }
      assert(found);
    }
  }
  if (argc == 1 || !std::strcmp(argv[1], "reduce")) {
    reduce(true);
    reduce(false);
  }
  std::puts("N1 public Sweep disable and Reduce protection/reason-lock controls PASS");
}
