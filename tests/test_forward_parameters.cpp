// Canonical strategy limits must not restrict explicit legal C1/C2 actions.
#undef NDEBUG
#include "tactix.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

static int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(tactix_rule_name(i), name))
      return i;
  assert(false);
  return -1;
}

static void add(tactix *s, std::initializer_list<int> lits) {
  for (int lit : lits)
    tactix_add(s, lit);
  tactix_add(s, 0);
}

static bool applicable(tactix *s, int id) {
  std::vector<int> ids(tactix_rule_count());
  const int count = tactix_rules_applicable(s, ids.data(), ids.size());
  return std::find(ids.begin(), ids.begin() + count, id) != ids.begin() + count;
}

static void readback() {
  tactix *s = tactix_init();
  const uint64_t defaults[] = {1000, 100, 10000, 1000, 1000};
  for (int i = 0; i < 5; ++i)
    assert(tactix_config(s, TACTIX_CFG_SUBSUME_CLAUSE_SIZE + i) ==
           defaults[i]);
  add(s, {1, 2, 3});
  for (bool nonroot : {false, true}) {
    if (nonroot)
      assert(tactix_decide(s, 1) == 0);
    for (uint32_t n : {0u, 1u, 999u, 1000u, 1001u, UINT32_MAX}) {
      tactix_set_subsume_limits(s, n, n, n);
      tactix_set_strengthen_limits(s, n, n);
      for (int i = 0; i < 5; ++i)
        assert(tactix_config(s, TACTIX_CFG_SUBSUME_CLAUSE_SIZE + i) == n);
      assert(applicable(s, rule("SubsumeForward")) == !nonroot);
      assert(applicable(s, rule("Strengthen")) == !nonroot);
    }
  }
  tactix_release(s);
}

static void canonical(bool c2, int field, uint32_t value, bool changed) {
  tactix *s = tactix_init();
  if (field == 2) {
    add(s, {1, 2});
    add(s, {1, 2, 3});
  } else {
    add(s, {1, 2, 3});
    add(s, {1, 2, 3, 4});
  }
  uint32_t limits[] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
  limits[field] = value;
  if (c2)
    tactix_set_strengthen_limits(s, limits[0], limits[1]);
  else
    tactix_set_subsume_limits(s, limits[0], limits[1], limits[2]);
  assert(tactix_apply(s, rule(c2 ? "Strengthen" : "SubsumeForward")) ==
         (changed ? 0 : 1));
  tactix_release(s);
}

static void explicit_size(bool c2, unsigned size, bool restricted) {
  tactix *s = tactix_init();
  if (c2)
    add(s, {1, -2});
  else
    add(s, {1, 2});
  for (unsigned i = 1; i <= size; ++i)
    tactix_add(s, static_cast<int>(i));
  tactix_add(s, 0);
  if (restricted) {
    tactix_set_subsume_limits(s, 0, 0, 0);
    tactix_set_strengthen_limits(s, 0, 0);
  }
  const int id = rule(c2 ? "Strengthen" : "SubsumeForward");
  uint64_t before[4], again[4];
  assert(tactix_rule_instances(s, id, before, 0) == 0);
  assert(tactix_rule_instances(s, id, before, 4) == 1);
  assert(tactix_rule_instances(s, id, again, 4) == 1 &&
         before[0] == again[0]);
  assert(tactix_apply_instance(s, id, before[0]) == 0);
  tactix_release(s);
}

static void explicit_population(bool c2, bool binary) {
  tactix *s = tactix_init();
  const unsigned copies = binary ? 10001 : 1001;
  for (unsigned i = 0; i < copies; ++i) {
    if (binary)
      add(s, {1, 2});
    else
      add(s, {1, 2, 3});
  }
  add(s, {1, 2, 3, 4});
  const int id = rule(c2 ? "Strengthen" : "SubsumeForward");
  std::vector<uint64_t> choices(copies + 1);
  const int count =
      tactix_rule_instances(s, id, choices.data(), choices.size());
  assert(count == static_cast<int>(binary ? 1 : copies + 1));
  assert(tactix_apply_instance(s, id, choices[count - 1]) == 0);
  tactix_release(s);
}

int main() {
  readback();
  for (bool c2 : {false, true}) {
    for (uint32_t n : {0u, 3u})
      canonical(c2, 0, n, false);
    for (uint32_t n : {4u, 5u, UINT32_MAX})
      canonical(c2, 0, n, true);
    canonical(c2, 1, 0, false);
    canonical(c2, 1, UINT32_MAX, true);
    for (unsigned size : {999u, 1000u, 1001u})
      for (bool restricted : {false, true})
        explicit_size(c2, size, restricted);
    for (bool binary : {false, true})
      explicit_population(c2, binary);
  }
  for (uint32_t n : {0u, 1u, 2u, UINT32_MAX})
    canonical(false, 2, n, n != 0);
  std::puts("Strategy forward limits and uncapped explicit C1/C2 surface PASS");
}
