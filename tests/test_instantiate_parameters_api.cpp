#undef NDEBUG
#include "tactix.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

static int rule_id(const char *name) {
  for (int id = 0; id < tactix_rule_count(); ++id) {
    const char *candidate = tactix_rule_name(id);
    const char *a = candidate, *b = name;
    while (*a && *a == *b)
      ++a, ++b;
    if (!*a && !*b)
      return id;
  }
  return -1;
}

static void add(tactix *solver, const int *clause) {
  while (*clause)
    tactix_add(solver, *clause++);
  tactix_add(solver, 0);
}

int main() {
  tactix *solver = tactix_init();
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE) ==
         3);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT) == 1);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED) ==
         3);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT) == 0);
  assert(tactix_config(
             solver, TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 1);
  tactix_set_instantiate_limits(solver, 17, 19, 23, 1);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_MINIMUM_CLAUSE_SIZE) ==
         17);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_OCCURRENCE_LIMIT) == 19);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_MINIMUM_UNASSIGNED) ==
         23);
  assert(tactix_config(solver, TACTIX_CFG_INSTANTIATE_INCLUDE_REDUNDANT) == 1);
  tactix_set_instantiate_material_threshold(solver, 29);
  assert(tactix_config(
             solver, TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 29);
  // Updating the older four-field surface must retain the additive setting.
  tactix_set_instantiate_limits(solver, 31, 37, 41, 0);
  assert(tactix_config(
             solver, TACTIX_CFG_INSTANTIATE_NEW_MATERIAL_THRESHOLD) == 29);

  tactix_set_instantiate_limits(solver, UINT32_MAX, 0, UINT32_MAX, 0);
  const int candidate[] = {1, 2, 3, 0};
  const int left[] = {-1, 4, 0};
  const int right[] = {-1, -4, 0};
  add(solver, candidate);
  add(solver, left);
  add(solver, right);
  const int instantiate = rule_id("Instantiate");
  assert(instantiate >= 0);
  uint64_t legacy[4], semantic[4];
  assert(tactix_rule_instances(solver, instantiate, legacy, 4) == 0);
  const int count =
      tactix_rule_semantic_instances(solver, instantiate, semantic, 4);
  assert(count == 3);
  assert(tactix_apply_semantic_instance(solver, instantiate, semantic[0]) ==
         0);
  tactix_release(solver);
  std::puts("Instantiate typed parameters and semantic C API PASS");
}
