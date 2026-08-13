#include "rules/support/flat_occurrence.hpp"

namespace tactix {

void FlatOccurrenceIndex::build(const Kernel &k,
                                const std::vector<Ref> &clauses) {
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  offsets.assign(nlits + 1, 0);
  size_t entries = 0;
  for (const Ref ref : clauses) {
    const Clause *clause = clause_at(k.arena, ref);
    entries += clause->size;
    for (const Lit lit : *clause)
      offsets[static_cast<size_t>(lit) + 1]++;
  }
  for (size_t lit = 1; lit <= nlits; lit++)
    offsets[lit] += offsets[lit - 1];
  refs.resize(entries);
  std::vector<uint32_t> next(offsets.begin(), offsets.end() - 1);
  for (const Ref ref : clauses)
    for (const Lit lit : *clause_at(k.arena, ref))
      refs[next[lit]++] = ref;
}

} // namespace tactix
