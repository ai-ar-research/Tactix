#include "policies/progress.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/strategy_settings.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

namespace {
constexpr uint64_t REPORT_INTERVAL_NANOS = 1000000000ull; // one line per second
}

void ProgressReport::start(const Kernel &k) {
  const char *setting = strategy_setting("TACTIX_VERBOSE");
  verbosity_ = setting ? std::atoi(setting) : 0;
  if (!verbosity_) return;
  start_nanos_ = last_nanos_ = now_nanos();
  std::printf("c progress: seconds conflicts decisions restarts reductions mode "
              "fixed-variables irredundant-clauses (of %u variables)\n", k.nvars);
  maybe_print(k, '*', true);
}

void ProgressReport::finish(const Kernel &k, int result) {
  if (!verbosity_) return;
  maybe_print(k, result == 10 ? '1' : result == 20 ? '0' : '?', true);
  verbosity_ = 0;
}

void ProgressReport::maybe_print(const Kernel &k, char event, bool force) {
  const uint64_t now = now_nanos();
  if (!force && now - last_nanos_ < REPORT_INTERVAL_NANOS) return;
  last_nanos_ = now;
  std::printf("c %c %8.2f %12llu %13llu %8llu %6llu %-7s %8llu %10llu\n", event,
              (now - start_nanos_) / 1e9,
              static_cast<unsigned long long>(k.stats.conflicts),
              static_cast<unsigned long long>(k.stats.decisions),
              static_cast<unsigned long long>(k.stats.restarts),
              static_cast<unsigned long long>(k.stats.reductions),
              k.h && k.h->stable ? "stable" : "focused",
              static_cast<unsigned long long>(k.stats.root_assignments),
              static_cast<unsigned long long>(k.live_irredundant_large));
  std::fflush(stdout);
}

} // namespace tactix
