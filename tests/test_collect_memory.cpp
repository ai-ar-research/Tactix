// Fixed collector fixtures verify observer boundaries without changing GC.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "occurrence_test_utils.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;

struct Capacities { size_t old_bytes, new_bytes; };
static Capacities fixture(bool dense_mode, bool empty) {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit dead[] = {internalize(1), internalize(2), internalize(3)};
  const Lit live[] = {internalize(4), internalize(5), internalize(6)};
  if (!empty) {
    const Ref doomed = kernel_add_large_clause(k, dead, 3, false, 0, 11);
    kernel_add_large_clause(k, live, 3, false, 0, 12);
    kernel_delete_clause(k, clause_at(k.arena, doomed));
  }
  OccIndex dense;
  if (dense_mode) {
    kernel_flush_large_watches(k);
    kernel_dense_connect(k, dense, true);
  }
  const size_t old_bytes = k.arena.capacity() * sizeof(Arena::value_type);
  const uint64_t generation = k.arena_generation;
  kernel_collect_garbage(k);
  assert(k.arena_generation == generation + 1);
  assert(k.arena.size() == (empty ? 0 : clause_words(3)));
  if (!empty) {
    assert(clause_at(k.arena, 0)->id() == 12);
    if (dense_mode)
      for (Lit lit : live)
        assert(large_occurrences(dense, lit).size() == 1 && large_occurrences(dense, lit)[0] == 0);
  }
  if (dense_mode) {
    kernel_dense_disconnect(k);
    kernel_rebuild_large_watches(k, false);
  }
  kernel_check_invariants(k);
  return {old_bytes, k.arena.capacity() * sizeof(Arena::value_type)};
}

int main() {
#ifdef TACTIX_ENABLE_EVENT_TRACE
  char path[] = "collect-memory-test-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  assert(setenv("TACTIX_SLOT_MEMORY_TRACE", path, 1) == 0);
#endif
  const std::vector<Capacities> capacities = {
      fixture(false, false), fixture(true, false), fixture(true, true)};
#ifdef TACTIX_ENABLE_EVENT_TRACE
  std::ifstream input(path);
  const std::vector<std::string> stages = {
      "entry", "before-reserve", "before-sort", "after-sort", "after-copy",
      "after-swap", "after-reindex", "exit"};
  std::string line;
  size_t row = 0, field_count = 0;
  while (std::getline(input, line)) {
    std::istringstream stream(line);
    std::string field;
    assert(std::getline(stream, field, '\t') && field == "Collect");
    assert(std::getline(stream, field, '\t') && field == stages[row % stages.size()]);
    assert(row / stages.size() < capacities.size());
    std::map<std::string, size_t> values;
    while (std::getline(stream, field, '\t')) {
      const auto equal = field.find('=');
      assert(equal != std::string::npos && equal + 1 < field.size());
      assert(field.find_first_not_of("0123456789", equal + 1) == std::string::npos);
      assert(values.emplace(field.substr(0, equal),
                            std::stoull(field.substr(equal + 1))).second);
    }
    if (!row) field_count = values.size();
    assert(values.size() == field_count);
    assert(values.count("current_rss_kib") && values.count("max_rss_kib"));
    assert(values.count("collector_order_capacity_bytes"));
    const auto c = capacities[row / stages.size()];
    const size_t stage = row % stages.size();
    const size_t temporary = stage < 2 || stage == 7 ? 0
                             : stage < 5 ? c.new_bytes : c.old_bytes;
    assert(values.at("collector_temporary_capacity_bytes") == temporary);
    row++;
  }
  assert(row == capacities.size() * stages.size());
  input.close();
  unsetenv("TACTIX_SLOT_MEMORY_TRACE");
  assert(std::remove(path) == 0);
#else
  assert(capacities.size() == 3);
#endif
}
