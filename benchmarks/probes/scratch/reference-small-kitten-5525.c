// Fixed reference fixtures, linked against the immutable native Kitten objects.
#include "internal.h"
#include "kitten.h"
#include <inttypes.h>
#include <stdio.h>

int main(void) {
  kissat *solver = kissat_init();
  kitten *embedded = kitten_embedded(solver);
  kitten_track_antecedents(embedded);
  const unsigned widths[] = {2, 3, 17, 270};
  unsigned ordinal = 0;
  for (unsigned shape = 0; shape < 4; shape++) {
    unsigned n = widths[shape];
    for (unsigned repetition = 0; repetition < 3; repetition++, ordinal++) {
      if (ordinal)
        kitten_clear(embedded);
      unsigned literals[270];
      for (unsigned i = 0; i < n; i++)
        literals[i] = 2 * i;
      uint64_t start = solver->statistics.kitten_ticks;
      kitten_clause_with_id_and_exception(embedded, 0, n, literals, UINT32_MAX);
      kitten_randomize_phases(embedded);
      int status = kitten_solve(embedded);
      if (status != 10)
        return 2;
      uint64_t initial = solver->statistics.kitten_ticks - start;
      unsigned flips = 0;
      for (unsigned i = 0; i < n; i++)
        flips += kitten_flip_literal(embedded, 2 * i);
      uint64_t model = 1469598103934665603ull;
      for (unsigned i = 0; i < n; i++)
        model = (model ^ (kitten_value(embedded, 2 * i) > 0 ? 1 : 2)) * 1099511628211ull;
      printf("%u %u %" PRIu64 " %" PRIu64 " %u %" PRIu64 " %" PRIu64 "\n",
             n, repetition, initial, solver->statistics.kitten_ticks - start,
             flips, kitten_trace_generator(embedded), model);
    }
  }
  kitten_release(embedded);
  kissat_release(solver);
  return 0;
}
