// tactix_cdcl_*: settings for the session's DefaultPolicy search episodes.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_strategy_config.hpp"

#include <cstring>

using namespace tactix;

struct tactix_cdcl {
  tactix_c_handle *session = nullptr;
  DefaultStrategyConfig settings;
};

namespace {
int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(tactix_rule_name(i), name)) return i;
  return -1;
}
} // namespace

extern "C" {
tactix_cdcl *tactix_cdcl_create(tactix_c_handle *s) {
  if (!s) return nullptr;
  auto *c = new tactix_cdcl;
  c->session = s;
  return c;
}

void tactix_cdcl_release(tactix_cdcl *c) { delete c; }

int tactix_cdcl_set(tactix_cdcl *c, const char *key, const char *value) {
  if (!c || !key || !value) return -1;
  DefaultStrategyConfig next = c->settings;
  if (!next.assign_search_setting(key, value) || !next.valid()) return -1;
  c->settings = next;
  return 0;
}

int tactix_cdcl_run(tactix_c_handle *s, tactix_cdcl *c, uint64_t conflicts) {
  if (!s || !c || c->session != s) return -1;
  const int status = tactix_status(s);
  if (status == 10 || status == 20) return status;
  const int result = api_detail::search_episode(s, c->settings, conflicts);
  if (result) return result;
  // Return to a propagated root; a root conflict refutes.
  static const int restart = rule("Restart"), propagate = rule("Propagate"),
                   analyze = rule("ConflictAnalysis");
  if (tactix_stat(s, TACTIX_STAT_LEVEL) && tactix_apply_instance(s, restart, 0) == 20)
    return 20;
  for (;;) {
    const int propagated = tactix_apply(s, propagate);
    if (propagated != 2) return propagated == 20 ? 20 : 0;
    if (tactix_apply(s, analyze) == 20) return 20;
  }
}
}
