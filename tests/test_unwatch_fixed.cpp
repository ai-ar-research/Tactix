// Remember the linked library's build mode before enabling test assertions.
#ifdef NDEBUG
static constexpr bool library_assertions = false;
#else
static constexpr bool library_assertions = true;
#endif
#include "unit_test.hpp"
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned copies(const Kernel &k, Lit lit, Ref ref) {
  unsigned n = 0;
  for (const WatchEntry w : k.watches.entries(lit))
    n += !w.binary() && w.ref() == ref;
  return n;
}

static void fixed_slot(bool positive, bool flushed) {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Lit slot = internalize(positive ? 1 : -1);
  const Lit prefix[3] = {slot, internalize(2), internalize(3)};
  const Ref ref = kernel_add_large_clause(k, prefix, 3, false, 0);
  const Ref start = add_external_clause(k, {4, 5, 6}, true, 3);
  kernel_assign_unit(k, internalize(1));
  if (flushed) {
    kernel_delete_clause(k, clause_at(k.arena, start));
    kernel_collect_garbage(k, start);
  }
  assert(copies(k, slot, ref) == (flushed ? 0u : 1u));
  kernel_unwatch_large(k, slot, ref);
  assert(copies(k, slot, ref) == 0);
  assert(copies(k, internalize(2), ref) == 1);
  kernel_check_invariants(k);
}

static void missing_nonroot_remains_fatal(int assigned) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    const rlimit no_core{0, 0};
    assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
    close(STDERR_FILENO);
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref ref = kernel_add_large_clause(k, lits, 3, false, 0);
    if (assigned) kernel_assign_decision(k, internalize(assigned));
    kernel_unwatch_large(k, lits[0], ref);
    kernel_unwatch_large(k, lits[0], ref); // now absent, but NOT root-fixed
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

int main() {
  for (bool positive : {false, true})
    for (bool flushed : {false, true}) fixed_slot(positive, flushed);
  if (library_assertions)
    for (int assigned : {0, 1, -1}) missing_nonroot_remains_fatal(assigned);
  puts("test_unwatch_fixed OK");
}
