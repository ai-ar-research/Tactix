// Actual old/new kernel fixture: a representation mismatch returns 1, no crash.
#include "kernel/kernel.hpp"
#include <cstdio>
using namespace tactix;
int main() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(4), internalize(5), 1);
  OccIndex dense;
  kernel_dense_connect(k, dense, true);
  kernel_flush_large_watches(k);
  const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
  const Ref ref = kernel_add_large_clause(k, lits, 3, false, 0, 2);
  unsigned sparse_words = 0, dense_entries = 0;
  for (const Lit l : lits) {
    sparse_words += k.watches.size(l);
    for (const Ref r : dense.large[l]) dense_entries += r == ref;
  }
  std::printf("tactix connected=%u sparse_words=%u dense_entries=%u\n",
              k.large_watches_connected, sparse_words, dense_entries);
  kernel_dense_disconnect(k);
  return sparse_words == 0 && dense_entries == 3 ? 0 : 1;
}
